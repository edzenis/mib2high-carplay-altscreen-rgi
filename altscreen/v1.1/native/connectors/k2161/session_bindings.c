#include <pthread.h>
#include "core/alt_session_ports.h"
#include "session_output_port.h"
#include "core/alt_trace_ports.h"
#include "core/alt_property_ports.h"
#include "input/trace_reader.h"
#include "input/trace_input.h"
#include "core/alt_capability_ports.h"
#include "connectors/k2161/composition/capabilities.h"
static pthread_once_t gate=PTHREAD_ONCE_INIT;
static void bind_ports(void) {
    alt_session_ports p;
    p.parse_mode=k2161_output_parse_mode;
    p.setup=k2161_output_session_setup;
    p.teardown=k2161_output_session_teardown;
    p.process_data=k2161_output_screen_data;
    alt_core_trace_input_port trace;
    trace.start=k2161_start_trace_readers;
    trace.classify=k2161_trace_classify;
    trace.gauge=k2161_gauge_from_trace;
    trace.zoom_area=k2161_zoom_area_from_trace;
    trace.wheel=k2161_wheel_from_trace;
    alt_core_bind_trace_input(&trace);
    alt_core_property_ports info;
    info.server_property=k2161_output_server_property;
    info.session_property=k2161_output_session_property;
    info.server_info=k2161_output_server_info;
    alt_core_bind_properties(&info);
    alt_capability_ports caps; caps.input=k2161_input_capabilities; caps.output=k2161_output_capabilities;
    alt_core_bind_capability_ports(&caps);
    alt_core_bind_session_ports(&p);
}
/* The platform composition root depends on output + core; neither input nor core
 * imports output-private implementation classes. */
void k2161_bind_session_ports(void) { (void)pthread_once(&gate,bind_ports); }
