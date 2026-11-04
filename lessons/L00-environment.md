# L00 · 环境与烧录：从零到"灯闪"

> 日期：2026-11-01 ｜ 30 分钟 ｜ 硬件：核心板 + DAPLink + 4 根杜邦线

## ① 本节要学到什么

1. 说清 **SWD 四根线**各干什么，以及为什么 `3V3` 不能省（VTREF）；
2. 知道 **板卡 ID** `vccgnd_f407zg_mini` 和 `upload_protocol = cmsis-dap` 的关系；
3. 看懂烧录日志里的**三行成功标志**，而不是靠"灯亮没亮"猜；
4. 遇到 `examination failed` / `cannot read IDR` 时，能**按表定位**，不瞎试。

## ② 怎么学（30 分钟）

| 时间 | 做什么 | 关键点 |
| --- | --- | --- |
| 5 min | 读下面的接线表 | 记住 `3V3` 是参考电压，不只是供电 |
| 10 min | 接线，打开设备管理器确认 **CMSIS-DAP** 出现 | 有黄色感叹号 → 装 `【5】下载器驱动` |
| 10 min | 打开 `00-pio-blink` → Build → Upload | 第一次会下载工具链，等它装完 |
| 5 min | 拔插一次，重复 Upload | 确认不是"碰巧成功" |

卡住 20 分钟就停，把日志最后 10 行发我。

## ③ 动手

### 接线（实测有效）

```
DAPLink 3V3   →  板子 3V3      ← VTREF + 供电，接了这个就别插 Type-C
DAPLink SWDIO →  板子 DIO      (PA13)
DAPLink SWCLK →  板子 CLK      (PA14)
DAPLink GND   →  板子 GND
```

### 验收标准

日志里必须出现这三行（顺序固定）：

```
** Programming Finished **
** Verify Started **
** Verified OK **
========================= [SUCCESS] =========================
```

同时 PC13 绿灯开始闪。

### 实验：复现一次失败

把 `3V3` 拔掉，再 Upload 一次：

- 预期报错：`Failed to read memory at 0xE000ED00` → `examination failed`
- 结论：**DP 能连上不代表能读内核**，VTREF 缺失就是这个症状。
- 接回去，确认恢复。

## ④ 自测三题

1. SWD 最少需要几根线？分别是哪几根？
2. 为什么只接 DIO/CLK/GND 会失败？
3. `cannot read IDR` 和 `examination failed` 分别先怀疑什么？

<details><summary>参考答案</summary>

1. 三根即可通信（SWDIO、SWCLK、GND），但**供电/VTREF** 要另外解决，实际接四根。
2. 缺 VTREF，DAP 不知道目标电平，SWD 信号不可靠。
3. `cannot read IDR` → 先怀疑 DIO/CLK 接反；`examination failed` → 先怀疑 VTREF/供电。

</details>

## 踩坑记录

| 现象 | 原因 | 处理 |
| --- | --- | --- |
| `Error connecting DP: cannot read IDR` | DIO/CLK 接反 | 对调两根线 |
| `Failed to read memory at 0xE000ED00` | 缺 VTREF（没接 3V3） | 接上 3V3，拔掉 Type-C |
| 按住复位不放还是失败 | 内核被摁在复位里 | 那是"按住"不是"connect under reset" |
| 第一次 Upload 卡住 | 在下载 OpenOCD（约 100 MB） | 等，别重复点 |
