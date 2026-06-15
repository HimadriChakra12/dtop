#include "../config.h"

void draw_section_header(int x, int y, int num, const char *title, uint32_t color) {
    tb_print(x, y, color, COLOR_BG, "[");
    tb_print(x + 1, y, color | TB_BOLD, COLOR_BG, SUPERSCRIPT[num]);
    tb_print(x + 1 + strlen(SUPERSCRIPT[num]), y, color, COLOR_BG, title);
    tb_print(x + 1 + strlen(SUPERSCRIPT[num]) + strlen(title), y, color, COLOR_BG, "]");
}

void draw_graph(int x, int y, int w, int h, float *data, int idx, uint32_t color) {
    const char *blocks[] = {"▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};
    int idx_start = (idx - w + HISTORY_SIZE) % HISTORY_SIZE;
    
    for (int col = 0; col < w && col < HISTORY_SIZE; col++) {
        int data_idx = (idx_start + col) % HISTORY_SIZE;
        float val = data[data_idx];
        int height = (int)((val / 100.0f) * h);
        float frac = ((val / 100.0f) * h) - height;
        int block_idx = (int)(frac * 7);
        if (block_idx > 6) block_idx = 6;
        if (block_idx < 0) block_idx = 0;
        
        for (int row = 0; row < h; row++) {
            int cy = y + h - 1 - row;
            if (cy < y) continue;
            if (row < height) {
                tb_print(x + col, cy, color, COLOR_BG, "█");
            } else if (row == height && block_idx >= 0) {
                tb_print(x + col, cy, color, COLOR_BG, blocks[block_idx]);
            } else {
                tb_set_cell(x + col, cy, ' ', COLOR_FG, COLOR_BG);
            }
        }
    }
}

void draw_mini_bar(int x, int y, int w, float percent, uint32_t color) {
    const char *blocks[] = {"▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};
    int filled = (int)((percent / 100.0f) * w);
    float frac = ((percent / 100.0f) * w) - filled;
    int frac_idx = (int)(frac * 7);
    
    for (int i = 0; i < w; i++) {
        if (i < filled) {
            tb_print(x + i, y, color, COLOR_BG, "█");
        } else if (i == filled && frac_idx >= 0) {
            tb_print(x + i, y, color, COLOR_BG, blocks[frac_idx]);
        } else {
            tb_set_cell(x + i, y, ' ', COLOR_FG, COLOR_BG);
        }
    }
}

void draw_sparkline_horizontal(int x, int y, int w, float *data, int idx, uint32_t color) {
    const char *blocks[] = {"▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};
    int idx_start = (idx - w + HISTORY_SIZE) % HISTORY_SIZE;
    
    for (int col = 0; col < w && col < HISTORY_SIZE; col++) {
        int data_idx = (idx_start + col) % HISTORY_SIZE;
        float val = data[data_idx];
        int block_idx = (int)((val / 100.0f) * 7);
        if (block_idx > 7) block_idx = 7;
        if (block_idx < 0) block_idx = 0;
        tb_print(x + col, y, color, COLOR_BG, blocks[block_idx]);
    }
}

void draw_cpu_section(int x, int y, int w, int h) {
    draw_section_header(x, y, 1, "cpu", COLOR_CPU);
    
    if (h < 3) return;
    
    uint32_t color = g_stats.overall.percent > 80 ? COLOR_HIGH :
                     g_stats.overall.percent > 50 ? COLOR_MED : COLOR_LOW;
    
    tb_printf(x + 6, y, COLOR_FG, COLOR_BG, "CPU ");
    int header_bar_w = (w > 40) ? 12 : (w > 30 ? 8 : 5);
    draw_mini_bar(x + 10, y, header_bar_w, g_stats.overall.percent, color);
    tb_printf(x + 10 + header_bar_w + 1, y, color, COLOR_BG, "%3.0f%%", g_stats.overall.percent);
    
    if (h < 4) return;
    
    int graph_w = w - 2;
    int graph_h = (h > 8) ? 2 : 1;
    if (graph_w > 60) graph_w = 60;
    draw_graph(x, y + 1, graph_w, graph_h, g_stats.overall.history, g_stats.overall.history_idx, COLOR_CPU);
    
    int core_start_y = y + 1 + graph_h;
    if (core_start_y >= y + h - 1) return;
    
    int core_label_width = (g_stats.num_cores >= 100) ? 4 : (g_stats.num_cores >= 10 ? 3 : 2);
    int available_core_rows = (y + h - 1) - core_start_y;
    
    int two_line_item_width = core_label_width + 12;
    int max_cores_per_row = (w - 2) / two_line_item_width;
    if (max_cores_per_row < 1) max_cores_per_row = 1;
    
    int min_rows_needed = (g_stats.num_cores + max_cores_per_row - 1) / max_cores_per_row;
    int use_two_line = (available_core_rows >= min_rows_needed * 2);
    
    if (use_two_line) {
        int best_cores_per_row = max_cores_per_row;
        int best_rows = min_rows_needed;
        
        for (int test_cores_per_row = max_cores_per_row; test_cores_per_row >= 1; test_cores_per_row--) {
            int rows_needed = (g_stats.num_cores + test_cores_per_row - 1) / test_cores_per_row;
            if (rows_needed * 2 > available_core_rows) break;
            
            int cores_in_last_row = g_stats.num_cores - (rows_needed - 1) * test_cores_per_row;
            int cores_in_full_rows = test_cores_per_row;
            
            if (cores_in_last_row == cores_in_full_rows || 
                (best_rows != rows_needed && rows_needed <= available_core_rows / 2)) {
                best_cores_per_row = test_cores_per_row;
                best_rows = rows_needed;
                if (cores_in_last_row == cores_in_full_rows) break;
            }
        }
        
        int cores_per_row = best_cores_per_row;
        
        int usable_width = w - 2;
        int actual_item_width = usable_width / cores_per_row;
        if (actual_item_width < two_line_item_width) actual_item_width = two_line_item_width;
        
        for (int i = 0; i < g_stats.num_cores && i < MAX_CPU_CORES; i++) {
            int col = i % cores_per_row;
            int row_group = i / cores_per_row;
            int cx = x + col * actual_item_width;
            int cy1 = core_start_y + row_group * 2;
            int cy2 = cy1 + 1;
            
            if (cy2 >= y + h - 1) break;
            
            CoreStat *core = &g_stats.cores[i];
            uint32_t ccolor = core->percent > 80 ? COLOR_HIGH :
                              core->percent > 50 ? COLOR_MED : COLOR_LOW;
            
            tb_printf(cx, cy1, COLOR_FG, COLOR_BG, "C%-*d", core_label_width - 1, i);
            int bar_width = actual_item_width - core_label_width - 5;
            if (bar_width > 10) bar_width = 10;
            draw_mini_bar(cx + core_label_width, cy1, bar_width, core->percent, ccolor);
            tb_printf(cx + core_label_width + bar_width + 1, cy1, ccolor, COLOR_BG, "%3.0f%%", core->percent);
            
            int spark_width = actual_item_width - 2;
            draw_sparkline_horizontal(cx + 1, cy2, spark_width, core->history, core->history_idx, ccolor);
        }
    } else {
        int single_line_item_width = core_label_width + 6;
        int cores_per_row = (w - 2) / single_line_item_width;
        if (cores_per_row < 1) cores_per_row = 1;
        
        int max_cores_display = cores_per_row * available_core_rows;
        if (max_cores_display > g_stats.num_cores) max_cores_display = g_stats.num_cores;
        
        for (int i = 0; i < max_cores_display && i < MAX_CPU_CORES; i++) {
            int cx = x + (i % cores_per_row) * single_line_item_width;
            int cy = core_start_y + (i / cores_per_row);
            
            if (cy >= y + h - 1) break;
            
            CoreStat *core = &g_stats.cores[i];
            uint32_t ccolor = core->percent > 80 ? COLOR_HIGH :
                              core->percent > 50 ? COLOR_MED : COLOR_LOW;
            
            tb_printf(cx, cy, COLOR_FG, COLOR_BG, "C%-*d", core_label_width - 1, i);
            draw_mini_bar(cx + core_label_width, cy, 2, core->percent, ccolor);
            tb_printf(cx + core_label_width + 3, cy, ccolor, COLOR_BG, "%2.0f%%", core->percent);
        }
    }
}

void draw_memory_section(int x, int y, int w, int h) {
    draw_section_header(x, y, 2, "mem", COLOR_MEM);
    
    if (h < 4) return;
    
    unsigned long used = g_stats.total_mem - g_stats.available_mem;
    unsigned long cached = g_stats.cached + g_stats.buffers;
    char buf[64];
    
    uint32_t used_color = g_stats.mem_percent > 80 ? COLOR_HIGH :
                          g_stats.mem_percent > 50 ? COLOR_MED : COLOR_LOW;
    
    int line = y + 2;
    int max_line = y + h - 1;
    
    if (line < max_line) {
        tb_printf(x, line, COLOR_FG, COLOR_BG, "Used:");
        format_bytes(used * 1024, buf, sizeof(buf));
        if (w > 30) {
            tb_printf(x + 10, line, used_color | TB_BOLD, COLOR_BG, "%10s", buf);
            draw_mini_bar(x + 22, line, w - 26, g_stats.mem_percent, used_color);
        } else {
            tb_printf(x + 6, line, used_color | TB_BOLD, COLOR_BG, "%s", buf);
        }
        line++;
    }
    
    if (line < max_line) {
        tb_printf(x, line, COLOR_FG, COLOR_BG, "Total:");
        format_bytes(g_stats.total_mem * 1024, buf, sizeof(buf));
        tb_printf(x + 10, line, COLOR_FG | TB_BOLD, COLOR_BG, "%10s", buf);
        line++;
    }
    
    if (line < max_line) {
        tb_printf(x, line, COLOR_FG, COLOR_BG, "Free:");
        format_bytes(g_stats.available_mem * 1024, buf, sizeof(buf));
        tb_printf(x + 10, line, COLOR_LOW | TB_BOLD, COLOR_BG, "%10s", buf);
        line++;
    }
    
    if (line < max_line && h > 6) {
        tb_printf(x, line, COLOR_FG, COLOR_BG, "Cached:");
        format_bytes(cached * 1024, buf, sizeof(buf));
        tb_printf(x + 10, line, COLOR_FG | TB_BOLD, COLOR_BG, "%10s", buf);
        line++;
    }
    
    if (line < max_line) {
        int graph_h = max_line - line;
        if (graph_h > 3) graph_h = 3;
        if (graph_h < 1) graph_h = 1;
        draw_graph(x, line, w - 2, graph_h, (float*)g_stats.mem_history, 
                   g_stats.history_index, COLOR_MEM);
    }
}

void draw_disk_section(int x, int y, int w, int h) {
    draw_section_header(x, y, 3, "disk", COLOR_DISK);
    
    if (h < 4) return;
    
    int line = y + 2;
    int max_line = y + h - 1;
    int disks_per_row = (w > 60) ? 2 : 1;
    int disk_width = (w - 2) / disks_per_row;
    
    for (int i = 0; i < g_stats.num_disks && line < max_line - 1; i++) {
        DiskInfo *disk = &g_stats.disks[i];
        char buf[32];
        int disk_x = x + (i % disks_per_row) * disk_width;
        
        tb_printf(disk_x, line, COLOR_DISK | TB_BOLD, COLOR_BG, "%-8s", disk->name);
        
        if (line + 1 < max_line) {
            tb_printf(disk_x, line + 1, COLOR_NET_DOWN, COLOR_BG, "▼");
            format_speed(disk->read_speed, buf, sizeof(buf));
            tb_printf(disk_x + 2, line + 1, COLOR_FG, COLOR_BG, "%-10s", buf);
        }
        
        if (line + 2 < max_line) {
            tb_printf(disk_x, line + 2, COLOR_NET_UP, COLOR_BG, "▲");
            format_speed(disk->write_speed, buf, sizeof(buf));
            tb_printf(disk_x + 2, line + 2, COLOR_FG, COLOR_BG, "%-10s", buf);
        }
        
        if (line + 3 < max_line && disk_width > 15) {
            int graph_h = 2;
            int graph_w = disk_width - 2;
            if (graph_w > 30) graph_w = 30;
            
            float combined_history[HISTORY_SIZE];
            for (int j = 0; j < HISTORY_SIZE; j++) {
                combined_history[j] = disk->history_rx[j] + disk->history_tx[j];
                if (combined_history[j] > 100) combined_history[j] = 100;
            }
            
            draw_graph(disk_x, line + 3, graph_w, graph_h, combined_history, 
                       g_stats.history_index, COLOR_DISK);
        }
        
        if ((i + 1) % disks_per_row == 0) {
            line += 6;
        }
    }
}

void draw_net_section(int x, int y, int w, int h) {
    draw_section_header(x, y, 4, "net", COLOR_NET_DOWN);
    
    if (h < 4) return;
    
    char buf[32];
    int line = y + 2;
    int max_line = y + h - 1;
    
    if (line < max_line) {
        tb_printf(x, line, COLOR_NET_DOWN, COLOR_BG, "▼ down ");
        format_speed(g_stats.net_rx_speed, buf, sizeof(buf));
        tb_printf(x + 10, line++, COLOR_FG | TB_BOLD, COLOR_BG, "%s", buf);
    }
    
    if (line + 1 < max_line && h > 5) {
        int graph_h = max_line - line - 2;
        if (graph_h > 2) graph_h = 2;
        if (graph_h > 0) {
            draw_graph(x, line, w - 2, graph_h, (float*)g_stats.net_history_rx, 
                       g_stats.history_index, COLOR_NET_DOWN);
            line += graph_h;
        }
    }
    
    if (line < max_line) {
        tb_printf(x, line, COLOR_NET_UP, COLOR_BG, "▲ up   ");
        format_speed(g_stats.net_tx_speed, buf, sizeof(buf));
        tb_printf(x + 10, line++, COLOR_FG | TB_BOLD, COLOR_BG, "%s", buf);
    }
    
    if (line < max_line && h > 6) {
        int graph_h = max_line - line;
        if (graph_h > 2) graph_h = 2;
        if (graph_h > 0) {
            draw_graph(x, line, w - 2, graph_h, (float*)g_stats.net_history_tx,
                       g_stats.history_index, COLOR_NET_UP);
        }
    }
}

void draw_process_list(int x, int y, int w, int h) {
    draw_section_header(x, y, 5, "proc", COLOR_PROC);
    
    if (h < 5) return;
    
    int list_start = y + 2;
    int list_height = h - 3;
    int max_line = y + h - 1;
    
    int pid_width = PROC_PID_WIDTH;
    int cpu_width = PROC_CPU_WIDTH;
    int mem_width = PROC_MEM_WIDTH;
    int min_prog_width = PROC_PROG_MIN_WIDTH;
    int min_cmd_width = PROC_CMD_MIN_WIDTH;
    int min_user_width = PROC_USER_MIN_WIDTH;
    
    int fixed_width = pid_width + mem_width + cpu_width + PROC_COLUMN_SPACING;
    int var_width = w - fixed_width;
    
    int show_user = 1;
    int show_cmd = 1;
    int prog_width, cmd_width, user_width;
    
    if (var_width < min_prog_width + min_user_width) {
        show_user = 0;
        show_cmd = 0;
        prog_width = var_width - PROC_NARROW_OFFSET;
        if (prog_width < 6) prog_width = 6;
    } else if (var_width < min_prog_width + min_cmd_width + min_user_width) {
        show_cmd = 0;
        prog_width = var_width * 60 / 100;
        if (prog_width < min_prog_width) prog_width = min_prog_width;
        user_width = var_width - prog_width - 1;
        if (user_width < min_user_width) user_width = min_user_width;
    } else {
        prog_width = var_width * 30 / 100;
        cmd_width = var_width * 40 / 100;
        user_width = var_width - prog_width - cmd_width - 2;
        if (prog_width < min_prog_width) prog_width = min_prog_width;
        if (cmd_width < min_cmd_width) cmd_width = min_cmd_width;
        if (user_width < min_user_width) user_width = min_user_width;
    }
    
    int cx = x;
    tb_printf(cx, list_start - 1, COLOR_HEADER | TB_BOLD, COLOR_BG, "%-*s", pid_width, "Pid:");
    cx += pid_width + 1;
    
    tb_printf(cx, list_start - 1, COLOR_HEADER | TB_BOLD, COLOR_BG, "%-*s", prog_width, "Program:");
    cx += prog_width + 1;
    
    if (show_cmd) {
        tb_printf(cx, list_start - 1, COLOR_HEADER | TB_BOLD, COLOR_BG, "%-*s", cmd_width, "Command:");
        cx += cmd_width + 1;
    }
    
    if (show_user) {
        tb_printf(cx, list_start - 1, COLOR_HEADER | TB_BOLD, COLOR_BG, "%-*s", user_width, "User:");
        cx += user_width + 1;
    }
    
    tb_printf(cx, list_start - 1, COLOR_HEADER | TB_BOLD, COLOR_BG, "%-*s", mem_width, "MemB");
    cx += mem_width + 1;
    
    tb_printf(cx, list_start - 1, COLOR_HEADER | TB_BOLD, COLOR_BG, "%-*s", cpu_width, "Cpu%");
    
    if (g_selected_process < g_scroll_offset) {
        g_scroll_offset = g_selected_process;
    } else if (g_selected_process >= g_scroll_offset + list_height) {
        g_scroll_offset = g_selected_process - list_height + 1;
    }
    
    for (int i = 0; i < list_height && (g_scroll_offset + i) < g_stats.process_count; i++) {
        int idx = g_scroll_offset + i;
        ProcessInfo *proc = &g_stats.processes[idx];
        int row = list_start + i;
        
        if (row >= max_line) break;
        
        uint32_t row_fg = COLOR_FG;
        uint32_t row_bg = COLOR_BG;
        
        if (idx == g_selected_process) {
            row_fg = TB_BLACK;
            row_bg = COLOR_HEADER;
        }
        
        char name[256], cmd[256], user[32], mem_buf[32];
        strncpy(name, proc->name, prog_width);
        name[prog_width] = '\0';
        
        if (show_cmd) {
            strncpy(cmd, proc->cmdline, cmd_width);
            cmd[cmd_width] = '\0';
        }
        
        if (show_user) {
            strncpy(user, proc->user, user_width);
            user[user_width] = '\0';
        }
        
        format_bytes(proc->mem_rss * 1024, mem_buf, sizeof(mem_buf));
        if ((int)strlen(mem_buf) > mem_width) {
            mem_buf[mem_width] = '\0';
        }
        
        uint32_t cpu_color = proc->cpu_percent > 50 ? COLOR_HIGH :
                             proc->cpu_percent > 20 ? COLOR_MED : COLOR_LOW;
        
        cx = x;
        tb_printf(cx, row, row_fg, row_bg, "%-*d", pid_width, proc->pid);
        cx += pid_width + 1;
        
        tb_printf(cx, row, row_fg, row_bg, "%-*s", prog_width, name);
        cx += prog_width + 1;
        
        if (show_cmd) {
            tb_printf(cx, row, row_fg, row_bg, "%-*s", cmd_width, cmd);
            cx += cmd_width + 1;
        }
        
        if (show_user) {
            tb_printf(cx, row, row_fg, row_bg, "%-*s", user_width, user);
            cx += user_width + 1;
        }
        
        tb_printf(cx, row, row_fg, row_bg, "%-*s", mem_width, mem_buf);
        cx += mem_width + 1;
        
        tb_printf(cx, row, cpu_color | (idx == g_selected_process ? 0 : TB_BOLD), row_bg,
                  "%*.1f", cpu_width - 1, proc->cpu_percent);
    }
    
    if (max_line > y + 2) {
        char status[128];
        snprintf(status, sizeof(status), "%d/%d | %d | Sort:%s",
                 g_stats.running_count, g_stats.process_count, g_selected_process + 1, get_sort_name());
        int status_len = strlen(status);
        if (status_len > w - 2) status_len = w - 2;
        tb_printf(x, max_line, COLOR_FG, COLOR_BG, "%s", status);
    }
}

void draw_top_bar(int w) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);
    
    tb_printf(w / 2 - 4, 0, COLOR_TIME | TB_BOLD, COLOR_BG, "%s", time_str);
    
    if (g_stats.battery_present) {
        int batt_x = w - 20;
        uint32_t batt_color = g_stats.battery_percent < 20 ? COLOR_HIGH :
                              g_stats.battery_percent < 50 ? COLOR_MED : COLOR_BATTERY;
        
        const char *icon = strstr(g_stats.battery_status, "Charging") ? "▲" :
                          strstr(g_stats.battery_status, "Discharging") ? "▼" : "●";
        
        tb_printf(batt_x, 0, batt_color, COLOR_BG, "BAT%s %d%%", icon, g_stats.battery_percent);
        draw_mini_bar(batt_x + 10, 0, 8, g_stats.battery_percent, batt_color);
    }
    
    tb_printf(2, 0, COLOR_HEADER | TB_BOLD, COLOR_BG, "ctop %s", CTOP_VERSION);
}

void draw_help_bar(int y, int w) {
    (void)w;  // Mark as intentionally unused
    tb_printf(2, y, COLOR_FG, COLOR_BG, 
              "1-5:toggle | C-f/b:sort | C-n/p:nav | C-v/M-v:page | C-a/e:home/end | k:t:s:signal | q:quit");
}

void draw_signal_menu(int w, int h) {
    int menu_w = 40;
    int menu_h = NUM_SIGNALS + 4;
    int x = (w - menu_w) / 2;
    int y = (h - menu_h) / 2;
    
    if (x < 2) x = 2;
    if (y < 2) y = 2;
    
    if (g_selected_process < 0 || g_selected_process >= g_stats.process_count) return;
    ProcessInfo *proc = &g_stats.processes[g_selected_process];
    
    for (int dy = 0; dy < menu_h; dy++) {
        for (int dx = 0; dx < menu_w; dx++) {
            uint32_t fg = COLOR_HEADER;
            uint32_t bg = COLOR_HEADER;
            uint32_t ch = ' ';
            
            if (dy == 0) {
                if (dx == 0) ch = '+';
                else if (dx == menu_w - 1) ch = '+';
                else ch = '-';
            } else if (dy == menu_h - 1) {
                if (dx == 0) ch = '+';
                else if (dx == menu_w - 1) ch = '+';
                else ch = '-';
            } else if (dx == 0 || dx == menu_w - 1) {
                ch = '|';
            }
            
            tb_set_cell(x + dx, y + dy, ch, fg, bg);
        }
    }
    
    tb_printf(x + 2, y, TB_BLACK, COLOR_HEADER, "Send signal to PID %d (%s)", proc->pid, proc->name);
    
    if (g_signal_selected < 0) g_signal_selected = 0;
    if (g_signal_selected >= NUM_SIGNALS) g_signal_selected = NUM_SIGNALS - 1;
    
    int scroll = 0;
    if (g_signal_selected > menu_h - 4) {
        scroll = g_signal_selected - (menu_h - 4);
    }
    if (scroll < 0) scroll = 0;
    
    for (int i = 0; i < NUM_SIGNALS && i < menu_h - 3; i++) {
        int idx = i + scroll;
        if (idx >= NUM_SIGNALS) break;
        
        int row = y + 2 + i;
        uint32_t fg = COLOR_FG;
        uint32_t bg = COLOR_HEADER;
        
        if (idx == g_signal_selected) {
            fg = TB_BLACK;
            bg = COLOR_HEADER;
        }
        
        tb_printf(x + 2, row, fg, bg, "%-8s %2d  %s", SIGNALS[idx].name, SIGNALS[idx].signum, SIGNALS[idx].desc);
    }
    
    tb_printf(x + 2, y + menu_h - 1, COLOR_HEADER, COLOR_HEADER, "Up/Dn:select Enter:send Esc:close");
}

void draw_signal_sent_message(int w, int h) {
    int msg_w = 35;
    int msg_h = 3;
    int x = (w - msg_w) / 2;
    int y = (h - msg_h) / 2;
    
    if (x < 2) x = 2;
    if (y < 2) y = 2;
    
    const char *sig_name = (g_signal_sent_sig == SIGKILL) ? "SIGKILL" : 
                          (g_signal_sent_sig == SIGTERM) ? "SIGTERM" : "signal";
    
    for (int dy = 0; dy < msg_h; dy++) {
        for (int dx = 0; dx < msg_w; dx++) {
            uint32_t fg = COLOR_LOW;
            uint32_t bg = COLOR_LOW;
            uint32_t ch = ' ';
            
            if (dy == 0) {
                if (dx == 0) ch = '+';
                else if (dx == msg_w - 1) ch = '+';
                else ch = '-';
            } else if (dy == msg_h - 1) {
                if (dx == 0) ch = '+';
                else if (dx == msg_w - 1) ch = '+';
                else ch = '-';
            } else if (dx == 0 || dx == msg_w - 1) {
                ch = '|';
            }
            
            tb_set_cell(x + dx, y + dy, ch, fg, bg);
        }
    }
    
    tb_printf(x + 2, y + 1, TB_BLACK, COLOR_LOW, "Sent %s to PID %d", sig_name, g_signal_sent_pid);
}

void draw_confirm_menu(int w, int h, const char *sig_name) {
    int menu_w = 45;
    int menu_h = 6;
    int x = (w - menu_w) / 2;
    int y = (h - menu_h) / 2;
    
    if (x < 2) x = 2;
    if (y < 2) y = 2;
    
    if (g_selected_process < 0 || g_selected_process >= g_stats.process_count) return;
    ProcessInfo *proc = &g_stats.processes[g_selected_process];
    
    for (int dy = 0; dy < menu_h; dy++) {
        for (int dx = 0; dx < menu_w; dx++) {
            uint32_t fg = COLOR_HEADER;
            uint32_t bg = COLOR_HEADER;
            uint32_t ch = ' ';
            
            if (dy == 0) {
                if (dx == 0) ch = '+';
                else if (dx == menu_w - 1) ch = '+';
                else ch = '-';
            } else if (dy == menu_h - 1) {
                if (dx == 0) ch = '+';
                else if (dx == menu_w - 1) ch = '+';
                else ch = '-';
            } else if (dx == 0 || dx == menu_w - 1) {
                ch = '|';
            }
            
            tb_set_cell(x + dx, y + dy, ch, fg, bg);
        }
    }
    
    tb_printf(x + 2, y + 1, TB_BLACK, COLOR_HEADER, "Send %s to PID %d (%s)?", sig_name, proc->pid, proc->name);
    tb_printf(x + 2, y + 3, COLOR_FG, COLOR_HEADER, "  Yes: Enter    No: Esc");
}

void draw_error_screen(int w, int h) {
    tb_clear();
    
    int min_w, min_h;
    calculate_minimum_size(&min_w, &min_h);
    
    int y = 2;
    int x = 2;
    
    tb_printf(x, y++, COLOR_HIGH | TB_BOLD, COLOR_BG, 
              "ERROR: Terminal too small!");
    y++;
    
    tb_printf(x, y++, COLOR_FG, COLOR_BG, 
              "Current size: %dx%d", w, h);
    tb_printf(x, y++, COLOR_FG, COLOR_BG, 
              "Required size: %dx%d (for current layout)", min_w, min_h);
    y++;
    
    tb_printf(x, y++, COLOR_HEADER | TB_BOLD, COLOR_BG, "Pane Status:");
    tb_printf(x, y++, g_show_cpu ? COLOR_LOW : COLOR_HIGH, COLOR_BG, 
              "  [1] CPU: %s", g_show_cpu ? "ON" : "OFF");
    tb_printf(x, y++, g_show_mem ? COLOR_LOW : COLOR_HIGH, COLOR_BG, 
              "  [2] Memory: %s", g_show_mem ? "ON" : "OFF");
    tb_printf(x, y++, g_show_disks ? COLOR_LOW : COLOR_HIGH, COLOR_BG, 
              "  [3] Disk: %s", g_show_disks ? "ON" : "OFF");
    tb_printf(x, y++, g_show_net ? COLOR_LOW : COLOR_HIGH, COLOR_BG, 
              "  [4] Network: %s", g_show_net ? "ON" : "OFF");
    tb_printf(x, y++, g_show_proc ? COLOR_LOW : COLOR_HIGH, COLOR_BG, 
              "  [5] Processes: %s", g_show_proc ? "ON" : "OFF");
    y++;
    
    tb_printf(x, y++, COLOR_FG, COLOR_BG, 
              "Press 1-5 to toggle panes, or resize terminal.");
    tb_printf(x, y++, COLOR_FG, COLOR_BG, 
              "Press 'q' to quit.");
    
    tb_present();
}

void draw_screen(void) {
    int w = tb_width();
    int h = tb_height();
    
    int min_w, min_h;
    calculate_minimum_size(&min_w, &min_h);
    
    if (w < min_w || h < min_h) {
        draw_error_screen(w, h);
        return;
    }
    
    tb_clear();
    
    draw_top_bar(w);
    
    int top_margin = 1;
    int bottom_margin = 1;
    int available_height = h - top_margin - bottom_margin;
    
    int cpu_height = 0;
    if (g_show_cpu) {
        cpu_height = 5;
        
        int min_proc_rows = 10;
        int other_panes = (g_show_mem || g_show_disks || g_show_net) ? 3 : 0;
        int needed_for_bottom = min_proc_rows + other_panes;
        
        if (available_height > cpu_height + needed_for_bottom) {
            int extra = available_height - cpu_height - needed_for_bottom;
            cpu_height += extra / 4;
            if (cpu_height > available_height / 3) cpu_height = available_height / 3;
        }
    }
    
    int bottom_height = available_height - cpu_height;
    if (bottom_height < 6 && (g_show_mem || g_show_disks || g_show_net || g_show_proc)) {
        if (g_show_cpu) {
            cpu_height = available_height - 6;
            if (cpu_height < 3) cpu_height = 0;
        }
        bottom_height = available_height - cpu_height;
    }
    
    int current_y = top_margin;
    if (g_show_cpu && cpu_height > 0) {
        draw_cpu_section(1, current_y, w - 2, cpu_height);
        current_y += cpu_height;
    }
    
    int bottom_y = current_y;
    
    int num_left_panes = g_show_mem + g_show_disks + g_show_net;
    int proc_width = 0;
    int left_width = 0;
    
    if (g_show_proc && num_left_panes == 0) {
        proc_width = w - 2;
    } else if (!g_show_proc && num_left_panes > 0) {
        left_width = w - 2;
    } else if (g_show_proc && num_left_panes > 0) {
        left_width = (w - 3) * 35 / 100;
        if (left_width < 18) left_width = 18;
        proc_width = (w - 3) - left_width;
        if (proc_width < 40) {
            proc_width = 40;
            left_width = (w - 3) - proc_width;
        }
    }
    
    if (num_left_panes > 0 && left_width > 0) {
        int left_y = bottom_y;
        int remaining_height = bottom_height;
        
        int base_pane_height = remaining_height / num_left_panes;
        
        if (g_show_mem && remaining_height > 0) {
            int pane_h = base_pane_height;
            if (pane_h < 4) pane_h = remaining_height;
            if (pane_h > remaining_height) pane_h = remaining_height;
            draw_memory_section(1, left_y, left_width, pane_h);
            left_y += pane_h;
            remaining_height -= pane_h;
        }
        
        if (g_show_disks && remaining_height > 0) {
            int pane_h = base_pane_height;
            if (pane_h < 4) pane_h = remaining_height;
            if (pane_h > remaining_height) pane_h = remaining_height;
            draw_disk_section(1, left_y, left_width, pane_h);
            left_y += pane_h;
            remaining_height -= pane_h;
        }
        
        if (g_show_net && remaining_height > 0) {
            draw_net_section(1, left_y, left_width, remaining_height);
        }
    }
    
    if (g_show_proc && proc_width > 0) {
        int proc_x = (num_left_panes > 0) ? left_width + 2 : 1;
        draw_process_list(proc_x, bottom_y, proc_width, bottom_height);
    }
    
    draw_help_bar(h - 1, w);
    
    if (g_signal_menu_active) {
        draw_signal_menu(w, h);
    }
    
    if (g_confirm_menu_active) {
        const char *sig_name = (g_confirm_signal == SIGKILL) ? "SIGKILL" : "SIGTERM";
        draw_confirm_menu(w, h, sig_name);
    }
    
    if (g_signal_sent) {
        draw_signal_sent_message(w, h);
    }
    
    tb_present();
}
