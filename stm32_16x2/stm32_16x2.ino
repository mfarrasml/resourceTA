#include<Wire.h>
#include"LiquidCrystal_I2C.h"
#include<Keypad.h>

//PINS
#define PUMP1 PA12
#define PUMP2 PA11
#define PUMP3 PA10
#define DIR0 PA9
#define DIR1 PA8

//Variables
#define LCD_ADDRESS 0x27

const byte ROWS = 4; //jumlah baris keypad matrix
const byte COLS = 4; //jumlah kolom keypad matrix

int seq = 0; //jumlah sekuens pengaliran
int i = 0;

int state = 0; //state sistem
int state_input = 0; //sub-state saat sedang menginput; 0 = input pompa, 1 = input arah, 2 = input flowrate, 3 = input durasi/volume
int state_info = 0; //sub-state interface informasi pengaliran
int state_durationvolume = 0; //0 jika parameter yang dipilih durasi, 1 jika dipilih volume
int state_input_final = 0; //sub-state pilihan pada interface input setelah mengisi semua parameter; 0 = hapus sekuens, 1 = tambah sekuens, 2 = mulai sekuens

unsigned long startTime = 0; //waktu dimulainya sequence
int timeLeft = 0; //waktu sisa sequence
int timeLeftLast = 0;

// Parameter aliran fluida
struct sequenceParameters {
  int pump[20]; //pump 1, 2, or 3
  int flowRate[20];
  int flowDir[20]; //0 = memompa/mendorong, 1 = menghisap
  unsigned long duration[20];
  int volume[20];
};

// variabel-variabel untuk menyimpan parameter sementara
int pumpstr = 1;
String flowRatestr = "";
int flowDirstr = 0;
String durationstr = "";
String volumestr = "";

//variabel yang menyimpan variabel dari keseluruhan sekuens
sequenceParameters sequenceData;

// Men-set karakter yang diterima ketika keypad ditekan user
char keypadKeys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

//karakter custom untuk panah kiri
byte leftArrow[] = {
  B00000,
  B00001,
  B00011,
  B00111,
  B00011,
  B00001,
  B00000,
  B00000
};

//karakter custom untuk panah kanan
byte rightArrow[] = {
  B00000,
  B10000,
  B11000,
  B11100,
  B11000,
  B10000,
  B00000,
  B00000
};

byte rowPins[ROWS] = {PA0, PA1, PA2, PA3};
byte colPins[COLS] = {PA4, PA5, PA6, PA7};

Keypad customKeypad = Keypad(makeKeymap(keypadKeys), rowPins, colPins, ROWS, COLS);

LiquidCrystal_I2C lcd(LCD_ADDRESS, 16, 2);

void setup() {
  Serial.begin(9600);
  //inisialisasi LCD
  lcd.init();
  lcd.backlight();

  //Set karakter custom untuk lcd
  lcd.createChar(0, leftArrow);
  lcd.createChar(1, rightArrow);

  //inisialisasi pin
  pinMode(PUMP1, OUTPUT);
  pinMode(PUMP2, OUTPUT);
  pinMode(PUMP3, OUTPUT);
  pinMode(DIR0, OUTPUT);
  pinMode(DIR1, OUTPUT);

  state = 1;
  state_input = 0;
  stateInput();
}

