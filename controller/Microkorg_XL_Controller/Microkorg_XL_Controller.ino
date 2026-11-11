/*
 * microKORG XL companion MIDI controller — beta
 * Extracted from Samplera_v06: tap tempo, sustain and preset buttons only.
 * Target: Arduino-ESP32 (original ESP32-S3 pin assignment).
 * MIDI_TX must feed a suitable 3.3 V MIDI OUT circuit, not a bare DIN socket.
 */
#include <Arduino.h>
#include <Wire.h>

constexpr uint8_t MIDI_CHANNEL = 1; // 1–16; match the synth's global MIDI channel.
constexpr int MIDI_TX = 17, SUSTAIN_PIN = 2, TAP_PIN = 1;
constexpr int PRESET_PINS[] = {14, 15, 16};
constexpr int ENCODER_A = 8, ENCODER_B = 9, ENCODER_SWITCH = 18;
constexpr int LCD_SDA = 21, LCD_SCL = 47;
constexpr int SUSTAIN_ACTIVE_LEVEL = LOW; // Normally-open switch closes to GND.
constexpr int ENCODER_STEPS = 4;
static_assert(MIDI_CHANNEL >= 1 && MIDI_CHANNEL <= 16, "MIDI channel must be 1–16");

// A11..A88 map to MIDI programs 0..63; B11..B88 map to 64..127.
constexpr int program(char bank, int genre, int category) {
  return (bank == 'B' ? 64 : 0) + (genre - 1) * 8 + category - 1;
}
struct Scene { const char *name; int presets[3]; };
// Examples only: replace with your own set list. -1 leaves a button unassigned.
const Scene scenes[] = {
  {"Scene 1", {program('A', 1, 1), program('A', 2, 5), program('B', 2, 4)}},
  {"Scene 2", {program('B', 3, 6), program('A', 3, 1), -1}},
  {"Scene 3", {program('B', 6, 1), program('B', 6, 4), program('A', 6, 5)}}
};
constexpr int SCENE_COUNT = sizeof(scenes) / sizeof(scenes[0]);
// Preserve v06's physical wiring: GPIO 14 → entry 1, 15 → entry 3, 16 → entry 2.
// Change to {0, 1, 2} if your buttons are wired in that order.
constexpr uint8_t PRESET_ENTRY[] = {0, 2, 1};

struct Button { int pin; bool raw = HIGH, stable = HIGH; uint32_t changedAt = 0; };
bool changed(Button &button); // Explicit declaration for Arduino preprocessing.
Button presetButtons[] = {{14}, {15}, {16}};
Button pedal{SUSTAIN_PIN}, tap{TAP_PIN}, confirm{ENCODER_SWITCH};
HardwareSerial midi(1);
uint8_t lcdAddress = 0, encoderState = 0;
int selectedScene = 0, activeScene = 0, encoderSteps = 0;
const int8_t transitions[16] = {0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0};
bool clockActive = false, hasTap = false, noticeVisible = false;
uint8_t tapCount = 0;
uint32_t lastTap = 0, tapPressedAt = 0, clockInterval = 20833, nextClock = 0, noticeAt = 0;

