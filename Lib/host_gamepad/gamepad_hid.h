#ifndef GAMEPAD_HID_H
#define GAMEPAD_HID_H

#include <stdint.h>

#define GP_MAX_FIELDS 64U
#define GP_MAX_REPORTS 8U
#define GP_MAX_PACKET 64U
#define GP_AXIS_COUNT 9U /* Generic Desktop usages 0x30..0x38, NOT a physical mapping. */

typedef struct {
    uint16_t bit_offset;
    uint16_t usage_page, usage;
    int32_t logical_min, logical_max;
    uint8_t bits, report_id;
} GP_Field;

typedef struct {
    uint16_t input_bits;
    uint8_t id;
} GP_Report;

typedef struct {
    GP_Field fields[GP_MAX_FIELDS];
    GP_Report reports[GP_MAX_REPORTS];
    uint8_t field_count, report_count, has_report_ids;
} GP_Layout;

typedef struct {
    uint32_t buttons; /* Button usages 1..32 -> bits 0..31. */
    int32_t axes[GP_AXIS_COUNT]; /* Raw logical values, consult layout for ranges. */
    uint16_t axes_valid;
    int8_t hat; /* -1 neutral/unknown; 0 north, clockwise through 7 northwest. */
    uint8_t hat_valid;
} GP_State;

/* 1: supported fields, 0: no gamepad fields, -1: malformed/unsupported/limit.
 * On failure layout is empty. Only absolute Variable input fields are mapped.
 */
int GP_ParseDescriptor(GP_Layout *layout, const uint8_t *data, uint16_t length);
/* 1: applied, 0: unknown/unmapped report, -1: short/invalid report.
 * Failed decoding leaves state untouched. Separate report IDs update only their fields.
 */
int GP_DecodeReport(const GP_Layout *layout, GP_State *state,
                    const uint8_t *data, uint16_t length);
void GP_ResetState(GP_State *state);

#endif
