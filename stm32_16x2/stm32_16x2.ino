#include<Wire.h>
#include<LiquidCrystal_I2C.h>
#include<Keypad.h>
#include<MapleFreeRTOS900.h>

//PINS
#define PUMP1 PB15          // pin led indikator pompa 1
#define PUMP2 PB14          // pin led indikator pompa 2
#define PUMP3 PB13          // pin led indikator pompa 3
#define DIR0 PA9            // pin led indikator arah mendorong/memompa
#define DIR1 PA8            // pin led indikator arah menghisap
#define PUMP_STEP PB3       // pin step driver untuk menjalankan pompa
#define PUMP_DIR PB4        // pin untuk mengatur arah pompa cairan
#define PUMP1_ENABLE PB5   // pin enable pompa 1
#define PUMP2_ENABLE PB8   // pin enable pompa 2
#define PUMP3_PWM PB1       // pin input pompa 3 (input PWM)
#define PUMP3_EN_1 PB10     // pin enable 1 pompa 3
#define PUMP3_EN_2 PB11     // pin enable 2 pompa 3

//Variables
#define LCD_ADDRESS 0x27 // address I2C LCD

const byte ROWS = 4; //jumlah baris keypad matrix
const byte COLS = 4; //jumlah kolom keypad matrix

int seq = 0; //jumlah sekuens pengaliran
int i = 0; // sekuens ke-i

int state = 0;                  //state sistem; 1 = state interface input, 2 = state interface informasi
int state_input = 0;            //sub-state saat sedang menginput; 0 = input pompa, 1 = input arah, 2 = input flowrate, 3 = input durasi/volume
int state_info = 0;             //sub-state interface informasi pengaliran
int state_durationvolume = 0;   //(Bukan state, hanya menandakan input user durasi/volume)sub-state durasi/volume 0 jika parameter yang dipilih durasi, 1 jika dipilih volume
int state_input_final = 0;      //sub-state pilihan pada interface input setelah mengisi semua parameter; 0 = hapus sekuens, 1 = tambah sekuens, 2 = mulai sekuens

unsigned long startTime = 0; //waktu dimulainya sequence
int timeLeft = 0; //waktu sisa sequence
int timeLeftms = 0; //waktu sisa dalam ms
int timeLeftLast = 0; //waktu sisa sebelumnya

// Parameter aliran fluida
struct sequenceParameters {
  int pump[20]; //pump 1, 2, or 3
  int flowRate[20];
  int flowDir[20]; //0 = memompa/mendorong, 1 = menghisap
  unsigned long duration[20];
  unsigned long volume[20];
};

// variabel-variabel untuk menyimpan parameter sementara
int pumpstr = 1;
int flowRatestr = 0;
int flowDirstr = 0;
long durationstr = 0;
long volumestr = 0;

//variabel yang menyimpan variabel dari keseluruhan sekuens
sequenceParameters sequenceData;

//variabel untuk mengatur nyala atau tidaknya pompa
boolean pump_start = false;

unsigned long delay_time = 0;

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

byte rowPins[ROWS] = {PA0, PA1, PA2, PA3};  // Pin-pin baris untuk keypad matrix 4x4
byte colPins[COLS] = {PA4, PA5, PA6, PA7};  // Pin-pin kolom untuk keypad matrix 4x4

Keypad customKeypad = Keypad(makeKeymap(keypadKeys), rowPins, colPins, ROWS, COLS); //inisialisasi keypad matrix 4x4

LiquidCrystal_I2C lcd(LCD_ADDRESS, 16, 2); //inisialisasi I2C LCD

void TaskInterface( void *pvParameters );  // Task yang mengatur keseluruhan interface interface
void TaskPump( void *pvParameters );       // Task untuk menjalankan pompa


void setup() {
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
  pinMode(PUMP_STEP, OUTPUT);
  pinMode(PUMP_DIR, OUTPUT);
  pinMode(PUMP1_ENABLE, OUTPUT);
  pinMode(PUMP2_ENABLE, OUTPUT);
  pinMode(PUMP3_PWM, PWM);
  pinMode(PUMP3_EN_1, OUTPUT);
  pinMode(PUMP3_EN_2, OUTPUT);

  turnOff();

  state = 1;
  state_input = 0;
  stateInput();

  //inisialisasi Task-task

  xTaskCreate(
    TaskInterface
    ,  (const portCHAR *)"Interface"  // A name just for humans
    ,  500  // This stack size can be checked & adjusted by reading the Stack Highwater
    ,  NULL //parameter
    ,  1  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
    ,  NULL );

  //menjalankan pompa
  xTaskCreate(
    TaskPump
    , (const portCHAR *) "Pumping"
    , 240
    , NULL
    , 1
    , NULL);

  vTaskStartScheduler();
}

