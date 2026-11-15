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
#include "stm32f4xx_hal_gpio_ex.h"

#define TONE_FREQ_HZ   1000U                        /* frekvence tonu bzucaku */
#define TONE_TIM_CLK   1000000U                     /* TIM1 tick po prescaleru: 1 MHz */
#define TONE_ARR       (TONE_TIM_CLK / TONE_FREQ_HZ) /* 1000 -> 1 kHz */

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

morseTask_t morse; /* Definice zde */

#define GAP_CHAR_UNITS 3U   /* mezera mezi znaky */
#define GAP_WORD_UNITS 7U   /* mezera mezi slovy */

/* Kruhovy buffer znaku k odvysilani (FIFO). Jedno misto zustava volne,
 * aby slo rozlisit plny / prazdny -> kapacita MORSE_BUF_SIZE znaku. */
static char     morse_buf[MORSE_BUF_SIZE + 1];
static volatile uint32_t buf_head;   /* sem se zapisuje (morse_put) */
static volatile uint32_t buf_tail;   /* odsud se cte (morse_task) */

/**
 * @brief  Vlozi znak do bufferu koderu.
 * @param  c  Znak: A-Z, a-z (prevede se na velka), mezera = mezera mezi slovy.
 *            Ostatni znaky se zahodi.
 * @return 1 = vlozeno, 0 = buffer plny nebo nepodporovany znak.
 */
uint32_t morse_put(char c)
{
  if (c >= 'a' && c <= 'z')
  {
    c = (char)(c - 'a' + 'A');
  }
  if (!((c >= 'A' && c <= 'Z') || c == ' '))
  {
    return 0;
  }

  uint32_t next = (buf_head + 1U) % (MORSE_BUF_SIZE + 1U);
  if (next == buf_tail)
  {
    return 0;                       /* plny */
  }
  morse_buf[buf_head] = c;
  buf_head = next;
  return 1;
}

/**
 * @brief  Vlozi retezec do bufferu koderu (po znacich pres morse_put).
 * @param  s  Retezec zakonceny nulou.
 * @return Pocet skutecne vlozenych znaku.
 */
uint32_t morse_puts(const char *s)
{
  uint32_t n = 0;
  while (*s)
  {
    n += morse_put(*s++);
  }
  return n;
}

/**
 * @brief  Vybere dalsi znak z bufferu a pripravi stav FSM.
 * @param  None.
 * @return Nic. Side effect: pismeno -> msChar s current_symbol;
 *         mezera -> msNextChar s cekanim do celkem 7 jednotek; prazdny buffer -> msIdle.
 */
static void morse_load_next(void)
{
  if (buf_tail == buf_head)
  {
    morse.state = msIdle;           /* nic k vysilani */
    return;
  }

  char c = morse_buf[buf_tail];
  buf_tail = (buf_tail + 1U) % (MORSE_BUF_SIZE + 1U);

  if (c == ' ')
  {
    /* 3 jednotky ticha uz probehly za predchozim znakem, doplnit na 7 */
    morse.wait  = GAP_WORD_UNITS - GAP_CHAR_UNITS;
    morse.state = msNextChar;
  }
  else
  {
    morse.current_symbol = morse_code[(uint8_t)c];
    morse.state = msChar;
  }
}

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
 * @brief  Nastavi PA11 (BUZZER_PWM) jako TIM1_CH4 a TIM1 na PWM 1 kHz, 50 % strida.
 *         Ton je po inicializaci vypnuty (CCR4 = 0 -> vystup trvale v 0).
 * @param  None.
 * @return Nic. Side effect: zapne hodiny GPIOA a TIM1.
 * @note   Pocita s TIM1 clock = 16 MHz (HSI, APB2 bez delicky).
 */
static void morse_tone_init(void)
{
  /* PA11 -> AF1 (TIM1_CH4) */
  RCC->AHB1ENR  |= RCC_AHB1ENR_GPIOAEN;
  GPIOA->AFR[1] &= ~GPIO_AFRH_AFSEL11_Msk;
  GPIOA->AFR[1] |= GPIO_AF1_TIM1 << GPIO_AFRH_AFSEL11_Pos;
  GPIOA->MODER  &= ~GPIO_MODER_MODE11_Msk;
  GPIOA->MODER  |= GPIO_MODE_AF_PP << GPIO_MODER_MODE11_Pos;

  /* TIM1: 16 MHz / 16 = 1 MHz, ARR 1000 -> 1 kHz */
  RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
  TIM1->PSC   = (16000000U / TONE_TIM_CLK) - 1U;
  TIM1->ARR   = TONE_ARR - 1U;
  TIM1->CCR4  = 0U;                                         /* ton vypnuty */
  TIM1->CCMR2 = TIM_CCMR2_OC4M_2 | TIM_CCMR2_OC4M_1        /* PWM mode 1 */
              | TIM_CCMR2_OC4PE;
  TIM1->CCER  = TIM_CCER_CC4E;
  TIM1->BDTR  = TIM_BDTR_MOE;                               /* advanced timer: povolit vystupy */
  TIM1->EGR   = TIM_EGR_UG;
  TIM1->CR1   = TIM_CR1_ARPE | TIM_CR1_CEN;
}

/**
 * @brief  Nastavi vystupy Morse koderu: LED1 (PC0) a pri morse.tone i ton na bzucaku.
 * @param  on  != 0 = vysilat (LED sviti, ton zni), 0 = ticho.
 * @return Nic.
 */
void morse_output(uint32_t on)
{
  GPIOC->BSRR = on ? GPIO_BSRR_BS0 : GPIO_BSRR_BR0;
  TIM1->CCR4  = (on && morse.tone) ? (TONE_ARR / 2U) : 0U;
}

/**
 * @brief  Inicializuje ulohu Morse koderu: LED1 (PC0), bzucak PA11 (TIM1_CH4, 1 kHz),
 *         perioda = 1 dit, start v msIdle, rezim tonu zapnuty.
 * @param  None.
 * @return Nic. Side effect: zapne hodiny GPIOA, GPIOC a TIM1.
 */
void morse_init(void)
{
  morse_output_init();
  morse_tone_init();
  morse.tone           = 1;
  morse.task_period    = T_DIT_MS;
  morse.task_cnt       = 1;
  morse.state          = msIdle;
  morse.current_symbol = 0;
  morse.wait           = 0;
  buf_head             = 0;
  buf_tail             = 0;

}

/**
 * @brief  Jeden krok koderu (volat kazdych T_DIT_MS). Vysila znaky z bufferu,
 *         kdyz je prazdny, ceka v msIdle (ticho).
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
      /* cekame na znak v bufferu */
      morse_load_next();
      if (morse.state != msChar)
      {
        break;
      }
      /* fall through: znak je pripraveny, vysilej hned */
      /* FALLTHRU */

    case msChar:
      /* vezmi posledni bit, nastav vystup, posun doprava */
      ret = morse.current_symbol & 1U;
      morse.current_symbol >>= 1;
      if (morse.current_symbol == 0)
      {
        morse.wait  = GAP_CHAR_UNITS;  /* konec znaku -> mezera mezi znaky */
        morse.state = msNextChar;
      }
      break;

    case msNextChar:
      /* odpocitej mezeru (ticho), pak nacti dalsi znak */
      if (--morse.wait == 0)
      {
        morse_load_next();
      }
      break;
  }

  return ret;
}