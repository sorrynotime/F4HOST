#ifndef TEST_USBH_CORE_H
#define TEST_USBH_CORE_H
#include <stdint.h>
#include <stddef.h>
#define USBH_MAX_NUM_INTERFACES 8U
#define USBH_MAX_NUM_ENDPOINTS 4U
#define USBH_MAX_SIZE_CONFIGURATION 512U
#define USB_EP_TYPE_INTR 3U
#define USB_REQ_GET_DESCRIPTOR 6U
#define HOST_USER_CLASS_ACTIVE 2U
typedef enum { USBH_OK, USBH_BUSY, USBH_FAIL, USBH_NOT_SUPPORTED } USBH_StatusTypeDef;
typedef enum { USBH_URB_IDLE, USBH_URB_DONE, USBH_URB_NOTREADY, USBH_URB_ERROR, USBH_URB_STALL } USBH_URBStateTypeDef;
typedef enum { CMD_SEND, CMD_WAIT } CMD_StateTypeDef;
typedef struct { uint8_t bEndpointAddress, bmAttributes, bInterval; uint16_t wMaxPacketSize; } USBH_EpDescTypeDef;
typedef struct {
    uint8_t bLength, bAlternateSetting, bInterfaceClass, bInterfaceSubClass;
    uint8_t bInterfaceProtocol, bInterfaceNumber, bNumEndpoints;
    USBH_EpDescTypeDef Ep_Desc[USBH_MAX_NUM_ENDPOINTS];
} USBH_InterfaceDescTypeDef;
struct _USBH_HandleTypeDef;
typedef struct {
    const char *Name;
    uint8_t ClassCode;
    USBH_StatusTypeDef (*Init)(struct _USBH_HandleTypeDef *);
    USBH_StatusTypeDef (*DeInit)(struct _USBH_HandleTypeDef *);
    USBH_StatusTypeDef (*Requests)(struct _USBH_HandleTypeDef *);
    USBH_StatusTypeDef (*BgndProcess)(struct _USBH_HandleTypeDef *);
    USBH_StatusTypeDef (*SOFProcess)(struct _USBH_HandleTypeDef *);
    void *pData;
} USBH_ClassTypeDef;
typedef struct _USBH_HandleTypeDef {
    USBH_ClassTypeDef *pActiveClass;
    CMD_StateTypeDef RequestState;
    uint32_t Timer;
    struct {
        uint8_t CfgDesc_Raw[USBH_MAX_SIZE_CONFIGURATION], address, speed;
        struct { uint16_t idVendor, idProduct; } DevDesc;
        struct { uint16_t wTotalLength; USBH_InterfaceDescTypeDef Itf_Desc[USBH_MAX_NUM_INTERFACES]; } CfgDesc;
    } device;
    struct {
        uint8_t pipe_in;
        struct { struct {
            uint8_t bmRequestType, bRequest;
            struct { uint16_t w; } wValue, wIndex, wLength;
        } b; } setup;
    } Control;
    void (*pUser)(struct _USBH_HandleTypeDef *, uint8_t);
} USBH_HandleTypeDef;
USBH_StatusTypeDef USBH_SelectInterface(USBH_HandleTypeDef *, uint8_t);
uint8_t USBH_AllocPipe(USBH_HandleTypeDef *, uint8_t);
USBH_StatusTypeDef USBH_OpenPipe(USBH_HandleTypeDef *, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint16_t);
USBH_StatusTypeDef USBH_FreePipe(USBH_HandleTypeDef *, uint8_t);
USBH_StatusTypeDef USBH_ClosePipe(USBH_HandleTypeDef *, uint8_t);
USBH_StatusTypeDef USBH_LL_SetToggle(USBH_HandleTypeDef *, uint8_t, uint8_t);
uint32_t USBH_LL_GetLastXferSize(USBH_HandleTypeDef *, uint8_t);
USBH_URBStateTypeDef USBH_LL_GetURBState(USBH_HandleTypeDef *, uint8_t);
uint32_t HAL_GetTick(void);
#endif
