/* K2161 writable Identification builder-table hook. */
#include <stdint.h>
#include <stddef.h>
#include "ident_hook.h"
#include "status.h"
#include "diagnostics.h"

typedef int (*ident_builder_fn)(void *ctx, void *pkt);
typedef int (*packet_addparam_blob_fn)(void *ctx, void *pkt, uint16_t parameter_id,
                                      const void *data, size_t data_length);
enum {
    K2161_IDENT_ID_OFFSET = 0x000a7d80,
    K2161_IDENT_BUILDER_OFFSET = 0x000a7d84,
    K2161_GPS_BUILDER_OFFSET = 0x0000a11c,
    K2161_ADDPARAM_BLOB_OFFSET = 0x0001d0d8
};
static ident_builder_fn original_gps_builder;
static ident_builder_fn *gps_builder_slot;
static packet_addparam_blob_fn packet_addparam_blob;
static void *installed_driver_base;
static volatile int hook_installed;
static const uint8_t rgi_component_payload[82] = {
    0x00,0x06,0x00,0x00,0x00,0x10,
    0x00,0x22,0x00,0x01,
    0x52,0x6f,0x75,0x74,0x65,0x47,0x75,0x69,0x64,0x61,0x6e,0x63,
    0x65,0x44,0x69,0x73,0x70,0x6c,0x61,0x79,0x43,0x6f,0x6d,0x70,
    0x6f,0x6e,0x65,0x6e,0x74,0x00,
    0x00,0x06,0x00,0x02,0x01,0x00, 0x00,0x06,0x00,0x03,0x01,0x00,
    0x00,0x06,0x00,0x04,0x01,0x00, 0x00,0x06,0x00,0x05,0x01,0x00,
    0x00,0x06,0x00,0x06,0x00,0x06, 0x00,0x06,0x00,0x07,0x01,0x00,
    0x00,0x06,0x00,0x08,0x00,0x06
};

/*
 * AltScreen compatibility component for the K2161 / Carlinkit 7115 proxy.
 * On the captured K2161 identification the stock VehicleInformation builder
 * exists but emits no top-level 0x0014. The proxy's route-message advertisement
 * was found to depend on receiving VehicleInformation. This is deliberately
 * firmware-specific; do not introduce the fabricated component into AltCore.
 *
 * Minimal VehicleInformationComponent (Apple iAP2 TLV):
 *   nested 0x0000 Identifier = 0x0011 (distinct from RGI 0x0010),
 *   nested 0x0001 Name = "Vehicle Information" (NUL-terminated).
 * No vehicle state, sensors, speed, range or engine type is claimed.
 */
static const uint8_t vehicle_info_payload[] = {
    0x00,0x06,0x00,0x00,0x00,0x11,
    0x00,0x18,0x00,0x01,
    'V','e','h','i','c','l','e',' ','I','n','f','o','r','m','a','t','i','o','n',0x00
};

/*
 * AltScreen note:
 *
 * The earlier dev experiment also synthesized IdentificationInformation
 * parameter 24 (WirelessCarPlayTransportComponent) with ThemeAssets support.
 * That is intentionally NOT emitted here anymore.  The native type-111 work
 * now follows the observed working architecture: keep iAP2 identification
 * stock/observed and negotiate the alternate display in the AirPlay layer.
 *
 * RouteGuidanceDisplayComponent (parameter 30) remains required for the
 * already-working semantic RGI feature and is unchanged.
 */

static int append_rgi_component(void *ctx, void *pkt) {
    int rc;
    k2161_status_ident_attempt(ctx);
    if (!packet_addparam_blob) {
        rgi_diag_log("IDENT", "emit_001e=FAIL reason=helper_missing");
        return -1;
    }
    rc = packet_addparam_blob(ctx, pkt, 0x001e, rgi_component_payload,
                              sizeof(rgi_component_payload));
    rgi_diag_log("IDENT", "emit_001e rc=%d bytes=%u component=0010", rc,
                 (unsigned)sizeof(rgi_component_payload));
    if (rc == 0) k2161_status_ident_success(ctx);
    return rc;
}

