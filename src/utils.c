#include "../config.h"

int is_number(const char *str) {
    while (*str) {
        if (!isdigit(*str)) return 0;
        str++;
    }
    return 1;
}

void get_username(int uid, char *buf, size_t buflen) {
    struct passwd pwd;
    struct passwd *result;
    char *pwdbuf;
    size_t pwdbuflen = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (pwdbuflen == (size_t)-1) pwdbuflen = 16384;
    
    pwdbuf = malloc(pwdbuflen);
    if (!pwdbuf) {
        snprintf(buf, buflen, "%d", uid);
        return;
    }
    
    if (getpwuid_r(uid, &pwd, pwdbuf, pwdbuflen, &result) == 0 && result) {
        strncpy(buf, pwd.pw_name, buflen - 1);
        buf[buflen - 1] = '\0';
    } else {
        snprintf(buf, buflen, "%d", uid);
    }
    free(pwdbuf);
}

void format_bytes(unsigned long bytes, char *buf, size_t buflen) {
    const char *units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    int unit = 0;
    double val = bytes;
    while (val >= 1024 && unit < 4) {
        val /= 1024;
        unit++;
    }
    snprintf(buf, buflen, "%.2f %s", val, units[unit]);
}

void format_speed(float kbps, char *buf, size_t buflen) {
    if (kbps >= 1024 * 1024) {
        snprintf(buf, buflen, "%.2f GiB/s", kbps / (1024 * 1024));
    } else if (kbps >= 1024) {
        snprintf(buf, buflen, "%.2f MiB/s", kbps / 1024);
    } else {
        snprintf(buf, buflen, "%.2f KiB/s", kbps);
    }
}

const char *get_sort_name(void) {
    switch (g_sort_mode) {
        case SORT_CPU_LAZY: return "CPU-L";
        case SORT_CPU_DIRECT: return "CPU-D";
        case SORT_MEM: return "Mem";
        case SORT_PID: return "PID";
        case SORT_NAME: return "Name";
        default: return "CPU-L";
    }
}

int64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

void get_config_dir(char *buf, size_t buflen) {
    const char *xdg_config = getenv("XDG_CONFIG_HOME");
    if (xdg_config && strlen(xdg_config) > 0) {
        snprintf(buf, buflen, "%s/ctop", xdg_config);
    } else {
        const char *home = getenv("HOME");
        if (home && strlen(home) > 0) {
            snprintf(buf, buflen, "%s/.config/ctop", home);
        } else {
            snprintf(buf, buflen, "/tmp/ctop");
        }
    }
}

void save_settings(void) {
    char config_dir[512];
    get_config_dir(config_dir, sizeof(config_dir));
    
    struct stat st;
    if (stat(config_dir, &st) != 0) {
        mkdir(config_dir, 0755);
    }
    
    char config_file[1024];
    snprintf(config_file, sizeof(config_file), "%s/config", config_dir);
    
    FILE *fp = fopen(config_file, "w");
    if (!fp) return;
    
    fprintf(fp, "# ctop configuration file\n");
    fprintf(fp, "show_cpu=%d\n", g_show_cpu);
    fprintf(fp, "show_mem=%d\n", g_show_mem);
    fprintf(fp, "show_disks=%d\n", g_show_disks);
    fprintf(fp, "show_net=%d\n", g_show_net);
    fprintf(fp, "show_proc=%d\n", g_show_proc);
    fprintf(fp, "sort_mode=%d\n", g_sort_mode);
    fprintf(fp, "refresh_rate=%d\n", g_refresh_rate_ms);
    
    fclose(fp);
}

void load_settings(void) {
    char config_dir[512];
    get_config_dir(config_dir, sizeof(config_dir));
    
    char config_file[1024];
    snprintf(config_file, sizeof(config_file), "%s/config", config_dir);
    
    FILE *fp = fopen(config_file, "r");
    if (!fp) return;
    
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        
        char key[64];
        int value;
        if (sscanf(line, "%63[^=]=%d", key, &value) == 2) {
            if (strcmp(key, "show_cpu") == 0) g_show_cpu = value;
            else if (strcmp(key, "show_mem") == 0) g_show_mem = value;
            else if (strcmp(key, "show_disks") == 0) g_show_disks = value;
            else if (strcmp(key, "show_net") == 0) g_show_net = value;
            else if (strcmp(key, "show_proc") == 0) g_show_proc = value;
            else if (strcmp(key, "sort_mode") == 0) {
                if (value >= 0 && value < SORT_MAX) g_sort_mode = value;
            }
            else if (strcmp(key, "refresh_rate") == 0) {
                if (value >= 100 && value <= 10000) g_refresh_rate_ms = value;
            }
        }
    }
    
    fclose(fp);
}
