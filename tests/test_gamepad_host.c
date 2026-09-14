#include "host_gamepad.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t transfers, actual, closed, freed, toggles, controls, events;
static uint8_t *rx;
static USBH_URBStateTypeDef urb;
static USBH_StatusTypeDef clear_status;
static const uint8_t descriptor[] = {
    0x05,1, 0x09,5, 0xA1,1, 0x09,0x30, 0x15,0, 0x26,0xFF,0,
    0x75,8, 0x95,1, 0x81,2, 0xC0
};
USBH_StatusTypeDef USBH_SelectInterface(USBH_HandleTypeDef *h, uint8_t i) { (void)h; assert(i == 1); return USBH_OK; }
uint8_t USBH_AllocPipe(USBH_HandleTypeDef *h, uint8_t ep) { (void)h; assert(ep == 0x82); return 0; }
USBH_StatusTypeDef USBH_OpenPipe(USBH_HandleTypeDef *h, uint8_t p, uint8_t ep, uint8_t a, uint8_t s, uint8_t t, uint16_t m)
{ (void)h; (void)a; (void)s; assert(p == 0 && ep == 0x82 && t == 3 && m == 64); return USBH_OK; }
USBH_StatusTypeDef USBH_FreePipe(USBH_HandleTypeDef *h, uint8_t p) { (void)h; assert(p == 0); ++freed; return USBH_OK; }
USBH_StatusTypeDef USBH_ClosePipe(USBH_HandleTypeDef *h, uint8_t p) { (void)h; assert(p == 0); ++closed; return USBH_OK; }
USBH_StatusTypeDef USBH_LL_SetToggle(USBH_HandleTypeDef *h, uint8_t p, uint8_t t)
{ (void)h; assert(p == 0 && t == 0); ++toggles; return USBH_OK; }
uint32_t USBH_LL_GetLastXferSize(USBH_HandleTypeDef *h, uint8_t p) { (void)h; return p == 7 ? sizeof(descriptor) : actual; }
USBH_URBStateTypeDef USBH_LL_GetURBState(USBH_HandleTypeDef *h, uint8_t p) { (void)h; (void)p; return urb; }
uint32_t HAL_GetTick(void) { return 123; }
USBH_StatusTypeDef USBH_CtlReq(USBH_HandleTypeDef *h, uint8_t *b, uint16_t n)
{
    assert(h->Control.setup.b.bmRequestType == 0x81 && h->Control.setup.b.bRequest == 6);
    assert(h->Control.setup.b.wValue.w == 0x2200 && h->Control.setup.b.wIndex.w == 3);
    assert(n == sizeof(descriptor));
    ++controls; memcpy(b, descriptor, n); return USBH_OK;
}
USBH_StatusTypeDef USBH_ClrFeature(USBH_HandleTypeDef *h, uint8_t ep)
{ (void)h; assert(ep == 0x82); return clear_status; }
USBH_StatusTypeDef USBH_InterruptReceiveData(USBH_HandleTypeDef *h, uint8_t *b, uint8_t n, uint8_t p)
{ (void)h; assert(n == 64 && p == 0); ++transfers; rx = b; urb = USBH_URB_IDLE; return USBH_OK; }
static void event(USBH_HandleTypeDef *h, uint8_t id) { (void)h; assert(id == HOST_USER_CLASS_ACTIVE); ++events; }

static void setup(USBH_HandleTypeDef *h, USBH_ClassTypeDef *cls)
{
    USBH_InterfaceDescTypeDef *itf;
    uint8_t cfg[] = {
        9,2,27,0,1,1,0,0x80,50,
        9,4,3,0,1,3,0,0,0,
        9,0x21,0x11,1,0,1,0x22,sizeof(descriptor),0
    };
    memset(h, 0, sizeof(*h)); h->pActiveClass = cls; h->pUser = event;
    h->Control.pipe_in = 7; h->device.CfgDesc.wTotalLength = sizeof(cfg);
    memcpy(h->device.CfgDesc_Raw, cfg, sizeof(cfg));
    itf = &h->device.CfgDesc.Itf_Desc[1];
    itf->bLength = 9; itf->bInterfaceClass = cls->ClassCode; itf->bInterfaceNumber = 3;
    itf->bNumEndpoints = 2;
    itf->Ep_Desc[0].bEndpointAddress = 1; /* OUT is first: must not drive IN size/interval. */
    itf->Ep_Desc[0].wMaxPacketSize = 8; itf->Ep_Desc[0].bmAttributes = 3;
    itf->Ep_Desc[1].bEndpointAddress = 0x82; itf->Ep_Desc[1].wMaxPacketSize = 64;
    itf->Ep_Desc[1].bInterval = 4; itf->Ep_Desc[1].bmAttributes = 3;
}

