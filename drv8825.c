#include <avr/io.h>
#include <util/delay.h>

const int stepsPerRevolution = 200;

int main(void) {
    DDRD = 0xFF;
    DDRB = 0xFF;
    PORTB = (1 << PB0) | (0 << PB1) | (1 << PB2);

    uint32_t frekuensi_step = 2; // dalam Hz

    while (1) {
        PORTD &= ~(1 << PD7);
        // Spin motor slowly
        for(int x = 0; x < stepsPerRevolution; x++)
        {
            PORTD |= (1 << PD6);
            _delay_ms((1000/frekuensi_step)/2);
            PORTD &= (0 << PD6);
            _delay_ms((1000/frekuensi_step)/2);
        }
        
        // // Set motor direction counterclockwise
        // digitalWrite(dirPin, LOW);

        // // Spin motor quickly
        // for(int x = 0; x < stepsPerRevolution; x++)
        // {
        //     digitalWrite(stepPin, HIGH);
        //     delayMicroseconds(1000);
        //     digitalWrite(stepPin, LOW);
        //     delayMicroseconds(1000);
        // }
        // delay(1000); // Wait a second
    }
}
