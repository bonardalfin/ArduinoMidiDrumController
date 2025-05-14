////////////////////////////////////////////////////////////////////////////////
// Dibuat oleh Bonard Alfin
// perfectsilentproject.blogspot.com

////////////////////////////////////////////////////////////////////////////////
// Untuk Arduino Pro Micro + Multiplexer CD74HC4067

////////////////////////////////////////////////////////////////////////////////
// === Prinsip Multiplexer (CD74HC4067) ===
// Multiplexer atau disebut Mux ini memungkinkan 16 input (C0–C15) dibaca lewat 1 pin analog, dikontrol oleh 4 pin digital selector (S0–S3):
// S0–S3: memilih channel 0–15
// SIG: terhubung ke pin analog Arduino

////////////////////////////////////////////////////////////////////////////////
// Komponen             Jumlah              Keterangan
// Pad langsung           8                 Pin Direct Arduino Pro Micro
// Pad via MUX            16                MUX input C0–C15 → Arduino A11
// Total Pad              24                MIDI channel 10 (default drum)

// Selector pin bisa pakai misalnya: 2, 3, 4, 5

////////////////////////////////////////////////////////////////////////////////
// === Wiring CD74HC4067 ke Arduino Leonardo ===
// MUX Pin        Arduino
// SIG            A9
// S0             2
// S1             3
// S2             4
// S3             5
// VCC            5V
// GND            GND
// C0–C15         Piezo input (dengan tambahan resistor 1M Ohm dan Dioda Zener 5.1V seperti 1N4733A 
// 7              Button (Digital) untuk open/close hihat

////////////////////////////////////////////////////////////////////////////////
// === Checklist: ===
// Total 24 pad (8 direct + 16 MUX)
// Piezo yang belum terpasang tidak terbaca
// Hi-hat switch open/close
// Bisa disesuaikan velocity/threshold per pad


////////////////////////////////////////////////////////////////////////////////
#include <MIDIUSB.h>

// === Debug Mode ===
// Debug mode untuk Serial Monitor
// true untuk aktifkan
// false untuk matikan
const bool debugMode = false;

const int muxSIGPin = A9;  // Analog pin untuk membaca MUX
const int muxS0 = 2;
const int muxS1 = 3;
const int muxS2 = 4;
const int muxS3 = 5;

const byte numDirectPads = 9; // Jumlah Pad yang Direct ke Arduino
const byte numMuxPads = 16; // Jumlah Pad yang digunakan di Multiplexer CD74HC4067
const byte totalPads = numDirectPads + numMuxPads;

const byte piezoDirectPins[numDirectPads] = {
  A0, A1, A2, A3, A6, A7, A8, A9, A10  // Piezo Pin yang digunakan direct Arduino
};

// === Konfigurasi  Aktif / Tidak Aktif (Wajib diubah) ===
// Aktifkan HANYA pad yang sudah dipasangi piezo
// Ini diperlukan untuk keperluan prototyping agar lebih mudah
// Kalo ini tidak di setting, PASTI ERROR
// True = Piezo sudah terpasang
// False = Piezo belum terpasang
const bool padEnabled[totalPads] = {
  false, // A0 direct
  false, // A1 direct
  false, // A2 direct
  false, // A3 direct
  false, // A6 direct
  false, // A7 direct
  false, // A8 direct
  false, // A9 direct
  true,  // A10 (hi-hat)
  
  false, // C0 mux
  false, // C1 mux
  false, // C2 mux
  false, // C3 mux
  false, // C4 mux
  false, // C5 mux
  false, // C6 mux
  false, // C7 mux
  false, // C8 mux
  false, // C9 mux
  false, // C10 mux
  false, // C11 mux
  false, // C12 mux
  false, // C13 mux
  false, // C14 mux
  false // C15 mux
};

// MIDI Notes per pad
// Midi Note Number (googling aja) dan hihat diletakan terakhir direct (disini hihat = 46)
byte midiNotes[totalPads] = {
  36, 38, 40, 41, 43, 45, 47, 48, 46, // direct
  50, 51, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72 // mux
};

// === Hi-Hat Switch ===
const int hiHatSwitchPin = 7;  // Pin Digital switch yang digunakan untuk ubah open atau close Hihat pada Arduino
const byte hiHatPadIndex = 8; // pad direct terakhir - Pin A10 pada Arduino
const byte hiHatOpenNote = 46; // Note hihat open, bisa diganti cek Midi Note Number
const byte hiHatClosedNote = 42;  // Note hihat tutup, bisa diganti cek Midi Note Number
// Untuk pedal hihat bisa Ctrl+F cari : MIDI note 44 - Nanti disana bisa ganti Midi Note Number nya


