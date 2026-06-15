#ifndef CONFIG_H
#define CONFIG_H

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

#define TB_OPT_ATTR_W 32
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "termbox2.h"
#pragma GCC diagnostic pop

/* Version and timing */
#define CTOP_VERSION "1.0.0"
#define REFRESH_RATE_MS 1000

/* Colors */
#define COLOR_BG 0x000000
#define COLOR_FG 0xcccccc
#define COLOR_CPU 0x88cc88
#define COLOR_CPU_GRAPH 0x44aa44
#define COLOR_MEM 0xccaa44
#define COLOR_NET_DOWN 0x44aaff
#define COLOR_NET_UP 0xff6666
#define COLOR_DISK 0xaa88cc
#define COLOR_PROC 0xcccccc
#define COLOR_HEADER 0x666666
#define COLOR_HIGH 0xff4444
#define COLOR_MED 0xffaa44
#define COLOR_LOW 0x44ff44
#define COLOR_BATTERY 0x88cc44
#define COLOR_TIME 0xffaa44

/* Limits */
#define MAX_PROCESSES 512
#define MAX_CPU_CORES 256
#define HISTORY_SIZE 120
#define MAX_DISKS 32

/* Process table column widths */
#define PROC_PID_WIDTH 8
#define PROC_CPU_WIDTH 6
#define PROC_MEM_WIDTH 8
#define PROC_PROG_MIN_WIDTH 8
#define PROC_CMD_MIN_WIDTH 8
#define PROC_USER_MIN_WIDTH 6
#define PROC_COLUMN_SPACING 4
#define PROC_NARROW_OFFSET 1

/* Sort modes */
#define SORT_CPU_LAZY 0
#define SORT_CPU_DIRECT 1
#define SORT_MEM 2
#define SORT_PID 3
#define SORT_NAME 4
#define SORT_MAX 5

/* Data structures */
typedef struct {
    char name[32];
    char device[32];
    char mount[256];
    unsigned long long total;
    unsigned long long used;
    unsigned long long free;
    unsigned long long read_sectors;
    unsigned long long write_sectors;
    unsigned long long prev_read;
    unsigned long long prev_write;
    float read_speed;
    float write_speed;
    int history_rx[HISTORY_SIZE];
    int history_tx[HISTORY_SIZE];
} DiskInfo;

typedef struct {
    int pid;
    int uid;
    char name[256];
    char cmdline[512];
    char user[32];
    char state;
    long utime;
    long stime;
    long prev_utime;
    long prev_stime;
    long mem_rss;
    float cpu_percent;
    float cpu_percent_lazy;
    float mem_percent;
} ProcessInfo;

typedef struct {
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
    unsigned long long prev_total, prev_idle;
    float percent;
    float history[HISTORY_SIZE];
    int history_idx;
} CoreStat;

typedef struct {
    int num_cores;
    CoreStat cores[MAX_CPU_CORES];
    CoreStat overall;
    unsigned long total_mem;
    unsigned long free_mem;
    unsigned long available_mem;
    unsigned long buffers;
    unsigned long cached;
    unsigned long swap_total;
    unsigned long swap_free;
    float mem_percent;
    float swap_percent;
    int process_count;
    int running_count;
    ProcessInfo processes[MAX_PROCESSES];
    int cpu_history[HISTORY_SIZE];
    int mem_history[HISTORY_SIZE];
    int history_index;
    unsigned long long net_rx_bytes;
    unsigned long long net_tx_bytes;
    unsigned long long prev_net_rx;
    unsigned long long prev_net_tx;
    float net_rx_speed;
    float net_tx_speed;
    int net_history_rx[HISTORY_SIZE];
    int net_history_tx[HISTORY_SIZE];
    DiskInfo disks[MAX_DISKS];
    int num_disks;
    int battery_percent;
    int battery_present;
    char battery_status[16];
} SystemStats;

typedef struct {
    int signum;
    const char *name;
    const char *desc;
} SignalInfo;

/* Global state */
extern SystemStats g_stats;
extern int g_running;
extern int g_selected_process;
extern int g_scroll_offset;
extern int g_signal_menu_active;
extern int g_signal_selected;
extern int g_confirm_menu_active;
extern int g_confirm_signal;
extern int g_signal_sent;
extern int g_signal_sent_pid;
extern int g_signal_sent_sig;
extern int64_t g_signal_sent_time;
extern int g_show_cpu;
extern int g_show_mem;
extern int g_show_disks;
extern int g_show_net;
extern int g_show_proc;
extern int g_sort_mode;
extern int g_refresh_rate_ms;
extern float g_elapsed_seconds;
extern long g_clk_tck;

extern const SignalInfo SIGNALS[];
extern const int NUM_SIGNALS;
extern const char *SUPERSCRIPT[];

/* Function declarations from utils.c */
int is_number(const char *str);
void get_username(int uid, char *buf, size_t buflen);
void format_bytes(unsigned long bytes, char *buf, size_t buflen);
void format_speed(float kbps, char *buf, size_t buflen);
const char *get_sort_name(void);
int64_t get_time_ms(void);
void get_config_dir(char *buf, size_t buflen);
void save_settings(void);
void load_settings(void);

/* Function declarations from stats.c */
void parse_cpu_stats(void);
void parse_meminfo(void);
void parse_net_stats(void);
void parse_disk_stats(void);
void parse_battery(void);
void parse_processes(void);
void update_stats(void);
int compare_processes(const void *a, const void *b);
int send_signal_to_process(int pid, int sig);

/* Function declarations from draw.c */
void draw_section_header(int x, int y, int num, const char *title, uint32_t color);
void draw_graph(int x, int y, int w, int h, float *data, int idx, uint32_t color);
void draw_mini_bar(int x, int y, int w, float percent, uint32_t color);
void draw_sparkline_horizontal(int x, int y, int w, float *data, int idx, uint32_t color);
void draw_cpu_section(int x, int y, int w, int h);
void draw_memory_section(int x, int y, int w, int h);
void draw_disk_section(int x, int y, int w, int h);
void draw_net_section(int x, int y, int w, int h);
void draw_process_list(int x, int y, int w, int h);
void draw_top_bar(int w);
void draw_help_bar(int y, int w);
void draw_signal_menu(int w, int h);
void draw_signal_sent_message(int w, int h);
void draw_confirm_menu(int w, int h, const char *sig_name);
void draw_error_screen(int w, int h);
void draw_screen(void);

/* Function declarations from ui.c */
void calculate_minimum_size(int *min_w, int *min_h);

#endif /* CONFIG_H */
