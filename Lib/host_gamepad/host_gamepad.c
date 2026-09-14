#include "host_gamepad.h"
#include "usbh_ctlreq.h"
#include "usbh_ioreq.h"
#include <string.h>

GamepadInfo g_gamepad;
/* One receiver, one active IN endpoint, no heap allocation. */
static struct {
    uint8_t pipe, opened, pending, stalled, request_stage;
    uint32_t submitted;
    uint8_t rx[GP_MAX_PACKET];
} transport;

static USBH_StatusTypeDef init(USBH_HandleTypeDef *host);
static USBH_StatusTypeDef deinit(USBH_HandleTypeDef *host);
static USBH_StatusTypeDef requests(USBH_HandleTypeDef *host);
static USBH_StatusTypeDef process(USBH_HandleTypeDef *host);
static USBH_StatusTypeDef sof(USBH_HandleTypeDef *host) { (void)host; return USBH_OK; }

USBH_ClassTypeDef GamepadHID_Class = {
    "Gamepad HID", 0x03U, init, deinit, requests, process, sof, NULL
};
USBH_ClassTypeDef GamepadRaw_Class = {
    "Receiver raw", 0xFFU, init, deinit, requests, process, sof, NULL
};

void Gamepad_Reset(void)
{
    memset(&g_gamepad, 0, sizeof(g_gamepad));
    GP_ResetState(&g_gamepad.state);
}

static uint16_t report_descriptor_length(USBH_HandleTypeDef *host, uint8_t number)
{
    const uint8_t *raw = host->device.CfgDesc_Raw;
    uint16_t end = host->device.CfgDesc.wTotalLength, pos = 0;
    uint8_t selected = 0;
    if (end > USBH_MAX_SIZE_CONFIGURATION) return 0;
    while (pos + 2U <= end) {
        uint8_t size = raw[pos], type = raw[pos + 1U];
        if (size < 2U || pos + size > end) return 0;
        if (type == 4U) {
            if (size < 9U) return 0;
            selected = (uint8_t)(raw[pos + 2U] == number && raw[pos + 3U] == 0U);
        } else if (type == 0x21U && selected) {
            uint8_t i;
            if (size < 6U || 6U + 3U * raw[pos + 5U] > size) return 0;
            for (i = 0; i < raw[pos + 5U]; ++i) {
                uint16_t off = (uint16_t)(pos + 6U + i * 3U);
                if (raw[off] == 0x22U)
                    return (uint16_t)(raw[off + 1U] | ((uint16_t)raw[off + 2U] << 8));
            }
        }
        pos = (uint16_t)(pos + size);
    }
    return 0;
}

