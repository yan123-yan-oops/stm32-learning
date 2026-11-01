/**
 * 02-systick —— 时钟树 + SysTick 精确延时
 *
 * 背景：上一个工程用 delay_loops(20000000) 做延时，实测 3 个闪烁周期花了 65 秒，
 *       反推主频只有 ~15 MHz —— 说明芯片根本没跑到 168 MHz（PLL 没起来，退回内部 HSI 16 MHz）。
 *
 *   原因：CMSIS 默认 HSE_VALUE = 25 MHz，而板上晶振是 8 MHz；PLL 输入频率算错 → 锁不上 → 退回 HSI。
 *
 * 本工程做两件事：
 *   1. 显式配置时钟：HSE 8 MHz → PLL(M=8,N=336,P=2) → 168 MHz，Flash 5WS
 *   2. SysTick 1 ms 中断做基准，delay_ms() 不再依赖空循环
 *
 * 预期：LED 每 500 ms 翻转一次，一个完整亮灭周期约 1 秒（对比之前的 21.7 秒）。
 */

#include "stm32f4xx.h"

static volatile uint32_t g_ms = 0;   /* SysTick 中断里自增的毫秒计数 */

void SysTick_Handler(void)
{
    g_ms++;
}

/* HSE 8 MHz → 168 MHz；失败返回 0（保持 HSI 16 MHz） */
static int clock_168m(void)
{
    /* 0. 先把系统时钟切回 HSI，并关掉 PLL。
     *    关键：PLLCFGR 只有在 PLLON = 0 时才可写；
     *    启动代码（SystemInit）可能已经按 25 MHz 的假设把 PLL 打开了，
     *    此时直接写 PLLCFGR 会被硬件忽略。 */
    RCC->CFGR &= ~RCC_CFGR_SW;
    while ((RCC->CFGR & RCC_CFGR_SWS) != 0u) {
    }
    RCC->CR &= ~RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) != 0u) {
    }

    /* 1. 开 HSE 并等就绪（带超时，避免晶振坏掉时死等） */
    RCC->CR |= RCC_CR_HSEON;
    uint32_t timeout = 0x50000u;
    while ((RCC->CR & RCC_CR_HSERDY) == 0u && --timeout != 0u) {
    }
    if ((RCC->CR & RCC_CR_HSERDY) == 0u) {
        return 0;
    }

    /* 2. Flash 等待周期：168 MHz 必须 5WS，并开预取与缓存 */
    FLASH->ACR = FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN
               | FLASH_ACR_LATENCY_5WS;

    /* 3. PLL：8 MHz / M8 = 1 MHz VCO 输入，×N336 = 336 MHz，/P2 = 168 MHz，/Q7 = 48 MHz */
    RCC->PLLCFGR = (8u   << RCC_PLLCFGR_PLLM_Pos)
                 | (336u << RCC_PLLCFGR_PLLN_Pos)
                 | (0u   << RCC_PLLCFGR_PLLP_Pos)
                 | RCC_PLLCFGR_PLLSRC_HSE
                 | (7u   << RCC_PLLCFGR_PLLQ_Pos);

    RCC->CR |= RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) == 0u) {
    }

    /* 4. 总线分频：AHB 168 MHz，APB1 42 MHz，APB2 84 MHz */
    RCC->CFGR |= RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV4 | RCC_CFGR_PPRE2_DIV2;

    /* 5. 切换系统时钟到 PLL */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {
    }

    SystemCoreClockUpdate();
    return 1;
}

static void led_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    GPIOC->MODER   &= ~(3u << (13u * 2u));
    GPIOC->MODER   |=  (1u << (13u * 2u));
    GPIOC->OTYPER  &= ~(1u << 13u);
    GPIOC->OSPEEDR &= ~(3u << (13u * 2u));
    GPIOC->PUPDR   &= ~(3u << (13u * 2u));
    GPIOC->BSRR = (1u << 13u);           /* 灭 */
}

static void delay_ms(uint32_t ms)
{
    uint32_t start = g_ms;
    while ((g_ms - start) < ms) {        /* 无符号减法，天然处理回绕 */
    }
}

int main(void)
{
    led_init();

    int pll_ok = clock_168m();

    /* SysTick 1 ms 中断：SystemCoreClock 只有配好时钟后才正确 */
    SysTick_Config(SystemCoreClock / 1000u);

    while (1) {
        if (pll_ok) {
            GPIOC->BSRR = (1u << (13u + 16u));   /* 亮 */
            delay_ms(500);
            GPIOC->BSRR = (1u << 13u);           /* 灭 */
            delay_ms(500);
        } else {
            /* PLL 没起来：100 ms 快闪，一眼看出时钟异常 */
            GPIOC->BSRR = (1u << (13u + 16u));
            delay_ms(100);
            GPIOC->BSRR = (1u << 13u);
            delay_ms(100);
        }
    }
}
