/*
 *    Author  : Atick Faisal
 *    Created : 22 Feb, 2019
 *    License : GPL-3.0
 */

#include <Arduino.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <math.h>

#define XTAL_FREQ 16000000
#define INVERTER_FREQ 50
#define STEPS_IN_SINE 200

void __init_registers(void);
void __gen_sin_values(void);

unsigned int max_cycle = (XTAL_FREQ / (INVERTER_FREQ * STEPS_IN_SINE));
const unsigned int half_wave_steps = (int) (STEPS_IN_SINE / 2);

unsigned int sin_half[half_wave_steps];

unsigned int count = 0;
byte toggle_tccr1a = 0x00;

void setup() {
  __gen_sin_values();
  __init_registers();
}

void loop() {;}

void __init_registers(void) {
  /*
   * TCCR1A register
   * +---------------------------------------------------------------+
   * |    7   |    6   |    5   |    4   |  3  |  2  |   1   |   0   |
   * +---------------------------------------------------------------+
   * | COM1A1 | COM1A0 | COM1B1 | COM1B0 |  -  |  -  | WGM11 | WGM10 |
   * +---------------------------------------------------------------+
   * TCCR1B register
   * +---------------------------------------------------------------+
   * |    7   |    6   |    5   |    4   |  3  |  2  |   1   |   0   |
   * +---------------------------------------------------------------+
   * |  ICNC1 |  ICES1 |    -   |  WGM13 |WGM12|CS12 | CS11  | WGM10 |
   * +---------------------------------------------------------------+
   */

  //-------Initialize the registers with zeros-----//
  TCCR1A = 0b00000000;
  TCCR1B = 0b00000000;
  TIMSK1 = 0b00000000;
  /*
   * Set Dual Compare Output mode (initially only COM1A1)
   * +----------------------------------------------------+
   * | COM1A1/COM1B1 | COM1A0/COM1B0 |     DESCRIPTION    |
   * +----------------------------------------------------+
   * |        1      |        0      |  ClEAR OC1A/OC1B   |
   * +----------------------------------------------------+
   */

   TCCR1A |= (1 << COM1A1);
   //TCCR1A |= (1 << COM1B1);
   TCCR1A &= ~(1 << COM1A0);
   //TCCR1A |= (1 << COM1B0);

   //------ set the toggle byte to switch between COM1A1 and COM1B1-----//
   toggle_tccr1a |= (1 << COM1A1);
   toggle_tccr1a |= (1 << COM1B1);
  
  /* 
   * Waveform Generation Mode 14 (page 136)
   * +-------------------------------------------------------------+
   * | WGM13 | WGM12 | WGM11 | WGM10 |   TOP   | UPDATE | TOV1FLAG |
   * +-------------------------------------------------------------+
   * |   1   |   1   |   1   |   0   |  OCR1A  | BOTTOM |   TOP    |
   * +-------------------------------------------------------------+
   */
   
  TCCR1A &= ~(1 << WGM10);
  TCCR1A |= (1 << WGM11);
  TCCR1B |= (1 << WGM12);
  TCCR1B |= (1 << WGM13);

  /* 
   *  Set prescalar for timer 1 (page 137)
   *  +--------------------------------+
   *  | CS12 | CS11 | CS10 | PRESCALAR |
   *  +--------------------------------+
   *  |  0   |  0   |  1   | NO PSCLR  |
   *  +--------------------------------+
   */

  TCCR1B |= (1 << CS10);
  TCCR1B &= ~(1 << CS11);
  TCCR1B &= ~(1 << CS12);

  //--------Enable overflow interrupt--------//
  TIMSK1 |= (1 << TOIE1);
  
  //--------Set TOP value--------//
  ICR1 = max_cycle;
  /*
   * TIMER1 value 
   *        /|     /|     /|``````````TOP_VALUE
   *       / |    / |    / |
   * -----/--|---/--|---/--|----------OCR1A/OCR1B
   *     /   |  /   |  /   |
   *    /    | /    | /    |
   *   /     |/     |/     |__________BOTTOM_VALUE
   *      TIME ------>
   *      
   *  OC1A/OC1B (PB1/PB2) pin output
   *  +--+   +--+   +--+   + `````````5V
   *  |  |   |  |   |  |   |
   *  |  |   |  |   |  |   |
   *  +  +---+  +---+  +---+__________0V
   *      TIME ------>
   */

  //--------Enable Global Interrupt----------//
  sei();

  /*----------------set output pin------------------*/
  /* OC1A (PB1) is cleared when timer reaches OCR1A
   * OC1B (PB2) is clearde when timer reaches OCR1B */
  
  DDRB |= (1 << PB1);
  DDRB |= (1 << PB2);
}

void __gen_sin_values(void) {
  
  //----------Generate values----------//
  for (int i = 0; i < half_wave_steps; i++) {
    double sin_value = sin(2 * M_PI * (i) / STEPS_IN_SINE);
    int clk_cycle = (int) (sin_value * max_cycle);
    sin_half[i] = clk_cycle;
  }
}

ISR(TIMER1_OVF_vect) {
  /*
   * High time values loaded in OCR1A:B are buffered
   * This is due to the execution time of the ISR
   * But TCCR1A changes immediately
   * which might clear the high values stored in OCCR1A:B
   * So we delay 1 ISR cycle before changing TCCR1A
   */

  static bool delay_isr_cycle = false;

  if (delay_isr_cycle) {
    //----- switch between COM1A1 and COM1B1------//
    TCCR1A ^= toggle_tccr1a;
    delay_isr_cycle = false;
  } else if (count >= half_wave_steps) {
    //--------reset conuter on overflow------//
    count = 0;
    delay_isr_cycle = true;
  }
  
  //------------set sine wave values into compare registers-------------//
  OCR1A = sin_half[count];
  OCR1B = sin_half[count];
  count++;
}