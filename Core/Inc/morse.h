/*                                                                            *
 * B2M37AMP Microprocessor Applications                                       *
 *                                                                            *
 * Morse encoder                                                              *
 *                                                                            *
 *                                                                            *
 * morse.h, Josef Krska 2026                                                  *
 */
#ifndef MORSE_H_HDR
#define MORSE_H_HDR

#include <stdint.h>

typedef enum morseState
{
  msIdle, msChar, msNextChar
} morseState;

typedef struct morseTask_s
{
  uint32_t task_period,
           task_cnt;

  morseState state;
  uint32_t current_symbol;
  uint32_t wait;
  uint32_t tone;          /* != 0: vysilat i ton 1 kHz na BUZZER_PWM (PA11) */
} morseTask_t;

/* Deklarace struktury ulohy, extern rika, ze definice se nachazi jinde,
 * extern take zviditelnuje promennou vsem modulum, ktere tento soubor pouzivaji */
extern morseTask_t morse; /*< Morse Koder data ulohy */

/** Inicializuj ulohu Morse koderu **/
void morse_init(void);

/** "Hlavni" funkce Morse koderu, koduje vstupni text morseovy kodem,
  * prepoklada se, ze je spoustena fixni periodou danou rychlosti WPM
  * \return != 0 kdyz ma byt vysilan signal, 0 kdy nema byt nic vysilano
  **/
uint32_t morse_task(void);

/** Nastav vystupy (LED1, pri morse.tone i ton bzucaku) podle vysledku morse_task() **/
void morse_output(uint32_t on);

#endif /* MORSE_H_HDR */