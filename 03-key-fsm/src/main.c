/**
 * 03-key-fsm —— 按键状态机（L03）
 *
 * 昨天的 01-key-led 用 delay_loops 做消抖，问题是：
 *   - 延时长度靠猜，主频一变就失效；
 *   - 消抖期间 CPU 干等，什么都做不了；
 *   - 长按、连发这类逻辑写不下去。
 *
 * 今天改成「定时采样 + 状态机」：
 *   - SysTick 提供 1 ms 时基；
 *   - 每 20 ms 采一次按键电平；
 *   - 用四个状态判断「抖动 / 真正按下 / 松开 / 长按」；
 *   - 主循环永远不阻塞，LED 还能同时闪。
 *
 * 硬件：LED = PC13（低电平亮）
 *       按键 = PA15、PE6（10K 上拉，按下为低）
 *
 * 模式循环：短按 0(灭) → 1(常亮) → 2(慢闪) → 0 …
 *           长按 1 秒 → 直接回 0(灭)
 */

#include "stm32f4xx.h"

/* ------------------------------------------------------------------ */
/* 时基                                                                */
/* ------------------------------------------------------------------ */
static volatile uint32_t g_ms = 0;

void SysTick_Handler(void)
{
    g_ms++;
}

static void delay_ms(uint32_t ms)
{
    uint32_t start = g_ms;
    while ((g_ms - start) < ms) {
    }
}

/* ------------------------------------------------------------------ */
/* 时钟：HSE 8 MHz → 168 MHz                                            */
/* ------------------------------------------------------------------ */
static int clock_168m(void)
{
    RCC->CFGR &= ~RCC_CFGR_SW;                 /* 先回 HSI */
    while ((RCC->CFGR & RCC_CFGR_SWS) != 0u) {
    }
    RCC->CR &= ~RCC_CR_PLLON;                  /* PLLCFGR 写保护，必须先关 */
    while ((RCC->CR & RCC_CR_PLLRDY) != 0u) {
    }

    RCC->CR |= RCC_CR_HSEON;
    uint32_t timeout = 0x50000u;
    while ((RCC->CR & RCC_CR_HSERDY) == 0u && --timeout != 0u) {
    }
    if ((RCC->CR & RCC_CR_HSERDY) == 0u) {
        return 0;
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
    return 1;
}

/* ------------------------------------------------------------------ */
/* LED                                                                 */
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

static void led_on(void)  { GPIOC->BSRR = (1u << (13u + 16u)); }  /* 低电平亮 */
static void led_off(void) { GPIOC->BSRR = (1u << 13u); }

/* ------------------------------------------------------------------ */
/* 按键：状态机                                                        */
/* ------------------------------------------------------------------ */
typedef enum {
    KEY_IDLE = 0,          /* 松开、稳定 */
    KEY_DEBOUNCE_DOWN,     /* 刚检测到按下，等待确认 */
    KEY_PRESSED,           /* 确认按下，计时判断长短按 */
    KEY_DEBOUNCE_UP        /* 刚检测到松开，等待确认 */
} key_state_t;

typedef enum {
    KEY_EVT_NONE = 0,
    KEY_EVT_SHORT,         /* 按下后 1 秒内松开 */
    KEY_EVT_LONG           /* 按住超过 1 秒（只触发一次） */
} key_event_t;

#define KEY_LONG_MS   1000u
#define KEY_SCAN_MS     20u

typedef struct {
    volatile uint32_t *idr;   /* GPIO 输入寄存器 */
    uint32_t           mask;  /* 位掩码 */
    key_state_t        state;
    uint32_t           press_ms;
    uint8_t            long_sent;
} key_t;

static key_t g_key_pa15;
static key_t g_key_pe6;

static void key_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOEEN;

    /* PA15：输入 + 上拉（注意：复位后是 JTDI，MODER 设成输入即当 GPIO 用） */
    GPIOA->MODER &= ~(3u << (15u * 2u));
    GPIOA->PUPDR &= ~(3u << (15u * 2u));
    GPIOA->PUPDR |=  (1u << (15u * 2u));

    /* PE6：输入 + 上拉 */
    GPIOE->MODER &= ~(3u << (6u * 2u));
    GPIOE->PUPDR &= ~(3u << (6u * 2u));
    GPIOE->PUPDR |=  (1u << (6u * 2u));

    g_key_pa15.idr = &GPIOA->IDR;  g_key_pa15.mask = (1u << 15u);
    g_key_pe6.idr  = &GPIOE->IDR;  g_key_pe6.mask  = (1u << 6u);
}

