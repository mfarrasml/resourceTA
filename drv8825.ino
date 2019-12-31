unsigned int frekuensi = 300;
unsigned long ticks = 0;
void setup() {
    pinMode(PB8, OUTPUT); // STEP input
    pinMode(PB9, OUTPUT); // DIR input
    pinMode(PB7, OUTPUT); // ENABLE input
    digitalWrite(PB9,HIGH);
}

void loop() {
    digitalWrite(PB8,HIGH);
    if (frekuensi > 500) {
        delayMicroseconds(1000000/frekuensi/2);
    }
    else delay(1000000/frekuensi/2);
    digitalWrite(PB8,LOW);
    if (frekuensi > 500) {
        delayMicroseconds(1000000/frekuensi/2);
    }
    else delay(1000000/frekuensi/2);
    ticks++;
    if (ticks > 1000) {
        digitalWrite(PB7, HIGH);
    }
}