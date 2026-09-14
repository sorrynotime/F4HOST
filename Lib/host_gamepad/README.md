# USB 接收器手柄首版

目标：STM32F401RCT6 的 OTG FS Host 连接北通 KP50 无线接收器，先获得可核对的描述符、原始输入和标准 HID 字段。没有预设 KP50 的 VID/PID 或字节布局，也没有经过 KP50 实机验证。

## 已查证的资料

- [北通官方产品说明书入口](https://community.betop-cn.com/#/)：列有鲲鹏50初代、第二代及联名款，版本不能混为一谈。
- 官网链接的[鲲鹏50初代电脑连接教程](https://www.yuque.com/fujuan-us0ly/ixqxx4/bzl7zbyo4h7m1644)：接收器插入 USB 后，打开手柄，长按 HOME 进入菜单，选择「模式切换 → PC」。页面中的 Xbox wireless controller 是蓝牙连接说明，不能拿来证明 USB 接收器的协议。

这些资料没有给出 USB 报告描述符、VID/PID、输入包定义或接收器初始化命令。本版因此通过设备枚举结果选择路径，不发送猜测的模式切换、震动或厂商命令。

## 文件与行为

- `gamepad_hid.c/.h`：独立于 STM32 的有界 HID 解析器。识别 Joystick/Game Pad Application Collection，处理 Report ID、Input 位偏移、常量填充、Usage 列表/范围、Global Push/Pop、有符号字段、4/8 方向帽及中立值。
- `host_gamepad.c/.h`：独立 Host 类，优先非 Boot HID，其次 class=FF 的厂商接口。只读取一个接口的一个 Interrupt IN 端点，使用其实际 MPS 和 bInterval。控制请求的 wIndex 使用 bInterfaceNumber，收包使用实际传输长度。
- `gamepad_app.c`：USART2 非阻塞诊断，Keil Watch 可同时观察 `g_gamepad`。
- `usb_host.c` 已注册新类，`main.c` 已调用新诊断入口。旧 `host_mouse.c` 保留但从本次 Keil 编译中排除，旧键鼠 HID 类没有注册。
- USB Core 的类选择改为遍历已注册类与已解析接口，不再只看接口 0；配置容量为 8 个接口、每接口 4 个端点、2 个注册类。

标准 HID 解析受限或失败时保留 RAW，`descriptor_status=-1`；没有识别到手柄字段或厂商接口为 0；成功建表为 1。不会用一套固定偏移强行解释未知设备。

## 上板操作

1. 使用 `Msp/MDK-ARM/F4HOST.uvprojx` 编译下载，或使用本次生成的 `.build/armclang/F4HOST.hex`。固件地址为 `0x08000000`。本次没有自动烧录。
2. 板子 PA2（USART2 TX）接 USB 转串口 RX，共地，115200、8 数据位、无校验、1 停止位。诊断直接使用 UART2 TXE/DR，因此 UART2 不能再同时用于其他 HAL 发送、DMA 或日志。
3. 保持工程既有 USB 接线：PA11 D−、PA12 D+；核对主机口 VBUS 有 5 V，PC9 的开关控制与板上电路一致。
4. 先打开串口记录，再上电/复位、插入接收器。初代按官方教程选择 PC 模式；二代按自己的说明书操作。
5. 等 CFG/HID/FIELD 输出完成。依次记录静止、每个按键单独按下、十字键四个方向、左右摇杆各轴两端、左右扳机。每个动作保持约 1 秒再松开，便于 100 ms 的日志采样捕获。
6. 拔掉接收器，再插回，确认 USB 状态变化、序号清零、旧按键清空。另测手柄关机但接收器不拔的情况，记录是否继续来包。

日志含义：

| 前缀 | 含义 |
| --- | --- |
| USB | Host 状态、VID/PID、配置长度；ABORT 表示未进入输入接收 |
| CFG | 配置描述符十六进制，带字节偏移 |
| PAD | 选中接口的 class/subclass/protocol、IN 地址、MPS、轮询周期、映射状态 |
| HID | HID Report Descriptor 十六进制，带字节偏移 |
| FIELD | Report ID、Usage Page/Usage、位偏移、宽度、逻辑范围 |
| RAW | 最新完整输入包、实际长度和序号；日志每 100 ms 最多采样一次 |
| AX | 同一采样序号的原始轴值，顺序为 X/Y/Z/Rx/Ry/Rz/Slider/Dial/Wheel |
| STATUS | 每秒显示 USB 就绪、解析有效、收包计数、解析计数、距最后一包的时间、错误数 |

`g_gamepad.state.buttons` 的 bit0..31 对应 HID Button Usage 1..32，尚未对应 A/B/X/Y。`axes_valid` 的 bit0..8 标记轴是否收到过值，轴的 min/max 见 FIELD。`hat=-1` 为中立/未知，0 为上，顺时针到 7。

`usb_ready` 只表示接收器 USB 链路已启用，`state_valid` 只表示解析状态可用，**两者都不是无线配对/手柄在线状态**。设备可能静止时不发包，也可能手柄断线后仍发接收器心跳，不能凭超时或有包就判定无线连接。上层控制还需要经实机确认的无线状态和失联策略。

## 首版范围

- 固定容量：输入包最多 64 字节、报告描述符 1024 字节、配置描述符 512 字节、最多 8 个 Report ID、64 个映射字段、32 个按钮、9 个轴、1 个方向帽。
- 只映射绝对值 Variable 输入。Array、相对字段、Long Item、Usage Delimiter、重复同名控件和超限布局会退回 RAW，避免产生错误控制值。短包不会部分更新状态；出错后清空解析状态。
- 复合设备默认选第一个符合条件的非 Boot HID 接口。它可能只是厂商配置接口，尚不自动遍历所有报告描述符寻找最佳手柄接口。查看 CFG 后，可在 `host_gamepad.h` 将 `GAMEPAD_INTERFACE_NUMBER` 改成实际接口号；该限制同时作用于 HID 和厂商类。
- class=FF 只支持被动 Interrupt IN 采集，未实现 Xbox/GIP/厂商握手。若需要初始化命令才上报，本版可能只有描述符而没有 RAW。这时需要接收器 VID/PID 和 PC 侧 USB 抓包继续适配。
- 不支持 USB Hub、多手柄同时接入、蓝牙直连、震动和灯光输出。
- 正常 NAK 按 bInterval 重试，STALL 使用 CLEAR_FEATURE 后恢复 DATA0。清除失败或 IN 传输持续 1 秒未完成时停止该端点、清空控制状态，通过 STATUS 报告，重新插拔恢复。

后续适配请保留开机到连接的全部日志，并注明 KP50 初代/二代、当前模式和每段 RAW 对应的动作。若 `PAD class=FF` 或 `map=0/-1`，先看接口与原始包，不急着写死偏移。

## 编译与测试

```powershell
python tools/build_armclang.py
python tools/check_gamepad.py --cc "<本机 clang.exe 路径>" --sanitize
```

`build_armclang.py` 使用已安装的 Arm Compiler 6，读取项目源文件、Include/Define 和 Flash/RAM 范围，采用 C99/O1、Cortex-M4 硬浮点，结合原启动文件完成编译、链接及 HEX 输出。它不调用 uVision，也不声称复现全部 IDE 专属选项；输出在 `.build/armclang/`。

`check_gamepad.py` 用 Arm GCC 编译项目 C 文件，再用本机 Clang 运行解析器与模拟 Host 测试。`--sanitize` 启用 AddressSanitizer/UBSan，覆盖字段边界、畸形描述符、单字节变异、Report ID、实际长度、NAK/STALL、接口号、重复提交保护及资源回收。模拟测试不能代替 USB 电气、无线配对及实机时序验证。

CubeMX 再生成时应检查 `usb_host.c` 中的自定义类注册、`usbh_core.c` 的类选择补丁、Keil 的 Gamepad 文件组/包含路径。`.ioc` 已同步容量参数，但 CubeMX 不会自动生成自定义类和 Core 补丁。
