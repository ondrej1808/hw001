#include <stdbool.h>
#include "stm32f4xx.h"
#include "morse.h"

static volatile bool morse_run = false;   /* flag: nastavuje SysTick, maze main */

/**
 * @brief  SysTick (1 ms): odpocitava periodu Morse ulohy a po jejim uplynuti nastavi flag.
 * @param  None.
 * @return Nic. Side effect: morse_run = true kazdych morse.task_period ms.
 */
void SysTick_Handler(void)
{
    if (--morse.task_cnt == 0)
    {
        morse.task_cnt = morse.task_period;
        morse_run = true;
    }
}

/**
 * @brief  Vstupni bod: nastavi SysTick 1 ms, Morse ulohu a v hlavni smycce ji spousti podle flagu.
 * @param  None.
 * @return Nikdy se nevraci.
 */
int main(void)
{
    SystemCoreClockUpdate();
    morse_init();
    SysTick_Config(SystemCoreClock / 1000U);   /* az po morse_init: task_cnt je nastaveny */

    while (true)
    {
        if (morse_run)
        {
            morse_run = false;
            morse_output(morse_task());   /* LED1 (PC0) + ton na PA11 */
        }
    }
}
