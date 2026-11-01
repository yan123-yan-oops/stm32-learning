/**
 * 00-pio-blink —— 鹿小班 F407ZGT6 核心板第一课
 *
 * LED: PC13，低电平点亮（板载 led.h 证据：LED1_ON = GPIOC->BRR = GPIO_PIN_13）
 * 注意: PC13 属备份域，驱动能力 3 mA / 2 MHz，点灯够用，别接大负载。
 *
 * 目标：不依赖 HAL，直接写寄存器，验证「开时钟 → 配模式 → 写 BSRR」这条链路。
 */

#include "stm32f4xx.h"

/* 粗糙延时：168 MHz 主频下大约每个循环几纳秒，具体值靠示波器/秒表校准。
 * volatile 不能省：否则 -O2 会把这个空循环删掉。 */
static void delay_loops(volatile uint32_t loops)
{
    while (loops--) {
        __NOP();
    }
}

int main(void)
{
    /* 1. 开 GPIOC 时钟（不开时钟，下面所有配置都写不进去） */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;

    /* 2. PC13 配成通用推挽输出，低速（PC13 上限 2 MHz） */
    GPIOC->MODER   &= ~(3u << (13u * 2u));
    GPIOC->MODER   |=  (1u << (13u * 2u));
    GPIOC->OTYPER  &= ~(1u << 13u);
    GPIOC->OSPEEDR &= ~(3u << (13u * 2u));
    GPIOC->PUPDR   &= ~(3u << (13u * 2u));

    while (1) {
        /* 3. BSRR：置位写高 16 位，复位写低 16 位，单次写、原子操作 */
        GPIOC->BSRR = (1u << 13u);          /* 高电平 → 灭 */
        delay_loops(20000000u);

        GPIOC->BSRR = (1u << (13u + 16u));  /* 低电平 → 亮 */
        delay_loops(20000000u);
    }
}

/* ---------------------------------------------------------------
 * 对照：HAL 版（看懂寄存器版之后再回来看这段）
 *
 * int main(void) {
 *     HAL_Init();
 *     __HAL_RCC_GPIOC_CLK_ENABLE();
 *     GPIO_InitTypeDef g = {0};
 *     g.Pin = GPIO_PIN_13; g.Mode = GPIO_MODE_OUTPUT_PP;
 *     g.Pull = GPIO_NOPULL; g.Speed = GPIO_SPEED_FREQ_LOW;
 *     HAL_GPIO_Init(GPIOC, &g);
 *     while (1) {
 *         HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
 *         HAL_Delay(500);
 *     }
 * }
 * --------------------------------------------------------------- */
