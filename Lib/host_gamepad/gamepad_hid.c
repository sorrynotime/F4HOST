#include "gamepad_hid.h"
#include <limits.h>
#include <string.h>

typedef struct {
    uint32_t page, size, count;
    int32_t min, max;
    uint8_t id;
} Globals;

static int32_t signed_item(uint32_t value, uint8_t bytes)
{
    uint8_t bits = (uint8_t)(bytes * 8U);
    if (bits && bits < 32U && (value & (1UL << (bits - 1U))))
        return (int32_t)((int64_t)value - ((int64_t)1 << bits));
    if (bits == 32U && value > INT32_MAX)
        return (int32_t)((int64_t)value - ((int64_t)1 << 32));
    return (int32_t)value;
}

static int parse(GP_Layout *l, const uint8_t *d, uint16_t n)
{
    Globals g = {0}, stack[4];
    uint32_t usages[64], umin = 0, umax = 0;
    uint8_t count = 0, have_min = 0, have_max = 0, sp = 0;
    uint8_t collection[16], depth = 0, active = 0;
    uint16_t pos = 0;
    while (pos < n) {
        uint8_t prefix = d[pos++], bytes, type, tag, i;
        uint32_t v = 0;
        if (prefix == 0xFEU) return -1; /* Long items are not interpreted. */
        bytes = prefix & 3U;
        if (bytes == 3U) bytes = 4U;
        if ((uint32_t)pos + bytes > n) return -1;
        for (i = 0; i < bytes; ++i) v |= (uint32_t)d[pos++] << (8U * i);
        type = (prefix >> 2) & 3U;
        tag = prefix >> 4;
        if (type == 1U) {
            switch (tag) {
            case 0: if (v > 0xFFFFU) return -1; g.page = v; break;
            case 1: g.min = signed_item(v, bytes); break;
            case 2:
                if (g.min >= 0 && v > INT32_MAX) return -1;
                g.max = g.min < 0 ? signed_item(v, bytes) : (int32_t)v;
                break;
            case 7: g.size = v; break;
            case 8:
                if (!v || v > 255U) return -1;
                g.id = (uint8_t)v; l->has_report_ids = 1; break;
            case 9: g.count = v; break;
            case 10: if (sp == 4U) return -1; stack[sp++] = g; break;
            case 11: if (!sp) return -1; g = stack[--sp]; break;
            default: break; /* Physical range/unit do not change bit layout. */
            }
        } else if (type == 2U) {
            uint32_t usage = bytes == 4U ? v : (g.page << 16) | v;
            if (tag == 0U) {
                if (count == 64U) return -1;
                usages[count++] = usage;
            } else if (tag == 1U) { umin = usage; have_min = 1; }
            else if (tag == 2U) { umax = usage; have_max = 1; }
            else if (tag == 10U) return -1; /* Delimiters need alternative usage sets. */
        } else if (type == 0U) {
            if (tag == 10U) {
                uint32_t usage = count ? usages[0] : umin;
                if (depth == 16U) return -1;
                collection[depth++] = active;
                if (v == 1U) active = (uint8_t)(usage == 0x10004UL || usage == 0x10005UL);
            } else if (tag == 12U) {
                if (!depth) return -1;
                active = collection[--depth];
            } else if (tag == 8U) {
                GP_Report *r;
                uint32_t bits;
                uint16_t j;
                if (!depth || !g.size || g.size > 32U || !g.count || g.count > 512U)
                    return -1;
                bits = g.size * g.count;
                for (i = 0; i < l->report_count; ++i)
                    if (l->reports[i].id == g.id) break;
                if (i == l->report_count) {
                    if (i == GP_MAX_REPORTS) return -1;
                    l->reports[i].id = g.id;
                    ++l->report_count;
                }
                r = &l->reports[i];
                if (r->input_bits + bits > (GP_MAX_PACKET - (g.id ? 1U : 0U)) * 8U)
                    return -1;
                if (have_min != have_max || (have_min &&
                    (umax < umin || (umin >> 16) != (umax >> 16)))) return -1;
                if (active && !(v & 1U)) {
                    if (!(v & 2U) || (v & 4U) || g.max < g.min) return -1;
                    for (j = 0; j < g.count; ++j) {
                        uint32_t u = 0;
                        uint16_t page, usage;
                        GP_Field *f;
                        if (count) u = usages[j < count ? j : count - 1U];
                        else if (have_min) u = umin + (j <= umax - umin ? j : umax - umin);
                        page = (uint16_t)(u >> 16); usage = (uint16_t)u;
                        if (!((page == 9U && usage >= 1U && usage <= 32U) ||
                              (page == 1U && usage >= 0x30U && usage <= 0x39U))) continue;
                        if (l->field_count == GP_MAX_FIELDS) return -1;
                        /* Ambiguous duplicate physical controls require a device mapping. */
                        for (i = 0; i < l->field_count; ++i)
                            if (l->fields[i].usage_page == page && l->fields[i].usage == usage)
                                return -1;
                        f = &l->fields[l->field_count++];
                        f->bit_offset = (uint16_t)(r->input_bits + j * g.size);
                        f->bits = (uint8_t)g.size; f->report_id = g.id;
                        f->usage_page = page; f->usage = usage;
                        f->logical_min = g.min; f->logical_max = g.max;
                    }
                }
                r->input_bits = (uint16_t)(r->input_bits + bits);
            } else if (tag != 9U && tag != 11U) return -1;
            count = have_min = have_max = 0; umin = umax = 0;
        } else return -1;
    }
    if (depth || sp) return -1;
    if (l->has_report_ids)
        for (pos = 0; pos < l->report_count; ++pos)
            if (l->reports[pos].id == 0U) return -1;
    return l->field_count ? 1 : 0;
}