int main(void)
{
    USBH_HandleTypeDef host;
    setup(&host, &GamepadHID_Class);
    assert(GamepadHID_Class.Init(&host) == USBH_OK);
    assert(GamepadHID_Class.Requests(&host) == USBH_OK && controls == 1 && events == 1);
    assert(g_gamepad.descriptor_status == 1 && g_gamepad.interval == 4);
    assert(GamepadHID_Class.BgndProcess(&host) == USBH_OK && transfers == 1);
    host.Timer = 8; GamepadHID_Class.BgndProcess(&host);
    assert(transfers == 1); /* Never overwrite an in-flight transfer. */
    rx[0] = 200; actual = 1; urb = USBH_URB_DONE;
    GamepadHID_Class.BgndProcess(&host);
    assert(g_gamepad.sequence == 1 && g_gamepad.raw_length == 1 && g_gamepad.state.axes[0] == 200);
    assert(g_gamepad.raw[1] == 0 && g_gamepad.state_valid && transfers == 2);
    GamepadHID_Class.BgndProcess(&host); assert(g_gamepad.sequence == 1);
    urb = USBH_URB_NOTREADY; host.Timer = 9;
    GamepadHID_Class.BgndProcess(&host); assert(transfers == 2 && g_gamepad.errors == 0);
    host.Timer = 12; GamepadHID_Class.BgndProcess(&host); assert(transfers == 3);
    urb = USBH_URB_STALL; GamepadHID_Class.BgndProcess(&host);
    assert(!g_gamepad.state_valid && g_gamepad.state.axes[0] == 0);
    clear_status = USBH_OK; GamepadHID_Class.BgndProcess(&host); assert(toggles == 2);
    host.Timer = 16; GamepadHID_Class.BgndProcess(&host); assert(transfers == 4);
    urb = USBH_URB_STALL; GamepadHID_Class.BgndProcess(&host);
    clear_status = USBH_FAIL; assert(GamepadHID_Class.BgndProcess(&host) == USBH_FAIL);
    assert(!g_gamepad.usb_ready);
    assert(GamepadHID_Class.BgndProcess(&host) == USBH_FAIL);
    GamepadHID_Class.DeInit(&host);
    assert(closed == 1 && freed == 1 && !host.pActiveClass->pData);
    assert(!g_gamepad.sequence && g_gamepad.state.hat == -1);
    setup(&host, &GamepadRaw_Class);
    assert(GamepadRaw_Class.Init(&host) == USBH_OK);
    assert(GamepadRaw_Class.Requests(&host) == USBH_OK && controls == 1);
    assert(g_gamepad.descriptor_status == 0 && g_gamepad.usb_ready);
    GamepadRaw_Class.BgndProcess(&host);
    host.Timer = 1001;
    assert(GamepadRaw_Class.BgndProcess(&host) == USBH_FAIL);
    assert(!g_gamepad.usb_ready && g_gamepad.errors == 1);
    GamepadRaw_Class.DeInit(&host);
    setup(&host, &GamepadHID_Class);
    host.device.CfgDesc.Itf_Desc[1].Ep_Desc[1].wMaxPacketSize = 65;
    assert(GamepadHID_Class.Init(&host) == USBH_FAIL && !g_gamepad.usb_ready);
    assert(closed == 2 && freed == 2);
    puts("gamepad Host: interface index, IN endpoint, actual lengths, NAK, STALL, no duplicate URB, pipe-0 cleanup and vendor raw mode passed");
    return 0;
}
