/**
 * FreeRTOSConfig.h —— 内核裁剪与钩子配置
 *
 * 这个文件决定了内核"有多大、跑多快、怎么报错"。
 * 每一行都有含义，L06 笔记里逐条解释。
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* ------------------------------ 调度 ------------------------------ */
#define configUSE_PREEMPTION                    1   /* 抢占式调度：高优先级就绪立刻切换 */
#define configUSE_TIME_SLICING                  1   /* 同优先级按时间片轮转 */
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configUSE_TICKLESS_IDLE                 0

/* ------------------------------ 时基 ------------------------------ */
#define configCPU_CLOCK_HZ                      ( 168000000UL )
#define configTICK_RATE_HZ                      ( 1000 )   /* 1 tick = 1 ms */
#define configMAX_PRIORITIES                    ( 7 )
#define configMINIMAL_STACK_SIZE                ( 128 )    /* 单位：word（4 字节） */
#define configMAX_TASK_NAME_LEN                 ( 16 )
#define configUSE_16_BIT_TICKS                  0

/* ------------------------------ 内存 ------------------------------ */
#define configTOTAL_HEAP_SIZE                   ( 16 * 1024 )  /* heap_4 管理的总堆 */
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configSUPPORT_STATIC_ALLOCATION         0

/* ------------------------------ 通信 ------------------------------ */
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configQUEUE_REGISTRY_SIZE               8
#define configUSE_QUEUE_SETS                    0

/* ------------------------------ 调试 ------------------------------ */
#define configCHECK_FOR_STACK_OVERFLOW          2   /* 2 = 检查栈尾魔数 + 越界 */
#define configUSE_MALLOC_FAILED_HOOK            1
#define configUSE_TRACE_FACILITY                1
#define configUSE_STATS_FORMATTING_FUNCTIONS    1
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0

/* --------------------------- 中断优先级 ---------------------------- */
/* STM32F4：4 位优先级（0-15），数值越大优先级越低 */
#define configPRIO_BITS                         4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY        15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY   5

#define configKERNEL_INTERRUPT_PRIORITY \
    ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )

/* 内核异常处理函数 → CMSIS 启动文件里的向量名 */
#define vPortSVCHandler                         SVC_Handler
#define xPortPendSVHandler                      PendSV_Handler
#define xPortSysTickHandler                     SysTick_Handler

/* ------------------------------ 断言 ------------------------------ */
extern void vAssertCalled(const char *file, int line);
#define configASSERT(x) \
    do { if ((x) == 0) { vAssertCalled(__FILE__, __LINE__); } } while (0)

/* --------------------------- 可选 API ----------------------------- */
/* 默认是 0，不打开的话 vTaskDelay 等函数不会被编译出来（链接报 undefined） */
#define INCLUDE_vTaskDelay                    1
#define INCLUDE_vTaskDelayUntil               1
#define INCLUDE_vTaskDelete                   1
#define INCLUDE_vTaskSuspend                  1
#define INCLUDE_xTaskGetSchedulerState        1
#define INCLUDE_uxTaskGetStackHighWaterMark   1
#define INCLUDE_xTaskGetCurrentTaskHandle     1

#endif /* FREERTOS_CONFIG_H */
