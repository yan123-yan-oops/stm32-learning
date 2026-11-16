# L06 · FreeRTOS 移植与三任务

> 日期：2026-11-16 ｜ 工程：`05-freertos` ｜ 目标岗：北云/威胜（RTOS 是共同硬要求）

## ① 本节要学到什么（这 8 条要能一字一句讲出来）

### 1. 任务是什么

> **一个任务 = 独立的栈 + 任务控制块（TCB）+ 一个优先级。**

栈存局部变量和函数调用现场；TCB 存任务状态、栈顶指针、优先级、事件列表项。
**每个任务有自己的栈**，这是它和"普通函数"最本质的区别。

### 2. 调度器与就绪列表

> **调度器只做一件事：从"就绪列表"里挑优先级最高的任务，把 CPU 给它。**

`configUSE_PREEMPTION=1` 是抢占式：高优先级任务一就绪，立刻打断低优先级任务。
同优先级之间按时间片轮转（`configUSE_TIME_SLICING=1`）。

### 3. 上下文切换发生在 PendSV

> **任务切换不是"函数调用"，而是在 PendSV 异常里保存/恢复寄存器。**

Cortex-M 用两个栈指针：主栈 MSP（中断用）、进程栈 PSP（任务用）。
`port.c` 的 `xPortPendSVHandler` 把 R4–R11 压到当前任务栈，再切到下一个任务的栈顶。
**为什么用 PendSV**：它优先级最低，可以等所有中断处理完再切换，不会打断中断。

### 4. tick 与延时

> **SysTick 每 1 ms 产生一次 tick，`vTaskDelay(500)` 是"相对延时 500 个 tick"。**

`configTICK_RATE_HZ=1000` → 1 tick = 1 ms。
`vTaskDelay` 是**相对**延时（从调用时刻算），`vTaskDelayUntil` 是**绝对**延时（周期不漂移）。
延时期间任务进阻塞态，CPU 给别人——这就是它和 `for` 空循环的根本区别。

### 5. 队列

> **队列是"值拷贝"，不是传指针；队列空时读任务阻塞，队列满时写任务阻塞（可设超时）。**

`xQueueSend(q, &value, timeout)` 把 4 字节**复制**进队列；`xQueueReceive` 复制出来。
所以发送方可以立刻改自己的变量，不影响队列里的值。
中断里必须用 `xQueueSendFromISR`，并且要在退出前调用 `portYIELD_FROM_ISR()`。

### 6. 中断优先级上限（最容易死机的一条）

> **优先级数值 ≤ `configMAX_SYSCALL_INTERRUPT_PRIORITY` 的中断，不能调用任何 FreeRTOS API。**

STM32F4 的 NVIC 优先级：**数值越小优先级越高**。`configMAX_SYSCALL_INTERRUPT_PRIORITY = 5<<4 = 0x50`，
意味着优先级 0–4 的中断**不能**调 FreeRTOS 函数；5–15 才可以。
违反这条，内核临界区会被打断，表现为随机死机。

### 7. 堆：heap_4

> **heap_4 = 首次适配 + 相邻空闲块合并，和我们 L02 写的内存池是同一套思路。**

`configTOTAL_HEAP_SIZE` 决定堆大小；`xTaskCreate`/`xQueueCreate` 都从这里分配。
用完记得 `vTaskDelete`（如果任务会结束），否则栈和 TCB 不回收。

### 8. 启动流程

> **main → xTaskCreate（任务还没跑）→ vTaskStartScheduler → SVC 异常 → 第一个任务开始执行。**

`xTaskCreate` 只是把任务挂到就绪列表；真正开跑是在 `vTaskStartScheduler()` 里，
它配置 SysTick、启动第一个任务（通过 SVC 异常进入）。调度器启动后，main 的 `for(;;)` 永远不会执行。

## ② 怎么学（今晚 60 分钟）

| 时间 | 做什么 |
| --- | --- |
| 10 min | 读上面 8 条，每条用自己的话复述一遍（不许看原文） |
| 15 min | 看 `src/freertos/portable/GCC/ARM_CM4F/port.c` 里的 `xPortPendSVHandler`（汇编，只认 r4–r11 / psp） |
| 15 min | 打开 `05-freertos`，Build → Upload，观察灯 |
| 10 min | 用 Debug 暂停，Watch 三个变量（见下） |
| 10 min | 做自测 6 题 |

## ③ 动手

### 工程结构

```
05-freertos/
├── platformio.ini          编译选项 + 浮点 ABI 修正
├── extra_float_abi.py      链接阶段补 -mfloat-abi=hard（PIO 的坑）
└── src/
    ├── FreeRTOSConfig.h    内核裁剪（8 条概念大多在这里）
    ├── main.c              三任务 + 队列
    └── freertos/           内核源码（tasks/queue/list/timers/port/heap_4）
```

### 现象

- LED 每 500 ms 翻转（LED 任务）
- 大约每 5 秒出现一次**快速双闪**（消费任务收到 5 个数据）

### 用调试器看内核

PlatformIO → **Debug** → 暂停，在 Watch 里加：

| 变量 | 说明 |
| --- | --- |
| `g_led_ticks` | LED 任务跑了多少次 |
| `g_produced` | 生产了多少个 |
| `g_consumed` | 消费了多少个 |
| `uxTaskGetStackHighWaterMark(NULL)` | 当前任务栈的**历史最小剩余**（word） |

