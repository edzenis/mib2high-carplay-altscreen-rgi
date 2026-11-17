#include "trace_input.h"
#include "gauge_bap54.h"
#include "core/alt_trace_ports.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int k2161_gauge_from_trace(const char *line,int buffered,int *slot,unsigned int *raw) {
    return !buffered && altscreen_bap54_layout_slot(line,slot,raw);
}
int k2161_zoom_area_from_trace(const char *line,int *slot,int *x,int *y,int *w,int *h) {
    const char *p=line?strstr(line,"DSIMapViewerZoomEngine#setZoomArea#"):NULL;
    if(!p || !(p=strstr(p,"Rect:")) || sscanf(p+5,"%d:%d:%d:%d",x,y,w,h)!=4) return 0;
    *slot=(*w<=560)?1:((*w<=800)?0:-1);
    return *slot>=0;
}
int k2161_wheel_from_trace(const char *line,int buffered,int *step) {
    const char *p=line?strstr(line,"DSIKeyPanelListener#updateEncoder2#"):NULL;
    const char *match=p?strstr(p,"|int:4|int:40|int:"):NULL;
    if(buffered || !match || !step) return 0;
    *step=atoi(match+strlen("|int:4|int:40|int:"));return 1;
}

/* Firmware-specific trace syntax lives exclusively in the input adapter.
 * The graphics output receives only normalized classifications/values. */
void k2161_trace_classify(const char *line,alt_trace_tags_t *t) {
    const char *enc;
    if (!line || !t) return;
    t->primary=(line[0]=='1' && line[1]=='#');
    t->has_keypanel=strstr(line,"DSIKeyPanelListener#")!=NULL;
    t->has_viewport=strstr(line,"DSIMapViewerControl")!=NULL &&
        (strstr(line,"ViewScreenViewPort")!=NULL || strstr(line,"ViewPort")!=NULL);
    t->has_zoomarea=strstr(line,"DSIMapViewerZoomEngine#setZoomArea#")!=NULL;
    enc=strstr(line,"DSIKeyPanelListener#updateEncoder2#");
    t->has_encoder=enc!=NULL;
    t->matches_zoom_encoder=enc && strstr(enc,"|int:4|int:40|int:")!=NULL;
    if (t->primary) {
        if(strstr(line,"DSIKOMOView")!=NULL)t->diag_class=0;
        else if(strstr(line,"DSIMapViewerZoomEngine")!=NULL)t->diag_class=1;
        else if(t->has_viewport)t->diag_class=3;
        else if(strstr(line,"DSIKeyPanel")!=NULL &&
              (strstr(line,"|int:4|int:40|")!=NULL || strstr(line,"|int:4|int:35|")!=NULL))
            t->diag_class=2;
    }
}
