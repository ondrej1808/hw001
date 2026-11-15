#include <stdbool.h>
#include "stm32f4xx.h"
#include "morse.h"
#include "button.h"
#include "seg7.h"
#include "uart.h"

#define DISPLAY_TASK_PERIOD_MS 2U   /* 1 cislice / 2 ms -> 4 cislice = 125 Hz */

static volatile bool morse_run;     /* flagy: nastavuje SysTick, maze main */
static volatile bool button_run;
static volatile bool display_run;

/**
 * @brief  SysTick (1 ms): planovac uloh, po uplynuti periody kazde ulohy nastavi jeji flag.
 * @param  None.
 * @return Nic. Side effect: nastavuje morse_run, button_run, display_run.
 */
void SysTick_Handler(void)
{
    static uint32_t button_cnt  = BUTTON_TASK_PERIOD_MS;
    static uint32_t display_cnt = DISPLAY_TASK_PERIOD_MS;

    if (--morse.task_cnt == 0)
    {
        morse.task_cnt = morse.task_period;
        morse_run = true;
    }
    if (--button_cnt == 0)
    {
        button_cnt = BUTTON_TASK_PERIOD_MS;
        button_run = true;
    }
    if (--display_cnt == 0)
    {
        display_cnt = DISPLAY_TASK_PERIOD_MS;
        display_run = true;
    }
}

/**
 * @brief  Zobrazi na displeji aktualni rezim Morse vystupu.
 * @param  None.
 * @return Nic. "tonE" = ton na bzucaku, "PULS" = jen LED.
 */
static void show_mode(void)
{
    seg7_print_text(morse.tone ? "tonE" : "PULS");
}

/**
 * @brief  Vstupni bod: inicializace, SysTick 1 ms a hlavni smycka spoustejici ulohy podle flagu.
 *         SW1 (PA0) prepina morse.tone, displej ukazuje rezim,
 *         znaky z UART (USB VCP, 115200) jdou do bufferu Morse koderu.
 * @param  None.
 * @return Nikdy se nevraci.
 */
int main(void)
{
    SystemCoreClockUpdate();
    morse_init();
    button_init();
    init_seg7();
    uartInit();
    show_mode();
    SysTick_Config(SystemCoreClock / 1000U);   /* az po init: task_cnt je nastaveny */

    while (true)
    {
        if (morse_run)
        {
            morse_run = false;
            morse_output(morse_task());   /* LED1 (PC0) + ton na PA11 */
        }
        if (button_run)
        {
            button_run = false;
            if (button_task())            /* stisk SW1 -> prepnout rezim */
            {
                morse.tone = !morse.tone;
                show_mode();
            }
        }
        if (run_rx_uart)
        {
            run_rx_uart = false;
            char c = uart_rx_char;
            /* echo do terminalu; '!' = znak nepridan (plny buffer / nepodporovany) */
            uart_putc(morse_put(c) ? c : '!');
        }
        if (display_run)
        {
            display_run = false;
            seg7_print_loop();            /* multiplex 1 cislice */
        }
    }
}
