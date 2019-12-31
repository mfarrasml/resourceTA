void setup() {
Serial.begin(9600);
}

void loop() {
  if (Serial.available() > 0) {
    int incoming = Serial.read();
    Serial.print("character recieved: ");
    Serial.print(incoming, DEC);
  }
}

/*#include<SoftwareSerial.h>

//Define PINs
SoftwareSerial RpiSerial(10,11);

//variables
char incomingBytes;
String data;

void setup() {
  // put your setup code here, to run once:
  RpiSerial.begin(9600);
  Serial.begin(9600);
}

void loop() {
  // put your main code here, to run repeatedly:
  data = "";
  if (RpiSerial.available()>0){
    incomingBytes = RpiSerial.read();
    data += RpiSerial;
  }
  Serial.print(data);
} */