// === Konfigurasi pad per-pad ===
struct PadConfig {
  int thresholdMin; // ambang sensitivitas minimum (mendeteksi pukulan) - Mengatur sensitivitas minimal deteksi pukulan
  int thresholdMax; // nilai maksimum untuk mapping velocity - Membatasi nilai maksimal untuk hit normal (hindari piezo spike)
  byte velocityMin; // batas velocity terendah - Kontrol range velocity MIDI untuk masing-masing pad
  byte velocityMax; // batas velocity tertinggi - Kontrol range velocity MIDI untuk masing-masing pad
  unsigned long scanTime; // waktu untuk mencari nilai puncak - Waktu pencarian peak (semakin pendek = lebih cepat, tapi kurang akurat)
  unsigned long maskTime; // waktu tunda sebelum pad bisa dipicu ulang - Waktu tunda untuk mencegah retrigger
};

// Konfigurasi spesifik untuk masing-masing pad
// Silahkan diubah sesuai selera
// Urutannya : thresholdMin, thresholdMax, velocityMin, velocityMax, scanTime, maskTime
// Lihat artinya di Konfigurasi Sensor Piezo diatas
PadConfig padConfigs[totalPads];
void initPadConfigs() {
  // Pad 0–10: Direct
  padConfigs[0]  = {50, 900, 40, 127, 10, 120}; // Kick
  padConfigs[1]  = {17, 550, 15, 127, 0, 50}; // Snare
  padConfigs[2]  = {60, 870, 50, 127, 10, 120}; // Tom 1
  padConfigs[3]  = {60, 870, 50, 127, 10, 120}; // Tom 2
  padConfigs[4]  = {60, 870, 50, 127, 10, 120}; // Floor Tom
  padConfigs[5]  = {50, 860, 40, 127, 10, 120}; // Crash 1
  padConfigs[6]  = {50, 860, 40, 127, 10, 120}; // Crash 2
  padConfigs[7]  = {55, 880, 45, 127, 10, 120}; // Ride
  padConfigs[8] = {15, 550, 20, 127, 0, 50}; // Hi-hat (switched open/closed)

  // Pad 11–26: MUX
  padConfigs[9] = {55, 890, 45, 127, 10, 120}; // C0 mux - Bell
  padConfigs[10] = {55, 890, 45, 127, 10, 120}; // C1 mux - Splash
  padConfigs[11] = {55, 890, 45, 127, 10, 120}; // C2 mux
  padConfigs[12] = {55, 890, 45, 127, 10, 120}; // C3 mux
  padConfigs[13] = {55, 890, 45, 127, 10, 120}; // C4 mux
  padConfigs[14] = {55, 890, 45, 127, 10, 120}; // C5 mux
  padConfigs[15] = {55, 890, 45, 127, 10, 120}; // C6 mux
  padConfigs[16] = {55, 890, 45, 127, 10, 120}; // C7 mux
  padConfigs[17] = {55, 890, 45, 127, 10, 120}; // C8 mux
  padConfigs[18] = {55, 890, 45, 127, 10, 120}; // C9 mux
  padConfigs[19] = {55, 890, 45, 127, 10, 120}; // C10 mux
  padConfigs[20] = {55, 890, 45, 127, 10, 120}; // C11 mux
  padConfigs[21] = {55, 890, 45, 127, 10, 120}; // C12 mux
  padConfigs[22] = {55, 890, 45, 127, 10, 120}; // C13 mux
  padConfigs[23] = {55, 890, 45, 127, 10, 120}; // C14 mux
  padConfigs[24] = {55, 890, 45, 1270, 10, 120}; // C15 mux
}

// === Variabel Internal ===
enum SensorState { IDLE, SCANNING, MASKING };
SensorState sensorStates[totalPads];
unsigned long scanStartTimes[totalPads];
unsigned long maskStartTimes[totalPads];
int peakValues[totalPads];

// === Setup awal ===
void setup() {
  if (debugMode) {
    Serial.begin(115200);
    while (!Serial);
    Serial.println("27-Pad MIDI Drum with Multiplexer");
  }

  pinMode(hiHatSwitchPin, INPUT_PULLUP);
  pinMode(muxS0, OUTPUT);
  pinMode(muxS1, OUTPUT);
  pinMode(muxS2, OUTPUT);
  pinMode(muxS3, OUTPUT);

  for (byte i = 0; i < numDirectPads; i++) {
    pinMode(piezoDirectPins[i], INPUT);
  }

  initPadConfigs(); // <- gunakan konfigurasi individual

  for (byte i = 0; i < totalPads; i++) {
    sensorStates[i] = IDLE;
  }
}



