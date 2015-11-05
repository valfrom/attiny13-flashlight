#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <util/delay.h>
#include <avr/sleep.h>

#define LED PB4
#define BUTTON PB3

#define BODS 7                   //BOD Sleep bit in MCUCR
#define BODSE 2                  //BOD Sleep enable bit in MCUCR

#define MAX_LED_LEVEL 255
#define MIN_LED_LEVEL 200

#define TURN_LED_ON() PORTB |= (1<<LED)
#define TURN_LED_OFF() PORTB &= ~(1<<LED);

#define IS_BUTTON_ON() !(PINB & (1<<BUTTON))

#define BLINK() TURN_LED_OFF();_delay_ms(100);TURN_LED_ON();_delay_ms(5);TURN_LED_OFF();

#define REGULATOR_DELAY 100


uint8_t volatile ledLevel = 0;
uint8_t volatile interrupt = false;
uint8_t volatile turnedOff = false;

uint8_t buttonPressed = false;
int8_t buttonDirection = -1;
uint8_t ignoreInterrupt = false;
uint16_t tick = 0;


void setLedLevel(uint8_t level);
void powerOff();
void powerOn();
void saveSettings();

// interrupt service routine
ISR(PCINT0_vect) {
    cli();
    interrupt = true;
}

void inline powerOn() {
    turnedOff = false;
    sei();
}

void inline powerOff() { 
    TURN_LED_OFF();
    turnedOff = true;
    saveSettings();
    // do a complete power-down
    set_sleep_mode(SLEEP_MODE_PWR_DOWN); 
    // enable sleep mode
    sleep_enable();
    // allow interrupts to end sleep mode
    sei();
    // go to sleep
    sleep_cpu();
    // disable sleep mode for safety
    sleep_disable();
}


void loadSettings() {
    ledLevel = eeprom_read_byte((uint8_t*)0);
    if(ledLevel == 0) {
        ledLevel = MAX_LED_LEVEL;
    }
}

void saveSettings() {
    eeprom_update_byte((uint8_t*)0, ledLevel);
}

void setup() {    
    // Set led pin to output
    DDRB |= (1 << LED);

    // Set button pin to input
    DDRB &= ~(1 << BUTTON);

    // Turn off ADC
    ADCSRA &= ~(1<<ADEN);
    // Disable the analog comparator
    ACSR |= _BV(ACD);
    // Turn off the brown-out detector
    MCUCR |= _BV(BODS) | _BV(BODSE); 

    // Pin change mask: listen to portb bit 3
    PCMSK |= (1<<PCINT3);   
    // Enable PCINT interrupt 
    GIMSK |= (1<<PCIE); 
    // Enable interrupts
    sei();

    loadSettings();

    powerOff();
}

void loop() {
    if(interrupt) {
        if(!ignoreInterrupt) {
            for(int i=0;i<100;i++) {    
                _delay_ms(5);
                if(!IS_BUTTON_ON()) {
                    break;
                }
            }
            if(!IS_BUTTON_ON()) {
                if(turnedOff) {
                    powerOn();
                } else {
                    powerOff();
                }
            } else {
                ignoreInterrupt = true;
            }
        } else {
            ignoreInterrupt = false;
        }
    }

    interrupt = false;    

    if(turnedOff) {
        powerOff();
        return;
    }

    if(tick % REGULATOR_DELAY == 0) {
        if(IS_BUTTON_ON()) {
            if(buttonDirection > 0) {
                if(ledLevel < MAX_LED_LEVEL) {
                    ledLevel += 1;
                    if(ledLevel == MAX_LED_LEVEL) {
                        BLINK();
                    }
                }
            } else {
                if(ledLevel > MIN_LED_LEVEL) {
                    ledLevel -= 1;
                    if(ledLevel == MIN_LED_LEVEL) {
                        BLINK();
                    }
                }
            }

            buttonPressed = true;
        } else if(buttonPressed) {
            buttonDirection = -buttonDirection;
            buttonPressed = false;
        }
    }
    
    TURN_LED_ON();
    _delay_us(1);    
    tick ++;
    TURN_LED_OFF();
    for(int i = 0; i < MAX_LED_LEVEL - ledLevel; i++) {
        _delay_us(1);
        tick ++;
    }
    _delay_us(1);
    tick ++;
}

int main(void) {
    _delay_ms(1000);
    setup();
    while(true) {
        loop();
    }
}