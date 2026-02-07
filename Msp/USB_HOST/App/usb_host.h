/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : usb_host.h
 * @version        : v1.0_Cube
 * @brief          : Header for usb_host.c file.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USB_HOST__H__
#define __USB_HOST__H__

#ifdef __cplusplus
extern "C"
{
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx.h"
#include "stm32f4xx_hal.h"

    /* USER CODE BEGIN INCLUDE */

    /* USER CODE END INCLUDE */

    /** @addtogroup USBH_OTG_DRIVER
     * @{
     */

    /** @defgroup USBH_HOST USBH_HOST
     * @brief Host file for Usb otg low level driver.
     * @{
     */

    /** @defgroup USBH_HOST_Exported_Variables USBH_HOST_Exported_Variables
     * @brief Public variables.
     * @{
     */

    /**
     * @}
     */

    /** Status of the application. */
    typedef enum
    {
        APPLICATION_IDLE = 0,
        APPLICATION_START,
        APPLICATION_READY,
        APPLICATION_DISCONNECT
    } ApplicationTypeDef;

    typedef struct
    {
        uint8_t button_left;   // 左键
        uint8_t button_right;  // 右键
        uint8_t button_middle; // 中键
        uint8_t button_side1;  // 侧键1
        uint8_t button_side2;  // 侧键2

        int16_t x;        // X轴位移（有符号）
        int16_t y;        // Y轴位移（有符号）
        int8_t wheel;     // 滚轮（部分鼠标无此字段）
        uint8_t is_valid; // 数据是否有效
    } Mouse_Data_t;

    /** @defgroup USBH_HOST_Exported_FunctionsPrototype USBH_HOST_Exported_FunctionsPrototype
     * @brief Declaration of public functions for Usb host.
     * @{
     */

    /* Exported functions -------------------------------------------------------*/

    /** @brief USB Host initialization function. */
    void MX_USB_HOST_Init(void);

    void MX_USB_HOST_Process(void);

    /**
     * @}
     */
    void USBH_UserLoop(void);
    /**
     * @}
     */

    /**
     * @}
     */

#ifdef __cplusplus
}
#endif

#endif /* __USB_HOST__H__ */