static USBH_StatusTypeDef init(USBH_HandleTypeDef *host)
{
    uint8_t i, e;
    Gamepad_Reset();
    memset(&transport, 0, sizeof(transport));
    g_gamepad.vid = host->device.DevDesc.idVendor;
    g_gamepad.pid = host->device.DevDesc.idProduct;
    for (i = 0; i < USBH_MAX_NUM_INTERFACES; ++i) {
        USBH_InterfaceDescTypeDef *itf = &host->device.CfgDesc.Itf_Desc[i];
        if (itf->bLength != 9U || itf->bAlternateSetting != 0U ||
            itf->bInterfaceClass != host->pActiveClass->ClassCode) continue;
#if GAMEPAD_INTERFACE_NUMBER >= 0
        if (itf->bInterfaceNumber != GAMEPAD_INTERFACE_NUMBER)
            continue;
#endif
        if (itf->bInterfaceClass == 3U && itf->bInterfaceSubClass == 1U) continue;
        for (e = 0; e < itf->bNumEndpoints && e < USBH_MAX_NUM_ENDPOINTS; ++e) {
            USBH_EpDescTypeDef *ep = &itf->Ep_Desc[e];
            if ((ep->bEndpointAddress & 0x80U) == 0U || (ep->bmAttributes & 3U) != 3U)
                continue;
            if (!ep->wMaxPacketSize || ep->wMaxPacketSize > GP_MAX_PACKET || !ep->bInterval)
                continue;
            if (USBH_SelectInterface(host, i) != USBH_OK) return USBH_FAIL;
            transport.pipe = USBH_AllocPipe(host, ep->bEndpointAddress);
            if (transport.pipe == 0xFFU) return USBH_FAIL;
            if (USBH_OpenPipe(host, transport.pipe, ep->bEndpointAddress, host->device.address,
                             host->device.speed, USB_EP_TYPE_INTR, ep->wMaxPacketSize) != USBH_OK) {
                USBH_FreePipe(host, transport.pipe);
                return USBH_FAIL;
            }
            transport.opened = 1;
            USBH_LL_SetToggle(host, transport.pipe, 0U);
            host->pActiveClass->pData = &transport;
            g_gamepad.interface_number = itf->bInterfaceNumber;
            g_gamepad.interface_class = itf->bInterfaceClass;
            g_gamepad.subclass = itf->bInterfaceSubClass;
            g_gamepad.protocol = itf->bInterfaceProtocol;
            g_gamepad.in_ep = ep->bEndpointAddress;
            g_gamepad.packet_size = ep->wMaxPacketSize;
            g_gamepad.interval = ep->bInterval;
            if (itf->bInterfaceClass == 3U) {
                g_gamepad.descriptor_length = report_descriptor_length(host, itf->bInterfaceNumber);
                if (!g_gamepad.descriptor_length || g_gamepad.descriptor_length > GAMEPAD_DESCRIPTOR_SIZE) {
                    g_gamepad.descriptor_length = 0;
                    g_gamepad.descriptor_status = -1;
                    transport.request_stage = 1;
                }
            } else transport.request_stage = 1; /* No HID requests to vendor interfaces. */
            return USBH_OK;
        }
    }
    return USBH_FAIL;
}

static USBH_StatusTypeDef deinit(USBH_HandleTypeDef *host)
{
    if (transport.opened) {
        USBH_ClosePipe(host, transport.pipe);
        USBH_FreePipe(host, transport.pipe);
    }
    memset(&transport, 0, sizeof(transport));
    host->pActiveClass->pData = NULL;
    Gamepad_Reset();
    return USBH_OK;
}

static USBH_StatusTypeDef requests(USBH_HandleTypeDef *host)
{
    USBH_StatusTypeDef status;
    if (transport.request_stage == 0U) {
        if (host->RequestState == CMD_SEND) {
            host->Control.setup.b.bmRequestType = 0x81U;
            host->Control.setup.b.bRequest = USB_REQ_GET_DESCRIPTOR;
            host->Control.setup.b.wValue.w = 0x2200U;
            host->Control.setup.b.wIndex.w = g_gamepad.interface_number;
            host->Control.setup.b.wLength.w = g_gamepad.descriptor_length;
        }
        status = USBH_CtlReq(host, g_gamepad.descriptor, g_gamepad.descriptor_length);
        if (status == USBH_BUSY) return USBH_BUSY;
        if (status == USBH_OK) {
            /* The control-IN channel retains the full data-stage byte count. */
            uint32_t actual = USBH_LL_GetLastXferSize(host, host->Control.pipe_in);
            if (actual == g_gamepad.descriptor_length)
                g_gamepad.descriptor_status = (int8_t)GP_ParseDescriptor(&g_gamepad.layout,
                    g_gamepad.descriptor, g_gamepad.descriptor_length);
            else {
                g_gamepad.descriptor_length = actual < g_gamepad.descriptor_length ? (uint16_t)actual : 0;
                g_gamepad.descriptor_status = -1;
            }
        } else {
            g_gamepad.descriptor_status = -1;
            g_gamepad.descriptor_length = 0;
            if (status != USBH_NOT_SUPPORTED) return USBH_FAIL;
        }
        transport.request_stage = 1;
    }
    /* Non-Boot HID needs neither SET_PROTOCOL nor an initial GET_REPORT.
     * Poll interrupt IN directly. No speculative mode/rumble commands are sent.
     */
    g_gamepad.usb_ready = 1;
    transport.submitted = host->Timer - g_gamepad.interval;
    host->pUser(host, HOST_USER_CLASS_ACTIVE);
    return USBH_OK;
}

