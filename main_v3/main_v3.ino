// Program      : main_v3.ino
// Date Created : 16/02/2020
// Description  : Program utama untuk pompa mikrofluida pada mikrokontroler stm32
// Last Updated : 20/02/2020 -Farras

#include<Wire.h>
#include<LiquidCrystal_I2C.h>
#include<Keypad.h>

//PINS
#define PUMP1 PB15          // pin led indikator pompa 1
#define PUMP2 PB14          // pin led indikator pompa 2
#define PUMP3 PB13          // pin led indikator pompa 3
#define DIR0 PA9            // pin led indikator arah mendorong/memompa
#define DIR1 PA8            // pin led indikator arah menghisap
#define PUMP_STEP PB3       // pin step driver untuk menjalankan pompa
#define PUMP_DIR PB4        // pin untuk mengatur arah pompa cairan
#define PUMP1_ENABLE PB5    // pin enable pompa 1
#define PUMP2_ENABLE PB8    // pin enable pompa 2
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
int state_input = 0;            //sub-state saat sedang menginput; 0 = input pompa, 1 = input arah,
//2 = input flowrate, 3 = input durasi/volume
int state_info = 0;             //sub-state interface informasi pengaliran
int state_input_final = 0;      //sub-state pilihan pada interface input setelah mengisi semua parameter;
// 0 = hapus sekuens, 1 = tambah sekuens, 2 = mulai sekuens
int state_input_review = 0;     //sub-state pada bagian review sekuens.
// 0 = ok/kembali, 1 = edit, 2 = delete, 3 = review sekuens.
int state_main = 0;             //sub-state interface utama; 0 = mode input sekuens, 1 = mode kalibrasi
// (jalanin 1 pompa saja)
int durationvolume = 0;         //(Bukan state, hanya menandakan input user durasi/volume)sub-state durasi/volume
// 0 jika parameter yang dipilih durasi, 1 jika dipilih volume
int pause_resume = 0;           // pada state pengaliran, menunjukkan tombol yang tersedia pause atau resume
// 0 = pause, 1 = resume;


unsigned long durationms = 0; //durasi sekuens dalam ms
unsigned long startTime = 0; //waktu dimulainya sequence
int timeLeft = 0; //waktu sisa sequence
int timeLeftms = 0; //waktu sisa dalam ms
int timeLeftLast = 0; //waktu sisa sebelumnya

// Parameter aliran fluida
struct sequenceParameters {
  int pump[20]; // pump 1, 2, or 3
  int flowRate[20]; // flowrate pengaliran
  int flowDir[20]; // 0 = memompa/mendorong, 1 = menghisap
  int DOVSel[20]; // variabel yang menandakan input DOV berupa durasi atau volume. 0 = durasi, 1 = volume.
  unsigned long DOV[20]; // durasi / volume
};

// variabel-variabel untuk menyimpan parameter sementara
int pumpstr = 1;
int flowRatestr = 0;
int flowDirstr = 0;
long durationstr = 0;
long volumestr = 0;

//variabel yang menyimpan variabel dari keseluruhan sekuens
sequenceParameters sequenceData;

unsigned long delay_time = 0;

// Men-set karakter yang diterima ketika keypad ditekan user
char keypadKeys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

// KARAKTER - KARAKTER CUSTOM UNTUK LCD
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

LiquidCrystal_I2C lcd(LCD_ADDRESS, 20, 4); //inisialisasi I2C LCD


void setup() {
  //inisialisasi LCD
  lcd.init();
  lcd.backlight();

  //Set karakter custom untuk lcd
  lcd.createChar(0, leftArrow);
  lcd.createChar(1, rightArrow);

  //men-disable port untuk debug (PA3, PA4) sehingga dapat digunakan untuk GPIO biasa
  //NOTE: USB harus dilepas-pasang lagi untuk nge debug lagi
  disableDebugPorts();

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

  //inisialisasi timer untuk sinyal step pompa
  Timer1.pause();
  Timer1.attachInterrupt(TIMER_CH1, pumpStepSignal);
  Timer1.refresh();

  //memastikan semua output mati
  pumpOff();

  //menginisialisasi halaman utama
  state = 0;
  mainInterface();
}

