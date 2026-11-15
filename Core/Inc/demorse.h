/*
 * Morse dekoder: cte signal na PB7 (F_MEASURE), funguje pro PULS i tonE (1 kHz).
 * Nezavisly modul - vlastni tabulka znaku.
 *
 * Princip: EXTI na PB7 zapisuje casovou znacku kazde hrany.
 * FSM v demorse_task() z casu prvni a posledni hrany meri delku signalu a ticha
 * a sklada z nich bity (1 jednotka T_DIT = 1 bit, signal = 1, ticho = 0).
 */
#ifndef DEMORSE_H_HDR
#define DEMORSE_H_HDR

#include <stdint.h>

extern const uint32_t DEMORSE_T_DIT_MS;   /*< delka casove jednotky [ms] (stejna jako koder) */

typedef enum
{
  dmIdle,     /* ticho, cekame na prvni hranu */
  dmMark,     /* probiha signal (tecka / carka) */
  dmSpace,    /* ticho uvnitr znaku, ceka se na dalsi signal nebo konec znaku */
  dmCharEnd   /* znak dekodovan, ceka se na dalsi znak nebo konec slova */
} demorseState;

/** Nastav PB7 jako vstup s EXTI prerusenim na obe hrany **/
void demorse_init(void);

/** Jeden krok dekoderu, volat kazdou 1 ms.
  * \return dekodovany znak ('A'-'Z', '?' neznamy, ' ' konec slova), 0 = nic noveho
  **/
char demorse_task(void);

#endif /* DEMORSE_H_HDR */
