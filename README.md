# F4HOST — USB 无线接收器手柄主机

使用 STM32F401RC（Cortex-M4）和 USB OTG FS Host 接入手柄接收器。当前目标设备为北通 KP50，首版已实现标准 HID 字段解析、厂商接口原始数据采集和串口诊断。

**当前状态：首版编译、链接和软件测试已通过；尚未验证 KP50 实机，不能认定其协议或完整兼容性。**

## 开始使用

- [文档目录与阅读顺序](docs/README.md)
- [上板调试与接线](docs/04-bringup.md)
- [Keil 工程](Msp/MDK-ARM/F4HOST.uvprojx)
- [手柄模块](Lib/host_gamepad/README.md)

诊断串口：USART2，PA2 TX，115200 8N1。首次采集请保存从复位、插入接收器到逐个按键动作的完整日志。

## 目录

| 目录 | 用途 |
| --- | --- |
| `Msp/` | CubeMX 配置、STM32 HAL、USB Host 中间件和 Keil 工程 |
| `Lib/host_gamepad/` | 手柄 Host 类、HID 解析器和诊断输出 |
| `docs/` | 协议基础、设备资料、实现说明、调试及验证文档 |
| `tests/` | 解析器测试与模拟 USB Host 测试 |
| `tools/` | 构建和验证脚本 |
| `.build/` | 本机构建产物；未纳入版本控制，可用脚本重新生成 |
