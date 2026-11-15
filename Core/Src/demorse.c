/*
 * Morse dekoder: cte signal na PB7 (F_MEASURE), funguje pro PULS i tonE (1 kHz).
 *
 * Casove znacky:
 *   - kazda hrana (nabezna i sestupna) ulozi do last_edge_ms aktualni cas,
 *   - start_ms = cas prvni hrany signalu (zacatek tecky / carky),
 *   - konec signalu = posledni hrana, po ktere uz MARK_TIMEOUT_MS zadna neprisla
 *     (v rezimu tonE prichazi hrana kazdych 0,5 ms, v PULS je jen jedna sestupna).
 *
 * Bity (LSB prvni, stejne jako tabulka): delka / T_DIT zaokrouhlena = pocet bitu,
 *   signal -> jednicky (tecka 1, carka 111), ticho uvnitr znaku -> nuly (0).
 *
 * Priklad 'A' (.-), T = 120 ms:
 *   signal 120 ms -> "1", ticho 120 ms -> "0", signal 360 ms -> "111"
 *   -> bits = 0b11101, ticho >= 3T -> konec znaku -> tabulka -> 'A'
 *
 * FSM:
 *   dmIdle    --hrana-------------------------------------------> dmMark
 *   dmMark    --zadna hrana MARK_TIMEOUT_MS (signal skoncil)-----> dmSpace   (+ jednicky)
 *   dmSpace   --hrana (dalsi signal)----------------------------> dmMark    (+ nuly)
 *   dmSpace   --ticho >= 3T-------------------------------------> dmCharEnd (vrati znak)
 *   dmCharEnd --hrana (dalsi znak)------------------------------> dmMark
 *   dmCharEnd --ticho >= 7T-------------------------------------> dmIdle    (vrati ' ')
 */
#include <stdbool.h>
#include "demorse.h"
#include "stm32f4xx.h"
#include "stm32f4xx_hal_gpio.h"

const uint32_t DEMORSE_T_DIT_MS = 120U;   /* definice, jedina kopie ve Flash */

#define T_DIT            DEMORSE_T_DIT_MS
#define MARK_TIMEOUT_MS  3U    /* bez hrany tak dlouho (a pin v 0) = signal skoncil */
#define GAP_CHAR_UNITS   3U    /* ticho mezi znaky */
#define GAP_WORD_UNITS   7U    /* ticho mezi slovy */
#define BITS_MAX         31U

/* Tabulka znaku (kopie z koderu): LSB prvni, 1 = signal, 0 = ticho, 1 bit = 1 jednotka */
static const uint32_t demorse_code[] =
{
  ['A'] = 0b11101,         ['B'] = 0b101010111,     ['C'] = 0b10111010111,
  ['D'] = 0b1010111,       ['E'] = 0b1,             ['F'] = 0b101110101,
  ['G'] = 0b101110111,     ['H'] = 0b1010101,       ['I'] = 0b101,
  ['J'] = 0b1110111011101, ['K'] = 0b111010111,     ['L'] = 0b101011101,
  ['M'] = 0b1110111,       ['N'] = 0b10111,         ['O'] = 0b11101110111,
  ['P'] = 0b10111011101,   ['Q'] = 0b1110101110111, ['R'] = 0b1011101,
  ['S'] = 0b10101,         ['T'] = 0b111,           ['U'] = 0b1110101,
  ['V'] = 0b111010101,     ['W'] = 0b111011101,     ['X'] = 0b11101010111,
  ['Y'] = 0b1110111010111, ['Z'] = 0b10101110111,
};

static volatile uint32_t now_ms;        /* cas dekoderu [ms], ++ v demorse_task */
static volatile uint32_t last_edge_ms;  /* casova znacka posledni hrany (z EXTI) */
static volatile bool     edge_flag;     /* od posledniho kroku prisla hrana */

static demorseState state = dmIdle;
static uint32_t start_ms;               /* zacatek aktualniho signalu (prvni hrana) */
static uint32_t space_ms;               /* zacatek ticha (posledni hrana signalu) */
static uint32_t bits;                   /* skladany kod znaku */
static uint32_t nbits;                  /* pocet bitu v bits */

/**
 * @brief  Nastavi PB7 jako vstup s pull-down a EXTI7 na nabeznou i sestupnou hranu.
 * @param  None.
 * @return Nic. Side effect: zapne hodiny GPIOB a SYSCFG, povoli EXTI9_5_IRQn.
 */
