/**
 * 01-key-led —— 按键控制 LED（板子第二课）
 *
 * 硬件（来自原理图）：
 *   LED1 = PC13，低电平亮
 *   用户按键 = KEY_PA15 和 KEY_PE6，10K 上拉到 3V3，按下为低
 *
 * 学到什么：
 *   1. 输入要配 MODER=00 + PUPDR=01（上拉），不是输出
 *   2. 读 IDR 而不是 ODR
 *   3. 消抖：机械按键按下瞬间有 5–20 ms 抖动
 *   4. "等松手"防止一次按下触发多次
 *
 * 注意：PA15 复位后是 JTDI（JTAG 数据输入）。把 MODER 设成输入后它可以当普通 GPIO 用，
 *      SWD（PA13/PA14）不受影响。如果按 PA15 没反应，按另一个键（PE6）试试。
 */

#include "stm32f4xx.h"

static void delay_loops(volatile uint32_t loops)
{
    while (loops--) {
        __NOP();
    }
}

static void led_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;

    GPIOC->MODER   &= ~(3u << (13u * 2u));
    GPIOC->MODER   |=  (1u << (13u * 2u));   /* 输出 */
    GPIOC->OTYPER  &= ~(1u << 13u);          /* 推挽 */
    GPIOC->OSPEEDR &= ~(3u << (13u * 2u));   /* PC13 上限 2 MHz */
    GPIOC->PUPDR   &= ~(3u << (13u * 2u));

    GPIOC->BSRR = (1u << 13u);               /* 先灭（高电平） */
}

static void key_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOEEN;

    /* PA15：输入 + 上拉 */
    GPIOA->MODER &= ~(3u << (15u * 2u));
    GPIOA->PUPDR &= ~(3u << (15u * 2u));
    GPIOA->PUPDR |=  (1u << (15u * 2u));

    /* PE6：输入 + 上拉 */
    GPIOE->MODER &= ~(3u << (6u * 2u));
    GPIOE->PUPDR &= ~(3u << (6u * 2u));
    GPIOE->PUPDR |=  (1u << (6u * 2u));
}

/* 任一个按键按下都算（按下 = 低电平） */
static int key_pressed(void)
{
    if ((GPIOA->IDR & (1u << 15u)) == 0u) {
        return 1;
    }
    if ((GPIOE->IDR & (1u << 6u)) == 0u) {
        return 1;
    }
    return 0;
}

int main(void)
{
    led_init();
    key_init();

    int led_on = 0;

    while (1) {
        if (key_pressed()) {
            delay_loops(400000u);          /* 约 10 ms 消抖 */
            if (key_pressed()) {           /* 还是按下 → 真的按了 */
                led_on = !led_on;
                if (led_on) {
                    GPIOC->BSRR = (1u << (13u + 16u));   /* 低 = 亮 */
                } else {
                    GPIOC->BSRR = (1u << 13u);           /* 高 = 灭 */
                }

                while (key_pressed()) { }  /* 等松手，防止连发 */
                delay_loops(400000u);      /* 松手也消抖 */
            }
        }
    }
}
