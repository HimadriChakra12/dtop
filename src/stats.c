#include "../config.h"

static unsigned int get_sector_size(const char *name) {
    char path[128];
    snprintf(path, sizeof(path), "/sys/block/%s/queue/hw_sector_size", name);
    FILE *fp = fopen(path, "r");
    if (!fp) return 512;
    unsigned int size = 512;
    fscanf(fp, "%u", &size);
    fclose(fp);
    return size > 0 ? size : 512;
}

int send_signal_to_process(int pid, int sig) {
    if (pid <= 0) return -1;
    return kill(pid, sig);
}

void parse_cpu_stats(void) {
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) return;
    
    char line[256];
    
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "cpu", 3) != 0) continue;
        
        CoreStat *core = NULL;
        if (line[3] == ' ') {
            core = &g_stats.overall;
        } else {
            int n;
            if (sscanf(line, "cpu%d", &n) == 1 && n < MAX_CPU_CORES) {
                core = &g_stats.cores[n];
                if (n >= g_stats.num_cores) g_stats.num_cores = n + 1;
            } else {
                continue;
            }
        }
        
        if (!core) continue;
        
        unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
        sscanf(line, "%*s %llu %llu %llu %llu %llu %llu %llu %llu",
               &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);
        
        unsigned long long total = user + nice + system + idle + iowait + irq + softirq + steal;
        unsigned long long idle_time = idle + iowait;
        
        if (core->prev_total > 0) {
            unsigned long long total_diff = total - core->prev_total;
            unsigned long long idle_diff = idle_time - core->prev_idle;
            if (total_diff > 0) {
                core->percent = ((total_diff - idle_diff) * 100.0f) / total_diff;
            }
        }
        
        core->history[core->history_idx] = core->percent;
        core->history_idx = (core->history_idx + 1) % HISTORY_SIZE;
        
        core->prev_total = total;
        core->prev_idle = idle_time;
    }
    
    g_stats.cpu_history[g_stats.history_index] = (int)g_stats.overall.percent;
    fclose(fp);
}

void parse_meminfo(void) {
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) return;
    
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        unsigned long val;
        if (sscanf(line, "MemTotal: %lu", &val) == 1) {
            g_stats.total_mem = val;
        } else if (sscanf(line, "MemFree: %lu", &val) == 1) {
            g_stats.free_mem = val;
        } else if (sscanf(line, "MemAvailable: %lu", &val) == 1) {
            g_stats.available_mem = val;
        } else if (sscanf(line, "Buffers: %lu", &val) == 1) {
            g_stats.buffers = val;
        } else if (sscanf(line, "Cached: %lu", &val) == 1) {
            g_stats.cached = val;
        } else if (sscanf(line, "SwapTotal: %lu", &val) == 1) {
            g_stats.swap_total = val;
        } else if (sscanf(line, "SwapFree: %lu", &val) == 1) {
            g_stats.swap_free = val;
        }
    }
    fclose(fp);
    
    if (g_stats.total_mem > 0) {
        unsigned long used = g_stats.total_mem - g_stats.available_mem;
        g_stats.mem_percent = (used * 100.0f) / g_stats.total_mem;
    }
    
    if (g_stats.swap_total > 0) {
        unsigned long used = g_stats.swap_total - g_stats.swap_free;
        g_stats.swap_percent = (used * 100.0f) / g_stats.swap_total;
    }
    
    g_stats.mem_history[g_stats.history_index] = (int)g_stats.mem_percent;
}

void parse_net_stats(void) {
    FILE *fp = fopen("/proc/net/dev", "r");
    if (!fp) return;
    
    char line[512];
    unsigned long long total_rx = 0, total_tx = 0;
    
    fgets(line, sizeof(line), fp);
    fgets(line, sizeof(line), fp);
    
    while (fgets(line, sizeof(line), fp)) {
        char iface[32];
        unsigned long long rx_bytes, tx_bytes;
        
        sscanf(line, "%s %llu %*u %*u %*u %*u %*u %*u %*u %llu",
               iface, &rx_bytes, &tx_bytes);
        
        if (strcmp(iface, "lo:") == 0) continue;
        
        total_rx += rx_bytes;
        total_tx += tx_bytes;
    }
    fclose(fp);
    
    if (g_stats.prev_net_rx > 0) {
        g_stats.net_rx_speed = (total_rx - g_stats.prev_net_rx) / 1024.0f;
        g_stats.net_tx_speed = (total_tx - g_stats.prev_net_tx) / 1024.0f;
    }
    
    g_stats.net_history_rx[g_stats.history_index] = (int)(g_stats.net_rx_speed / 100);
    g_stats.net_history_tx[g_stats.history_index] = (int)(g_stats.net_tx_speed / 100);
    
    g_stats.prev_net_rx = total_rx;
    g_stats.prev_net_tx = total_tx;
}