int k2161_wrapped_gps_builder(void *ctx, void *pkt) {
    int rc;
    if (!original_gps_builder) {
        rgi_diag_log("IDENT", "builder=FAIL reason=original_null");
        return -1;
    }
    rc = original_gps_builder(ctx, pkt);
    rgi_diag_log("IDENT", "stock_gps_builder rc=%d", rc);
    if (rc == -1) return -1;
    /* Advertise minimal VehicleInformation before RGI; do not change
     * the existing RGI TLV bytes or HUD/BAP forwarding path. */
    if (!packet_addparam_blob) {
        rgi_diag_log("IDENT", "emit_0014=FAIL reason=helper_missing");
        return -1;
    }
    {
        int vehicle_rc = packet_addparam_blob(ctx, pkt, 0x0014,
                                vehicle_info_payload, sizeof(vehicle_info_payload));
        rgi_diag_log("IDENT", "emit_0014 rc=%d bytes=%u component=0011", vehicle_rc,
                     (unsigned)sizeof(vehicle_info_payload));
        if (vehicle_rc != 0) return -1;
    }
    if (append_rgi_component(ctx, pkt) != 0) return -1;
    rgi_diag_log("IDENT", "builder_completed emitted_0014=1 emitted_001e=1");
    return 1;
}

int k2161_ident_try_install_base(void *driver_base) {
    uint8_t *base = (uint8_t *)driver_base;
    ident_builder_fn expected, current;
    packet_addparam_blob_fn helper;

    if (!base) return 0;
    if (hook_installed && installed_driver_base == base) return 1;
    rgi_diag_log("IDENT", "install_attempt driver_base=%p", driver_base);

    if (*(volatile uint32_t *)(base + K2161_IDENT_ID_OFFSET) != 0x0016) {
        rgi_diag_log("IDENT", "install=FAIL reason=builder_id_mismatch");
        return -1;
    }

    gps_builder_slot =
        (ident_builder_fn *)(base + K2161_IDENT_BUILDER_OFFSET);

    expected =
        (ident_builder_fn)(base + K2161_GPS_BUILDER_OFFSET);

    current = *(volatile ident_builder_fn *)gps_builder_slot;

    if (current == k2161_wrapped_gps_builder) {
        hook_installed = 1;
        installed_driver_base = base;
        return 1;
    }

    if (current != expected) {
        rgi_diag_log("IDENT", "install=FAIL reason=builder_ptr_mismatch");
        return -1;
    }

    /*
     * driver_base has already been independently validated by the
     * dual-export resolver before this function is called.
     */
    helper = (packet_addparam_blob_fn)
        (base + K2161_ADDPARAM_BLOB_OFFSET);

    packet_addparam_blob = helper;
    original_gps_builder = expected;
    installed_driver_base = base;

    /*
     * K2161 ARMv7: naturally aligned 32-bit function-pointer store.
     * Validate immediately before and after replacement.
     */
    if (*(volatile ident_builder_fn *)gps_builder_slot != expected)
        return -1;

    *(volatile ident_builder_fn *)gps_builder_slot =
        k2161_wrapped_gps_builder;

    if (*(volatile ident_builder_fn *)gps_builder_slot !=
            k2161_wrapped_gps_builder) {
        original_gps_builder = NULL;
        packet_addparam_blob = NULL;
        installed_driver_base = NULL;
        gps_builder_slot = NULL;
        return -1;
    }

    hook_installed = 1;
    k2161_status_ident_hook_ready();
    rgi_diag_log("IDENT", "install=PASS source=validated_k2161_builder_table");
    return 1;
}

int k2161_ident_restore(void) {
    if (!hook_installed || !gps_builder_slot || !original_gps_builder) return 0;
    if (*(volatile ident_builder_fn *)gps_builder_slot != k2161_wrapped_gps_builder)
        return -1;
    *(volatile ident_builder_fn *)gps_builder_slot = original_gps_builder;
    hook_installed = 0;
    return 1;
}
