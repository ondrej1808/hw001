/*                                                                            *
 * B2M37AMP Microprocessor Applications                                       *
 *                                                                            *
 * Morse encoder                                                              *
 *                                                                            *
 *                                                                            *
 * morse.c, Josef Krska 2026                                                  *
 */
#include "morse.h"

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

static const T_DIT_MS = 120;

morseTask_t morse; /* Definice zde */

void morse_init(void)
{
  // PA11 - vystup na timer, casovac, prescaler
  // timer má periodu 120 ms takže f = 1/120m = 0.0083333 Hz
}

uint32_t morse_task(void)
{
  uint32_t ret = 0;
  return ret;
}