// Task yang mengatur keseluruhan interface
void loop() {
  char buffer[20];
  char customKey = customKeypad.getKey();
  if (state == 0) { //state pada halaman utama
    if (customKey) {
      if (customKey == '#') {
        state = 1;
        state_input = 0;
        stateInput();
      }
      else if ( (customKey == 'C') || (customKey == 'D')) {
        if (state_main == 0) {
          state_main = 1;
        }
        else { ///state_main == 1
          state_main = 0;
        }
        stateMain();
      }
    }
  }
  else if (state == 1) { //state saat user menginput parameter
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
            if (state_main == 0) {
              state_input = 3;
              stateInput();
            }
            else {
              saveParameter();
              i = 0;
              state = 2;
              informationInterface();
            }
            break;
          case 3:
            if (durationstr != 0) {
              state_input = 5;
              saveParameter();
              stateInput();
            }
            break;
          case 4:
            if (volumestr != 0) {
              state_input = 5;
              saveParameter();
              stateInput();
            }
            break;
          case 5:
            if (state_input_final == 0) {
              //tambah sequence baru
              state_input = 0;
              stateInput();
            }
            else if (state_input_final == 1) {
              //hapus data sequence ini
              if (seq > 0) {
                seq--;
                stateInput();
              }
            }
            else if (state_input_final == 2) {
              // mereview/menampilkan sekuens-sekuense pengaliran yang telah diinput oleh pengguna
              state_input = 6;
              stateInput();
            }
            else if (state_input_final == 3) {
              // mulai sequence pengaliran
              if (seq > 0) {
                i = 0;
                state = 2;
                informationInterface();
              }
            }
            break;
          case 6:
            if (state_input_review == 0) {
              state_input = 5;
              stateInput();
            }
            else if (state_input_review == 1) {
              //TODO: edit sequence
            }
            else if (state_input_review == 2) {
              //TODO: delete sequence
            }
          default:
            break;
        }
      }
      else if (customKey == '*') { // tombol hapus
        deleteButton();
      }
      else if (customKey == 'A') { // tombol panah atas
        switch (state_input) {
          case 5:
            if (state_input_final > 0) {
              state_input_final --;
            }
            else {
              state_input_final = 3;
            }
            stateInputFinal();
            break;
          case 6:
            if (state_input_review < 3) {
              state_input_review = 3;
            }
            else {
              state_input_review = 0;
            }
            stateInputReview();
            break;
          default:
            break;
        }
      }
      else if (customKey == 'B') { // tombol panah bawah
        switch (state_input) {
          case 5:
            if (state_input_final < 3) {
              state_input_final ++;
            }
            else {
              state_input_final = 0;
            }
            stateInputFinal();
            break;
          case 6:
            if (state_input_review < 3) {
              state_input_review = 3;
            }
            else {
              state_input_review = 0;
            }
            stateInputReview();
            break;
          default:
            break;
        }
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
            if (state_input_final < 3) {
              state_input_final = 3;
            }
            else {
              state_input_final = 0;
            }
            stateInputFinal();
            break;
          case 6:
            if (state_input_review == 0) {
              state_input_review = 2;
            }
            else if (state_input_review < 3) {
              state_input_review--;
            }
            else {
              if (i < seq - 1) {
                i++;
              }
            }
            stateInputReview();
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
              flowDirstr = 0;
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
            if (state_input_final == 3) {
              state_input_final = 0;
            }
            else {
              state_input_final = 3;
            }
            stateInputFinal();
            break;
          case 6:
            if (state_input_review == 2) {
              state_input_review = 0;
            }
            else if (state_input_review < 2) {
              state_input_review ++;
            }
            else { //state_input_review = 3
              if (i > 0) {
                i--;
              }
            }
            stateInputReview();
            break;
          default:
            break;
        }
      }
      else {
        switch (state_input) {
          case 2:
            if (flowRatestr < 10000) {
              flowRatestr = addCharToInt(flowRatestr, customKey);
              updateParameter();
            }
            break;
          case 3:
            if (durationstr < 10000) {
              durationstr = addCharToInt(durationstr, customKey);
              updateParameter();
            }
            break;
          case 4:
            if (volumestr < 10000) {
              volumestr = addCharToInt(volumestr, customKey);
              updateParameter();
            }
            break;
        }
      }
    }
  }
  else if (state == 2) { //state saat pompa berjalan, informasi pengaliran ditampilkan kepada user
    if (state_main == 0) { // mode pengaliran sekuensial
      if ((customKey == 'C') || (customKey == 'D')) {
        if (state_info == 0) {
          state_info = 1;
          lcd.setCursor(5, 0);
          sprintf(buffer, "Sequence %d", i + 1);
          lcd.print(buffer);
          lcd.setCursor(0, 1);
          lcd.print("  ");
          lcd.write(0);
          lcd.print("TIME:       s   ");
          lcd.write(1);
          lcd.setCursor(9, 1);
          lcd.print(timeLeft);
        }
        else if (state_info == 1) {
          state_info = 0;
          lcd.setCursor(3, 0);
          sprintf(buffer, "Sequence %d", i + 1);
          lcd.print(buffer);
          lcd.setCursor(0, 1);
          lcd.print("  ");
          lcd.write(0);
          lcd.print("FLOW:       uL/m");
          lcd.write(1);
          lcd.setCursor(9, 1);
          lcd.print(sequenceData.flowRate[i]);
        }
      }
      if (timeLeftms > 0) { // count sisa waktu pengaliran
        timeLeftms = (durationms -  (millis() - startTime));
        timeLeft = timeLeftms / 1000;
        //update durasi sisa
        if (timeLeft != timeLeftLast) {
          if (state_info == 1) {
            lcd.setCursor(9, 1);
            lcd.print("      s ");
            lcd.setCursor(9, 1);
            lcd.print(timeLeft);
          }
          timeLeftLast = timeLeft;
        }
      }
      else { // durasi pengaliran selesai
        Timer1.pause(); //menghentikan timer, sehingga pompa berhenti
        Timer1.refresh();
        pumpOff();
        i++;
        if (i >= seq) { // pengaliran selesai, kembali ke interface input sekuens
          seq = 0;
          i = 0;
          state = 0;
          state_input = 0;
          mainInterface();
        }
        else { // sekuens selanjutnya
          informationInterface();
        }
      }
    }
    else { //mode pengaliran kontinu/kalibrasi
      if (customKey == '#') {
        switch (state_info) {
          case 0: // pause/resume aliran pompa
            if (pause_resume == 0) {
              pause_resume = 1;
              lcd.setCursor(5, 0);
              lcd.print("RES ");
              pumpPause();
            }
            else { //pause_resume = 1
              pause_resume = 0;
              lcd.setCursor(5, 0);
              lcd.print("STOP");
              pumpResume();
            }
            break;
          case 1: // aliran pompa dihentikan, kembali ke halaman utama
            pumpOff();
            pause_resume = 0;
            seq = 0;
            i = 0;
            state = 0;
            state_input = 0;
            mainInterface();
            break;
        }
      }
      else if ((customKey == 'C') || (customKey == 'D')) {
        if (state_info == 0) {
          state_info = 1;
          lcd.setCursor(4, 0);
          lcd.print(' ');
          lcd.setCursor(11, 0);
          lcd.write(1);
        }
        else {
          state_info = 0;
          lcd.setCursor(11, 0);
          lcd.print(' ');
          lcd.setCursor(4, 0);
          lcd.write(1);
        }
      }
    }
  }
}

