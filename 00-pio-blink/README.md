# 00-pio-blink — 第一个 PlatformIO 工程

板子：鹿小班 STM32F407ZGT6 核心板 · LED = PC13（低电平亮）· 调试器 DAPLink

## 一次性准备

1. VS Code 装扩展 **PlatformIO IDE**（第一次启动会自动下载 Python 与工具链，约几分钟）。
2. 板子 **Type-C 接电脑**供电（电源指示灯亮）。
3. DAPLink 接 `SWCLK / SWDIO / GND` 三根到板子的 DIO/CLK/GND，不要接 3V3。

## 打开与烧录

- VS Code → 文件 → **打开文件夹** → 选 `00-pio-blink` 这一层（不要选 `E:\code\STM32`，PIO 找不到 `platformio.ini`）。
- 首次构建：点底部工具栏 **✓ Build**，会下载工具链。
- 烧录：点 **→ Upload**（或 `Ctrl+Alt+U`）。
- 成功标志：板载 PC13 的 LED 开始闪（约 0.35 秒一次）。

## 命令行等价操作（可选）

```powershell
pio run                 # 编译
pio run -t upload       # 编译 + 烧录
pio device monitor      # 串口监视（USB CDC 或外接 USB-TTL 时用）
```

## 烧不进去时按这个顺序查

| 现象 | 排查 |
| --- | --- |
| `Error: No debug probe found` | DAP 没被识别；换 USB 口，或装 `E:\code\STM32\【5】下载器驱动` |
| `Target not responding` | 板子没供电（Type-C 没插）；或 SWCLK/SWDIO 接反 |
| 一直连不上，之前烧过程序 | 程序可能把 PA13/PA14 复用掉了：按住复位再点 Upload |
| 编译报错找不到 `stm32f4xx.h` | 选错文件夹了，或 `framework = stm32cube` 没生效 |
| 灯不闪但烧录成功 | PC13 是低电平亮；确认 delay 没被优化（`volatile`） |

## 下一步

灯闪之后，把 `main.c` 里的 `delay_loops(20000000)` 改成 5000000 / 50000000，用手机秒表对一下时间——
你会发现"空循环延时"完全不靠谱。这就是下一课 SysTick 定时器的动机。
