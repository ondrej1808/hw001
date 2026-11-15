/*
 * Obsluha tlacitka SW1 (PA0, active low, externi pull-up 10k)
 */
#ifndef BUTTON_H_HDR
#define BUTTON_H_HDR

#include <stdint.h>

#define BUTTON_TASK_PERIOD_MS 10U   /* perioda volani button_task() */

/** Nastav PA0 (SW1) jako vstup **/
void button_init(void);

/** Odrusena obsluha tlacitka, volat kazdych BUTTON_TASK_PERIOD_MS.
  * \return 1 v okamziku stisku (hrana), jinak 0
  **/
uint32_t button_task(void);

#endif /* BUTTON_H_HDR */