如果 `g_produced` 比 `g_consumed` 大很多，说明队列在丢数据（生产快于消费）。

### 两个真实踩过的坑

1. **`INCLUDE_vTaskDelay` 默认是 0** → 不打开就链接报 `undefined reference to vTaskDelay`。
2. **浮点 ABI 混用** → 框架是硬浮点编译的，链接时必须也加 `-mfloat-abi=hard`，否则报
   `uses VFP register arguments, firmware.elf does not`。PlatformIO 的 `build_flags` 只管编译，所以用了 `extra_scripts` 补链接参数。

## ④ 自测 6 题（答完才算过）

1. 一个任务由哪三部分组成？为什么每个任务都要自己的栈？
2. 任务切换发生在哪个异常里？为什么不用普通中断？
3. `vTaskDelay(100)` 和 `for(i=0;i<100000;i++)` 的本质区别是什么？
4. 队列存的是指针还是值？如果发送方发完立刻改了自己的变量，队列里的值会变吗？
5. 优先级 3 的中断里能不能调 `xQueueSend`？为什么？
6. `xTaskCreate` 之后任务立刻开始跑吗？真正开始跑是在哪一步？

<details><summary>参考答案</summary>

1. 栈、TCB、优先级；每个任务独立调用链，共享栈会互相覆盖。
2. PendSV；它优先级最低，不会打断其他中断，切换更安全。
3. 前者主动让出 CPU 并进入阻塞态，后者占着 CPU 空转。
4. 值拷贝；不会变。
5. 不能。优先级数值 3 高于宏定义的门槛（5），属于"不受内核管理"的中断，调用会被打断临界区。
6. 不会；`vTaskStartScheduler()` 之后，通过 SVC 异常进入第一个任务。

</details>

### 用 PlatformIO Debug 看变量（一步不落）

**准备**

1. 接线：DAP `3V3→3V3`、`SWDIO→DIO`、`SWCLK→CLK`、`GND→GND`；**不要插 Type-C**。
2. VS Code「文件 → 打开文件夹」选 `05-freertos` 这一层（不是 `STM32` 根目录）。
3. 等左下角出现 PlatformIO 状态栏（✓ 编译、⌁ 调试图标）。

**启动调试**

4. 按 **F5**；或点左侧「运行和调试」图标（Ctrl+Shift+D），顶部下拉选 `PlatformIO Debug (f407zg)`。
5. 第一次会先编译、再烧录，然后停在 `main()` 第一行（PIO 默认在 main 下断点）。

**加 Watch**

6. 左侧「运行和调试」面板 → **WATCH** → 点 `+` → 输入变量名 → 回车。
7. 依次加：`g_led_ticks`、`g_produced`、`g_consumed`、`g_stack_led`、`g_stack_prod`、`g_stack_cons`。
8. 按 **F5 继续**跑 5 秒，再按暂停（F6）。看数值：
   - `g_led_ticks` ≈ 运行秒数 × 2；
   - `g_produced` ≥ `g_consumed`；
   - `g_stack_*` 是"历史最小剩余栈"，单位 word（×4 = 字节）。

**看不到变量时**

- 这三个变量是 `static`，GDB 里换个写法：`'main.c'::g_led_ticks`；
- 确认是 Debug 启动（PIO 会自动用 `-Og -g`），不是普通 Upload；
- 若被优化掉，在 `platformio.ini` 加 `build_type = debug` 再试。

**看任务栈的另一种方式**

在 WATCH 里直接输入 `uxTaskGetStackHighWaterMark(NULL)` 回车，GDB 会在目标上执行一次函数返回当前值。
注意它返回的是"当前被暂停的那个任务"的水位；想知道别的任务，需要先切任务上下文（GDB 的 FreeRTOS 感知属于进阶话题）。

## ⑦ 你要能背下来的"移植清单"（面试直接问）

1. **内核源码**：`tasks.c`、`queue.c`、`list.c`、`port.c`（ARM_CM4F）、`heap_4.c`
2. **头文件路径**：`Source/include` + `portable/GCC/ARM_CM4F`
3. **`FreeRTOSConfig.h`**：tick 频率、最大优先级、堆大小、`configMAX_SYSCALL_INTERRUPT_PRIORITY`
4. **异常向量映射**：`vPortSVCHandler→SVC_Handler`、`xPortPendSVHandler→PendSV_Handler`、`xPortSysTickHandler→SysTick_Handler`
5. **中断优先级**：数值 ≤ 5 的中断不能调内核 API
6. **编译/链接选项一致**：`-mfpu=fpv4-sp-d16 -mfloat-abi=hard` 必须同时出现在编译和链接里
7. **启动顺序**：`xTaskCreate` → `vTaskStartScheduler` → SVC → 第一个任务

这七条就是"你移植过 FreeRTOS"的完整证据链。

## 和 JD 的对应

做完这课，简历技能段可以加一行：

```
· 系统：裸机 + FreeRTOS（任务/队列/信号量，完成采集-解析-输出三任务改造，栈水位实测）
```

对应北云"VxWorks 移植"（原理相通）、威胜"FreeRTOS 优先"。
