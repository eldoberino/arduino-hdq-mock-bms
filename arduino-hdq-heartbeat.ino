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
//   DRIVER_NFET 0 (default): pin -> HDQ line directly, open-drain. The bike
//     supplies the pull-up; do NOT add one (except a ~10k for bench testing).
//   DRIVER_NFET 1: pin -> gate of a logic-level N-FET (via ~150R, 100k gate
//     pulldown), drain -> HDQ line, source -> GND. Inverted (gate high = bus low).
//   Always share ground with the battery/bus negative. Never drive the line high.

#include <avr/pgmspace.h>

#define HDQ_PIN     7
#define DRIVER_NFET 0          // 0 = direct GPIO open-drain, 1 = N-FET buffer
#define PERIOD_MS   499        // measured heartbeat period (~498.98 ms)

// Timing tables (microseconds), alternating LOW,HIGH,...  even index = LOW.
// BURST_BIKE = pattern recorded on the assembled bike (the replay target).
const uint16_t BURST_BIKE[] PROGMEM = {
  828, 621, 415, 103, 104, 207, 104, 517, 207, 104,
  104, 103, 414, 518, 104, 103, 104, 207, 414
};
// BURST_STANDALONE = a different (newer) battery captured on its own. Reference.
const uint16_t BURST_STANDALONE[] PROGMEM = {
  207, 208, 104, 104, 207, 623, 623, 208, 104, 519,
  207, 104, 104, 104, 415, 519, 104, 104, 104, 208, 415
};

// Active pattern (set to the bike recording).
#define ACTIVE_BURST BURST_BIKE
const uint8_t BURST_LEN = sizeof(ACTIVE_BURST) / sizeof(ACTIVE_BURST[0]);

// Cached port registers for fast, low-jitter toggling.
static volatile uint8_t *g_port, *g_ddr;
static uint8_t g_bit;

// --- open-drain helpers: only ever pull LOW or release (Hi-Z / high) ---
static inline __attribute__((always_inline)) void busRelease() {
#if DRIVER_NFET
  *g_port &= ~g_bit;   // gate low  -> FET off -> bus pulled high by bus pull-up
#else
  *g_ddr  &= ~g_bit;   // input Hi-Z -> bus pull-up brings line high
#endif
}

static inline __attribute__((always_inline)) void busAssertLow() {
#if DRIVER_NFET
  *g_port |= g_bit;    // gate high -> FET on -> bus pulled low
#else
  *g_ddr  |= g_bit;    // output (PORT bit stays 0) -> drive line low
#endif
}

void setup() {
  g_bit = digitalPinToBitMask(HDQ_PIN);
  uint8_t p = digitalPinToPort(HDQ_PIN);
  g_port = portOutputRegister(p);
  g_ddr  = portModeRegister(p);
#if DRIVER_NFET
  *g_port &= ~g_bit;   // gate low
  *g_ddr  |= g_bit;    // pin is a push-pull output to the FET gate
#else
  *g_port &= ~g_bit;   // disable internal pull-up so INPUT is true Hi-Z
  *g_ddr  &= ~g_bit;   // start released (input)
#endif
}

void playBurst() {
  for (uint8_t i = 0; i < BURST_LEN; i++) {
    if ((i & 1) == 0) busAssertLow();   // even index = LOW pulse
    else              busRelease();      // odd index  = HIGH gap
    delayMicroseconds(pgm_read_word(&ACTIVE_BURST[i]));
  }
  busRelease();                    // return to idle-high after the last LOW
}

void loop() {
  unsigned long t0 = micros();
  playBurst();
  unsigned long elapsed = micros() - t0;    // ~5.3 ms
  unsigned long period = (unsigned long)PERIOD_MS * 1000UL;
  if (elapsed < period) delay((period - elapsed) / 1000UL);
}
