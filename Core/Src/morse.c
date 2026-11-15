/*                                                                            *
 * B2M37AMP Microprocessor Applications                                       *
 *                                                                            *
 * Morse encoder                                                              *
 *                                                                            *
 *                                                                            *
 * morse.c, Josef Krska 2026                                                  *
 */
#include "morse.h"
#include "stm32f4xx.h"
#include "stm32f4xx_hal_gpio.h"

const uint32_t T_DIT_MS = 120;

const uint32_t morse_code[] =
{
  ['A'] = 0b11101, /* a-kat */
  ['B'] = 0b101010111, /* bly-ska-vi-ce */
  ['C'] = 0b10111010111, /* ci-lov-ni-ci */
  ['D'] = 0b1010111, /* da-la-va */
  ['E'] = 0b1, /* erb */
  ['F'] = 0b101110101, /* fi-li-al-ka */
  ['G'] = 0b101110111, /* gron-ska-zem */
  ['H'] = 0b1010101, /* har-mo-ni-ka */
  ['I'] = 0b101, /* i-bis */
  ['J'] = 0b1110111011101, /* jas-min-bi-ly */
  ['K'] = 0b111010111, /* ka-vo-vnik */
  ['L'] = 0b101011101, /* lu-pi-ne-cek */
  ['M'] = 0b1110111, /* ma-va */
  ['N'] = 0b10111, /* na-rod */
  ['O'] = 0b11101110111, /* o-nas-pan */
  ['P'] = 0b10111011101, /* po-zar-ni-ci */
  ['Q'] = 0b1110101110111, /* kvi-li-or-kan */
  ['R'] = 0b01011101, /* ra-ra-sek */
  ['S'] = 0b10101, /* sve-tlus-ka */
  ['T'] = 0b111, /* tram */
  ['U'] = 0b1110101, /* u-li-cnik */
  ['V'] = 0b111010101, /* vi-no-bra-ni */
  ['W'] = 0b111011101, /* dvoj-te-ve */
  ['X'] = 0b11101010111, /* Xe-ni-e-ma */
  ['Y'] = 0b1110111010111, /* y-se-ztra-ci */
  ['Z'] = 0b10101110111, /* zpi-va-me-si */
};

static uint32_t idx = 0;
static const uint32_t msg[] =
{
  morse_code['A'], morse_code['M'], morse_code['P']
};


morseTask_t morse; /* Definice zde */

#define MSG_LEN        (sizeof(msg) / sizeof(msg[0]))
#define GAP_CHAR_UNITS 3U   /* mezera mezi znaky */
#define GAP_WORD_UNITS 7U   /* mezera mezi slovy (po konci zpravy) */

/**
 * @brief  Nastavi PC0 (LED1) jako push-pull vystup (signal Morse koderu), vychozi stav 0.
 * @param  None.
 * @return Nic. Side effect: zapne hodiny GPIOC.
 */
static void morse_output_init(void)
{
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
  GPIOC->BSRR   = GPIO_BSRR_BR0;
  GPIOC->MODER &= ~GPIO_MODER_MODE0_Msk;
  GPIOC->MODER |= GPIO_MODE_OUTPUT_PP << GPIO_MODER_MODE0_Pos;
}

/**
 * @brief  Inicializuje ulohu Morse koderu: vystup PC0 (LED1), perioda = 1 dit, start v msIdle.
 * @param  None.
 * @return Nic. Side effect: zapne hodiny GPIOC a nastavi PC0 jako vystup.
 */
void morse_init(void)
{
  morse_output_init();
  morse.task_period    = T_DIT_MS;
  morse.task_cnt       = 1;
  morse.state          = msIdle;
  morse.current_symbol = 0;
  morse.wait           = 0;
}

/**
 * @brief  Jeden krok koderu (volat kazdych T_DIT_MS). Vysila zpravu msg[] dokola.
 *         Kod znaku: LSB se vysila prvni, 1 = signal, 0 = ticho (1 jednotka).
 * @param  None.
 * @return 1 kdyz ma byt vysilan signal, 0 kdyz ticho.
 */
uint32_t morse_task(void)
{
  uint32_t ret = 0;

  switch (morse.state)
  {
    case msIdle:
      /* start zpravy: nacti prvni znak */
      idx = 0;
      morse.current_symbol = msg[idx++];
      morse.state = msChar;
      break;

    case msChar:
      /* vezmi posledni bit, nastav vystup, posun doprava */
      ret = morse.current_symbol & 1U;
      morse.current_symbol >>= 1;
      if (morse.current_symbol == 0)
      {
        /* konec znaku -> mezera; za poslednim znakem mezera mezi slovy */
        if (idx >= MSG_LEN)
        {
          idx = 0;
          morse.wait = GAP_WORD_UNITS;
        }
        else
        {
          morse.wait = GAP_CHAR_UNITS;
        }
        morse.state = msNextChar;
      }
      break;

    case msNextChar:
      /* odpocitej mezeru (ticho), pak nacti dalsi znak */
      if (--morse.wait == 0)
      {
        morse.current_symbol = msg[idx++];
        morse.state = msChar;
      }
      break;
  }

  return ret;
}