// Fungsi untuk menginisialisasi interface utama/awal
void mainInterface() {
  char buffer[20];

  lcd.clear();

  lcd.setCursor(4, 0);
  sprintf(buffer, "Select Mode");
  lcd.print(buffer);

  stateMain();
}

// Fungsi untuk mengubah interface utama berdasarkan sub-state nya
void stateMain() {
  lcd.setCursor(0, 2);
  lcd.print("     SEQ    CAL");
  switch (state_main) {
    case 0:
      lcd.setCursor(4, 2);
      lcd.write(1);
      lcd.setCursor(4, 2);
      break;
    case 1:
      lcd.setCursor(11, 2);
      lcd.write(1);
      lcd.setCursor(11, 2);
      break;
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
      lcd.setCursor(5, 0);
      sprintf(buffer, "Sequence %d", seq + 1);
      lcd.print(buffer);
      lcd.setCursor(0, 1);
      lcd.print("   PUMP: ");
      lcd.write(1);
      lcd.print("1  2  3");
      lcd.setCursor(10, 1);
      break;
    case 1:
      lcd.cursor();
      lcd.blink_on();
      lcd.setCursor(5, 0);
      sprintf(buffer, "Sequence %d", seq + 1);
      lcd.print(buffer);
      lcd.setCursor(0, 1);
      lcd.print("   DIR : ");
      lcd.write(1);
      lcd.print("P  S");
      lcd.setCursor(10, 1);
      break;
    case 2:
      lcd.cursor();
      lcd.blink_on();
      lcd.setCursor(5, 0);
      sprintf(buffer, "Sequence %d", seq + 1);
      lcd.print(buffer);
      lcd.setCursor(0, 1);
      lcd.print("  FLOW: 0     uL/m");
      lcd.setCursor(8, 1);
      break;
    case 3:
      durationvolume = 0;
      lcd.cursor();
      lcd.blink_on();
      lcd.setCursor(5, 0);
      sprintf(buffer, "Sequence %d", seq + 1);
      lcd.print(buffer);
      lcd.setCursor(2, 1);
      lcd.write(0);
      lcd.print("TIME: 0     s ");
      lcd.write(1);
      lcd.setCursor(9, 1);
      break;
    case 4:
      durationvolume = 1;
      lcd.cursor();
      lcd.blink_on();
      lcd.setCursor(5, 0);
      sprintf(buffer, "Sequence %d", seq + 1);
      lcd.print(buffer);
      lcd.setCursor(2, 1);
      lcd.write(0);
      lcd.print("VOL : 0     uL");
      lcd.write(1);
      lcd.setCursor(9, 1);
      break;
    case 5:
      state_input_final = 0;
      lcd.noCursor();
      lcd.blink_off();
      lcd.setCursor(5, 0);
      sprintf(buffer, "Sequence %d", seq); lcd.print(buffer);
      lcd.setCursor(0, 1);
      lcd.print("    ADD    START");
      lcd.setCursor(0, 2);
      lcd.print("    DEL");
      lcd.setCursor(0, 3);
      lcd.print("    REVIEW");
      lcd.setCursor(3, 1);
      lcd.write(1);
      break;
    case 6:
      state_input_review = 0;
      i = 0;
      lcd.noCursor();
      lcd.blink_off();
      lcd.setCursor(5, 0);
      sprintf(buffer, "Sequence %d", seq); lcd.print(buffer);
      showSequenceNReview(i);
      stateInputReview();
    default:
      break;
  }
}

