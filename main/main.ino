#include<Wire.h>
#include<LiquidCrystal_I2C.h>
#include<Keypad.h>


//Variables
#define LCD_ADDRESS 0x27

const byte ROWS = 4; //jumlah baris keypad matrix
const byte COLS = 4; //jumlah kolom keypad matrix
int cursorRow = 0; // baris yang sedang dipilih pada LCD
int cursorCol = 0; // kolom yang sedang dipilih pada LCD

int seq = 0; //jumlah sekuens pengaliran
int i = 0;

int state = 0; //state sistem
int state_input = 0; //sub-state saat sedang menginput; 0 = input pompa, 1 = input arah, 2 = input flowrate, 3 = input durasi/volume
int state_durationvolume = 0; //0 jika parameter yang dipilih durasi, 1 jika dipilih volume

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

byte rightArrowFull[] = {
  B00000,
  B00100,
  B11110,
  B11111,
  B11110,
  B00100,
  B00000,
  B00000
};

byte fullDot[] = {
  B01110,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B01110,
};

byte rowPins[ROWS] = {9, 8, 7, 6};
byte colPins[COLS] = {5, 4, 3, 2};

Keypad customKeypad = Keypad(makeKeymap(keypadKeys), rowPins, colPins, ROWS, COLS);

LiquidCrystal_I2C lcd(LCD_ADDRESS, 20, 4);

void setup() {
  Serial.begin(9600);
  //inisialisasi LCD
  lcd.init();
  lcd.backlight();

  //Set karakter custom untuk lcd
  lcd.createChar(0, leftArrow);
  lcd.createChar(1, rightArrow);
  lcd.createChar(2, rightArrowFull);
  lcd.createChar(3, fullDot);

  inputInterface();


}

void loop() {
  const char customKey = customKeypad.getKey();
  if (state == 1) {
    if (customKey) {
      if (customKey == '#') {
        if (state_input == 4) {
          saveParameter(); //simpan parameter sekuens
          inputInterface(); // me-refresh interface input untuk sekuns selanjutnya
        }
        else if (state_input == 5) {
          saveParameter();
          informationInterface();
        }
      }
      else if (customKey == '*') {
        deleteButton();
      }
      else if (customKey == 'A') {
        if (state_input > 0) {
          state_input--;
          stateInput();
        }
      }
      else if (customKey == 'B') {
        if (state_input < 5) {
          state_input ++;
          stateInput();
        }
      }
      else if (customKey == 'C') {
        if (state_input != 2) {
          rightButton();
        }
      }
      else if (customKey == 'D') {
        if (state_input != 2) {
          leftButton();
        }
      }
      else {
        if (state_input != 1) {
          parameterInput(customKey);
          stateInput();
        }
      }
    }
  }
  else if (state == 2) {
    informationInterface();
    //Menjalankan sequence pompa
    // sinyal untuk menandakan pompa yang berjalan //
    // sinyal untuk menandakan arah jalan pompa //

    lcd.setCursor(8, 1);
    lcd.print(sequenceData.pump[i]);
    lcd.setCursor(17, 1);
    lcd.print(getDirection(sequenceData.flowDir[i]));
    lcd.setCursor(8, 2);
    lcd.print(sequenceData.flowRate[i]);
    delay(500); //delay sejenak

    startTime = millis();
    timeLeftLast = sequenceData.duration[i] / 1000;
    timeLeft = timeLeftLast;

    while (timeLeft >= 0) {
      //Set sinyal step untuk mengatur flowrate pompa//

      timeLeft = (sequenceData.duration[i] -  (millis() - startTime)) / 1000;
      Serial.println(timeLeft);
      if (timeLeft != timeLeftLast) {
        lcd.setCursor(8, 3);
        lcd.print("      s ");
        lcd.setCursor(8, 3);
        lcd.print(timeLeft);
        timeLeftLast = timeLeft;

      }
    }
    i++;
    if (i >= seq) {
      seq = 0;
      i = 0;
      inputInterface();
    }
  }
}

// Setting awal interface input parameter
void inputInterface() {
  char buffer[20];

  lcd.clear();
  lcd.setCursor(0, 0);
  sprintf(buffer, "Sequence %d Parameter", seq + 1);
  lcd.print(buffer);
  lcd.setCursor(0, 1);
  lcd.print(" PUMP :");
  lcd.write(0);
  lcd.print("1");
  lcd.write(1);
  lcd.print(" DIR :");
  lcd.write(0);
  lcd.print("P");
  lcd.write(1);
  lcd.setCursor(0, 2);
  lcd.print(" FLOW : 0     uL/min");
  lcd.setCursor(0, 3);
  lcd.write(0);
  lcd.print("TIME");
  lcd.setCursor(6, 3);
  lcd.print(": 0     s ");
  lcd.write(1);
  lcd.setCursor(18, 3);
  lcd.write(2);
  lcd.write(3);

  lcd.setCursor (8, 1);
  lcd.blink_on();
  lcd.cursor();
  state = 1;
  state_input = 0;
  state_durationvolume = 0;
}

//Setting awal interface informasi
void informationInterface() {
  char buffer[20];

  lcd.clear();
  lcd.noCursor();
  lcd.blink_off();
  lcd.setCursor(4, 0);
  sprintf(buffer, "Sequence %d", i + 1);
  lcd.print(buffer);
  lcd.setCursor(0, 1);
  lcd.print(" PUMP :    DIR :");
  lcd.setCursor(0, 2);
  lcd.print(" FLOW : 0     uL/min");
  lcd.setCursor(8, 2);
  lcd.print(sequenceData.flowRate[i]);
  lcd.setCursor(0, 3);
  lcd.print(" TIME : 0     s ");
  lcd.setCursor(8, 3);
  lcd.print(sequenceData.duration[i] / 1000);
  state = 2;
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

//Mengatur penempatan interface saat sub-state input berganti
void stateInput() {
  int n = 0;
  switch (state_input) {
    case 0:
      lcd.setCursor(8, 1);
      break;
    case 1:
      lcd.setCursor(17, 1);
      break;
    case 2:
      n = flowRatestr.length();
      lcd.setCursor(8 + n, 2);
      break;
    case 3:
      if (state_durationvolume == 0) {
        n = durationstr.length();
        lcd.setCursor(8 + n, 3);
      }
      else {
        n = volumestr.length();
        lcd.setCursor(8 + n, 3);
      }
      break;
    case 4:
      lcd.setCursor(18, 3);
      break;
    case 5:
      lcd.setCursor(19, 3);
      break;
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
        lcd.setCursor(8 + n - 1, 2);
        lcd.print(' ');
        lcd.setCursor(8 + n - 1, 2);
        flowRatestr.remove(n - 1);
      }
      break;
    case 3:
      if (state_durationvolume == 0) {
        n = durationstr.length();
        if (n > 0) {
          lcd.setCursor(8 + n - 1, 3);
          lcd.print(' ');
          lcd.setCursor(8 + n - 1, 3);
          durationstr.remove(n - 1);
        }
      }
      else {
        n = volumestr.length();
        if (n > 0) {
          lcd.setCursor(8 + n - 1, 3);
          lcd.print(' ');
          lcd.setCursor(8 + n - 1, 3);
          volumestr.remove(n - 1);
        }
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
  sequenceData.flowRate[seq] = flowRatestr.toInt();
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
