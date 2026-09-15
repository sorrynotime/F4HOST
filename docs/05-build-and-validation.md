# 编译、验证与工程维护

[返回文档目录](README.md)

以下命令均在仓库根目录执行。

## 构建方式

```powershell
python tools/build_armclang.py
python tools/check_gamepad.py --cc "<本机 clang.exe 路径>" --sanitize
```

`build_armclang.py` 使用已安装的 Arm Compiler 6，读取项目源文件、Include/Define 和 Flash/RAM 范围，采用 C99/O1、Cortex-M4 硬浮点，结合原启动文件完成编译、链接及 HEX 输出。它不调用 uVision，也不声称复现全部 IDE 专属选项；输出在 `.build/armclang/`。

`check_gamepad.py` 用 Arm GCC 编译项目 C 文件，再用本机 Clang 运行解析器与模拟 Host 测试。`--sanitize` 启用 AddressSanitizer/UBSan，覆盖字段边界、畸形描述符、单字节变异、Report ID、实际长度、NAK/STALL、接口号、重复提交保护及资源回收。模拟测试不能代替 USB 电气、无线配对及实机时序验证。

CubeMX 再生成时应检查 `usb_host.c` 中的自定义类注册、`usbh_core.c` 的类选择补丁、Keil 的 Gamepad 文件组/包含路径。`.ioc` 已同步容量参数，但 CubeMX 不会自动生成自定义类和 Core 补丁。

## 工具与输出

| 路径/工具 | 说明 |
| --- | --- |
| [Keil 工程](../Msp/MDK-ARM/F4HOST.uvprojx) | STM32F401RC，Flash 256 KiB、RAM 64 KiB |
| [Arm Compiler 构建脚本](../tools/build_armclang.py) | Python 3；默认工具目录为 `C:/Keil_v5/ARM/ARMCLANG/bin`，可用 `--bin` 指定 |
| [检查脚本](../tools/check_gamepad.py) | `arm-none-eabi-gcc` 需在 PATH 中；本机 Clang 可用 `--cc` 指定 |
| `.build/armclang/F4HOST.axf` | 链接后的调试镜像 |
| `.build/armclang/F4HOST.hex` | Intel HEX 固件，Flash 起始地址 `0x08000000` |
| `.build/armclang/F4HOST.map` | 内存布局和占用记录 |
| `.build/gamepad/` | Arm GCC 目标文件与本机测试程序 |

构建产物不纳入版本控制。使用另一台电脑或切换代码后先重新构建，不把本地残留的 HEX 视为当前源码产物。两个脚本都不执行烧录。

## 已完成的验证记录

以下是 2026-09-14 首版代码实现时的验证结果。本轮仅整理文档，未重新运行固件测试。

| 验证 | 结果与范围 |
| --- | --- |
| Arm GCC 14.2 | 40 个项目 C 源文件 Cortex-M4 编译通过；新手柄模块按警告即错误检查 |
| Arm Compiler 6.24 | 编译、原启动文件汇编、链接和 HEX 生成通过 |
| 解析器测试 | 按钮、符号位、跨字节轴、方向帽、多 Report ID、短包、异常描述符及单字节变异通过 |
| Host 模拟测试 | 接口号与下标差异、IN 端点选择、实际长度、NAK/STALL、重复提交保护、Pipe 0 回收、厂商模式、未完成传输超时通过 |
| AddressSanitizer/UBSan | 上述本机测试启用检查后通过 |
| 内存占用 | 此次 O1 脚本构建：ROM 22,988 字节，RW+ZI 12,936 字节；含链接器计入的静态栈/堆区域，非实测峰值 |
| 遗留警告 | Arm GCC 的部分 HAL 未使用参数；Arm Compiler 的旧汇编器弃用提示及既有 HID parser 未使用变量 |
| KP50 上板 | 未进行，USB 电气、配对、真实报文和时序仍待验证 |

测试入口：[解析器测试](../tests/test_gamepad_hid.c)、[Host 模拟测试](../tests/test_gamepad_host.c)。模拟桩位于 `tests/stubs/`，模拟测试不构成对整个 STM32 HAL 或整个 USB Core 的完整覆盖。

## 修改后的检查范围

- 修改纯文档：核对源码术语、文件链接和事实状态即可，无需重新运行固件测试。
- 修改解析器：运行解析器测试，保留错误输入和短包的覆盖；如有真实报告样本，增加对应回归用例。
- 修改 Host/状态处理：运行 Host 模拟测试，重新编译链接；涉及真实 USB 时序的修改仍需上板确认。
- 修改 CubeMX/Keil 配置：检查类注册、入口、包含路径、源文件是否参与编译和内存范围；脚本与 IDE 的构建行为都应核对。

目前的 `GP_MAX_PACKET`、接口容量等是工程上限，不是已经从 KP50 测得的参数。调整容量时需同时核对缓冲区、描述符解析和测试边界。
