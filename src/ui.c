#include "../config.h"

void calculate_minimum_size(int *min_w, int *min_h) {
    *min_w = 80;
    *min_h = 24;
    
    int num_left_panes = g_show_mem + g_show_disks + g_show_net;
    
    int base_h = 2;
    int content_h = 0;
    
    if (g_show_cpu) {
        content_h += 5;
    }
    
    if (num_left_panes > 0 || g_show_proc) {
        int left_panes_h = num_left_panes * 4;
        
        int proc_h = g_show_proc ? 6 : 0;
        
        int bottom_h = (left_panes_h > proc_h) ? left_panes_h : proc_h;
        if (bottom_h < 6) bottom_h = 6;
        
        content_h += bottom_h;
        
        int min_proc_width = g_show_proc ? 45 : 0;
        int min_left_width = num_left_panes > 0 ? 20 : 0;
        int required_width = min_left_width + min_proc_width + 4;
        
        if (required_width > *min_w) *min_w = required_width;
    }
    
    *min_h = base_h + content_h;
    if (*min_h < 10) *min_h = 10;
}
