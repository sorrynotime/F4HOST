/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file            : usb_host.c
 * @version         : v1.0_Cube
 * @brief           : This file implements the USB Host
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/

#include "usb_host.h"
#include "usbh_core.h"
#include "usbh_hid.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/

/* USER CODE END PV */

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/

/* USER CODE END PFP */

/* USB Host core handle declaration */
USBH_HandleTypeDef hUsbHostFS;
ApplicationTypeDef Appli_state = APPLICATION_IDLE;

/*
 * -- Insert your variables declaration here --
 */
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*
 * user callback declaration
 */
static void USBH_UserProcess(USBH_HandleTypeDef *phost, uint8_t id);

/*
 * -- Insert your external function declaration here --
 */
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/**
 * Init USB host library, add supported class and start the library
 * @retval None
 */
void MX_USB_HOST_Init(void)
{
    /* USER CODE BEGIN USB_HOST_Init_PreTreatment */

    /* USER CODE END USB_HOST_Init_PreTreatment */

    /* Init host Library, add supported class and start the library. */
    if (USBH_Init(&hUsbHostFS, USBH_UserProcess, HOST_FS) != USBH_OK)
    {
        Error_Handler();
    }
    if (USBH_RegisterClass(&hUsbHostFS, USBH_HID_CLASS) != USBH_OK)
    {
        Error_Handler();
    }
    if (USBH_Start(&hUsbHostFS) != USBH_OK)
    {
        Error_Handler();
    }
    /* USER CODE BEGIN USB_HOST_Init_PostTreatment */

    /* USER CODE END USB_HOST_Init_PostTreatment */
}

/*
 * Background task
 */
void MX_USB_HOST_Process(void)
{
    /* USB Host Background task */
    USBH_Process(&hUsbHostFS);
}
/*
 * user callback definition
 */
static void USBH_UserProcess(USBH_HandleTypeDef *phost, uint8_t id)
{
    /* USER CODE BEGIN CALL_BACK_1 */
    switch (id)
    {
    case HOST_USER_SELECT_CONFIGURATION:
        break;

    case HOST_USER_DISCONNECTION:
        Appli_state = APPLICATION_DISCONNECT;
        break;

    case HOST_USER_CLASS_ACTIVE:
        Appli_state = APPLICATION_READY;
        break;

    case HOST_USER_CONNECTION:
        Appli_state = APPLICATION_START;
        break;

    default:
        break;
    }
    /* USER CODE END CALL_BACK_1 */
}

/**
 * @}
 */

#include "usbh_hid_mouse.h"
#include "usbh_ioreq.h"

/**
 * 读取鼠标数据（在主循环中调用）
 * @param  phost       USB Host 句柄指针（如 &hUsbHostFS）
 * @param  mouse_data  输出参数，存储解析后的鼠标数据
 * @retval 0: 成功获取新数据
 *         1: 无新数据（设备未连接/未枚举/无数据到达）
 *        -1: 错误状态
 */
int8_t USBH_Mouse_ReadData(USBH_HandleTypeDef *phost, Mouse_Data_t *mouse_data)
{
    HID_HandleTypeDef *hid;
    static uint8_t last_button_left = 0;
    static uint8_t last_button_right = 0;
    static uint8_t last_button_middle = 0;
    static uint8_t last_button_side1 = 0;
    static uint8_t last_button_side2 = 0;
    static int16_t last_x = 0;
    static int16_t last_y = 0;
    static int8_t last_wheel = 0;

    uint8_t report_len;

    uint8_t tmp_button_left, tmp_button_right, tmp_button_middle, tmp_button_side1, tmp_button_side2;
    int16_t x, y;
    int8_t wheel;
    uint8_t has_new_data = 0;

    // static uint8_t report_buffer[8];

    if (phost == NULL || mouse_data == NULL)
    {
        return -1;
    }

    /* 1. 维护USB状态机（必须定期调用） */
    // USBH_Process(phost);

    /* 2. 检查设备是否已枚举并进入HID类状态 */
    if (phost->gState != HOST_CLASS || phost->pActiveClass == NULL)
    {
        mouse_data->is_valid = 0;
        return 1;
    }

    /* 3. 获取HID类驱动私有数据 */
    hid = (HID_HandleTypeDef *)phost->pActiveClass->pData;
    if (hid == NULL || hid->pData == NULL)
    {
        mouse_data->is_valid = 0;
        return 1;
    }

    /* 4. 检查HID报告长度（标准鼠标至少3字节） */
    report_len = hid->length;
    if (report_len < 3)
    {
        mouse_data->is_valid = 0;
        return 1;
    }

    /* 5. 解析鼠标数据（根据标准鼠标报告格式） */
    tmp_button_left = (hid->pData[1] & 0x01) ? 1 : 0;
    tmp_button_right = (hid->pData[1] & 0x02) ? 1 : 0;
    tmp_button_middle = (hid->pData[1] & 0x04) ? 1 : 0;
    tmp_button_side1 = (hid->pData[1] & 0x08) ? 1 : 0;
    tmp_button_side2 = (hid->pData[1] & 0x10) ? 1 : 0;

    x = (int8_t)hid->pData[2] || ((int8_t)(hid->pData[3]) << 8); // 处理X轴高4位（如果有）
    y = (int8_t)hid->pData[4] || ((int8_t)(hid->pData[5]) << 8); // 处理Y轴高4位（如果有）
    wheel = (report_len >= 6) ? (int8_t)hid->pData[6] : 0;

    /* 6. 检测数据变化（位移非零 或 按钮状态变化） */
    if ((x != 0) ||
        (y != 0) ||
        (wheel != 0) ||
        (tmp_button_left != last_button_left) ||
        (tmp_button_right != last_button_right) ||
        (tmp_button_middle != last_button_middle) ||
        (tmp_button_side1 != last_button_side1) ||
        (tmp_button_side2 != last_button_side2))
    {
        has_new_data = 1;
    }

    /* 7. 更新上次数据缓存（用于下次比较） */

    last_button_left = tmp_button_left;
    last_button_right = tmp_button_right;
    last_button_middle = tmp_button_middle;
    last_button_side1 = tmp_button_side1;
    last_button_side2 = tmp_button_side2;
    last_x = x;
    last_y = y;
    last_wheel = wheel;

    /* 8. 填充输出结构体 */
    mouse_data->button_left = tmp_button_left;
    mouse_data->button_right = tmp_button_right;
    mouse_data->button_middle = tmp_button_middle;
    mouse_data->button_side1 = tmp_button_side1;
    mouse_data->button_side2 = tmp_button_side2;
    mouse_data->x = x;
    mouse_data->y = y;
    mouse_data->wheel = wheel;
    mouse_data->is_valid = has_new_data;

    return (has_new_data) ? 0 : 1;
}

void USBH_UserLoop(void)
{
    static Mouse_Data_t lz_mouse_data;

    if (Appli_state == APPLICATION_READY)
    {
        if (USBH_Mouse_ReadData(&hUsbHostFS, &lz_mouse_data) == 0)
        {
        }
    }
}
/**
 * @}
 */