void parse_disk_stats(void) {
    FILE *fp = fopen("/proc/diskstats", "r");
    if (!fp) return;
    
    char line[256];
    DiskInfo new_disks[MAX_DISKS];
    int new_disk_count = 0;
    
    while (fgets(line, sizeof(line), fp) && new_disk_count < MAX_DISKS) {
        char name[32];
        unsigned long long read_sectors, write_sectors;
        
        if (sscanf(line, "%*d %*d %s %*u %*u %llu %*u %*u %*u %llu",
                   name, &read_sectors, &write_sectors) == 3) {
            
            if (strncmp(name, "loop", 4) == 0 || 
                strncmp(name, "ram", 3) == 0 ||
                strncmp(name, "dm-", 3) == 0) continue;
            
            unsigned int sector_size = get_sector_size(name);
            
            DiskInfo *disk = &new_disks[new_disk_count];
            memset(disk, 0, sizeof(DiskInfo));
            // Around line 166, change from:
            strncpy(disk->name, name, sizeof(disk->name) - 1);
            disk->name[sizeof(disk->name) - 1] = '\0';

            for (int i = 0; i < g_stats.num_disks; i++) {
                if (strcmp(g_stats.disks[i].name, name) == 0) {
                    unsigned long long read_diff = (read_sectors - g_stats.disks[i].read_sectors) * sector_size;
                    unsigned long long write_diff = (write_sectors - g_stats.disks[i].write_sectors) * sector_size;
                    disk->read_speed = read_diff / 1024.0f;
                    disk->write_speed = write_diff / 1024.0f;
                    memcpy(disk->history_rx, g_stats.disks[i].history_rx, sizeof(disk->history_rx));
                    memcpy(disk->history_tx, g_stats.disks[i].history_tx, sizeof(disk->history_tx));
                    break;
                }
            }
            
            disk->read_sectors = read_sectors;
            disk->write_sectors = write_sectors;
            disk->history_rx[g_stats.history_index] = (int)(disk->read_speed / 100);
            disk->history_tx[g_stats.history_index] = (int)(disk->write_speed / 100);
            
            new_disk_count++;
        }
    }
    fclose(fp);
    
    memcpy(g_stats.disks, new_disks, sizeof(new_disks[0]) * new_disk_count);
    g_stats.num_disks = new_disk_count;
}

void parse_battery(void) {
    DIR *dir = opendir("/sys/class/power_supply");
    if (!dir) {
        g_stats.battery_present = 0;
        return;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "BAT", 3) != 0) continue;
        
        char path[512];
        
        snprintf(path, sizeof(path), "/sys/class/power_supply/%s/capacity", entry->d_name);
        FILE *fp = fopen(path, "r");
        if (fp) {
            fscanf(fp, "%d", &g_stats.battery_percent);
            fclose(fp);
            g_stats.battery_present = 1;
        }
        
        snprintf(path, sizeof(path), "/sys/class/power_supply/%s/status", entry->d_name);
        fp = fopen(path, "r");
        if (fp) {
            fgets(g_stats.battery_status, sizeof(g_stats.battery_status), fp);
            g_stats.battery_status[strcspn(g_stats.battery_status, "\n")] = '\0';
            fclose(fp);
        }
        break;
    }
    closedir(dir);
}

int compare_processes(const void *a, const void *b) {
    const ProcessInfo *pa = (const ProcessInfo *)a;
    const ProcessInfo *pb = (const ProcessInfo *)b;
    
    switch (g_sort_mode) {
        case SORT_CPU_DIRECT:
            if (pb->cpu_percent > pa->cpu_percent) return 1;
            if (pb->cpu_percent < pa->cpu_percent) return -1;
            break;
        case SORT_CPU_LAZY:
            if (pb->cpu_percent_lazy > pa->cpu_percent_lazy) return 1;
            if (pb->cpu_percent_lazy < pa->cpu_percent_lazy) return -1;
            break;
        case SORT_MEM:
            if (pb->mem_rss > pa->mem_rss) return 1;
            if (pb->mem_rss < pa->mem_rss) return -1;
            break;
        case SORT_PID:
            if (pa->pid > pb->pid) return 1;
            if (pa->pid < pb->pid) return -1;
            break;
        case SORT_NAME:
            return strcasecmp(pa->name, pb->name);
    }
    
    if (pa->pid > pb->pid) return 1;
    if (pa->pid < pb->pid) return -1;
    return 0;
}