void loop() {
  const char customKey = customKeypad.getKey();
  char buffer[20];
  if (state == 1) {
    if (customKey) {
      if (customKey == '#') { // tombol enter
        switch (state_input) {
          case 0:
            state_input = 1;
            stateInput();
            break;
          case 1:
            state_input = 2;
            stateInput();
            break;
          case 2:
            state_input = 3;
            stateInput();
            break;
          case 3:
            state_input = 5;
            if (durationstr != "") {
              saveParameter();
              stateInput();
            }
            break;
          case 4:
            state_input = 5;
            if (volumestr != "") {
              saveParameter();
              stateInput();
            }
            break;
          case 5:
            if (state_input_final == 0) {
              //hapus data sequence ini
              if (seq > 0) {
                seq--;
                stateInput();
              }
            }
            else if (state_input_final == 1) {
              //tambah sequence baru
              state_input = 0;
              stateInput();
            }
            else if (state_input_final == 2) {
              if (seq > 0) {
                i = 0;
                state = 2;
                informationInterface();
              }
            }
            break;
          default:
            break;
        }
      }
      else if (customKey == '*') { // tombol hapus
        deleteButton();
      }
      else if (customKey == 'A') { // tombol panah atas

      }
      else if (customKey == 'B') { // tombol panah bawah

      }
      else if (customKey == 'C') { // tombol panah kanan
        switch (state_input) {
          case 0:
            if (pumpstr < 3) {
              pumpstr++;
            }
            else {
              pumpstr = 1;
            }
            stateInputPump();
            break;
          case 1:
            if (flowDirstr < 1) {
              flowDirstr ++;
            }
            else {
              flowDirstr = 0;
            }
            stateInputDir();
            break;
          case 3:
            state_input = 4;
            stateInput();
            break;
          case 4:
            state_input = 3;
            stateInput();
            break;
          case 5:
            if (state_input_final < 2) {
              state_input_final++;
            }
            else {
              state_input_final = 0;
            }
            stateInputFinal();
            break;
          default:
            break;
        }
      }
      else if (customKey == 'D') { // tombol panah kiri
        switch (state_input) {
          case 0:
            if (pumpstr > 1) {
              pumpstr--;
            }
            else {
              pumpstr = 3;
            }
            stateInputPump();
            break;
          case 1:
            if (flowDirstr > 0) {
              flowDirstr --;
            }
            else {
              flowDirstr = 1;
            }
            stateInputDir();
            break;
          case 3:
            state_input = 4;
            stateInput();
            break;
          case 4:
            state_input = 3;
            stateInput();
            break;
          case 5:
            if (state_input_final > 0) {
              state_input_final--;
            }
            else {
              state_input_final = 2;
            }
            stateInputFinal();
            break;
          default:
            break;
        }
      }
      else {
        switch (state_input) {
          case 2:
            if (flowRatestr.length() < 5) {
              if (!((flowRatestr.length() == 0) && (customKey == '0'))) {
                flowRatestr += customKey;
                updateParameter();
              }
            }
            break;
          case 3:
            if (durationstr.length() < 5) {
              if (!((durationstr.length() == 0) && (customKey == '0'))) {
                durationstr += customKey;
                updateParameter();
              }
            }

            break;
          case 4:
            if (volumestr.length() < 5) {
              if (!((volumestr.length() == 0) && (customKey == '0'))) {
                volumestr += customKey;
                updateParameter();
              }
            }
            break;
        }
      }
    }
  }
  else if (state == 2) {
    //Menjalankan sequence pompa
    // sinyal untuk menandakan pompa yang berjalan //
    // sinyal untuk menandakan arah jalan pompa //
    if ((customKey == 'C') || (customKey == 'D')) {
      if (state_info == 0) {
        state_info = 1;
        lcd.setCursor(3, 0);
        sprintf(buffer, "Sequence %d", i + 1);
        lcd.print(buffer);
        lcd.setCursor(0, 1);
        lcd.write(0);
        lcd.print("TIME:       s ");
        lcd.write(1);
        lcd.print(" ");
        lcd.setCursor(7, 1);
        lcd.print(timeLeft);
      }
      else if (state_info == 1) {
        state_info = 0;
        lcd.setCursor(3, 0);
        sprintf(buffer, "Sequence %d", i + 1);
        lcd.print(buffer);
        lcd.setCursor(0, 1);
        lcd.print("FLOW:       uL/m");
        lcd.setCursor(6, 1);
        lcd.print(sequenceData.flowRate[i]);
      }
    }
    if (timeLeft >= 0) { // count sisa waktu pengaliran
      timeLeft = (sequenceData.duration[i] -  (millis() - startTime)) / 1000;
      //update durasi sisa
      if (timeLeft != timeLeftLast) {
        if (state_info == 1) {
          lcd.setCursor(7, 1);
          lcd.print("      s ");
          lcd.setCursor(7, 1);
          lcd.print(timeLeft);
        }
        timeLeftLast = timeLeft;
      }
    }
    else { // durasi pengaliran selesai
      i++;
      if (i >= seq) { //kembali ke interface input sekuens
        seq = 0;
        i = 0;
        state = 1;
        state_input = 0;
        turnOff();
        stateInput();
      }
      else {
        informationInterface();
      }
    }
  }
}


