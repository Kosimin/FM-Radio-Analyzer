#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <arduinoFFT.h>

// ═══════════════════════════════════════════════
// DISPLAY SETTINGS
// ═══════════════════════════════════════════════
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1       // no reset pin
#define OLED_ADDR     0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,
                          &Wire, OLED_RESET);

// ═══════════════════════════════════════════════
// PIN DEFINITIONS
// ═══════════════════════════════════════════════
#define AUDIO_PIN   A0
#define BUTTON_PIN  2

// ═══════════════════════════════════════════════
// RDA5807M ADDRESSES
// ═══════════════════════════════════════════════
#define RDA_SEQ  0x10
#define RDA_RND  0x11

// ═══════════════════════════════════════════════
// FFT SETTINGS
// ═══════════════════════════════════════════════
#define SAMPLES      64
#define SAMPLE_RATE  8000
double vReal[SAMPLES];
double vImag[SAMPLES];
ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag,
                                             SAMPLES, SAMPLE_RATE);

// ═══════════════════════════════════════════════
// STATION LIST
// ═══════════════════════════════════════════════
struct Station {
  uint16_t freq10;
  const char* name;
};

Station stations[] = {
  { 911,  "91.1" },
  { 927,  "92.7" },
  { 935,  "93.5" },
  { 983,  "98.3" },
  { 1064, "106.4"},
};
const uint8_t TOTAL_STATIONS = sizeof(stations) / sizeof(stations[0]);
uint8_t currentStation = 3; // Start at 98.3

// ═══════════════════════════════════════════════
// BUTTON STATE
// ═══════════════════════════════════════════════
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
#define DEBOUNCE_DELAY 50

// ═══════════════════════════════════════════════
// RDA5807M FUNCTIONS
// ═══════════════════════════════════════════════
void writeSequential(uint16_t r02, uint16_t r03,
                     uint16_t r04, uint16_t r05) {
  Wire.beginTransmission(RDA_SEQ);
  Wire.write(r02 >> 8); Wire.write(r02 & 0xFF);
  Wire.write(r03 >> 8); Wire.write(r03 & 0xFF);
  Wire.write(r04 >> 8); Wire.write(r04 & 0xFF);
  Wire.write(r05 >> 8); Wire.write(r05 & 0xFF);
  Wire.endTransmission();
}

void setVolume(uint8_t vol) {
  vol = constrain(vol, 0, 15);
  Wire.beginTransmission(RDA_RND);
  Wire.write(0x05);
  uint16_t reg05 = 0x8880 | vol;
  Wire.write(reg05 >> 8);
  Wire.write(reg05 & 0xFF);
  Wire.endTransmission();
}

void initRadio() {
  Wire.beginTransmission(RDA_SEQ);
  Wire.write(0x00); Wire.write(0x02);
  Wire.endTransmission();
  delay(500);
  writeSequential(0xC005, 0x0000, 0x0400, 0x888F);
  delay(500);
}

void tuneTo(uint16_t freq10) {
  uint16_t channel = freq10 - 870;
  uint16_t reg03   = (channel << 6) | 0x0010;
  writeSequential(0xC005, reg03, 0x0400, 0x888F);
  delay(800);
}

uint8_t getRSSI() {
  Wire.requestFrom(RDA_SEQ, 2);
  uint16_t status = 0;
  if (Wire.available()) status = Wire.read() << 8;
  if (Wire.available()) status |= Wire.read();
  return (status >> 9) & 0x3F;
}

// ═══════════════════════════════════════════════
// BAND LABEL from frequency
// ═══════════════════════════════════════════════
const char* getBandLabel(double freq) {
  if (freq < 250)   return "SUB-BASS";
  if (freq < 500)   return "BASS";
  if (freq < 1000)  return "LOW-MID";
  if (freq < 2000)  return "MID";
  if (freq < 3000)  return "HIGH-MID";
  return                   "TREBLE";
}

