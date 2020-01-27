unsigned int frekuensi = 1000;
unsigned long ticks = 0;
void setup() {
    pinMode(PB13, OUTPUT); // STEP input
    pinMode(PB4, OUTPUT); // DIR input
    pinMode(PB5, OUTPUT); // ENABLE input
    digitalWrite(PB4,HIGH);
}

void loop() {
    digitalWrite(PB13,HIGH);
    delay(1);
    digitalWrite(PB13,LOW);
    delay(1);
}
