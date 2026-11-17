#include "rgd_update_merge.h"
#include <string.h>
void alt_rgi_merge_update(rgd_update_t *c,const rgd_update_t* upd) {
    if (!upd) return;

    if (upd->present & RGD_UPD_COMPONENT_IDS) {
        c->component_count = upd->component_count;
        if (c->component_count > MAX_COMPONENT_LIST) c->component_count = MAX_COMPONENT_LIST;
        memcpy(c->component_ids, upd->component_ids, sizeof(c->component_ids));
    }
    if (upd->present & RGD_UPD_ROUTE_STATE) c->route_state = upd->route_state;
    if (upd->present & RGD_UPD_MANEUVER_STATE) c->maneuver_state = upd->maneuver_state;
    if (upd->present & RGD_UPD_CURRENT_ROAD) memcpy(c->current_road, upd->current_road, sizeof(c->current_road));
    if (upd->present & RGD_UPD_DESTINATION) memcpy(c->destination, upd->destination, sizeof(c->destination));
    if (upd->present & RGD_UPD_ETA) c->eta = upd->eta;
    if (upd->present & RGD_UPD_TIME_REMAINING) c->time_remaining = upd->time_remaining;
    if (upd->present & RGD_UPD_DISTANCE_REMAINING) c->distance_remaining = upd->distance_remaining;
    if (upd->present & RGD_UPD_DISTANCE_STRING) memcpy(c->distance_string, upd->distance_string, sizeof(c->distance_string));
    if (upd->present & RGD_UPD_DISTANCE_UNITS) c->distance_units = upd->distance_units;
    if (upd->present & RGD_UPD_DIST_TO_MANEUVER) c->dist_to_maneuver = upd->dist_to_maneuver;
    if (upd->present & RGD_UPD_DIST_TO_MANEUVER_STR) memcpy(c->dist_to_maneuver_string, upd->dist_to_maneuver_string, sizeof(c->dist_to_maneuver_string));
    if (upd->present & RGD_UPD_DIST_TO_MANEUVER_UNI) c->dist_to_maneuver_units = upd->dist_to_maneuver_units;
    if (upd->present & RGD_UPD_MANEUVER_COUNT) c->maneuver_count = upd->maneuver_count;
    if (upd->present & RGD_UPD_MANEUVER_LIST) {
        c->maneuver_list_count = upd->maneuver_list_count;
        if (c->maneuver_list_count > MAX_MANEUVER_LIST) c->maneuver_list_count = MAX_MANEUVER_LIST;
        memcpy(c->maneuver_list, upd->maneuver_list, sizeof(c->maneuver_list));
    }
    if (upd->present & RGD_UPD_VISIBLE_IN_APP) c->visible_in_app = upd->visible_in_app;
    if (upd->present & RGD_UPD_LANE_INDEX) c->lane_guidance_index = upd->lane_guidance_index;
    if (upd->present & RGD_UPD_LANE_SLOT) c->lane_guidance_slot = upd->lane_guidance_slot;
    if (upd->present & RGD_UPD_LANE_TOTAL) c->lane_guidance_total = upd->lane_guidance_total;
    if (upd->present & RGD_UPD_LANE_SHOWING) c->lane_guidance_showing = upd->lane_guidance_showing;
    if (upd->present & RGD_UPD_SOURCE_NAME) memcpy(c->source_name, upd->source_name, sizeof(c->source_name));
    if (upd->present & RGD_UPD_SOURCE_SUPPORTS_RG) c->source_supports_route_guidance = upd->source_supports_route_guidance;

    c->present |= upd->present;
}