// ═══════════════════════════════════════════════
// DISPLAY FUNCTION
// ═══════════════════════════════════════════════
void updateDisplay(float freqMHz, uint8_t rssi,
                   double peakHz, const char* band) {
  display.clearDisplay();

  // ── Row 1: FM Frequency ── large text ─────────
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(F("FM "));
  display.print(freqMHz, 1);
  display.println(F("MHz"));

  // ── Divider line ──────────────────────────────
  display.drawLine(0, 18, 127, 18, SSD1306_WHITE);

  // ── Row 2: RSSI ── medium text ────────────────
  display.setTextSize(1);
  display.setCursor(0, 22);
  display.print(F("RSSI : "));
  display.print(rssi);
  display.println(F(" dBuV"));

  // ── RSSI bar ──────────────────────────────────
  // RSSI range 0-63, map to 0-100 pixels
  int rssiBar = map(rssi, 0, 63, 0, 100);
  display.drawRect(0, 32, 102, 6, SSD1306_WHITE);
  display.fillRect(0, 32, rssiBar, 6, SSD1306_WHITE);

  // ── Row 3: FFT Peak Frequency ─────────────────
  display.setTextSize(1);
  display.setCursor(0, 42);
  display.print(F("FFT  : "));
  display.print(peakHz, 1);
  display.println(F(" Hz"));

  // ── Row 4: Band Label ─────────────────────────
  display.setCursor(0, 54);
  display.print(F("BAND : "));
  display.println(band);

  display.display();
}

// ═══════════════════════════════════════════════
// FFT FUNCTION
// ═══════════════════════════════════════════════
double runFFT() {
  for (int i = 0; i < SAMPLES; i++) {
    vReal[i] = analogRead(AUDIO_PIN);
    vImag[i] = 0;
    delayMicroseconds(1000000 / SAMPLE_RATE);
  }
  FFT.windowing(FFTWindow::Hamming, FFTDirection::Forward);
  FFT.compute(FFTDirection::Forward);
  FFT.complexToMagnitude();
  return FFT.majorPeak();
}

// ═══════════════════════════════════════════════
// TUNING SCREEN
// ═══════════════════════════════════════════════
void showTuningScreen(const char* freq) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(20, 20);
  display.println(F("Tuning to..."));
  display.setTextSize(2);
  display.setCursor(20, 36);
  display.print(F("FM "));
  display.println(freq);
  display.display();
}

// ═══════════════════════════════════════════════
// SETUP
// ═══════════════════════════════════════════════
void setup() {
  Wire.begin();
  Wire.setClock(100000);
  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Init OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("OLED not found!"));
    while (true); // stop if display missing
  }

  // Splash screen
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 10);
  display.println(F("FM RADIO"));
  display.setTextSize(1);
  display.setCursor(25, 40);
  display.println(F("Initializing..."));
  display.display();
  delay(1500);

  // Init radio
  initRadio();
  showTuningScreen(stations[currentStation].name);
  tuneTo(stations[currentStation].freq10);
  setVolume(6);
}

// ═══════════════════════════════════════════════
// LOOP
// ═══════════════════════════════════════════════
void loop() {
  // ── Button handling ───────────────────────────
  bool buttonState = digitalRead(BUTTON_PIN);
  if (buttonState != lastButtonState) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (buttonState == LOW) {
      currentStation = (currentStation + 1) % TOTAL_STATIONS;
      showTuningScreen(stations[currentStation].name);
      tuneTo(stations[currentStation].freq10);
      setVolume(6);
      delay(300);
    }
  }
  lastButtonState = buttonState;

  // ── FFT ───────────────────────────────────────
  double peakHz = runFFT();

  // ── Get RSSI ──────────────────────────────────
  uint8_t rssi = getRSSI();

  // ── Get current frequency ─────────────────────
  float freqMHz = stations[currentStation].freq10 / 10.0;

  // ── Update display ────────────────────────────
  updateDisplay(freqMHz, rssi, peakHz, getBandLabel(peakHz));

  // ── Serial mirror (optional) ──────────────────
  Serial.print(F("FM:")); Serial.print(freqMHz, 1);
  Serial.print(F(" RSSI:")); Serial.print(rssi);
  Serial.print(F(" FFT:")); Serial.print(peakHz, 1);
  Serial.print(F(" Hz Band:")); Serial.println(getBandLabel(peakHz));
}