void demorse_init(void)
{
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
  RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

  GPIOB->MODER &= ~GPIO_MODER_MODE7_Msk;                        /* vstup */
  GPIOB->PUPDR &= ~GPIO_PUPDR_PUPD7_Msk;
  GPIOB->PUPDR |= GPIO_PULLDOWN << GPIO_PUPDR_PUPD7_Pos;         /* nezapojeny = ticho */

  SYSCFG->EXTICR[1] &= ~SYSCFG_EXTICR2_EXTI7_Msk;
  SYSCFG->EXTICR[1] |= SYSCFG_EXTICR2_EXTI7_PB;                 /* EXTI7 <- PB7 */
  EXTI->RTSR |= EXTI_RTSR_TR7;
  EXTI->FTSR |= EXTI_FTSR_TR7;
  EXTI->PR    = EXTI_PR_PR7;
  EXTI->IMR  |= EXTI_IMR_MR7;

  state = dmIdle;
  NVIC_EnableIRQ(EXTI9_5_IRQn);
}

/**
 * @brief  Preruseni EXTI9_5: hrana na PB7 -> ulozi casovou znacku.
 * @param  None.
 * @return Nic. Side effect: last_edge_ms = now_ms, edge_flag = true.
 */
void EXTI9_5_IRQHandler(void)
{
  if (EXTI->PR & EXTI_PR_PR7)
  {
    EXTI->PR     = EXTI_PR_PR7;
    last_edge_ms = now_ms;
    edge_flag    = true;
  }
}

/**
 * @brief  Prevede delku v ms na pocet casovych jednotek (zaokrouhleno).
 * @param  ms  Delka useku [ms].
 * @return Pocet jednotek T_DIT (120 ms -> 1, 360 ms -> 3).
 */
static uint32_t to_units(uint32_t ms)
{
  return (ms + T_DIT / 2U) / T_DIT;
}

/**
 * @brief  Prida do bits zadany pocet stejnych bitu (LSB prvni).
 * @param  bit    1 = signal, 0 = ticho.
 * @param  units  Pocet bitu.
 * @return Nic. Pri preteceni nastavi nbits > BITS_MAX (znak pak bude '?').
 */
static void add_bits(uint32_t bit, uint32_t units)
{
  if (nbits + units > BITS_MAX)
  {
    nbits = BITS_MAX + 1U;
    return;
  }
  if (bit)
  {
    bits |= ((1UL << units) - 1UL) << nbits;
  }
  nbits += units;
}

/**
 * @brief  Najde slozeny kod v tabulce a vynuluje ho pro dalsi znak.
 * @param  None.
 * @return Znak 'A'-'Z', nebo '?' kdyz kod v tabulce neni.
 */
static char take_char(void)
{
  char c = '?';
  if (nbits <= BITS_MAX)
  {
    for (char ch = 'A'; ch <= 'Z'; ch++)
    {
      if (demorse_code[(uint8_t)ch] == bits)
      {
        c = ch;
        break;
      }
    }
  }
  bits  = 0;
  nbits = 0;
  return c;
}

/**
 * @brief  Jeden krok dekoderu (FSM), volat kazdou 1 ms.
 * @param  None.
 * @return Dekodovany znak ('A'-'Z', '?' neznamy, ' ' konec slova), 0 = nic noveho.
 */
char demorse_task(void)
{
  char out = 0;

  now_ms++;

  /* atomicky prevezmi udaje z preruseni */
  __disable_irq();
  bool     edge = edge_flag;
  uint32_t last = last_edge_ms;
  edge_flag = false;
  __enable_irq();

  bool pin_high = (GPIOB->IDR & GPIO_IDR_ID7) != 0U;

  switch (state)
  {
    case dmIdle:
      /* prvni hrana = zacatek signalu */
      if (edge)
      {
        start_ms = last;
        state    = dmMark;
      }
      break;

    case dmMark:
      /* signal trva, dokud je pin v 1 nebo chodi hrany (ton) */
      if (!pin_high && (now_ms - last) >= MARK_TIMEOUT_MS)
      {
        add_bits(1U, to_units(last - start_ms));   /* tecka "1" / carka "111" */
        space_ms = last;                            /* posledni hrana = konec signalu */
        state    = dmSpace;
      }
      break;

    case dmSpace:
      if (edge)
      {
        /* dalsi signal ve stejnem znaku: ticho -> nuly */
        add_bits(0U, to_units(last - space_ms));
        start_ms = last;
        state    = dmMark;
      }
      else if (to_units(now_ms - space_ms) >= GAP_CHAR_UNITS)
      {
        out   = take_char();                        /* konec znaku */
        state = dmCharEnd;
      }
      break;

    case dmCharEnd:
      if (edge)
      {
        start_ms = last;                            /* dalsi znak */
        state    = dmMark;
      }
      else if (to_units(now_ms - space_ms) >= GAP_WORD_UNITS)
      {
        out   = ' ';                                /* konec slova */
        state = dmIdle;
      }
      break;
  }

  return out;
}
