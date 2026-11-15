#include "seg7.h"
#include "stm32f4xx.h"

#define SEG7_DIGITS 4

/* Hodnoty MODER / AFR (dříve z HAL, zde bez závislosti na HAL) */
#define SEG7_MODE_OUTPUT_PP 0x1UL
#define SEG7_MODE_AF_PP     0x2UL
#define SEG7_AF2_TIM3       0x2UL

static const uint8_t seg7_font[16] =
{
  0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, /* 0..7 */
  0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71  /* 8..F */
};

static const uint8_t seg7_digit_sel[SEG7_DIGITS] = { 0x10, 0x20, 0x40, 0x80 };

static volatile uint8_t seg7_buf[SEG7_DIGITS];
static uint8_t seg7_brightness = SEG7_BRIGHTNESS_MAX;
static uint8_t seg7_pos;

/**
 * @brief  Nastaví datový vstup shift registru (PC12).
 * @param  state  true = log. 1, false = log. 0.
 * @return Nic.
 */
void seg7_write_data(bool state)
{
  GPIOC->BSRR = state ? GPIO_BSRR_BS12 : GPIO_BSRR_BR12;
}

/**
 * @brief  Vygeneruje jeden hodinový impuls SHCP (PC10) - posun o 1 bit.
 * @param  None.
 * @return Nic.
 */
void seg7_shift(void)
{
  GPIOC->BSRR = GPIO_BSRR_BS10;
  GPIOC->BSRR = GPIO_BSRR_BR10;
}

/**
 * @brief  Nastaví datový bit a posune ho do shift registru.
 * @param  state  Hodnota bitu (true = 1, false = 0).
 * @return Nic.
 */
void seg7_writeAndShift(bool state)
{
  seg7_write_data(state);
  seg7_shift();
}

/**
 * @brief  Přepíše shift registr do výstupního bufferu (impuls RCLK, PA15).
 * @param  None.
 * @return Nic.
 */
void seg7_output_buffer_write(void)
{
  GPIOA->BSRR = GPIO_BSRR_BS15;
  GPIOA->BSRR = GPIO_BSRR_BR15;
}

/**
 * @brief  Vymaže shift registr (SRCLR, PC8) a propíše prázdný obsah na výstup.
 * @param  None.
 * @return Nic.
 */
void seg7_clear_registers(void)
{
  GPIOC->BSRR = GPIO_BSRR_BR8;
  GPIOC->BSRR = GPIO_BSRR_BS8;
  seg7_output_buffer_write();
}

/**
 * @brief  Inicializuje GPIO (PC8,9,10,12, PA15), PWM jasu na TIM3_CH4 (PB1)
 *         a vymaže displej.
 * @param  None.
 * @return Nic. Side effect: zapíná hodiny GPIOA/B/C a TIM3.
 * @note   PSC = 8-1 předpokládá 16 MHz HSI (PWM ~20 kHz při ARR = 100).
 */
void init_seg7(void)
{
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN;

  GPIOC->BSRR = GPIO_BSRR_BS8 | GPIO_BSRR_BR9 | GPIO_BSRR_BR10 | GPIO_BSRR_BR12;
  GPIOA->BSRR = GPIO_BSRR_BR15;

  GPIOC->MODER &= ~(GPIO_MODER_MODE8_Msk | GPIO_MODER_MODE9_Msk |
                    GPIO_MODER_MODE10_Msk | GPIO_MODER_MODE12_Msk);
  GPIOC->MODER |= (SEG7_MODE_OUTPUT_PP << GPIO_MODER_MODE8_Pos) |
                  (SEG7_MODE_OUTPUT_PP << GPIO_MODER_MODE9_Pos) |
                  (SEG7_MODE_OUTPUT_PP << GPIO_MODER_MODE10_Pos) |
                  (SEG7_MODE_OUTPUT_PP << GPIO_MODER_MODE12_Pos);
  GPIOA->MODER &= ~GPIO_MODER_MODE15_Msk;
  GPIOA->MODER |= SEG7_MODE_OUTPUT_PP << GPIO_MODER_MODE15_Pos;

  RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
  TIM3->PSC = 8 - 1;
  TIM3->ARR = SEG7_BRIGHTNESS_MAX - 1;
  TIM3->CCR4 = seg7_brightness;
  TIM3->CCMR2 = TIM_CCMR2_OC4M_2 | TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4PE;
  TIM3->CCER = TIM_CCER_CC4E | TIM_CCER_CC4P;
  TIM3->EGR = TIM_EGR_UG;
  TIM3->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;

  GPIOB->AFR[0] &= ~GPIO_AFRL_AFSEL1_Msk;
  GPIOB->AFR[0] |= SEG7_AF2_TIM3 << GPIO_AFRL_AFSEL1_Pos;
  GPIOB->MODER &= ~GPIO_MODER_MODE1_Msk;
  GPIOB->MODER |= SEG7_MODE_AF_PP << GPIO_MODER_MODE1_Pos;

  seg7_clear_registers();
}