static USBH_StatusTypeDef process(USBH_HandleTypeDef *host)
{
    if (!g_gamepad.usb_ready) return USBH_FAIL;
    if (transport.stalled) {
        USBH_StatusTypeDef status = USBH_ClrFeature(host, g_gamepad.in_ep);
        if (status == USBH_OK) {
            USBH_LL_SetToggle(host, transport.pipe, 0U);
            transport.stalled = 0;
        } else if (status != USBH_BUSY) {
            ++g_gamepad.errors;
            g_gamepad.usb_ready = g_gamepad.state_valid = 0;
            GP_ResetState(&g_gamepad.state);
            /* Stop this endpoint until replug rather than loop on a failed clear. */
            return USBH_FAIL;
        }
        return USBH_OK;
    }
    if (transport.pending) {
        USBH_URBStateTypeDef urb = USBH_LL_GetURBState(host, transport.pipe);
        if (urb == USBH_URB_DONE) {
            uint32_t length = USBH_LL_GetLastXferSize(host, transport.pipe);
            transport.pending = 0;
            if (length && length <= g_gamepad.packet_size) {
                int decoded;
                memset(g_gamepad.raw, 0, sizeof(g_gamepad.raw));
                memcpy(g_gamepad.raw, transport.rx, length);
                g_gamepad.raw_length = (uint16_t)length;
                ++g_gamepad.sequence;
                g_gamepad.last_report_ms = HAL_GetTick();
                decoded = GP_DecodeReport(&g_gamepad.layout, &g_gamepad.state,
                                           transport.rx, (uint16_t)length);
                if (decoded > 0) {
                    ++g_gamepad.decoded_sequence;
                    g_gamepad.state_valid = 1;
                } else if (decoded < 0) {
                    ++g_gamepad.errors;
                    g_gamepad.state_valid = 0;
                    GP_ResetState(&g_gamepad.state);
                }
            } else if (length) ++g_gamepad.errors;
        } else if (urb == USBH_URB_NOTREADY) {
            transport.pending = 0; /* NAK: normal; retry at advertised interval. */
        } else if (urb == USBH_URB_STALL || urb == USBH_URB_ERROR) {
            transport.pending = 0;
            ++g_gamepad.errors;
            g_gamepad.state_valid = 0;
            GP_ResetState(&g_gamepad.state);
            if (urb == USBH_URB_STALL) transport.stalled = 1;
        } else if ((uint32_t)(host->Timer - transport.submitted) > 1000U) {
            /* An interrupt NAK completes as NOTREADY in this STM32 HCD. A channel
             * stuck without completion must not keep the last controls valid.
             */
            USBH_ClosePipe(host, transport.pipe);
            USBH_FreePipe(host, transport.pipe);
            transport.opened = transport.pending = 0;
            ++g_gamepad.errors;
            g_gamepad.usb_ready = g_gamepad.state_valid = 0;
            GP_ResetState(&g_gamepad.state);
            return USBH_FAIL;
        }
    }
    if (!transport.pending && !transport.stalled &&
        (uint32_t)(host->Timer - transport.submitted) >= g_gamepad.interval) {
        /* Do not resubmit an in-flight URB or reset the data toggle on normal polling. */
        if (USBH_InterruptReceiveData(host, transport.rx, (uint8_t)g_gamepad.packet_size,
                                      transport.pipe) == USBH_OK) {
            transport.pending = 1;
            transport.submitted = host->Timer;
        }
    }
    return USBH_OK;
}