bool changed(Button &button) {
  const bool value = digitalRead(button.pin);
  if (value != button.raw) { button.raw = value; button.changedAt = millis(); }
  if (value != button.stable && uint32_t(millis() - button.changedAt) >= 30) {
    button.stable = value; return true;
  }
  return false;
}
void controlChange(uint8_t controller, uint8_t value) {
  midi.write(uint8_t(0xB0 | (MIDI_CHANNEL - 1))); midi.write(controller); midi.write(value);
}
void lcdRaw(uint8_t value) { Wire.beginTransmission(lcdAddress); Wire.write(value | 8); Wire.endTransmission(); }
void lcdNibble(uint8_t value) { lcdRaw(value | 4); delayMicroseconds(2); lcdRaw(value & ~4); delayMicroseconds(50); }
void lcdSend(uint8_t value, bool data) {
  const uint8_t rs = data ? 1 : 0; lcdNibble((value & 0xF0) | rs); lcdNibble(((value << 4) & 0xF0) | rs);
}
void lcdCommand(uint8_t value) { lcdSend(value, false); if (value == 1 || value == 2) delay(2); }
void lcdLine(uint8_t address, const char *text) {
  if (!lcdAddress) return;
  lcdCommand(address); int i = 0;
  while (*text && i < 16) { lcdSend(*text++, true); ++i; }
  while (i++ < 16) lcdSend(' ', true);
}
void showScene() {
  char text[17]; snprintf(text, sizeof(text), "%02d %.12s", selectedScene + 1, scenes[selectedScene].name);
  lcdLine(0x80, text); lcdLine(0xC0, selectedScene == activeScene ? "ACTIVE" : "Press to confirm");
  noticeVisible = false;
}
void showNotice(const char *text) {
  lcdLine(0xC0, text); noticeVisible = true; noticeAt = millis();
}
void initLCD() {
  Wire.begin(LCD_SDA, LCD_SCL); Wire.setClock(100000);
  for (int address = 0x20; address <= 0x3F; ++address) {
    if (address > 0x27 && address < 0x38) continue;
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) { lcdAddress = address; break; }
  }
  if (!lcdAddress) { Serial.println("No LCD found; MIDI controller remains available"); return; }
  delay(50); lcdNibble(0x30); delay(5); lcdNibble(0x30); delayMicroseconds(150);
  lcdNibble(0x30); delayMicroseconds(150); lcdNibble(0x20);
  lcdCommand(0x28); lcdCommand(0x08); lcdCommand(1); lcdCommand(0x06); lcdCommand(0x0C); showScene();
}
void selectPreset(int value) {
  if (value < 0 || value > 127) { showNotice("Unassigned"); return; }
  controlChange(0, 0); controlChange(32, 0);
  midi.write(uint8_t(0xC0 | (MIDI_CHANNEL - 1))); midi.write(uint8_t(value));
  char text[17]; const int number = value % 64;
  snprintf(text, sizeof(text), "Preset %c%d%d", value >= 64 ? 'B' : 'A', number / 8 + 1, number % 8 + 1);
  showNotice(text); Serial.println(text);
}
void tickClock() {
  const uint32_t now = micros();
  if (!clockActive || int32_t(now - nextClock) < 0) return;
  midi.write(uint8_t(0xF8)); nextClock += clockInterval;
  // Skip missed ticks instead of sending a burst after a delayed loop.
  if (int32_t(now - nextClock) >= 0) nextClock = now + clockInterval;
}
void registerTap(uint32_t pressedAt) {
  const uint32_t interval = pressedAt - lastTap;
  if (!hasTap || interval > 2000) { hasTap = true; tapCount = 1; lastTap = pressedAt; return; }
  if (interval < 250) return; // Supported range: 30–240 BPM.
  lastTap = pressedAt; if (tapCount < 3) ++tapCount;
  if (tapCount < 3) return;
  const float bpm = 60000.0f / interval;
  clockInterval = uint32_t(60000000.0f / (bpm * 24.0f));
  if (!clockActive) { clockActive = true; nextClock = micros() + clockInterval; }
  char text[17]; snprintf(text, sizeof(text), "Tempo %.1f BPM", bpm); showNotice(text); Serial.println(text);
}
void activateScene(int index) {
  if (index != activeScene) { clockActive = false; hasTap = false; tapCount = 0; }
  activeScene = index; showScene(); // Confirming a scene does not send a preset.
}
void setup() {
  Serial.begin(115200);
  for (const int pin : PRESET_PINS) pinMode(pin, INPUT_PULLUP);
  const int inputPins[] = {SUSTAIN_PIN, TAP_PIN, ENCODER_A, ENCODER_B, ENCODER_SWITCH};
  for (const int pin : inputPins) pinMode(pin, INPUT_PULLUP);
  encoderState = (digitalRead(ENCODER_A) << 1) | digitalRead(ENCODER_B);
  midi.begin(31250, SERIAL_8N1, -1, MIDI_TX);
  controlChange(64, digitalRead(SUSTAIN_PIN) == SUSTAIN_ACTIVE_LEVEL ? 127 : 0);
  initLCD(); Serial.println("microKORG XL MIDI controller ready");
}
void loop() {
  tickClock();
  if (changed(tap)) {
    if (tap.stable == LOW) tapPressedAt = millis();
    else registerTap(tapPressedAt); // Calculate timing from press edges, process after release.
  }
  const uint8_t state = (digitalRead(ENCODER_A) << 1) | digitalRead(ENCODER_B);
  if (state != encoderState) {
    encoderSteps += transitions[(encoderState << 2) | state]; encoderState = state;
    if (abs(encoderSteps) >= ENCODER_STEPS) {
      selectedScene = (selectedScene + (encoderSteps > 0 ? SCENE_COUNT - 1 : 1)) % SCENE_COUNT;
      encoderSteps = 0; showScene();
    }
  }
  if (changed(confirm) && confirm.stable == LOW) activateScene(selectedScene);
  if (changed(pedal)) controlChange(64, pedal.stable == SUSTAIN_ACTIVE_LEVEL ? 127 : 0);
  for (int i = 0; i < 3; ++i) if (changed(presetButtons[i]) && presetButtons[i].stable == LOW)
    selectPreset(scenes[activeScene].presets[PRESET_ENTRY[i]]);
  if (noticeVisible && uint32_t(millis() - noticeAt) >= 2000) showScene();
}
