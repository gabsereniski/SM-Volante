/*---------------------------------------
2252813 Reginaldo Gregório de Souza Neto
2349388 Ingrid Reupke Sbeguen Moran
2349400 João Vitor Souza Rocha
2349345 Gabriela Paola Sereniski
2349280 Catarine Soares Cruz
2304562 Luis Felipe Galleguillos Campos
---------------------------------------*/

#define ATUA_CW PB4
#define ATUA_CCW PB5 // HIGH aqui gira CCW
#define ATUA_STRENGTH PB3

#define ROTARY_ENC_A 6
#define ROTARY_ENC_B 7
#define ROTARY_ENC_PCINT_A PCINT22
#define ROTARY_ENC_PCINT_B PCINT23
#define ROTARY_ENC_PCINT_AB_IE PCIE2

#define POS_SENSOR PB2 // switch (absolute position)
#define GP_BUTTON PB1  // general purpose button
#define GP_BUTTON_GND PB0
#define POWER 170

#define EEPROM_ADRESS_STATE 8
#define EEPROM_ADRESS_COUNT 0

#include <EEPROM.h>
#include "Rotary.h"
volatile long count = 0;           // encoder_rotativo = posicao relativa depois de ligado
volatile bool absolute_sw = false; // chave de posicao do volante ativa?
volatile int eeprom_state;         // usado pra saber se a eeprom tem dado valido na memoria ou nao
volatile long last_read = 0;       // ultima vez que o botao de uso geral foi pressionado
volatile long click_button = 0;    // momento do clique do botao
volatile long last_click = 0;      //momento do clique na interrupcao

Rotary r = Rotary(ROTARY_ENC_A, ROTARY_ENC_B);

volatile unsigned long long na_beradinha = 0; // tempo que o volante comecou a centralizar

void initPWM()
{
  OCR2A = 0;
  TCCR2A = (1 << WGM20);
  TCCR2B = (1 << CS21);
  TCCR2A |= (1 << COM2A1);
}

void setPWM(char val)
{
  OCR2A = val;
}

void Stop()
{
  PORTB |= (1 << ATUA_CW);
  PORTB |= (1 << ATUA_CCW);
}

void Idle()
{
  setPWM(0);
  PORTB &= ~(1 << ATUA_CW);
  PORTB &= ~(1 << ATUA_CCW);
}

void Move(char power, bool cw = true)
{
  if (power == 0)
    Idle();
  else
  {
    if (cw)
    {
      PORTB |= (1 << ATUA_CW);
      PORTB &= ~(1 << ATUA_CCW);
    }
    else
    {
      PORTB &= ~(1 << ATUA_CW);
      PORTB |= (1 << ATUA_CCW);
    }
    setPWM(power);
  }
}

bool last;

void beradinha()
{
  EEPROM.get(EEPROM_ADRESS_COUNT, count); // pega dado do centro gravado na eeprom
  na_beradinha = millis();                // armazena inicio do processo de centralizacao
  last = count < 0;                       // pega ultima direcao de giro do volante
  int p = POWER;                          // determina a potencia do servo motor

  // define limiar de aceitacao e permanece no loop por 3 segundos
  while ((count <= -5 || count >= 5) || millis() - na_beradinha < 3000)
  {
    Move(p, count < 0); // inverte direcao de giro do volante sempre que chega no zero
    delay(1);
    if ((count < 0) != last) // caso a direcao mude, atualiza direcao e diminui potencia do motor
    {
      last = !last;
      p -= 6;
    }
  }
}

void setup()
{
  Serial.begin(115200);
  r.begin(true);
  PCICR |= (1 << ROTARY_ENC_PCINT_AB_IE);
  PCMSK2 |= (1 << ROTARY_ENC_PCINT_A) | (1 << ROTARY_ENC_PCINT_B);

  // configura interrupcao do botao, caso o usuario queira usar o reset
  PCICR |= (1 << PCIE0);
  PCMSK0 |= (1 << PCINT1);

  DDRB |= (1 << GP_BUTTON_GND);
  DDRB &= ~((1 << GP_BUTTON) | (1 << POS_SENSOR));
  DDRB |= (1 << ATUA_CW) | (1 << ATUA_CCW) | (1 << ATUA_STRENGTH);

  PORTB |= (1 << POS_SENSOR);
  PORTB &= ~(1 << GP_BUTTON_GND);
  PORTB |= (1 << GP_BUTTON);
  PORTB &= ~(1 << ATUA_CW);
  PORTB &= ~(1 << ATUA_CCW);

  initPWM();
  sei();

  Idle();

  EEPROM.get(EEPROM_ADRESS_STATE, eeprom_state); // verifica se eeprom foi gravada ou nao
  // se nao foi gravada
  if (eeprom_state != 1)                         
  {
    // espera o usuario pressionar o botao quando o volante estiver na posicao desejada
    PCICR &= ~(1 << PCIE0);
    PCMSK0 &= ~(1 << PCINT1);
    while (1)
    {
      char read_button = PINB & (1 << GP_BUTTON);

      // verifica se houve mudança de estado nos botões
      // e quando foi a última mudança (debounce)
      if (read_button != last_read && (millis() - click_button) > 10)
      {
        click_button = millis();
        if (read_button == 0)
        {
          count = 0;
          EEPROM.put(EEPROM_ADRESS_STATE, 1); // atualiza estado da eeprom
          break;
        }
        last_read = read_button;
      }
    }
    PCICR |= (1 << PCIE0);
    PCMSK0 |= (1 << PCINT1);

    // encontra chave
    Move(POWER, false);
    while (absolute_sw == 1){}
    delay(10);
    while (absolute_sw == 0){}

    // grava deslocamento da chave na eeprom
    EEPROM.put(EEPROM_ADRESS_COUNT, count);
    Serial.print("COUNT: ");
    Serial.println(count);
  }

  Move(POWER, true);
  while (absolute_sw == 0){} // gira volante caso ele esteja na chave
  delay(10);
  while (absolute_sw == 1){} // gira volante ate que ele encontre a chave novamente

  // chama funcao de centralizacao
  beradinha();

  // para o pwm
  Move(0);
}

void loop()
{
  //  debug only info
  if (millis() % 300 == 0)
  {
    Serial.print(count);
    Serial.print(", ");
    Serial.println(absolute_sw == true ? '1' : '0');
  }
}

ISR(PCINT2_vect)
{
  unsigned char result = r.process();
  if (result == DIR_NONE)
  {
    // do nothing
  }
  else if (result == DIR_CW)
  {
    count--;
  }
  else if (result == DIR_CCW)
  {
    count++;
  }

  absolute_sw = 0 == (PINB & (1 << POS_SENSOR));
}

ISR(PCINT0_vect)
{
  if (millis() - last_click > 5000)
  {
    if (0 != (PINB & (1 << PB1)))
    {
      EEPROM.put(EEPROM_ADRESS_STATE, 0); // reset, diz que eeprom nao esta gravada
      setup();
    }
  }

  last_click = millis();
}