//pergantian state pompa yang dipilih (pompa 1,  3, atau 3)
void stateInputPump() {
  lcd.setCursor(9, 1);
  lcd.print(" 1  2  3");

  switch (pumpstr) {
    case 1:
      lcd.setCursor(9, 1);
      break;
    case 2:
      lcd.setCursor(12, 1);
      break;
    case 3:
      lcd.setCursor(15, 1);
      break;
    default:
      break;
  }
  lcd.write(1);
}

// pergantian state arah laju yang dipilih (dorong atau hisap)
void stateInputDir() {
  lcd.setCursor(9, 1);
  lcd.print(" P  S");

  switch (flowDirstr) {
    case 0:
      lcd.setCursor(9, 1);
      break;
    case 1:
      lcd.setCursor(12, 1);
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
  lcd.print("    ADD    START");
  lcd.setCursor(0, 2);
  lcd.print("    DEL");
  lcd.setCursor(0, 3);
  lcd.print("    REVIEW");


  switch (state_input_final) {
    case 0:
      lcd.setCursor(3, 1);
      break;
    case 1:
      lcd.setCursor(3, 2);
      break;
    case 2:
      lcd.setCursor(3, 3);
      break;
    case 3:
      lcd.setCursor(10, 1);
    default:
      break;
  }
  lcd.write(1);
}

//mengatur perubahan antara state pemilihan parameter atau durasi
void timeOrVolumeSelect() {
  if (durationvolume == 0) {
    lcd.setCursor(1, 1);
    lcd.print("TIME :       s ");
    lcd.setCursor(10, 1);
    lcd.print(durationstr);
  }
  else { //(durationvolume == 1)
    lcd.setCursor(1, 1);
    lcd.print("VOL  :       uL");
    lcd.setCursor(10, 1);
    lcd.print(volumestr);
  }
}

// mengatur perubahan pada state/halaman review sekuens
// 0: ok/kembali, 1: edit sekunes, 2: hapus sekuens, 3: review sekuens
void stateInputReview() {
  char buffer[20];

  switch (state_input_review) {
    case 0:
      //hapus panah pada bagian data sekuens
      lcd.setCursor(0, 2);
      lcd.print(' ');
      lcd.setCursor(19, 2);
      lcd.print(' ');

      lcd.setCursor(19, 0);
      lcd.write(1);
      break;
    case 1:
      //hapus panah pada bagian data sekuens
      lcd.setCursor(0, 2);
      lcd.print(' ');
      lcd.setCursor(19, 2);
      lcd.print(' ');

      lcd.setCursor(14, 0);
      lcd.write(1);
      break;
    case 2:
      //hapus panah pada bagian data sekuens
      lcd.setCursor(0, 2);
      lcd.print(' ');
      lcd.setCursor(19, 2);
      lcd.print(' ');

      lcd.setCursor(9, 0);
      lcd.write(1);
      break;
    case 3:
      //menampilkan data-data parameter sekuens ke-i
      showSequenceNReview(i)

      //menunjukkan panah
      if (i > 0) {
        lcd.setCursor(0, 2);
        lcd.write(0);
      }
      if (i < seq - 1) {
        lcd.setCursor(19, 2);
        lcd.write(1);
      }
      break;
    default:
      break;
  }
}

// menampilkan semua sequence yang telah diinput oleh pengguna
void showSequenceNReview(int n) {
  char buffer[20];

  lcd.clear();
  lcd.setCursor(0, 0);
  sprintf(buffer, "SEQ%d   DEL  EDT  OK", n + 1);
  lcd.print(buffer);

  lcd.setCursor(0, 1);
  sprintf(buffer, "  PUMP/DIR: %d/%c  ", sequenceData.pump[n], getDirChar(sequenceData.flowDir[n]));
  lcd.print(buffer);

  lcd.setCursor(0, 2);
  lcd.print("  FLOW:      ");
  lcd.setCursor(8, 2);
  lcd.print(sequenceData.flowRate[n]);

  //TODO: jika inputnya volume maka yang ditunjukkan volume, jika yg diinput durasi maka yang ditunjukkan durasi.
  //Bikin parameter yang disimpen dari inputnya jadi gitu dulu baru benerin ini.
  // Untuk sementara nunjukin durasi dulu

  lcd.setCursor(0, 3);
  lcd.print("  DUR :      ");
  lcd.setCursor();
  lcd.print(sequenceData.DOV[n]);

}

//Inisilisasi awal interface informasi
void informationInterface() {
  char buffer[20];

  lcd.clear();
  lcd.noCursor();
  lcd.blink_off();

  if (sequenceData.DOVSel[i] == 0) {
    durationms = 1000 * sequenceData.DOV[i];
  }
  else { // NOTE: untuk sementara, untuk sekuens berdasarkan volume, batas pengalirannya masih berdasarkan durasi dulu.
    durationms = sequenceData.DOV[i] * 1000 * 60 / sequenceData.flowRate[i];
  }

  if (state_main == 0) {
    switch (state_info) {
      case 0:
        lcd.setCursor(5, 0);
        sprintf(buffer, "Sequence %d", i + 1);
        lcd.print(buffer);
        lcd.setCursor(2, 1);
        lcd.write(0);
        lcd.print("FLOW:       uL/m");
        lcd.write(1);
        lcd.setCursor(9, 1);
        lcd.print(sequenceData.flowRate[i]);
        break;
      case 1:
        lcd.setCursor(3, 0);
        sprintf(buffer, "Sequence %d", i + 1);
        lcd.print(buffer);
        lcd.setCursor(2, 1);
        lcd.write(0);
        lcd.print("TIME:       s   ");
        lcd.write(1);
        lcd.setCursor(9, 1);
        lcd.print(durationms / 1000);
        break;
    }
  }
  else { //state_main = 1, kalibrasi
    lcd.setCursor(0, 0);
    lcd.print("     STOP   END");
    lcd.setCursor(0, 1);
    lcd.print("  FLOW:       uL/m");
    lcd.setCursor(8, 1);
    lcd.print(sequenceData.flowRate[i]);
    switch (state_info) {
      case 0:
        lcd.setCursor(4, 0);
        lcd.write(1);
        break;
      case 1:
        lcd.setCursor(11, 0);
        lcd.write(1);
        break;
    }
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
      break;
  }

  //menyalakan LED yang menunjukkan arah aliran fluida dan mengatur arah pompa
  switch (sequenceData.flowDir[i]) {
    case 0:
      digitalWrite(DIR0, HIGH);
      digitalWrite(DIR1, LOW);

      if (sequenceData.pump[i] == 3) {
        digitalWrite(PUMP3_EN_1, LOW);
        digitalWrite(PUMP3_EN_2, HIGH);
      }
      else {
        digitalWrite(PUMP_DIR, LOW);
      }
      break;
    case 1:
      digitalWrite(DIR0, LOW);
      digitalWrite(DIR1, HIGH);

      if (sequenceData.pump[i] == 3) {
        digitalWrite(PUMP3_EN_1, HIGH);
        digitalWrite(PUMP3_EN_2, LOW);
      }
      else {
        digitalWrite(PUMP_DIR, HIGH);
      }
      break;
  }

  delay(1000); //delay sejenak untuk memberi waktu transisi antar sekuens pompa

  // menghitung durasi pengaliran, khusus untuk mode pengaliran sekuensial
  if (state_main == 0) {
    startTime = millis();
    timeLeftms = durationms;
    timeLeft = timeLeftms / 1000;
    timeLeftLast = timeLeft;
  }

  // mengaktifkan dan mengatur periode interrupt untuk menjalankan pompa dan mengatur flowrate pompa
  if (sequenceData.pump[i] == 1 || sequenceData.pump[i] == 2) {
    delay_time = flowToDelay(sequenceData.flowRate[i]);
    Timer1.setPeriod(delay_time);
    Timer1.resume();
  }
  else { //pompa 3 (pompa udara)
    analogWrite(PUMP3_PWM, map(sequenceData.flowRate[i], 0, 1000, 0, 255));
  }
}


// Fungsi yang mengatur sinyal square untuk menjalankan pompa
void pumpStepSignal() {
  static int signal_state = 0;
  if (signal_state == 0) {
    digitalWrite(PUMP_STEP, HIGH);
    signal_state = 1;
  }
  else {
    digitalWrite(PUMP_STEP, LOW);
    signal_state = 0;
  }
}

// mematikan seluruh output sekuens dan mematikan/menghentikan gerakan pompa
void pumpOff() {
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
  analogWrite(PUMP3_PWM, 0);
  digitalWrite(PUMP3_EN_1, LOW);
  digitalWrite(PUMP3_EN_2, LOW);
}

// menghentikan aliran pompa sejenak
void pumpPause() {

  // untuk pompa cairan, selain mematikan sinyal step dengan menghentikan interrupt timer,
  // pompa juga di disable dengan driver agar keadan pompa benar-benar mati untuk menghindari kerusakan pada driver

  switch (sequenceData.pump[i]) {
    case 1:
      digitalWrite(PUMP1_ENABLE, HIGH);
      Timer1.pause();
      break;
    case 2:
      digitalWrite(PUMP2_ENABLE, HIGH);
      Timer1.pause();
      break;
    case 3: //untuk pompa udara, sinyal PWMnya saja yang di matikan (0% duty cycle)
      analogWrite(PUMP3_PWM, 0);
      break;
  }
}

// kembali menjalankan pompa
void pumpResume() {
  switch (sequenceData.pump[i]) {
    case 1:
      digitalWrite(PUMP1_ENABLE, LOW);
      Timer1.resume();
      break;
    case 2:
      digitalWrite(PUMP2_ENABLE, LOW);
      Timer1.resume();
      break;
    case 3:
      analogWrite(PUMP3_PWM, map(sequenceData.flowRate[i], 0, 1000, 0, 255));
      break;
  }
}

//update nilai parameter input
void updateParameter() {
  switch (state_input) {
    case 2: //flowrate
      lcd.setCursor(8, 1);
      if (flowRatestr == 0) {
        lcd.print('0');
        lcd.setCursor(6, 1);
      }
      else {
        lcd.print("     ");
        lcd.setCursor(8, 1);
        lcd.print(flowRatestr);
      }
      break;
    case 3: //durasi
      lcd.setCursor(9, 1);
      if (durationstr == 0) {
        lcd.print('0');
        lcd.setCursor(9, 1);
      }
      else {
        lcd.print("     ");
        lcd.setCursor(9, 1);
        lcd.print(durationstr);
      }

      break;
    case 4: //volume
      lcd.setCursor(9, 1);
      if (volumestr == 0) {
        lcd.print('0');
        lcd.setCursor(9, 1);
      }
      else {
        lcd.print("     ");
        lcd.setCursor(9, 1);
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
      if (durationvolume == 0) {
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

// menghapus 1 karakter akhir parameter yang dipilih
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

  // durasi, volume, dan pertambahan sekuens hanya diperhatikan pada mode pengaliran sekuensial
  if (state_main == 0) {
    sequenceData.DOVSel[seq] = durationvolume;
    if (durationvolume == 0) {
      sequenceData.DOV[seq] = durationstr;
    }
    else {
      sequenceData.DOV[seq] = volumestr;
    }
    seq++;
  }

  // reset parameter-parameter sementara
  pumpstr = 1;
  flowRatestr = 0;
  flowDirstr = 0;
  durationstr = 0;
  volumestr = 0;
}

int addCharToInt(int x, char c) {
  int result = 0;
  result = x * 10 + c - '0';
  return result;
}

char getDirChar(int dir) {
  if (dir = 1) {
    return 'S';
  }
  else { //jika dir = 0, maka arah pompa mendorong/pumping 'P'
    return 'P';
  }
}

// Fungsi untuk memperoleh delay yang dibutuhkan untuk sinyal kotak (delay = 1/2 Periode sinyal kotak),
// delay dalam microsecond
unsigned long flowToDelay(int spd) {
  double freq = 1.90262 * pow(spd, 0.98124);
  int delay_time = 1000000 / (2 * freq);
  return delay_time;
}
