#ifndef HOST_MOUSE_H
#define HOST_MOUSE_H

#ifdef __cplusplus
extern "C"
{
#endif

// Add your includes here
#include <stdint.h>

    // Add your macros here

    // Add your type definitions here
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
    } t_mouse_data;
    // Add your function declarations here
    void USBH_UserLoop(void);

#ifdef __cplusplus
}
#endif

#endif // HOST_MOUSE_H