void stateInput() {
  char buffer[20];
  lcd.clear();

  switch (state_input) {
    case 0:
      lcd.cursor();
      lcd.blink_on();
      lcd.setCursor(3, 0);
      sprintf(buffer, "Sequence %d", seq + 1);
      lcd.print(buffer);
      lcd.setCursor(0, 1);
      lcd.print(" PUMP: ");
      lcd.write(1);
      lcd.print("1  2  3");
      lcd.setCursor(8, 1);
      break;
    case 1:
      lcd.cursor();
      lcd.blink_on();
      lcd.setCursor(3, 0);
      sprintf(buffer, "Sequence %d", seq + 1);
      lcd.print(buffer);
      lcd.setCursor(0, 1);
      lcd.print(" DIR : ");
      lcd.write(1);
      lcd.print("P  S");
      lcd.setCursor(8, 1);
      break;
    case 2:
      lcd.cursor();
      lcd.blink_on();
      lcd.setCursor(3, 0);
      sprintf(buffer, "Sequence %d", seq + 1);
      lcd.print(buffer);
      lcd.setCursor(0, 1);
      lcd.print("FLOW: 0     uL/m");
      lcd.setCursor(6, 1);
      break;
    case 3:
      state_durationvolume = 0;
      lcd.cursor();
      lcd.blink_on();
      lcd.setCursor(3, 0);
      sprintf(buffer, "Sequence %d", seq + 1);
      lcd.print(buffer);
      lcd.setCursor(0, 1);
      lcd.write(0);
      lcd.print("TIME: 0     s ");
      lcd.write(1);
      lcd.setCursor(7, 1);
      break;
    case 4:
      state_durationvolume = 1;
      lcd.cursor();
      lcd.blink_on();
      lcd.setCursor(3, 0);
      sprintf(buffer, "Sequence %d", seq + 1);
      lcd.print(buffer);
      lcd.setCursor(0, 1);
      lcd.write(0);
      lcd.print("VOL : 0     uL");
      lcd.write(1);
      lcd.setCursor(7, 1);
      break;
    case 5:
      state_input_final = 0;
      lcd.noCursor();
      lcd.blink_off();
      lcd.setCursor(3, 0);
      sprintf(buffer, "Sequence %d", seq); lcd.print(buffer);
      lcd.setCursor(0, 1);
      lcd.print(" DEL  ADD  STRT");
      lcd.setCursor(0, 1);
      lcd.write(1);
      break;
    default:
      break;
  }
}

//pergantian state pompa yang dipilih (pompa 1,  3, atau 3)
void stateInputPump() {
  lcd.setCursor(7, 1);
  lcd.print(" 1  2  3");

  switch (pumpstr) {
    case 1:
      lcd.setCursor(7, 1);
      break;
    case 2:
      lcd.setCursor(10, 1);
      break;
    case 3:
      lcd.setCursor(13, 1);
      break;
    default:
      break;
  }
  lcd.write(1);
}

// pergantian state arah laju yang dipilih (dorong atau hisap)
void stateInputDir() {
  lcd.setCursor(7, 1);
  lcd.print(" P  S");

  switch (flowDirstr) {
    case 0:
      lcd.setCursor(7, 1);
      break;
    case 1:
      lcd.setCursor(10, 1);
      break;
    default:
      break;
  }
  lcd.write(1);
}

void stateInputFinal() {
  lcd.setCursor(0, 1);
  lcd.print(" DEL  ADD  STRT");

  switch (state_input_final) {
    case 0:
      lcd.setCursor(0, 1);
      break;
    case 1:
      lcd.setCursor(5, 1);
      break;
    case 2:
      lcd.setCursor(10, 1);
      break;
    default:
      break;
  }
  lcd.write(1);
}


