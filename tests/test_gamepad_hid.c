#include "gamepad_hid.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Synthetic descriptors: independent of any unverified KP50 byte layout. */
static const uint8_t basic[] = {
    0x05,1, 0x09,5, 0xA1,1,
    0x05,9, 0x19,1, 0x29,12, 0x15,0, 0x25,1, 0x75,1, 0x95,12, 0x81,2,
    0x05,1, 0x09,0x39, 0x15,0, 0x25,7, 0x75,4, 0x95,1, 0x81,0x42,
    0x09,0x30, 0x09,0x31, 0x15,0x81, 0x25,0x7F, 0x75,8, 0x95,2, 0x81,2,
    0xC0
};

int main(void)
{
    GP_Layout layout;
    GP_State state, old;
    uint8_t report[] = {0x01,0x80,0x81,0x7F};
    uint8_t numbered[] = {
        0x05,1, 0x09,5, 0xA1,1,
        0x85,1, 0x09,0x30, 0x15,0, 0x26,0xFF,0, 0x75,8, 0x95,1, 0x81,2,
        0x85,2, 0x05,9, 0x09,1, 0x15,0, 0x25,1, 0x75,1, 0x95,1, 0x81,2,
        0x75,7, 0x95,1, 0x81,1, 0xC0
    };
    uint8_t cross[] = {
        0x05,1, 0x09,4, 0xA1,1,
        0x75,3, 0x95,1, 0x81,1,
        0x09,0x30, 0x16,0,0xF8, 0x26,0xFF,7, 0x75,12, 0x95,1, 0x81,2,
        0xC0
    };
    uint8_t signed_cross[] = {0xF8,0x7F}; /* 12-bit -1 at bit 3 */
    uint8_t id1[] = {1,200}, id2[] = {2,1}, unknown[] = {3,9};
    uint8_t malformed[sizeof(basic)];
    unsigned i, j;

    GP_ResetState(&state);
    assert(state.hat == -1);
    assert(GP_ParseDescriptor(&layout, basic, sizeof(basic)) == 1);
    assert(layout.field_count == 15 && layout.reports[0].input_bits == 32);
    assert(GP_DecodeReport(&layout, &state, report, sizeof(report)) == 1);
    assert(state.buttons == 1 && state.hat == -1);
    assert(state.axes[0] == -127 && state.axes[1] == 127 && state.axes_valid == 3);
    report[1] = 0x70;
    assert(GP_DecodeReport(&layout, &state, report, sizeof(report)) == 1 && state.hat == 7);
    old = state;
    assert(GP_DecodeReport(&layout, &state, report, 3) == -1);
    assert(memcmp(&old, &state, sizeof(old)) == 0);
    report[2] = 0x80; /* Outside logical minimum: reject atomically. */
    assert(GP_DecodeReport(&layout, &state, report, sizeof(report)) == -1);
    assert(memcmp(&old, &state, sizeof(old)) == 0);

    GP_ResetState(&state);
    assert(GP_ParseDescriptor(&layout, numbered, sizeof(numbered)) == 1);
    assert(GP_DecodeReport(&layout, &state, id1, sizeof(id1)) == 1);
    assert(GP_DecodeReport(&layout, &state, id2, sizeof(id2)) == 1);
    assert(state.axes[0] == 200 && state.buttons == 1);
    old = state;
    assert(GP_DecodeReport(&layout, &state, unknown, sizeof(unknown)) == 0);
    assert(GP_DecodeReport(&layout, &state, id1, 1) == -1);
    assert(memcmp(&old, &state, sizeof(old)) == 0);

    assert(GP_ParseDescriptor(&layout, cross, sizeof(cross)) == 1);
    assert(GP_DecodeReport(&layout, &state, signed_cross, sizeof(signed_cross)) == 1);
    assert(state.axes[0] == -1);
    for (i = 0; i < sizeof(basic); ++i)
        assert(GP_ParseDescriptor(&layout, basic, (uint16_t)i) != 1);
    memcpy(malformed, basic, sizeof(basic));
    malformed[3] = 2; /* Mouse application: not a gamepad. */
    assert(GP_ParseDescriptor(&layout, malformed, sizeof(malformed)) == 0);
    malformed[3] = 5; malformed[21] = 0; /* Array input: unsupported, keep raw only. */
    assert(GP_ParseDescriptor(&layout, malformed, sizeof(malformed)) == -1);
    assert(layout.field_count == 0);
    {
        const uint8_t pop[] = {0xB4};
        const uint8_t long_item[] = {0xFE,0xFF,0};
        const uint8_t invalid_id[] = {0x85,0};
        assert(GP_ParseDescriptor(&layout, pop, sizeof(pop)) == -1);
        assert(GP_ParseDescriptor(&layout, long_item, sizeof(long_item)) == -1);
        assert(GP_ParseDescriptor(&layout, invalid_id, sizeof(invalid_id)) == -1);
    }
    /* Exhaustive single-byte mutations exercise malformed lengths/counts/usages.
     * Run under host AddressSanitizer/UBSan when available.
     */
    for (i = 0; i < sizeof(basic); ++i) for (j = 0; j < 256; ++j) {
        uint8_t full[GP_MAX_PACKET] = {0};
        memcpy(malformed, basic, sizeof(basic)); malformed[i] = (uint8_t)j;
        if (GP_ParseDescriptor(&layout, malformed, sizeof(malformed)) >= 0) {
            GP_ResetState(&state);
            (void)GP_DecodeReport(&layout, &state, full, sizeof(full));
        } else assert(layout.field_count == 0);
    }
    puts("gamepad HID: fields, signed/cross-byte axes, hat, report IDs, short packets, malformed descriptors and mutations passed");
    return 0;
}