int GP_ParseDescriptor(GP_Layout *l, const uint8_t *d, uint16_t n)
{
    int result;
    if (!l) return -1;
    memset(l, 0, sizeof(*l));
    if (!d || !n) return -1;
    result = parse(l, d, n);
    if (result < 0) memset(l, 0, sizeof(*l));
    return result;
}

void GP_ResetState(GP_State *s)
{
    memset(s, 0, sizeof(*s));
    s->hat = -1;
}

int GP_DecodeReport(const GP_Layout *l, GP_State *s, const uint8_t *d, uint16_t n)
{
    GP_State next;
    uint8_t id = 0, i, found = 0;
    if (!l || !s || !d || !n || n > GP_MAX_PACKET) return -1;
    if (l->has_report_ids) { id = *d++; --n; }
    for (i = 0; i < l->report_count; ++i) if (l->reports[i].id == id) break;
    if (i == l->report_count) return 0;
    if ((uint32_t)n * 8U < l->reports[i].input_bits) return -1;
    next = *s;
    for (i = 0; i < l->field_count; ++i) {
        const GP_Field *f = &l->fields[i];
        uint8_t b;
        uint32_t raw = 0;
        int32_t value;
        if (f->report_id != id) continue;
        for (b = 0; b < f->bits; ++b) {
            uint16_t bit = (uint16_t)(f->bit_offset + b);
            raw |= (uint32_t)((d[bit / 8U] >> (bit % 8U)) & 1U) << b;
        }
        value = (int32_t)raw;
        if (f->logical_min < 0 && (raw & (1UL << (f->bits - 1U))))
            value = (int32_t)((int64_t)raw - ((int64_t)1 << f->bits));
        if (f->usage == 0x39U && f->usage_page == 1U) {
            int64_t range = (int64_t)f->logical_max - f->logical_min + 1;
            next.hat = -1; next.hat_valid = 1;
            if (value >= f->logical_min && value <= f->logical_max && (range == 8 || range == 4))
                next.hat = (int8_t)((value - f->logical_min) * (range == 4 ? 2 : 1));
        } else {
            if (value < f->logical_min || value > f->logical_max) return -1;
            if (f->usage_page == 9U) {
                uint32_t mask = 1UL << (f->usage - 1U);
                next.buttons = (next.buttons & ~mask) | (value ? mask : 0U);
            } else {
                uint8_t axis = (uint8_t)(f->usage - 0x30U);
                next.axes[axis] = value; next.axes_valid |= (uint16_t)(1U << axis);
            }
        }
        found = 1;
    }
    if (found) *s = next;
    return found;
}
