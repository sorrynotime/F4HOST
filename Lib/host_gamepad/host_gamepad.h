#ifndef HOST_GAMEPAD_H
#define HOST_GAMEPAD_H

#include "usbh_core.h"
#include "gamepad_hid.h"

/* -1: first non-Boot HID / vendor interface with interrupt IN.
 * Set an actual bInterfaceNumber to inspect another interface of a composite receiver.
 */
#ifndef GAMEPAD_INTERFACE_NUMBER
#define GAMEPAD_INTERFACE_NUMBER (-1)
#endif
#define GAMEPAD_DESCRIPTOR_SIZE 1024U

typedef struct {
    uint16_t vid, pid, packet_size, descriptor_length;
    uint8_t interface_number, interface_class, subclass, protocol, in_ep, interval;
    uint8_t usb_ready; /* Receiver USB link only; NOT proof of radio pairing. */
    int8_t descriptor_status; /* 1 mapped, 0 unmapped/vendor, -1 invalid/unavailable. */
    uint8_t state_valid;
    uint32_t sequence, decoded_sequence, last_report_ms, errors;
    uint16_t raw_length;
    uint8_t raw[GP_MAX_PACKET];
    uint8_t descriptor[GAMEPAD_DESCRIPTOR_SIZE];
    GP_Layout layout;
    GP_State state;
} GamepadInfo;

extern USBH_ClassTypeDef GamepadHID_Class;
extern USBH_ClassTypeDef GamepadRaw_Class;
extern GamepadInfo g_gamepad; /* Main-loop owned; convenient Keil Watch entry. */
void Gamepad_Reset(void);
void Gamepad_AppProcess(USBH_HandleTypeDef *host);

#endif
