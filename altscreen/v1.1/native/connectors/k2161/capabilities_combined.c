/* AltScreen: explicit declaration of what the existing K2161 adapter already implements.
 * Do not infer unsupported future firmware features from this. */
#include "connectors/k2161/composition/capabilities.h"
#include "core/alt_actions.h"
unsigned int k2161_capabilities(void) {
    return k2161_input_capabilities() | k2161_output_capabilities();
}