/**
 * @brief  Odešle 16bitový rámec (výběr číslice + segmenty) a propíše ho na výstup.
 * @param  digit     Maska výběru číslice (horní bajt rámce).
 * @param  segments  Maska segmentů (dolní bajt rámce).
 * @return Nic.
 */
static void seg7_send(uint8_t digit, uint8_t segments)
{
  uint16_t frame = ((uint16_t)digit << 8) | segments;
  for (uint8_t i = 0; i < 16; i++)
  {
    seg7_writeAndShift((frame >> (15 - i)) & 1);
  }
  seg7_output_buffer_write();
}

/**
 * @brief  Nastaví jas displeje (PWM střída na OE, PB1).
 * @param  brightness  0..SEG7_BRIGHTNESS_MAX; větší hodnoty se oříznou.
 * @return Nic.
 */
void seg7_set_brightness(uint8_t brightness)
{
  seg7_brightness = (brightness > SEG7_BRIGHTNESS_MAX) ? SEG7_BRIGHTNESS_MAX : brightness;
  TIM3->CCR4 = seg7_brightness;
}

/**
 * @brief  Zapíše surové segmentové masky do bufferu všech 4 číslic.
 * @param  message  4 bajty; nejvyšší bajt = číslice 0 (vlevo).
 * @return Nic.
 */
void seg7_print_raw(uint32_t message)
{
  for (uint8_t pos = 0; pos < SEG7_DIGITS; pos++)
  {
    seg7_buf[pos] = (uint8_t)(message >> (8 * (SEG7_DIGITS - 1 - pos)));
  }
}

/**
 * @brief  Multiplex: vykreslí jednu číslici a přejde na další. Volat periodicky
 *         (např. z časovače, ~1 kHz nebo rychleji).
 * @param  None.
 * @return Nic.
 */
void seg7_print_loop(void)
{
  seg7_send(seg7_digit_sel[seg7_pos], seg7_buf[seg7_pos]);
  seg7_pos = (seg7_pos + 1) % SEG7_DIGITS;
}

/**
 * @brief  Zobrazí hexadecimální číslici na dané pozici.
 * @param  pos  Pozice 0..3 (0 = vlevo); mimo rozsah se ignoruje.
 * @param  num  Hodnota 0..15 (použije se dolních 4 bitů).
 * @return Nic.
 */
void seg7_print_digit(uint8_t pos, uint8_t num)
{
  if (pos < SEG7_DIGITS)
  {
    seg7_buf[pos] = seg7_font[num & 0x0F];
  }
}

/**
 * @brief  Zobrazí desítkové číslo na 4 číslicích (s úvodními nulami).
 * @param  num  Číslo 0..9999 (vyšší řády se zahodí).
 * @return Nic.
 */
void seg7_print_num(uint16_t num)
{
  for (int8_t pos = SEG7_DIGITS - 1; pos >= 0; pos--)
  {
    seg7_print_digit((uint8_t)pos, num % 10);
    num /= 10;
  }
}

/**
 * @brief  Převede znak na masku segmentů.
 * @param  c  Znak (0-9, A-F, a-f, P, U, u, L, S, T, O, N, '-').
 * @return Maska segmentů; 0 (zhasnuto) pro nepodporovaný znak.
 */
uint8_t seg7_charToSegments(char c)
{
  if (c >= '0' && c <= '9') return seg7_font[c - '0'];
  if (c >= 'A' && c <= 'F') return seg7_font[c - 'A' + 10];
  if (c >= 'a' && c <= 'f') return seg7_font[c - 'a' + 10];

  switch (c)
  {
    case 'P':
    case 'p':
      return 0b01110011;
    case 'U':
      return 0b00111110;
    case 'u':
      return 0b00011100;
    case 'L':
    case 'l':
      return 0b00111000;
    case 'S':
    case 's':
      return 0b01101101;
    case 'T':
    case 't':
      return 0b01111000;
    case 'O':
    case 'o':
      return 0b01011100;
    case 'N':
    case 'n':
      return 0b01010100;
    case '-':
      return 0b01000000;
    default:
      return 0b00000000;
  }
}

/**
 * @brief  Zobrazí znak na dané pozici.
 * @param  pos  Pozice 0..3 (0 = vlevo); mimo rozsah se ignoruje.
 * @param  c    Znak (viz seg7_charToSegments).
 * @return Nic.
 */
void seg7_print_char(uint8_t pos, char c)
{
  if (pos < SEG7_DIGITS)
  {
    seg7_buf[pos] = seg7_charToSegments(c);
  }
}

/**
 * @brief  Zobrazí až 4 znaky textu; chybějící znaky se zhasnou.
 * @param  text  Řetězec zakončený nulou (nesmí být NULL).
 * @return Nic.
 */
void seg7_print_text(const char *text)
{
  for (uint8_t pos = 0; pos < SEG7_DIGITS; pos++)
  {
    seg7_print_char(pos, *text);
    if (*text)
    {
      text++;
    }
  }
}
