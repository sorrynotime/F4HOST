#include "host_gamepad.h"
#include "usart.h"
#include <stdio.h>

/* USART2 / PA2 TX / 115200 8N1, exclusively owned by this diagnostic logger.
 * One pending line, drained by TXE without blocking or interrupts. The descriptor
 * is dumped incrementally; live reports are sampled, never queued without bound.
 */
static char line[384];
static uint16_t line_size, line_pos;
static uint16_t dump_pos;
static uint8_t dump_stage;
static uint8_t axes_pending;
static uint32_t last_sequence, last_log;
static uint32_t last_health;
static GP_State logged_state;
static int last_host_state = -1;

static void set_line(int length)
{
    line_pos = 0;
    line_size = length > 0 && length < (int)sizeof(line) ? (uint16_t)length : 0;
}

static void hex_line(const char *name, const uint8_t *data, uint16_t length)
{
    uint16_t i, count = length - dump_pos;
    int used;
    if (count > 16U) count = 16U;
    used = snprintf(line, sizeof(line), "%s %04X:", name, (unsigned)dump_pos);
    for (i = 0; i < count; ++i)
        used += snprintf(line + used, sizeof(line) - (unsigned)used, " %02X", data[dump_pos + i]);
    used += snprintf(line + used, sizeof(line) - (unsigned)used, "\r\n");
    dump_pos = (uint16_t)(dump_pos + count);
    set_line(used);
}

void Gamepad_AppProcess(USBH_HandleTypeDef *host)
{
    if (line_pos < line_size) {
        if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TXE) != RESET)
            huart2.Instance->DR = (uint8_t)line[line_pos++];
        return;
    }
    if ((int)host->gState != last_host_state) {
        last_host_state = (int)host->gState;
        if (host->gState == HOST_CLASS || host->gState == HOST_ABORT_STATE) {
            dump_stage = 1; dump_pos = 0; last_sequence = 0;
            set_line(snprintf(line, sizeof(line),
                "USB state=%u VID=%04X PID=%04X interfaces=%u cfg=%u%s\r\n",
                (unsigned)host->gState, host->device.DevDesc.idVendor,
                host->device.DevDesc.idProduct, host->device.CfgDesc.bNumInterfaces,
                host->device.CfgDesc.wTotalLength,
                host->gState == HOST_ABORT_STATE ? " ABORT: inspect CFG / interface limits" : ""));
        } else {
            if (host->gState == HOST_IDLE || host->gState == HOST_DEV_DISCONNECTED) {
                dump_stage = 0;
                axes_pending = 0;
                Gamepad_Reset();
            }
            set_line(snprintf(line, sizeof(line), "USB state=%u enum=%u\r\n",
                              (unsigned)host->gState, (unsigned)host->EnumState));
        }
        return;
    }
    if (dump_stage == 1U) {
        uint16_t len = host->device.CfgDesc.wTotalLength;
        if (len > USBH_MAX_SIZE_CONFIGURATION) len = USBH_MAX_SIZE_CONFIGURATION;
        if (dump_pos < len) { hex_line("CFG", host->device.CfgDesc_Raw, len); return; }
        dump_stage = 2; dump_pos = 0;
        set_line(snprintf(line, sizeof(line),
            "PAD if=%u class=%02X sub=%02X proto=%02X IN=%02X mps=%u interval=%u desc=%u map=%d fields=%u\r\n",
            g_gamepad.interface_number, g_gamepad.interface_class, g_gamepad.subclass,
            g_gamepad.protocol, g_gamepad.in_ep, g_gamepad.packet_size, g_gamepad.interval,
            g_gamepad.descriptor_length, g_gamepad.descriptor_status, g_gamepad.layout.field_count));
        return;
    }
    if (dump_stage == 2U) {
        if (dump_pos < g_gamepad.descriptor_length) {
            hex_line("HID", g_gamepad.descriptor, g_gamepad.descriptor_length); return;
        }
        dump_stage = 3; dump_pos = 0;
    }
    if (dump_stage == 3U) {
        if (dump_pos < g_gamepad.layout.field_count) {
            const GP_Field *f = &g_gamepad.layout.fields[dump_pos++];
            set_line(snprintf(line, sizeof(line),
                "FIELD id=%u page=%04X usage=%04X bit=%u size=%u min=%ld max=%ld\r\n",
                f->report_id, f->usage_page, f->usage, f->bit_offset, f->bits,
                (long)f->logical_min, (long)f->logical_max));
            return;
        }
        dump_stage = 0;
    }
    if (axes_pending) {
        axes_pending = 0;
        set_line(snprintf(line, sizeof(line),
            "AX seq=%lu X=%ld Y=%ld Z=%ld Rx=%ld Ry=%ld Rz=%ld Slider=%ld Dial=%ld Wheel=%ld\r\n",
            (unsigned long)last_sequence, (long)logged_state.axes[0],
            (long)logged_state.axes[1], (long)logged_state.axes[2],
            (long)logged_state.axes[3], (long)logged_state.axes[4],
            (long)logged_state.axes[5], (long)logged_state.axes[6],
            (long)logged_state.axes[7], (long)logged_state.axes[8]));
        return;
    }
    if (g_gamepad.usb_ready && g_gamepad.sequence != last_sequence &&
        (uint32_t)(HAL_GetTick() - last_log) >= 100U) {
        uint16_t i;
        int used;
        last_sequence = g_gamepad.sequence; last_log = HAL_GetTick();
        axes_pending = g_gamepad.state_valid;
        logged_state = g_gamepad.state;
        used = snprintf(line, sizeof(line), "RAW seq=%lu len=%u:",
                        (unsigned long)g_gamepad.sequence, g_gamepad.raw_length);
        for (i = 0; i < g_gamepad.raw_length; ++i)
            used += snprintf(line + used, sizeof(line) - (unsigned)used, " %02X", g_gamepad.raw[i]);
        used += snprintf(line + used, sizeof(line) - (unsigned)used,
            " | valid=%u B=%08lX hat=%d axes=%03X err=%lu\r\n",
            g_gamepad.state_valid, (unsigned long)g_gamepad.state.buttons,
            g_gamepad.state.hat, g_gamepad.state.axes_valid, (unsigned long)g_gamepad.errors);
        set_line(used);
        return;
    }
    if ((uint32_t)(HAL_GetTick() - last_health) >= 1000U) {
        last_health = HAL_GetTick();
        set_line(snprintf(line, sizeof(line),
            "STATUS usb=%u valid=%u seq=%lu decoded=%lu age_ms=%lu err=%lu (radio unknown)\r\n",
            g_gamepad.usb_ready, g_gamepad.state_valid, (unsigned long)g_gamepad.sequence,
            (unsigned long)g_gamepad.decoded_sequence,
            (unsigned long)(last_health - g_gamepad.last_report_ms), (unsigned long)g_gamepad.errors));
    }
}
