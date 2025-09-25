////////////////////////////////////////////////////////////////////////////////
// Dibuat oleh Bonard Alfin
// bonardalfinproject.blogspot.com

////////////////////////////////////////////////////////////////////////////////
// Untuk Arduino Pro Micro

////////////////////////////////////////////////////////////////////////////////
// Komponen             Jumlah              Keterangan
// Pad langsung           9                 Pin Direct Arduino Pro Micro
// Total Pad              9                 MIDI channel 10 (default drum)

////////////////////////////////////////////////////////////////////////////////
// === Checklist: ===
// Total 9 pad direct
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

// === Konfigurasi ===
const byte numDirectPads = 9; // Tidak perlu diubah - Jumlah Pad yang Direct ke Arduino
const byte piezoDirectPins[numDirectPads] = {A0, A1, A2, A3, A6, A7, A8, A9, A10};  // Tidak perlu diubah

// === Konfigurasi  Aktif / Tidak Aktif (Wajib diubah) ===
// Aktifkan HANYA pad yang sudah dipasangi piezo
// Ini diperlukan untuk keperluan prototyping agar lebih mudah
// Kalo ini tidak di setting, PASTI ERROR
// True = Piezo sudah terpasang
// False = Piezo belum terpasang
const bool padEnabled[numDirectPads] = {
  false, // A0
  false, // A1
  false, // A2
  false, // A3
  false, // A6
  false, // A7
  false, // A8
  false, // A9
  true  // A10 (hi-hat)
};

// MIDI Notes per pad
// Midi Note Number (googling aja) dan hihat diletakan terakhir direct (disini hihat = 46)
byte midiNotes[numDirectPads] = {
  36, // A0
  38, // A1
  40, // A2
  41, // A3
  43, // A6
  45, // A7
  47, // A8
  48, // A9
  46  // A10 (hi-hat)
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
PadConfig padConfigs[numDirectPads];
void initPadConfigs() {
  padConfigs[0]  = {50, 900, 40, 127, 10, 120}; // A0
  padConfigs[1]  = {5, 550, 15, 127, 0, 50}; // A1
  padConfigs[2]  = {60, 870, 50, 127, 10, 120}; // A2
  padConfigs[3]  = {60, 870, 50, 127, 10, 120}; // A3
  padConfigs[4]  = {60, 870, 50, 127, 10, 120}; // A6
  padConfigs[5]  = {50, 860, 40, 127, 10, 120}; // A7
  padConfigs[6]  = {50, 860, 40, 127, 10, 120}; // A8
  padConfigs[7]  = {55, 880, 45, 127, 10, 120}; // A9
  padConfigs[8]  = {10, 200, 20, 127, 0, 50};  // A10 Hi-hat (switched open/closed)
}

// === Variabel Internal ===
enum SensorState { IDLE, SCANNING, MASKING };
SensorState sensorStates[numDirectPads];
unsigned long scanStartTimes[numDirectPads];
unsigned long maskStartTimes[numDirectPads];
int peakValues[numDirectPads];

// === Setup awal ===
void setup() {
  if (debugMode) {
    Serial.begin(115200);
    while (!Serial);
    Serial.println("MIDI Drum");
  }

  pinMode(hiHatSwitchPin, INPUT_PULLUP);
  for (byte i = 0; i < numDirectPads; i++) {
    pinMode(piezoDirectPins[i], INPUT);
  }

  initPadConfigs();

  for (byte i = 0; i < numDirectPads; i++) {
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
        Serial.println("Note 44 triggered by hi-hat switch");  // Midi Note Number 44 Bisa diganti ke Note lain
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

  for (byte i = 0; i < numDirectPads; i++) {
    if (!padEnabled[i]) continue;

    int piezoValue = analogRead(piezoDirectPins[i]);
    if (piezoValue >= 1020) continue;

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

void noteOn(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOn = {0x09, 0x90 | (channel - 1), pitch, velocity};
  MidiUSB.sendMIDI(noteOn);
}

void noteOff(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOff = {0x08, 0x80 | (channel - 1), pitch, velocity};
  MidiUSB.sendMIDI(noteOff);
}

void printDebug(byte padIndex, int value, const char* state) {
  Serial.print("Pad ");
  Serial.print(padIndex);
  Serial.print(" [");
  Serial.print(state);
  Serial.print("] Value: ");
  Serial.println(value);
}

