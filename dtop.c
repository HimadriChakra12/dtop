#define TB_IMPL
#include "config.h"

/* Global state definitions */
const char *SUPERSCRIPT[] = {"", "¹", "²", "³", "⁴", "⁵"};

const SignalInfo SIGNALS[] = {
    {1, "SIGHUP", "Hangup"},
    {2, "SIGINT", "Interrupt (Ctrl+C)"},
    {3, "SIGQUIT", "Quit"},
    {6, "SIGABRT", "Abort"},
    {9, "SIGKILL", "Kill (cannot be caught)"},
    {11, "SIGSEGV", "Segmentation fault"},
    {15, "SIGTERM", "Terminate (graceful)"},
    {17, "SIGSTOP", "Stop (cannot be caught)"},
    {18, "SIGTSTP", "Stop (Ctrl+Z)"},
    {19, "SIGCONT", "Continue"},
    {20, "SIGTTIN", "Background read"},
    {21, "SIGTTOU", "Background write"},
    {22, "SIGUSR1", "User-defined 1"},
    {23, "SIGUSR2", "User-defined 2"},
    {24, "SIGWINCH", "Window changed"},
    {25, "SIGIO", "I/O possible"},
    {26, "SIGSYS", "Bad system call"},
};
const int NUM_SIGNALS = sizeof(SIGNALS) / sizeof(SIGNALS[0]);

