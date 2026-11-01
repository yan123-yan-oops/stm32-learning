# L00 · 环境与烧录：PlatformIO + DAPLink

> 日期：2026-11-01 ｜ 目标：从零到「灯闪」，并且知道失败时怎么查。

## 今天解决了什么

- 用 **PlatformIO（VS Code）** 替代 Keil，工具链自动下载，无授权问题；
- 板卡 ID：`vccgnd_f407zg_mini`（同款 F407ZGT6，官方支持 `cmsis-dap`）；
- 第一次烧录成功：`Programming Finished → Verified OK → SUCCESS`。

## 关键概念

| 概念 | 说明 |
| --- | --- |
| SWD | 两线调试：`SWDIO`(PA13) + `SWCLK`(PA14)，加上 GND 就能通信 |
| VTREF | 调试器用目标板电压做电平参考；**不接就时好时坏** |
| CMSIS-DAP | DAPLink 使用的调试协议，OpenOCD 通过它访问芯片 |
| OpenOCD | PlatformIO 调用的烧录/调试后端 |
| `upload_protocol` | 决定用哪种探针，本项目是 `cmsis-dap` |

## 接线（实测有效）

```
DAPLink 3V3  →  板子 3V3     ← 既是 VTREF，也是唯一电源
DAPLink SWDIO→  板子 DIO     (PA13)
DAPLink SWCLK→  板子 CLK     (PA14)
DAPLink GND  →  板子 GND
Type-C：不插（两路电源会冲突）
```

## 踩坑记录（都是真机踩出来的）

| 现象 | 原因 | 结论 |
| --- | --- | --- |
| `Failed to read memory at 0xE000ED00` → `examination failed` | 只接了 DIO/CLK/GND，DAP 缺 VTREF | **必须接 3V3** |
| `Error connecting DP: cannot read IDR` | DIO/CLK 接反 | 这条报错基本可判定线接反 |
| 按住复位不放，还是 `examination failed` | 内核一直被摁在复位里 | 「按住复位」要配合时序，不是按着不放 |
| 第一次 Upload 卡在下载 | 在装 OpenOCD（约 100 MB） | 等它装完，别重复点 |

## 自测三题

1. 为什么 DAP 只接三根线（DIO/CLK/GND）会失败？
2. 看到 `cannot read IDR` 和 `examination failed`，分别先查什么？
3. 为什么接 DAP 3V3 之后不能再插 Type-C？

<details><summary>参考答案</summary>

1. 缺 VTREF，调试器不知道目标电平，SWD 信号不可靠。
2. `cannot read IDR` → 先怀疑 DIO/CLK 接反；`examination failed` → 先怀疑 VTREF/供电。
3. 板子 5V 与 3V3 两路供电同时存在会倒灌，原理图明确要求二选一。

</details>
