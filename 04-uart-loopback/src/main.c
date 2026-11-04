/**
 * 04-uart-loopback —— UART 自发自收（L04）
 *
 * 板子上没有 USB 转串口芯片，也没买 USB-TTL？
 * 没关系：把 PA9(TX) 和 PA10(RX) 用一根杜邦线短接，就能自己发给自己。
 *
 * 流程：
 *   每 2 秒发一条命令（ON / OFF 交替）
 *   → 短接线把它送回 RX
 *   → RXNE 中断把字节塞进环形缓冲
 *   → 主循环拼出一行、解析、控制 LED
 *
 * 现象：
 *   接好短接线 → LED 每 2 秒亮灭切换（说明 发送/接收/解析 全通）
 *   没接短接线 → 5 秒后进入快闪（收不到任何字节）
 *
 * 硬件：LED = PC13（低电平亮）；USART1 = PA9/PA10
 */

#include "stm32f4xx.h"
#include <string.h>

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
    GPIOC->BSRR = (1u << 13u);
}

static void led_on(void)  { GPIOC->BSRR = (1u << (13u + 16u)); }
static void led_off(void) { GPIOC->BSRR = (1u << 13u); }

/* ------------------------------------------------------------------ */
/* 环形缓冲：中断里只写，主循环只读                                     */
/* ------------------------------------------------------------------ */
#define RB_SIZE 64

static volatile uint8_t  g_rb[RB_SIZE];
static volatile uint16_t g_rb_head = 0;
static volatile uint16_t g_rb_tail = 0;

static void rb_put(uint8_t byte)
{
    uint16_t next = (uint16_t)((g_rb_head + 1u) % RB_SIZE);
    if (next != g_rb_tail) {          /* 满了就丢最新字节 */
        g_rb[g_rb_head] = byte;
        g_rb_head = next;
    }
}

static int rb_get(uint8_t *byte)
{
    if (g_rb_tail == g_rb_head) {
        return 0;
    }
    *byte = g_rb[g_rb_tail];
    g_rb_tail = (uint16_t)((g_rb_tail + 1u) % RB_SIZE);
    return 1;
}

/* ------------------------------------------------------------------ */
/* USART1：PA9=TX, PA10=RX, 115200 8N1                                 */
/* ------------------------------------------------------------------ */
static void uart1_init(uint32_t baud)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    /* PA9/PA10 → 复用功能 AF7 */
    GPIOA->MODER &= ~((3u << (9u * 2u)) | (3u << (10u * 2u)));
    GPIOA->MODER |=  ((2u << (9u * 2u)) | (2u << (10u * 2u)));
    GPIOA->AFR[1] &= ~((0xFu << 4) | (0xFu << 8));    /* AFRH9 / AFRH10 */
    GPIOA->AFR[1] |=  ((7u << 4) | (7u << 8));

    /* USART1 在 APB2 = 84 MHz：BRR = fCK / baud（16 倍过采样） */
    USART1->BRR = (84000000u + baud / 2u) / baud;

    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE;
    NVIC_SetPriority(USART1_IRQn, 1);
    NVIC_EnableIRQ(USART1_IRQn);
}

void USART1_IRQHandler(void)
{
    if ((USART1->SR & USART_SR_RXNE) != 0u) {
        uint8_t byte = (uint8_t)(USART1->DR & 0xFFu);
        rb_put(byte);                  /* 中断里只搬运 */
    }
}

static void uart_putc(char c)
{
    while ((USART1->SR & USART_SR_TXE) == 0u) {
    }
    USART1->DR = (uint8_t)c;
}

static void uart_puts(const char *s)
{
    while (*s != '\0') {
        uart_putc(*s++);
    }
}

/* ------------------------------------------------------------------ */
/* 应用：解析一行命令                                                   */
/* ------------------------------------------------------------------ */
static void apply_cmd(const char *cmd)
{
    if (strcmp(cmd, "ON") == 0) {
        led_on();
    } else if (strcmp(cmd, "OFF") == 0) {
        led_off();
    }
    /* 其它命令忽略 */
}

int main(void)
{
    led_init();
    int pll_ok = clock_168m();
    uart1_init(115200u);
    SysTick_Config(SystemCoreClock / 1000u);

    if (!pll_ok) {
        while (1) { led_on(); delay_ms(100); led_off(); delay_ms(100); }
    }

    /* 先发一条，验证发送通道 */
    uart_puts("OFF\r\n");

    char     line[16];
    uint8_t  len = 0;
    uint32_t last_tx = 0;
    uint32_t last_rx = g_ms;
    uint8_t  send_on = 1;

    while (1) {
        /* 1. 取字节、拼行、解析（全部在主循环） */
        uint8_t byte;
        while (rb_get(&byte)) {
            last_rx = g_ms;
            if (byte == '\n') {
                line[len] = '\0';
                apply_cmd(line);
                len = 0;
            } else if (byte != '\r' && len < (uint8_t)(sizeof(line) - 1u)) {
                line[len++] = (char)byte;
            }
        }

        /* 2. 每 2 秒发一条命令，ON / OFF 交替 */
        if ((g_ms - last_tx) >= 2000u) {
            last_tx = g_ms;
            uart_puts(send_on ? "ON\r\n" : "OFF\r\n");
            send_on = (uint8_t)!send_on;
        }

        /* 3. 5 秒没收到任何字节 → 回环没接好，快闪提示 */
        if ((g_ms - last_rx) >= 5000u) {
            led_on();  delay_ms(60);
            led_off(); delay_ms(60);
            last_rx = g_ms;            /* 重置，进入下一轮提示 */
        }
    }
}