void parse_processes(void) {
    DIR *dir = opendir("/proc");
    if (!dir) return;
    
    struct dirent *entry;
    
    ProcessInfo prev_procs[MAX_PROCESSES];
    int prev_count = g_stats.process_count;
    memcpy(prev_procs, g_stats.processes, sizeof(ProcessInfo) * prev_count);
    
    g_stats.process_count = 0;
    g_stats.running_count = 0;
    
    while ((entry = readdir(dir)) != NULL && g_stats.process_count < MAX_PROCESSES) {
        if (!is_number(entry->d_name)) continue;
        
        char path[512];
        snprintf(path, sizeof(path), "/proc/%s/stat", entry->d_name);
        
        FILE *fp = fopen(path, "r");
        if (!fp) continue;
        
        char line[1024];
        if (fgets(line, sizeof(line), fp)) {
            ProcessInfo *proc = &g_stats.processes[g_stats.process_count];
            memset(proc, 0, sizeof(ProcessInfo));
            
            char *p = strchr(line, '(');
            if (!p) { fclose(fp); continue; }
            
            sscanf(line, "%d", &proc->pid);
            
            char *end = strrchr(p, ')');
            if (!end) { fclose(fp); continue; }
            
            int comm_len = end - p - 1;
            if (comm_len < 0) comm_len = 0;
            if (comm_len >= 255) comm_len = 255;
            strncpy(proc->name, p + 1, comm_len);
            proc->name[comm_len] = '\0';
            
            int ppid, pgrp, session, tty_nr, tpgid, uid = 0;
            unsigned int flags;
            unsigned long minflt, cminflt, majflt, cmajflt, utime, stime;
            
            sscanf(end + 2, "%c %d %d %d %d %d %u %lu %lu %lu %lu %lu %lu",
                   &proc->state, &ppid, &pgrp, &session, &tty_nr, &tpgid,
                   &flags, &minflt, &cminflt, &majflt, &cmajflt, &utime, &stime);
            
            snprintf(path, sizeof(path), "/proc/%s/status", entry->d_name);
            FILE *status_fp = fopen(path, "r");
            if (status_fp) {
                char status_line[256];
                proc->mem_rss = 0;
                uid = 0;
                while (fgets(status_line, sizeof(status_line), status_fp)) {
                    unsigned long val;
                    if (sscanf(status_line, "VmRSS: %lu", &val) == 1) {
                        proc->mem_rss = val;
                    }
                    if (strncmp(status_line, "Uid:", 4) == 0) {
                        sscanf(status_line, "Uid: %d", &uid);
                    }
                }
                fclose(status_fp);
            }

            if (proc->uid != uid) {
                proc->uid = uid;
                get_username(uid, proc->user, sizeof(proc->user));
            }
            
            snprintf(path, sizeof(path), "/proc/%s/cmdline", entry->d_name);
            FILE *cmd_fp = fopen(path, "r");
            if (cmd_fp) {
                size_t n = fread(proc->cmdline, 1, sizeof(proc->cmdline) - 1, cmd_fp);
                proc->cmdline[n] = '\0';
                for (size_t i = 0; i < n; i++) {
                    if (proc->cmdline[i] == '\0') proc->cmdline[i] = ' ';
                }
                fclose(cmd_fp);
            }
            
            if (strlen(proc->cmdline) == 0) {
                strncpy(proc->cmdline, proc->name, sizeof(proc->cmdline) - 1);
            }
            
            proc->mem_percent = g_stats.total_mem > 0 ? 
                (proc->mem_rss * 100.0f) / g_stats.total_mem : 0;
            
            long current_utime = utime;
            long current_stime = stime;
            
            proc->cpu_percent = 0.0f;
            for (int j = 0; j < prev_count; j++) {
                if (prev_procs[j].pid == proc->pid) {
                    long delta_utime = current_utime - prev_procs[j].prev_utime;
                    long delta_stime = current_stime - prev_procs[j].prev_stime;
                    long delta_total = delta_utime + delta_stime;
                    
                    if (g_clk_tck <= 0) g_clk_tck = sysconf(_SC_CLK_TCK);
                    if (g_clk_tck <= 0) g_clk_tck = 100;
                    
                    float cpu_raw = 0.0f;
                    if (g_stats.num_cores > 0 && g_elapsed_seconds > 0) {
                        cpu_raw = (delta_total * 100.0f) / (g_clk_tck * g_elapsed_seconds * g_stats.num_cores);
                    }
                    proc->cpu_percent = cpu_raw;
                    
                    if (proc->cpu_percent_lazy > 0 || cpu_raw > 0) {
                        proc->cpu_percent_lazy = proc->cpu_percent_lazy * 0.7f + cpu_raw * 0.3f;
                    } else {
                        proc->cpu_percent_lazy = cpu_raw;
                    }
                    break;
                }
            }
            
            proc->prev_utime = current_utime;
            proc->prev_stime = current_stime;
            
            if (proc->state == 'R') {
                g_stats.running_count++;
            }
            
            g_stats.process_count++;
        }
        
        fclose(fp);
    }
    
    closedir(dir);
    qsort(g_stats.processes, g_stats.process_count, sizeof(ProcessInfo), compare_processes);
}

void update_stats(void) {
    parse_cpu_stats();
    parse_meminfo();
    parse_net_stats();
    parse_disk_stats();
    parse_battery();
    parse_processes();
    g_stats.history_index = (g_stats.history_index + 1) % HISTORY_SIZE;
}
