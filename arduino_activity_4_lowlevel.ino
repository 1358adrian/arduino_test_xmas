#include <avr/io.h>
#include <avr/interrupt.h>

// State machine enumeration
volatile enum State { 
    IDLE,           // System waiting state
    LED_SEQUENCE,   // Active LED sequencing
    SERVO_FORWARD,  // Servo moving 0->180 degrees
    BLINK_LEDS,     // All LEDs blinking phase
    SERVO_BACKWARD  // Servo returning 180->0 degrees
} state = IDLE;

// State control variables
volatile uint8_t led_step = 0;     // Current LED in sequence (0-3)
volatile uint8_t servo_pos = 0;    // Servo position (0-255 values are accepted)
volatile uint8_t blink_count = 0;  // Completed blink cycles
volatile uint8_t blink_phase = 0;  // Blink on/off state

// Constant servo values by trial and error
const uint8_t servo_min_value = 25;
const uint8_t servo_max_value = 183;

void setup() {
    // ========== LED CONFIGURATION (PB0-PB3 = D8-D11) ==========
    DDRB |= 0x0F;     // Set PB0-PB3 as outputs
    PORTB &= ~0x0F;   // Turn off all LEDs initially

    // ========== SERVO CONFIGURATION (PD6 = D6 = OC0A) ==========
    DDRD |= (1 << DDD6);  // Set PD6 as output (servo control)
    // Configure Timer0 for Fast PWM mode (8-bit)
    TCCR0A = (1 << COM0A1) |  // Clear OC0A (Pin 6) on compare match (PWM: sets LOW)
             (1 << WGM01) | (1 << WGM00);  // Fast PWM mode
    TCCR0B = (1 << CS02);      // Prescaler 256 (244.14Hz PWM)
					                     // Computation: ((255+1)*256/16,000,000)^-1 = 244.14Hz
    OCR0A = servo_min_value;   // Initial duty cycle (servo_min_value/255 * 100)%

    // ========== BUTTON CONFIGURATION (PD2 = D2 = INT0) ==========
    DDRD &= ~(1 << DDD2);      // Set PD2 as input
    PORTD |= (1 << PORTD2);    // Enable internal pull-up
    EICRA |= (1 << ISC01);     // Trigger on falling edge
    EIMSK |= (1 << INT0);      // Enable INT0 interrupt

    sei();  // Enable global interrupts; you don't need to manually call `sei()`
            // as this sketch will typically have global interrupts enabled
            // by default by the time setup() runs. You can just comment out that
            // `sei()` line. It's `interrupts()` in Arduino-style coding basically.
}

// ========== EXTERNAL INTERRUPT 0 (BUTTON PRESS) ==========
ISR(INT0_vect) {
    if (state == IDLE) {
        state = LED_SEQUENCE;  // Start sequence
        led_step = 0;          // Reset LED position
        PORTB = (1 << 0);      // Turn on first LED
        
        // Configure Timer1 for 250ms intervals
        TCCR1A = 0;                      // Normal mode
        TCCR1B = (1 << WGM12) |          // CTC mode
                 (1 << CS12);            // Prescaler 256
        TCNT1 = 0;                       // Reset counter
        OCR1A = 15624;                   // 250ms compare value for four LEDs that
                                         // light one by one in LED_SEQUENCE enum
                                         // Computation:
                                         // (15624+1)*256/16,000,000 = 0.25 seconds
        TIMSK1 |= (1 << OCIE1A);         // Enable compare match interrupt
    }
}

// ========== TIMER1 COMPARE MATCH INTERRUPT ==========
ISR(TIMER1_COMPA_vect) {
    switch(state) {
        case LED_SEQUENCE:
            if (led_step < 4) {
                // Turn off previous LED
                if (led_step > 0) PORTB &= ~(1 << (led_step-1));
                // Turn on next LED
                PORTB |= (1 << led_step);
                led_step++;
            } else {
                // Sequence complete
                PORTB &= ~0x0F;  // Turn off all LEDs
                state = SERVO_FORWARD;
                servo_pos = servo_min_value;
                OCR0A = servo_min_value;  // Reset servo position
                
                // Reconfigure timer for 7.8ms intervals (128Hz)
                TCCR1B = (1 << WGM12) |          // CTC mode
                         (1 << CS11) | (1 << CS10); // Prescaler 64
                TCNT1 = 0;       // Reset counter
                OCR1A = 3163;     // ~12.7ms intervals * (servo_max_value - servo_min_value) = 2secs;
                                  // servo movement speed controlled by Timer1:
                                  // the higher the value, the slower the movement;
                                  // servo position is controlled by Timer0 (8-bit,
                                  // OCR0A in Fast PWM Mode, Prescaler 256,
                                  // Clear OC0A (Pin 6) on compare match (PWM: sets LOW)
            }
            break;

        case SERVO_FORWARD:
            if (++servo_pos >= servo_max_value) {  // Full 180° position
                state = BLINK_LEDS;
                // Configure timer for 300ms intervals
                TCCR1B = (1 << WGM12) |          // CTC mode
                         (1 << CS11) | (1 << CS10); // Prescaler 64
                TCNT1 = 0;
                OCR1A = 37499;         // 300ms duration for all four LEDs that
                                       // blink thrice in BLINK_LEDS enum
                                       // (150ms compare value for 3 LEDs being on, and
                                       // 150ms compare value for 3 LEDs being off)
                blink_count = 0;
                blink_phase = 0;
                PORTB |= 0x0F;         // Turn on all LEDs
            }
            OCR0A = servo_pos;  // Update servo position
            break;

        case BLINK_LEDS:
            if (blink_phase ^= 1) {  // Toggle phase
                PORTB &= ~0x0F;      // LEDs off
            } else {
                if (++blink_count >= 3) {  // Complete 3 blinks
                    state = SERVO_BACKWARD;
                    servo_pos = servo_max_value;       // Start from 180°
                    OCR0A = servo_max_value;
                    
                    // Reconfigure timer for 7.8ms intervals
                    TCCR1B = (1 << WGM12) | 
                             (1 << CS11) | (1 << CS10);
                    TCNT1 = 0;
                    OCR1A = 3163;  // again, ~12.7ms intervals * (servo_max_value - servo_min_value) = 2secs;
                                   // the higher the value, the slower the movement
                    break;
                }
                PORTB |= 0x0F;  // LEDs on
            }
            break;

        case SERVO_BACKWARD:
            if (--servo_pos == servo_min_value) {  // Returned to 0°
                state = IDLE;
                TIMSK1 &= ~(1 << OCIE1A);  // Disable timer interrupt
            }
            OCR0A = servo_pos;  // Update servo position
            break;
    }
}

void loop() {}  // All logic handled via interrupts