// === Loop utama ===
void loop() {
  unsigned long now = millis();

  // --- Debounce untuk Hi-Hat Switch ---
  static unsigned long lastDebounceTime = 0;   // Waktu saat tombol terakhir kali dibaca
  static bool lastHiHatState = HIGH;            // Status terakhir tombol
  static bool hiHatPressed = false;             // Status apakah tombol sudah ditekan

  bool currentHiHatState = digitalRead(hiHatSwitchPin);  // Status tombol saat ini

  if (currentHiHatState != lastHiHatState) {  // Cek jika status tombol berubah
    lastDebounceTime = now;  // Catat waktu saat status tombol berubah
  }

  if ((now - lastDebounceTime) > 50) {  // Cek jika sudah lebih dari 50ms
    // Jika tombol baru ditekan
    if (currentHiHatState == LOW && !hiHatPressed) {
      hiHatPressed = true;
      noteOn(10, 44, 20); // Kirim MIDI note 44 - Midi Note Number 44 Bisa diganti ke Midi Note Number lain
      MidiUSB.flush();
      delay(10); // Tahan selama 10ms
      noteOff(10, 44, 0); // Matikan MIDI note 44 - Midi Note Number 44 Bisa diganti ke Midi Note Number lain
      MidiUSB.flush();

      if (debugMode) {
        Serial.println("Note 44 triggered by hi-hat switch");
      }
    }
    // Jika tombol dilepaskan
    if (currentHiHatState == HIGH) {
      hiHatPressed = false;
    }
  }

  lastHiHatState = currentHiHatState;  // Simpan status tombol terakhir

  // --- Update hi-hat note ---
  bool isHiHatClosed = digitalRead(hiHatSwitchPin) == LOW;
  midiNotes[hiHatPadIndex] = isHiHatClosed ? hiHatClosedNote : hiHatOpenNote;

  // --- Loop untuk deteksi pad lainnya ---
  for (byte i = 0; i < totalPads; i++) {
    if (!padEnabled[i]) continue;

    int piezoValue = 0;

    if (i < numDirectPads) {
      // Pad langsung
      piezoValue = analogRead(piezoDirectPins[i]);
    } else {
      // Pad via MUX
      byte muxChannel = i - numDirectPads;
      setMuxChannel(muxChannel);
      delayMicroseconds(10); // stabilisasi MUX
      piezoValue = analogRead(muxSIGPin);
    }

    if (piezoValue >= 1020) continue; // noise pin mengambang

    PadConfig cfg = padConfigs[i];

    switch (sensorStates[i]) {
      case IDLE:
        if (piezoValue >= cfg.thresholdMin) {
          scanStartTimes[i] = now;
          peakValues[i] = piezoValue;
          sensorStates[i] = SCANNING;
          if (debugMode) printDebug(i, piezoValue, "SCANNING");
        }
        break;

      case SCANNING:
        if (piezoValue > peakValues[i]) {
          peakValues[i] = piezoValue;
        }

        if (now - scanStartTimes[i] >= cfg.scanTime) {
          peakValues[i] = constrain(peakValues[i], cfg.thresholdMin, cfg.thresholdMax);
          byte velocity = map(peakValues[i], cfg.thresholdMin, cfg.thresholdMax, cfg.velocityMin, cfg.velocityMax);
          velocity = constrain(velocity, cfg.velocityMin, cfg.velocityMax);

          byte note = midiNotes[i];
          noteOn(10, note, velocity);
          MidiUSB.flush();
          noteOff(10, note, 0);
          MidiUSB.flush();

          if (debugMode) printDebug(i, peakValues[i], "TRIGGERED");

          maskStartTimes[i] = now;
          sensorStates[i] = MASKING;
        }
        break;

      case MASKING:
        if (now - maskStartTimes[i] >= cfg.maskTime) {
          sensorStates[i] = IDLE;
        }
        break;
    }
  }
}


// === Set MUX channel ===
void setMuxChannel(byte channel) {
  digitalWrite(muxS0, channel & 0x01);
  digitalWrite(muxS1, (channel >> 1) & 0x01);
  digitalWrite(muxS2, (channel >> 2) & 0x01);
  digitalWrite(muxS3, (channel >> 3) & 0x01);
}

// === MIDI functions ===
void noteOn(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOn = {0x09, 0x90 | (channel - 1), pitch, velocity};
  MidiUSB.sendMIDI(noteOn);
}

void noteOff(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOff = {0x08, 0x80 | (channel - 1), pitch, velocity};
  MidiUSB.sendMIDI(noteOff);
}

// === Debug print ===
void printDebug(byte padIndex, int value, const char* state) {
  Serial.print("Pad ");
  Serial.print(padIndex);
  Serial.print(" [");
  Serial.print(state);
  Serial.print("] Value: ");
  Serial.println(value);
}
