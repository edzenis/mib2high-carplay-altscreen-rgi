#include "alt_display_policy.h"
#include <limits.h>
alt_display_plan alt_core_display_plan(unsigned int views) {
    alt_display_plan result;
    /* AltScreen classic CarPlay feature policy: no virtual HID/feature-2 opt-in. */
    result.features=0;
    result.initial_view=0;
    result.shows_instruments=1;
    result.draw_outside_safe_area=0;
    result.view_transition_control=0;
    result.views=views;
    return result;
}
int alt_core_display_rect_valid(int x,int y,int w,int h,int canvas_w,int canvas_h) {
    /* Use subtraction to avoid signed overflow on hostile profile data. */
    return x>=0 && y>=0 && w>0 && h>0 && canvas_w>0 && canvas_h>0
        && x<=canvas_w && y<=canvas_h && w<=canvas_w-x && h<=canvas_h-y;
}
