# 手柄与 USB 协议基础

[返回文档目录](README.md)

本页介绍通用知识，不能用来推断某一代 KP50 的实际包格式。该设备的已确认信息见[设备资料](02-kp50.md)。

## 1. 无线接收器的连接关系

```mermaid
flowchart LR
    Pad[手柄] <-->|厂商无线链路| Receiver[USB 接收器]
    Receiver <-->|USB| Host[STM32 USB Host]
    Host --> Decode[报告解析]
    Decode --> App[应用层状态]
```

使用专用接收器时，STM32 面对的是接收器暴露的 USB 设备。无线配对通常由手柄与接收器负责；是否还需要主机发初始化命令，必须按接收器协议确认。

要区分三件事：USB 枚举成功、收到输入包、手柄无线在线。接收器即使没有配对也可能完成枚举；手柄离线后接收器也可能继续发送状态包。

## 2. 常见协议类型

| 类型 | 通常如何处理 | 注意点 |
| --- | --- | --- |
| 标准 USB HID 手柄 | 获取 Report Descriptor，再按其定义提取输入字段 | 不存在适用于所有手柄的固定字节布局 |
| Xbox 类 USB 协议 | 按具体代际、接口和报文格式适配 | 可能是厂商类，可能需要初始化；不能直接交给 HID 解析器 |
| PlayStation 类设备 | USB 下通常基于 HID；扩展功能单独适配 | 输入、灯光、震动等可能有多种 Report ID |
| Switch 类设备 | 按对应输入报告及初始化流程适配 | 模式和连接方式会影响报文 |
| 通用厂商接收器 | 先读取接口、端点和原始数据 | `class=FF` 只表示厂商类，不等于已识别 Xbox 协议 |

DInput 和 XInput 是 PC 端输入 API 概念。手柄菜单上的 D/X 模式可能影响 USB 枚举结果，但 MCU 开发必须以实际描述符及报文为依据。

## 3. 从枚举到输入

| 对象 | 需要关注的内容 | 在工程中的用途 |
| --- | --- | --- |
| Device Descriptor | VID、PID、设备版本等 | 识别设备及后续匹配专用适配 |
| Configuration Descriptor | 接口、端点、总长度 | 确认复合设备结构及容量是否足够 |
| Interface Descriptor | 接口号、类、子类、协议、Alternate Setting | 选择驱动，设置控制请求的 `wIndex` |
| Endpoint Descriptor | IN/OUT、传输类型、MPS、`bInterval` | 建立输入管道并确定接收长度与轮询周期 |
| HID Descriptor | Report Descriptor 类型和长度 | 请求报告描述符 |
| HID Report Descriptor | Usage、Report ID、位宽、数量、范围等 | 建立字段映射 |
| Input Report | 每次收到的实际输入数据 | 更新按钮、轴和方向帽 |

接口数组下标与 `bInterfaceNumber` 不一定相等。端点数组第 0 项也不一定是 IN 端点。`wMaxPacketSize` 是端点包容量，不能当作每次实际收到的报告长度。

HID Report Descriptor 与 Input Report 是两种不同数据：前者描述布局，后者携带值。第一版通过 EP0 获取描述符，再从 Interrupt IN 接收输入。

## 4. HID 字段解析要点

- **Usage Page / Usage**：说明控件语义。Generic Desktop 页为 `0x01`，Button 页为 `0x09`；Joystick 和 Game Pad Application Usage 分别为 `0x04`、`0x05`。
- **Report Size / Count**：定义每个字段占多少位、连续多少个字段。字段可以跨字节，也可能有常量填充位。
- **Logical Minimum / Maximum**：决定取值范围，并影响符号扩展。不能将所有轴都当作无符号 8 位。
- **Report ID**：使用编号时，输入包首字节是报告编号。每种 Input Report ID 独立累计位偏移；Output 和 Feature 的位数不应计入 Input 布局。
- **Input 标志**：需要区分常量/数据、Variable/Array、绝对/相对。当前实现只映射支持范围内的绝对 Variable 控件。
- **Hat Switch**：常见为 4 或 8 个方向，并有中立值。中立值可能位于逻辑范围之外。

下面只是教学示例，**不是 KP50 报文**：

| 无 Report ID 的示例布局 | 位范围 |
| --- | --- |
| 12 个按钮 | bit 0–11 |
| 4 位方向帽 | bit 12–15 |
| 8 位 X 轴 | bit 16–23 |
| 8 位 Y 轴 | bit 24–31 |

例如 `01 80 81 7F`：若描述符规定方向帽有效值 0–7、8 为中立，轴范围为 −127–127，则表示按钮 1 按下、方向帽中立、X=−127、Y=127。缺少描述符时，不能从这四个字节得出这些含义。

## 5. 原始控件到应用语义

HID Button 1 不一定是 A；Z 不一定是左扳机；Rx/Ry 不一定就是右摇杆。因此首版保留 Usage 和原始逻辑值，等实机逐个动作确认后再建立物理映射。

后续应用层可增加按钮命名、摇杆方向统一、中点校准、死区、归一化和扳机范围转换。它们都依赖具体映射，当前代码尚未实现。震动和灯光属于输出路径，也不属于首版输入采集范围。

## 6. 时序与异常

USB 主机主动轮询 IN 端点。本工程使用 FS Host，根据选中端点的 `bInterval` 调度；串口每 100 ms 的采样输出是另一个周期，不代表 USB 只按 10 Hz 收包。

NAK 通常表示暂时没有数据，不应当成设备断开。STALL 表示端点停止，需要按 USB 流程恢复。无线失联则要结合接收器协议判断，不能直接用 NAK、静止时无包或 USB 枚举成功替代。

后续查规范时，重点查阅 USB-IF 的《Device Class Definition for Human Interface Devices (HID)》及《HID Usage Tables》。本页为工程知识整理，未新增在线规范版本核验。
