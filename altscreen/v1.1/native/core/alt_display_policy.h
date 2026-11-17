#ifndef ALT_CORE_DISPLAY_POLICY_H
#define ALT_CORE_DISPLAY_POLICY_H
/* Firmware-neutral display contract. A connector supplies geometry and strings;
 * this core policy chooses which CarPlay descriptor capabilities to advertise.
 * No CFDictionary, QNX, Audi or device identifiers appear here. */
typedef struct alt_display_plan {
    int features;
    int initial_view;
    int shows_instruments;
    int draw_outside_safe_area;
    int view_transition_control;
    unsigned int views;
} alt_display_plan;
__attribute__((visibility("hidden"))) alt_display_plan alt_core_display_plan(unsigned int available_views);
__attribute__((visibility("hidden"))) int alt_core_display_rect_valid(int x,int y,int w,int h,int canvas_w,int canvas_h);
#endif
