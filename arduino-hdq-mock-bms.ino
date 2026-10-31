// HDQ battery "heartbeat" generator — emulates the presence beacon the old
// Panasonic battery puts on the HDQ line so the bike controller keeps working
// after the battery/BMS is replaced.
//
// The real battery emits a fixed burst of LOW pulses every ~499 ms and nothing
// else (the controller never polls — verified from a full ride capture). This
// sketch just replays that burst on an open-drain line.
//
// Timing uses direct port-register toggles (not digitalWrite/pinMode) so each
// pulse edge lands within ~1 us of the target instead of ~10-15 us late.
//
// Wiring (bus idles ~5 V):
//   pin -> gate of a logic-level N-FET (via ~150R, 100k gate
//   pulldown), drain -> HDQ line, source -> GND. Inverted (gate high = bus low).
//   Always share ground with the battery/bus negative. Never drive the line high.

#include <avr/pgmspace.h>
#include "MockBatteryMgmtSystem.h"

// The AVR toolchain used by the Arduino CLI requires an out-of-class
// definition when this static constexpr array is odr-used by the sketch.
constexpr int MockBatteryMgmtSystem::LedThresholds[5];

#define HDQ_PIN     12
#define PERIOD_MS   499        // measured heartbeat period (~498.98 ms)

constexpr float adcToVolt = 5.0F / 1023.0F;

class RealArduinoBoard final : public ArduinoBoard {
public:
  RealArduinoBoard()
      : ArduinoBoard(3), ledPins{6, 5, 4, 3, 2} {
    for (uint8_t pin : ledPins) {
      pinMode(pin, OUTPUT);
      digitalWrite(pin, LOW);
    }
    pinMode(chargerPin, OUTPUT);
    digitalWrite(chargerPin, LOW);
    pinMode(buttonPin, INPUT_PULLUP);
  }

  void setLED(unsigned index, bool active) override {
    if (index < 5) {
      digitalWrite(ledPins[index], active ? HIGH : LOW);
    }
  }

  void activateCharger(bool active) override {
    digitalWrite(chargerPin, active ? HIGH : LOW);
  }

  float doGetBatteryVoltage() const override {
    static constexpr float voltageDivider = 10. / (56. + 10.) * 24.94 /*displayed*/ / 24.7 /*real input*/;
    const float voltage = analogRead(A3) * adcToVolt / voltageDivider;
    // Serial.print("Battery [V]: "); Serial.println(voltage);
    return voltage;
  }

  float doGetTIDVoltage() const override {
    static constexpr float voltageDivider = 10. / (56. + 10.) * 10.35 /*displayed*/ / 10.1 /*real input*/;
    const float voltage = analogRead(A0) * adcToVolt / voltageDivider;
    Serial.print("T/ID [V]: "); Serial.println(voltage);
    return voltage;
  }

private:
  bool doGetButtonState() const override {
    return digitalRead(buttonPin) == LOW;
  }

  const uint8_t ledPins[5];
  static constexpr uint8_t buttonPin = 7;
  static constexpr uint8_t chargerPin = 10;
};

RealArduinoBoard realArduinoBoard;
MockBatteryMgmtSystem mockBatteryMgmtSystem{realArduinoBoard};
unsigned long displayStartedAt = 0;
unsigned long nextBmsUpdateAt = 0;
bool previousButtonState = false;
bool displayTimerActive = false;

// Timing tables (microseconds), alternating LOW,HIGH,...  even index = LOW.
// pattern recorded on the assembled bike (the replay target).
const uint16_t ACTIVE_BURST[] PROGMEM = {
  828, 621, 415, 103, 104, 207, 104, 517, 207, 104,
  104, 103, 414, 518, 104, 103, 104, 207, 414
};

const uint8_t BURST_LEN = sizeof(ACTIVE_BURST) / sizeof(ACTIVE_BURST[0]);

// Cached port registers for fast, low-jitter toggling.
static volatile uint8_t *g_port, *g_ddr;
static uint8_t g_bit;

// --- open-drain helpers: only ever pull LOW or release (Hi-Z / high) ---
static inline __attribute__((always_inline)) void busRelease() {
  *g_port &= ~g_bit;   // gate low  -> FET off -> bus pulled high by bus pull-up
}

static inline __attribute__((always_inline)) void busAssertLow() {
  *g_port |= g_bit;    // gate high -> FET on -> bus pulled low
}

void setup() {
  Serial.begin(9600);

  g_bit = digitalPinToBitMask(HDQ_PIN);
  uint8_t p = digitalPinToPort(HDQ_PIN);
  g_port = portOutputRegister(p);
  g_ddr  = portModeRegister(p);
  *g_port &= ~g_bit;   // gate low
  *g_ddr  |= g_bit;    // pin is a push-pull output to the FET gate
}

unsigned long sendHeartbeatIfDue(unsigned long nextHeartbeatAt) {
  const unsigned long now = millis();
  if (static_cast<long>(now - nextHeartbeatAt) < 0) {
    return nextHeartbeatAt;  // not due, yet
  }
  for (uint8_t i = 0; i < BURST_LEN; i++) {
    if ((i & 1) == 0) busAssertLow();   // even index = LOW pulse
    else              busRelease();      // odd index  = HIGH gap
    delayMicroseconds(pgm_read_word(&ACTIVE_BURST[i]));
  }
  busRelease();                    // return to idle-high after the last LOW
  nextHeartbeatAt += PERIOD_MS;
  if (static_cast<long>(now - nextHeartbeatAt) >= 0) {
    nextHeartbeatAt = millis() + PERIOD_MS;
  }
  return nextHeartbeatAt;
}

static unsigned long nextHeartbeatAt = 0;

void loop() {
  if (!mockBatteryMgmtSystem.isBatteryInCharger()) {
    nextHeartbeatAt = sendHeartbeatIfDue(nextHeartbeatAt);
  }

  const unsigned long bmsNow = millis();
  if (static_cast<long>(bmsNow - nextBmsUpdateAt) >= 0) {
    nextBmsUpdateAt += 100UL;
    if (static_cast<long>(bmsNow - nextBmsUpdateAt) >= 0) {
      nextBmsUpdateAt = bmsNow + 100UL;
    }

    mockBatteryMgmtSystem.update();
  }

  const unsigned long nextDue =
    static_cast<long>(nextBmsUpdateAt - nextHeartbeatAt) < 0 ?
    nextBmsUpdateAt : nextHeartbeatAt;
  const unsigned long sleepNow = millis();
  if (static_cast<long>(nextDue - sleepNow) > 0) {
    const unsigned long sleepMs = nextDue - sleepNow;
    delay(sleepMs > 1UL ? sleepMs - 1UL : 0UL);
  }
}
