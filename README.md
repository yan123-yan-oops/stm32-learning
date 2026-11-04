# STM32F407ZGT6 学习笔记与代码

鹿小班 STM32F407ZGT6 核心板 · PlatformIO + DAPLink · 边做边学的记录。

## 硬件

| 项 | 值 |
| --- | --- |
| MCU | STM32F407ZGT6，168 MHz，1 MB Flash / 192 KB RAM（128 KB SRAM + 64 KB CCM） |
| LED | PC13，**低电平点亮**（备份域，3 mA / 2 MHz） |
| 按键 | PA15（KEY_PA15）、PE6（KEY_PE6），10K 上拉，按下为低 |
| 调试 | DAPLink（CMSIS-DAP），SWDIO=PA13、SWCLK=PA14 |
| 串口 | **无 USB 转串口芯片**；Type-C 直连 PA11/PA12，可做 USB CDC |
| 供电 | 5V（Type-C）或 3V3（DAP）**二选一** |

## 目录

```
├── lessons/              课时笔记（复习看这里）
│   ├── README.md         索引：L00–L05 + 待上课程
│   ├── TEACHING-FORMAT.md  每节课的固定四段格式
│   ├── L00-environment.md      环境、SWD/VTREF、烧录排错
│   ├── L01-gpio-blink.md       GPIO 四步、BSRR 原子性
│   ├── L02-clock-systick.md    时钟树、PLL、SysTick
│   ├── L03-key-fsm.md          按键消抖、四状态机
│   ├── L04-uart-loopback.md    UART 回环、命令解析
│   └── L05-ringbuffer.md       环形缓冲（已并入 L04 代码）
├── homework/             作业与批改（每题一份）
├── 00-pio-blink/         L00/L01：点灯
├── 01-key-led/           L03 的早期版本：阻塞式消抖
├── 02-systick/           L02：168 MHz + 1 ms 基准
├── 03-key-fsm/           L03：定时采样 + 状态机
└── 04-uart-loopback/     L04：UART 自发自收
```

**约定**：工程目录编号 = 课程顺序；每个工程里的 `README.md` 都指向对应课时笔记。

## 环境

1. VS Code 安装 **PlatformIO IDE** 扩展；
2. 打开某个工程目录（含 `platformio.ini` 的那一层），不要打开仓库根目录；
3. 点 `Build` / `Upload`。

DAPLink 接线（实测有效）：

```
3V3  → 3V3      （VTREF + 供电，接了这个就别插 Type-C）
SWDIO→ DIO      （PA13）
SWCLK→ CLK      （PA14）
GND  → GND
```

## 三个数字（L02）

| 版本 | 闪烁周期 | 说明 |
| --- | --- | --- |
| 空循环延时 | 21.7 s | 实际跑在 16 MHz HSI |
| SysTick 初版 | 3.17 s | `HSE_VALUE` 默认 25 MHz，算错主频 |
| 修正后 | 1.013 s | 168 MHz + 1 ms 基准 |

## 说明

厂商资料（参考例程、手册、原理图、封装）体积大且版权归原厂，未纳入本仓库，本地路径见 `lessons/L00-environment.md`。