SystemStats g_stats = {0};
int g_running = 1;
int g_selected_process = 0;
int g_scroll_offset = 0;
int g_signal_menu_active = 0;
int g_signal_selected = 0;
int g_confirm_menu_active = 0;
int g_confirm_signal = 0;
int g_signal_sent = 0;
int g_signal_sent_pid = 0;
int g_signal_sent_sig = 0;
int64_t g_signal_sent_time = 0;
int g_show_cpu = 1;
int g_show_mem = 1;
int g_show_disks = 1;
int g_show_net = 1;
int g_show_proc = 1;
int g_sort_mode = SORT_CPU_LAZY;
int g_refresh_rate_ms = REFRESH_RATE_MS;
float g_elapsed_seconds = 1.0f;
long g_clk_tck = 0;

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    int ret = tb_init();
    if (ret != TB_OK) {
        fprintf(stderr, "Failed to initialize termbox: %s\n", tb_strerror(ret));
        return 1;
    }
    
    tb_set_output_mode(TB_OUTPUT_TRUECOLOR);
    tb_hide_cursor();
    
    load_settings();
    
    update_stats();
    draw_screen();
    
    int64_t last_update = get_time_ms();
    int64_t prev_update = last_update;
    
    while (g_running) {
        int w = tb_width();
        int h = tb_height();
        int min_w, min_h;
        calculate_minimum_size(&min_w, &min_h);
        int in_error_mode = (w < min_w || h < min_h);
        
        int64_t now = get_time_ms();
        int64_t time_until_update = g_refresh_rate_ms - (now - last_update);
        if (time_until_update < 0) time_until_update = 0;
        
        struct tb_event ev;
        ret = tb_peek_event(&ev, (int)time_until_update);
        
        int need_redraw = 0;
        int pane_toggled = 0;
        int sort_changed = 0;
        
        if (ret == TB_OK) {
            if (ev.type == TB_EVENT_KEY) {
                if (ev.ch == '1') {
                    g_show_cpu = !g_show_cpu;
                    need_redraw = 1;
                    pane_toggled = 1;
                } else if (ev.ch == '2') {
                    g_show_mem = !g_show_mem;
                    need_redraw = 1;
                    pane_toggled = 1;
                } else if (ev.ch == '3') {
                    g_show_disks = !g_show_disks;
                    need_redraw = 1;
                    pane_toggled = 1;
                } else if (ev.ch == '4') {
                    g_show_net = !g_show_net;
                    need_redraw = 1;
                    pane_toggled = 1;
                } else if (ev.ch == '5') {
                    g_show_proc = !g_show_proc;
                    need_redraw = 1;
                    pane_toggled = 1;
                } else if (ev.key == TB_KEY_CTRL_F) {
                    g_sort_mode = (g_sort_mode + 1) % SORT_MAX;
                    need_redraw = 1;
                    sort_changed = 1;
                } else if (ev.key == TB_KEY_CTRL_B) {
                    g_sort_mode = (g_sort_mode - 1 + SORT_MAX) % SORT_MAX;
                    need_redraw = 1;
                    sort_changed = 1;
                } else if (g_signal_menu_active) {
                    if (ev.key == TB_KEY_ESC) {
                        g_signal_menu_active = 0;
                        need_redraw = 1;
                    } else if (ev.key == TB_KEY_ARROW_UP || ev.key == TB_KEY_CTRL_P) {
                        if (g_signal_selected > 0) g_signal_selected--;
                        need_redraw = 1;
                    } else if (ev.key == TB_KEY_ARROW_DOWN || ev.key == TB_KEY_CTRL_N) {
                        if (g_signal_selected < NUM_SIGNALS - 1) g_signal_selected++;
                        need_redraw = 1;
                    } else if (ev.key == TB_KEY_ENTER) {
                        if (g_selected_process >= 0 && g_selected_process < g_stats.process_count) {
                            int pid = g_stats.processes[g_selected_process].pid;
                            int sig = SIGNALS[g_signal_selected].signum;
                            send_signal_to_process(pid, sig);
                            g_signal_sent = 1;
                            g_signal_sent_pid = pid;
                            g_signal_sent_sig = sig;
                            g_signal_sent_time = get_time_ms();
                        }
                        g_signal_menu_active = 0;
                        need_redraw = 1;
                    }
                } else if (g_confirm_menu_active) {
                    if (ev.key == TB_KEY_ESC) {
                        g_confirm_menu_active = 0;
                        need_redraw = 1;
                    } else if (ev.key == TB_KEY_ENTER) {
                        if (g_selected_process >= 0 && g_selected_process < g_stats.process_count) {
                            int pid = g_stats.processes[g_selected_process].pid;
                            int sig = g_confirm_signal;
                            send_signal_to_process(pid, sig);
                            g_signal_sent = 1;
                            g_signal_sent_pid = pid;
                            g_signal_sent_sig = sig;
                            g_signal_sent_time = get_time_ms();
                        }
                        g_confirm_menu_active = 0;
                        need_redraw = 1;
                    }
                } else if (g_show_proc && g_stats.process_count > 0 && (ev.ch == 'k' || ev.ch == 'K')) {
                    g_confirm_menu_active = 1;
                    g_confirm_signal = SIGKILL;
                    need_redraw = 1;
                } else if (g_show_proc && g_stats.process_count > 0 && (ev.ch == 't' || ev.ch == 'T')) {
                    g_confirm_menu_active = 1;
                    g_confirm_signal = SIGTERM;
                    need_redraw = 1;
                } else if (g_show_proc && g_stats.process_count > 0 && (ev.ch == 's' || ev.ch == 'S')) {
                    g_signal_menu_active = 1;
                    g_signal_selected = 0;
                    need_redraw = 1;
                } else if (ev.ch == 'q' || ev.ch == 'Q' || ev.key == TB_KEY_ESC || 
                          ev.key == TB_KEY_CTRL_C) {
                    g_running = 0;
                } else if (!in_error_mode && g_show_proc && (ev.key == TB_KEY_CTRL_N || ev.key == TB_KEY_ARROW_DOWN)) {
                    if (g_selected_process < g_stats.process_count - 1) g_selected_process++;
                    need_redraw = 1;
                } else if (!in_error_mode && g_show_proc && (ev.key == TB_KEY_CTRL_P || ev.key == TB_KEY_ARROW_UP)) {
                    if (g_selected_process > 0) g_selected_process--;
                    need_redraw = 1;
                } else if (!in_error_mode && g_show_proc && (ev.key == TB_KEY_CTRL_V || ev.key == TB_KEY_PGDN)) {
                    g_selected_process += 10;
                    if (g_selected_process >= g_stats.process_count)
                        g_selected_process = g_stats.process_count - 1;
                    need_redraw = 1;
                } else if (!in_error_mode && g_show_proc && ((ev.key == 'v' && (ev.mod & TB_MOD_ALT)) || ev.key == TB_KEY_PGUP)) {
                    g_selected_process -= 10;
                    if (g_selected_process < 0) g_selected_process = 0;
                    need_redraw = 1;
                } else if (!in_error_mode && g_show_proc && (ev.key == TB_KEY_CTRL_A || ev.key == TB_KEY_HOME)) {
                    g_selected_process = 0;
                    need_redraw = 1;
                } else if (!in_error_mode && g_show_proc && (ev.key == TB_KEY_CTRL_E || ev.key == TB_KEY_END)) {
                    g_selected_process = g_stats.process_count - 1;
                    need_redraw = 1;
                }
            } else if (ev.type == TB_EVENT_RESIZE) {
                need_redraw = 1;
            }
        }
        
        if (need_redraw) {
            draw_screen();
        }
        
        if (g_signal_sent && get_time_ms() - g_signal_sent_time > 2000) {
            g_signal_sent = 0;
            need_redraw = 1;
        }
        
        now = get_time_ms();
        if (pane_toggled || sort_changed || now - last_update >= g_refresh_rate_ms) {
            if (pane_toggled) {
                save_settings();
            }
            g_elapsed_seconds = (now - prev_update) / 1000.0f;
            if (g_elapsed_seconds <= 0) g_elapsed_seconds = 1.0f;
            if (!in_error_mode || pane_toggled || sort_changed) {
                update_stats();
            }
            prev_update = now;
            last_update = now;
            draw_screen();
        }
    }
    
    save_settings();
    tb_shutdown();
    return 0;
}