void loop() {
  //TIdak digunakan
}

// Task yang mengatur keseluruhan interface
void TaskInterface(void *pvParameters __attribute__((unused))) {
  for (;;) {
    char buffer[20];
    char customKey = customKeypad.getKey();
    if (state == 1) { //state saat user menginput parameter
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
              if (durationstr != 0) {
                saveParameter();
                stateInput();
              }
              break;
            case 4:
              state_input = 5;
              if (volumestr != 0) {
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
              if (flowRatestr < 3200) {
                if (!((flowRatestr == 0) && (customKey == '0'))) {
                  flowRatestr = addCharToInt(flowRatestr, customKey);
                  updateParameter();
                }
              }
              break;
            case 3:
              if (durationstr < 10000) {
                if (!((durationstr == 0) && (customKey == '0'))) {
                  durationstr = addCharToInt(durationstr, customKey);
                  updateParameter();
                }
              }

              break;
            case 4:
              if (volumestr < 3200) {
                if (!((volumestr == 0) && (customKey == '0'))) {
                  volumestr = addCharToInt(volumestr, customKey);
                  updateParameter();
                }
              }
              break;
          }
        }
      }
    }
    else if (state == 2) { //state saat pompa berjalan, informasi pengaliran ditampilkan kepada user
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
      if (timeLeftms > 0) { // count sisa waktu pengaliran
        timeLeftms = (sequenceData.duration[i] -  (millis() - startTime));
        timeLeft = timeLeftms / 1000;
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
        pump_start = false;
        if (i >= seq) { // pengaliran selesai, kembali ke interface input sekuens
          seq = 0;
          i = 0;
          state = 1;
          state_input = 0;
          turnOff();
          stateInput();
        }
        else { // sekuens selanjutnya
          informationInterface();
        }
      }
    }
  }
}

// Task yang mengatur sinyal square untuk menjalankan pompa
void TaskPump(void *pvParameters __attribute__((unused))) {

  for (;;) {
    // menjalankan pompa jika menerima perintah pump_start = true
    while (pump_start) {
      if ((sequenceData.pump[i] == 1) || (sequenceData.pump[i] == 2)) {// Menjalankan pompa
        digitalWrite(PUMP_STEP, HIGH);
        delayMicroseconds(delay_time);
        digitalWrite(PUMP_STEP, LOW);
        delayMicroseconds(delay_time);
      }
      else if (sequenceData.pump[i] == 3) {
        analogWrite(PUMP3_PWM,115);
      }
    }

    //TODO: masukin fungsi turnOff() disini kalo bocor
  }
}


// Fungsi untuk mengubah interface input berdasarkan state
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

// pergantian state pada interface input terakhir untuk menentukan perintah yang dilakukan selanjutnya
//(0: hapus sekuens, 1: sekuens selanjutnya, 2: mulai menjalankan sekuens)
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
      lcd.print(sequenceData.duration[i] / 1000);
      break;
  }

  // menyalakan LED yang menunjukkan pompa yang sedang berjalan dan meng-enable pompa yang ingin dijalankan
  switch (sequenceData.pump[i]) {
    case 1:
      digitalWrite(PUMP1, HIGH);
      digitalWrite(PUMP2, LOW);
      digitalWrite(PUMP3, LOW);

      digitalWrite(PUMP1_ENABLE, LOW);
      break;
    case 2:
      digitalWrite(PUMP1, LOW);
      digitalWrite(PUMP2, HIGH);
      digitalWrite(PUMP3, LOW);

      digitalWrite(PUMP2_ENABLE, LOW);
      break;
    case 3:
      digitalWrite(PUMP1, LOW);
      digitalWrite(PUMP2, LOW);
      digitalWrite(PUMP3, HIGH);

      //TODO: enable pompa 3
      break;
  }

  //menyalakan LED yang menunjukkan arah aliran fluida dan mengatur arah pompa
  switch (sequenceData.flowDir[i]) {
    case 0:
      digitalWrite(DIR0, HIGH);
      digitalWrite(DIR1, LOW);
      
      digitalWrite(PUMP_DIR, LOW);
      digitalWrite(PUMP3_EN_1, LOW);
      digitalWrite(PUMP3_EN_2, HIGH);
      break;
    case 1:
      digitalWrite(DIR0, LOW);
      digitalWrite(DIR1, HIGH);
      
      digitalWrite(PUMP_DIR, HIGH);
      digitalWrite(PUMP3_EN_1, HIGH);
      digitalWrite(PUMP3_EN_2, LOW);
      break;
  }

  //menghitung delay yang dibutuhkan untuk sinyal kotak
  delay_time = speedToDelay(sequenceData.flowRate[i]);
  
  delay(1000);

  startTime = millis();
  timeLeftms = sequenceData.duration[i];
  timeLeft = timeLeftms / 1000;
  timeLeftLast = timeLeft;
  
  pump_start = true;


}

