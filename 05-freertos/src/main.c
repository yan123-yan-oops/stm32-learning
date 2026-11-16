/**
 * 05-freertos —— 最小移植 + 三任务 + 队列
 *
 * 结构：
 *   LED 任务    ：每 500 ms 翻转 PC13
 *   生产任务    ：每 1 s 把递增计数塞进队列
 *   消费任务    ：阻塞等队列，每收到 5 个做一次"快速双闪"
 *
 * 现象：
 *   默认：LED 慢闪（0.5 s）
 *   每 5 秒左右：出现一次快速双闪（说明队列在传数据）
 *
 * 看内核状态：用 PlatformIO Debug 暂停，Watch 这几个变量：
 *   g_led_ticks / g_produced / g_consumed
 *   uxTaskGetStackHighWaterMark(NULL)  ← 各任务栈剩余最小值
 */

#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* ------------------------------------------------------------------ */
/* 硬件                                                                */
/* ------------------------------------------------------------------ */
static void led_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    GPIOC->MODER   &= ~(3u << (13u * 2u));
    GPIOC->MODER   |=  (1u << (13u * 2u));
    GPIOC->OTYPER  &= ~(1u << 13u);
    GPIOC->OSPEEDR &= ~(3u << (13u * 2u));
    GPIOC->PUPDR   &= ~(3u << (13u * 2u));
    GPIOC->BSRR = (1u << 13u);                 /* 灭 */
}

static void led_on(void)  { GPIOC->BSRR = (1u << (13u + 16u)); }
static void led_off(void) { GPIOC->BSRR = (1u << 13u); }
static void led_toggle(void) { GPIOC->ODR ^= (1u << 13u); }

static void board_clock_168m(void)
{
    RCC->CFGR &= ~RCC_CFGR_SW;
    while ((RCC->CFGR & RCC_CFGR_SWS) != 0u) {
    }
    RCC->CR &= ~RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) != 0u) {
    }

    RCC->CR |= RCC_CR_HSEON;
    uint32_t timeout = 0x50000u;
    while ((RCC->CR & RCC_CR_HSERDY) == 0u && --timeout != 0u) {
    }
    if ((RCC->CR & RCC_CR_HSERDY) == 0u) {
        return;                                 /* 退回 HSI，灯会快闪报警 */
    }

    FLASH->ACR = FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN
               | FLASH_ACR_LATENCY_5WS;

    RCC->PLLCFGR = (8u   << RCC_PLLCFGR_PLLM_Pos)
                 | (336u << RCC_PLLCFGR_PLLN_Pos)
                 | (0u   << RCC_PLLCFGR_PLLP_Pos)
                 | RCC_PLLCFGR_PLLSRC_HSE
                 | (7u   << RCC_PLLCFGR_PLLQ_Pos);

    RCC->CR |= RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) == 0u) {
    }

    RCC->CFGR |= RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV4 | RCC_CFGR_PPRE2_DIV2;
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {
    }

    SystemCoreClockUpdate();
}

/* ------------------------------------------------------------------ */
/* 任务间共享                                                          */
/* ------------------------------------------------------------------ */
static QueueHandle_t g_queue;

/* volatile：让调试器/优化器都当它是"会被别人改的量" */
static volatile uint32_t g_led_ticks   = 0;
static volatile uint32_t g_produced    = 0;
static volatile uint32_t g_consumed    = 0;

/* ------------------------------------------------------------------ */
/* 任务 1：LED 慢闪                                                    */
/* ------------------------------------------------------------------ */
static void vLedTask(void *param)
{
    (void)param;
    for (;;) {
        led_toggle();
        g_led_ticks++;
        vTaskDelay(pdMS_TO_TICKS(500));         /* 500 ms，期间 CPU 让给别人 */
    }
}

/* ------------------------------------------------------------------ */
/* 任务 2：生产数据（每 1 s 发一个数）                                  */
/* ------------------------------------------------------------------ */
static void vProducerTask(void *param)
{
    (void)param;
    uint32_t value = 0;

    for (;;) {
        value++;
        g_produced = value;

        /* 队列满时最多等 100 ms，超时就丢弃——不阻塞生产节奏 */
        (void)xQueueSend(g_queue, &value, pdMS_TO_TICKS(100));

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ------------------------------------------------------------------ */
/* 任务 3：消费（阻塞等队列）                                          */
/* ------------------------------------------------------------------ */
static void vConsumerTask(void *param)
{
    (void)param;
    uint32_t value = 0;

    for (;;) {
        /* portMAX_DELAY：队列空就一直睡，不占 CPU */
        if (xQueueReceive(g_queue, &value, portMAX_DELAY) == pdPASS) {
            g_consumed++;
            if ((g_consumed % 5u) == 0u) {      /* 每 5 个做一次快速双闪 */
                led_on();  vTaskDelay(pdMS_TO_TICKS(80));
                led_off(); vTaskDelay(pdMS_TO_TICKS(80));
                led_on();  vTaskDelay(pdMS_TO_TICKS(80));
                led_off();
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* 钩子：出错时停在这里，方便调试器抓现场                               */
/* ------------------------------------------------------------------ */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    led_on();
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}

void vApplicationMallocFailedHook(void)
{
    led_on();
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}

void vAssertCalled(const char *file, int line)
{
    (void)file;
    (void)line;
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */
int main(void)
{
    led_init();
    board_clock_168m();

    g_queue = xQueueCreate(4, sizeof(uint32_t));
    if (g_queue == NULL) {
        for (;;) { led_on(); for (volatile int i = 0; i < 2000000; i++) {} led_off(); }
    }

    /* 参数：函数 / 名字 / 栈(word) / 参数 / 优先级 / 句柄 */
    xTaskCreate(vLedTask,      "LED",  128, NULL, 2, NULL);
    xTaskCreate(vProducerTask, "PROD", 256, NULL, 3, NULL);
    xTaskCreate(vConsumerTask, "CONS", 256, NULL, 2, NULL);

    vTaskStartScheduler();      /* 正常不会返回 */

    for (;;) {                  /* 走到这说明堆不够，创建任务失败 */
        led_on();
        for (volatile int i = 0; i < 1000000; i++) {}
        led_off();
        for (volatile int i = 0; i < 1000000; i++) {}
    }
}