//Setting awal interface informasi
void informationInterface() {
  char buffer[20];
  switch (state_info) {
    case 0:
      lcd.setCursor(3, 0);
      sprintf(buffer, "Sequence %d", i + 1);
      lcd.print(buffer);
      lcd.setCursor(0, 1);
      lcd.print("FLOW:       uL/m");
      lcd.setCursor(6, 1);
      lcd.print(sequenceData.flowRate[i]);
      break;
    case 1:
      lcd.setCursor(3, 0);
      sprintf(buffer, "Sequence %d", i + 1);
      lcd.print(buffer);
      lcd.setCursor(0, 1);
      lcd.write(0);
      lcd.print("TIME:       s ");
      lcd.write(1);
      lcd.setCursor(7, 1);
      lcd.print(sequenceData.duration[i]/1000);
      break;
  }

  // menyalakan LED yang menunjukkan pompa yang sedang berjalan
  switch (sequenceData.pump[i]) {
    case 1:
      digitalWrite(PUMP1, HIGH);
      digitalWrite(PUMP2, LOW);
      digitalWrite(PUMP3, LOW);
      break;
    case 2:
      digitalWrite(PUMP1, LOW);
      digitalWrite(PUMP2, HIGH);
      digitalWrite(PUMP3, LOW);
      break;
    case 3:
      digitalWrite(PUMP1, LOW);
      digitalWrite(PUMP2, LOW);
      digitalWrite(PUMP3, HIGH);
      break;
  }

  //menyalakan LED yang menunjukkan arah aliran fluida
  switch (sequenceData.flowDir[i]) {
    case 0:
      digitalWrite(DIR0, HIGH);
      digitalWrite(DIR1, LOW);
      break;
    case 1:
      digitalWrite(DIR0, LOW);
      digitalWrite(DIR1, HIGH);
      break;
  }


  delay(1000);

  startTime = millis();
  timeLeft = sequenceData.duration[i] / 1000;
  timeLeftLast = timeLeft;
}

// menyelesaikan sekuens dan mematikan seluruh output sekuens
void turnOff() {
  digitalWrite(PUMP1, LOW);
  digitalWrite(PUMP2, LOW);
  digitalWrite(PUMP3, LOW);
  digitalWrite(DIR0, LOW);
  digitalWrite(DIR1, LOW);


}

//mengatur perubahan antara state pemilihan parameter atau durasi
void timeOrVolumeSelect() {
  if (state_durationvolume == 0) {
    lcd.setCursor(1, 3);
    lcd.print("TIME :       s ");
    lcd.setCursor(8, 3);
    lcd.print(durationstr);
  }
  else { //(state_durationvolume == 1)
    lcd.setCursor(1, 3);
    lcd.print("VOL  :       uL");
    lcd.setCursor(8, 3);
    lcd.print(volumestr);
  }
}


//update nilai parameter input
void updateParameter() {
  switch (state_input) {
    case 2:
      lcd.setCursor(6, 1);
      if (flowRatestr == "") {
        lcd.print('0');
        lcd.setCursor(6, 1);
      }
      else {
        lcd.print(flowRatestr);
      }
      break;
    case 3:
      lcd.setCursor(7, 1);
      if (durationstr == "") {
        lcd.print('0');
        lcd.setCursor(7, 1);
      }
      else {
        lcd.print(durationstr);
      }

      break;
    case 4:
      lcd.setCursor(7, 1);
      if (volumestr == "") {
        lcd.print('0');
        lcd.setCursor(7, 1);
      }
      else {
        lcd.print(volumestr);
      }
  }
}

// menuliskan input user dan menyimpannya pada variabel yang sesuai
void parameterInput(char c) {
  switch (state_input) {
    case 0:
      if ((c == '1') || (c == '2') || (c == '3')) {
        pumpstr = c - '0';
        lcd.print(c);
      }
      break;
    case 2:
      if (flowRatestr.length() < 5) {
        if (!((flowRatestr.length() == 0) && (c == '0'))) {
          flowRatestr += c;
          lcd.print(c);
        }
      }
      break;
    case 3:
      if (state_durationvolume == 0) {
        if (durationstr.length() < 5) {
          if (!((durationstr.length() == 0) && (c == '0'))) {
            durationstr += c;
          }
          lcd.print(c);
        }
      }
      else {
        if (volumestr.length() < 5) {
          if (!((volumestr.length() == 0) && (c == '0'))) {
            volumestr += c;
          }
          lcd.print(c);
        }
      }
      break;
  }
}