// menyelesaikan sekuens dan mematikan seluruh output sekuens
void turnOff() {
  //mematikan indikator
  digitalWrite(PUMP1, LOW);
  digitalWrite(PUMP2, LOW);
  digitalWrite(PUMP3, LOW);
  digitalWrite(DIR0, LOW);
  digitalWrite(DIR1, LOW);

  //mematikan pompa
  digitalWrite(PUMP_STEP, LOW);
  digitalWrite(PUMP1_ENABLE, HIGH);
  digitalWrite(PUMP2_ENABLE, HIGH);
  analogWrite(PUMP3_PWM,0);
  digitalWrite(PUMP3_EN_1, LOW);
  digitalWrite(PUMP3_EN_2, LOW);
  //TODO: disable pompa 3
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
      if (flowRatestr == 0) {
        lcd.print('0');
        lcd.setCursor(6, 1);
      }
      else {
        lcd.print("     ");
        lcd.setCursor(6, 1);
        lcd.print(flowRatestr);
      }
      break;
    case 3:
      lcd.setCursor(7, 1);
      if (durationstr == 0) {
        lcd.print('0');
        lcd.setCursor(7, 1);
      }
      else {
        lcd.print("     ");
        lcd.setCursor(7, 1);
        lcd.print(durationstr);
      }

      break;
    case 4:
      lcd.setCursor(7, 1);
      if (volumestr == 0) {
        lcd.print('0');
        lcd.setCursor(7, 1);
      }
      else {
        lcd.print("     ");
        lcd.setCursor(7, 1);
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
      if (flowRatestr < 10000) {
        if (!((flowRatestr == 0) && (c == '0'))) {
          flowRatestr = addCharToInt(flowRatestr, c);
          lcd.print(c);
        }
      }
      break;
    case 3:
      if (state_durationvolume == 0) {
        if (durationstr < 10000) {
          if (!((durationstr == 0) && (c == '0'))) {
            durationstr = addCharToInt(durationstr, c);
          }
          lcd.print(c);
        }
      }
      else {
        if (volumestr < 10000) {
          if (!((volumestr == 0) && (c == '0'))) {
            volumestr = addCharToInt(volumestr, c);
          }
          lcd.print(c);
        }
      }
      break;
  }
}

// menghapus 1 karakter akhir parameter yang dipilih NOTE:BELOM DIUBAH
void deleteButton() {
  switch (state_input) {
    case 2:
      if (flowRatestr > 0) {
        flowRatestr = flowRatestr / 10;
        updateParameter();
      }
      break;
    case 3:
      if (durationstr > 0) {
        durationstr = durationstr / 10;
        updateParameter();
      }
    case 4:
      if (volumestr > 0) {
        volumestr = volumestr / 10;
        updateParameter();
      }
      break;
  }
}



// menyimpan semua parameter untuk sekuens ke-seq
void saveParameter() {
  sequenceData.pump[seq] = pumpstr;
  if (flowRatestr == 0) {
    sequenceData.flowRate[seq] = 0;
  }
  else {
    sequenceData.flowRate[seq] = flowRatestr;
  }
  sequenceData.flowDir[seq] = flowDirstr;
  if (state_durationvolume == 0) {
    sequenceData.duration[seq] = durationstr * 1000;
  }
  else {
    sequenceData.duration[seq] = volumestr * 60 * 1000 / (sequenceData.flowRate[seq]);
  }
  //sequenceData.volume[seq] = volumestr.toInt();
  seq++;

  pumpstr = 1;
  flowRatestr = 0;
  flowDirstr = 0;
  durationstr = 0;
  volumestr = 0;
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

int addCharToInt(int x, char c) {
  int result = 0;
  result = x * 10 + c - '0';
  return result;
}

// Fungsi untuk menghitung frekuensi sinyal kotak dari flowrate yang diinput
// frekuensi dalam satuan Hz
double speedToFrequence(int spd){
  double freq = 1.90262 * pow(spd, 0.98124);
  return freq;
}

// Fungsi untuk memperoleh delay yang dibutuhkan untuk sinyal kotak (delay = 1/2 Periode sinyal kotak), 
// delay dalam microsecond
unsigned long speedToDelay(int spd){
  int delay_time = 1000000/(2*speedToFrequence(spd)); 
  return delay_time;
}