static int key_is_down(const key_t *k)
{
    return (*(k->idr) & k->mask) == 0u;   /* 上拉输入：按下 = 低 */
}

/**
 * 每 20 ms 调用一次。返回本周期产生的事件（同一周期最多一个）。
 */
static key_event_t key_scan(key_t *k, uint32_t now_ms)
{
    int down = key_is_down(k);

    switch (k->state) {
        case KEY_IDLE:
            if (down) {
                k->state = KEY_DEBOUNCE_DOWN;
                k->press_ms = now_ms;
                k->long_sent = 0;
            }
            break;

        case KEY_DEBOUNCE_DOWN:
            if (!down) {
                k->state = KEY_IDLE;            /* 抖动，忽略 */
            } else {
                k->state = KEY_PRESSED;         /* 稳定 20 ms，确认按下 */
            }
            break;

        case KEY_PRESSED:
            if (!down) {
                k->state = KEY_DEBOUNCE_UP;
            } else if (!k->long_sent && (now_ms - k->press_ms) >= KEY_LONG_MS) {
                k->long_sent = 1;
                return KEY_EVT_LONG;
            }
            break;

        case KEY_DEBOUNCE_UP:
            if (down) {
                k->state = KEY_PRESSED;         /* 松开时抖动，回到按下 */
            } else {
                k->state = KEY_IDLE;
                if (!k->long_sent) {
                    return KEY_EVT_SHORT;       /* 短按：松开时判定 */
                }
            }
            break;
    }
    return KEY_EVT_NONE;
}

/* ------------------------------------------------------------------ */
/* 应用：短按切换模式，长按熄灭                                        */
/* ------------------------------------------------------------------ */
static volatile uint8_t g_mode = 0;   /* 0=灭 1=常亮 2=慢闪 */

static void handle_event(key_event_t evt)
{
    if (evt == KEY_EVT_SHORT) {
        g_mode = (uint8_t)((g_mode + 1u) % 3u);
    } else if (evt == KEY_EVT_LONG) {
        g_mode = 0;                     /* 长按直接熄灭 */
    }
}

int main(void)
{
    led_init();
    key_init();

    int pll_ok = clock_168m();
    SysTick_Config(SystemCoreClock / 1000u);

    if (!pll_ok) {
        while (1) {                     /* 时钟没配起来：快闪报警 */
            led_on();  delay_ms(100);
            led_off(); delay_ms(100);
        }
    }

    uint32_t last_scan = 0;
    uint8_t  blink_state = 0;

    while (1) {
        uint32_t now = g_ms;

        /* 1. 每 20 ms 采样一次按键（不在中断里做） */
        if ((now - last_scan) >= KEY_SCAN_MS) {
            last_scan = now;
            handle_event(key_scan(&g_key_pa15, now));
            handle_event(key_scan(&g_key_pe6, now));
        }

        /* 2. 根据模式更新 LED（非阻塞） */
        switch (g_mode) {
            case 0:
                led_off();
                break;
            case 1:
                led_on();
                break;
            case 2: {
                uint8_t phase = (uint8_t)((now / 500u) % 2u);
                if (phase != blink_state) {
                    blink_state = phase;
                    if (phase) { led_on(); } else { led_off(); }
                }
                break;
            }
            default:
                break;
        }
    }
}