// menghapus 1 karakter akhir parameter yang dipilih NOTE:BELOM DIUBAH
void deleteButton() {
  int n;
  switch (state_input) {
    case 2:
      n = flowRatestr.length();
      if (n > 0) {
        lcd.setCursor(6 + n - 1, 1);
        lcd.print(' ');
        lcd.setCursor(6 + n - 1, 1);
        flowRatestr.remove(n - 1);
        updateParameter();
      }
      break;
    case 3:
      n = durationstr.length();
      if (n > 0) {
        lcd.setCursor(7 + n - 1, 3);
        lcd.print(' ');
        lcd.setCursor(7 + n - 1, 3);
        durationstr.remove(n - 1);
        updateParameter();
      }
    case 4:
      n = volumestr.length();
      if (n > 0) {
        lcd.setCursor(7 + n - 1, 3);
        lcd.print(' ');
        lcd.setCursor(7 + n - 1, 3);
        volumestr.remove(n - 1);
        updateParameter();
      }
      break;
  }
}

// berjalan saat tombol panah kanan (C) ditekan
void rightButton() {
  switch (state_input ) {
    case 0:
      if (pumpstr < 3) {
        pumpstr ++;
      }
      else {
        pumpstr = 1;
      }
      lcd.print(pumpstr);
      stateInput();
      break;
    case 1:
      if (flowDirstr < 1) {
        flowDirstr ++;
        lcd.print('S');
      }
      else {
        flowDirstr = 0;
        lcd.print('P');
      }
      stateInput();
      break;
    case 3:
      if (state_durationvolume == 0) {
        state_durationvolume = 1;
      }
      else { // state_durationvolume = 1;
        state_durationvolume = 0;
      }
      timeOrVolumeSelect();
      stateInput();
      break;
    default:
      break;
  }
}

// berjalan saat tombol panah kiri (D) ditekan
void leftButton() {
  switch (state_input) {
    case 0:
      if (pumpstr > 1) {
        pumpstr --;
      }
      else {
        pumpstr = 3;
      }
      lcd.print(pumpstr);
      stateInput();
      break;
    case 1:
      if (flowDirstr > 0) {
        flowDirstr --;
        lcd.print('P');
      }
      else {
        flowDirstr = 1;
        lcd.print('S');
      }
      stateInput();
      break;
    case 3:
      if (state_durationvolume == 0) {
        state_durationvolume = 1;
      }
      else { // state_durationvolume = 1;
        state_durationvolume = 0;
      }
      timeOrVolumeSelect();
      stateInput();
      break;
    default:
      break;
  }
}

// menyimpan semua parameter untuk sekuens ke-seq
void saveParameter() {
  sequenceData.pump[seq] = pumpstr;
  if (flowRatestr == "") {
    sequenceData.flowRate[seq] = 0;
  }
  else {
    sequenceData.flowRate[seq] = flowRatestr.toInt();
  }
  sequenceData.flowDir[seq] = flowDirstr;
  if (state_durationvolume == 0) {
    sequenceData.duration[seq] = durationstr.toInt() * 1000;
  }
  else {
    sequenceData.duration[seq] = volumestr.toInt() * 60 * 1000 / (sequenceData.flowRate[seq]);
  }
  //sequenceData.volume[seq] = volumestr.toInt();
  seq++;

  pumpstr = 1;
  flowRatestr = "";
  flowDirstr = 0;
  durationstr = "";
  volumestr = "";
}

char getDirection(int dir) {
  char dirChar;
  if (dir == 0) {
    dirChar = 'P';
  }
  else {
    dirChar = 'S';
  }
  return dirChar;
}
