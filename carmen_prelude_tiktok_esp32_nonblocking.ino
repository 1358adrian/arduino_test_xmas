#include <Arduino.h> // don't forget to add this in PlatformIO!

#define PIN 25
#define CH 0
#define DUTY 127
#define RES 8

// Can be moved in header file i.e notes.h
#define ARRAY_LEN(array) (sizeof(array) / sizeof(array[0]))
#define D6 1175
#define A5 880
#define G5 784
#define E6 1319
#define Fb6 1480
#define Cb6 1109
#define G6 1568
#define C6 1047
#define A6 1760
#define B6 1976
#define F6 1397
#define B5 988

const int midi1[88][3] = {
 {D6, 229, 271},
 {D6, 167, 146},
 {D6, 125, 63},
 {D6, 167, 146},
 {A5, 125, 63},
 {G5, 188, 167},
 {A5, 104, 63},
 {D6, 167, 292},
 {D6, 167, 167},
 {D6, 125, 21},
 {D6, 188, 167},
 {E6, 125, 63},
 {Fb6, 188, 146},
 {E6, 146, 42},
 {D6, 208, 271},
 {D6, 208, 125},
 {D6, 125, 63},
 {E6, 167, 146},
 {D6, 125, 42},
 {Cb6, 188, 146},
 {D6, 125, 63},
 {E6, 1479, 500},
 {G6, 229, 271},
 {G6, 167, 146},
 {G6, 125, 63},
 {G6, 188, 104},
 {D6, 146, 42},
 {C6, 229, 104},
 {D6, 125, 83},
 {G6, 188, 271},
 {G6, 167, 167},
 {G6, 125, 63},
 {G6, 208, 125},
 {A6, 125, 83},
 {B6, 167, 125},
 {A6, 125, 83},
 {G6, 208, 292},
 {G6, 167, 125},
 {Fb6, 146, 63},
 {E6, 167, 313},
 {E6, 146, 167},
 {D6, 146, 42},
 {Cb6, 1604, 417},
 {D6, 229, 229},
 {D6, 208, 125},
 {D6, 146, 42},
 {D6, 188, 125},
 {A5, 125, 63},
 {G5, 250, 63},
 {A5, 125, 83},
 {D6, 229, 271},
 {D6, 167, 125},
 {D6, 125, 63},
 {D6, 188, 125},
 {E6, 125, 63},
 {Fb6, 167, 146},
 {E6, 125, 63},
 {D6, 250, 229},
 {D6, 208, 104},
 {D6, 146, 63},
 {E6, 167, 167},
 {D6, 125, 63},
 {Cb6, 188, 146},
 {D6, 125, 63},
 {E6, 1479, 500},
 {A6, 229, 250},
 {A6, 188, 125},
 {A6, 146, 42},
 {A6, 208, 146},
 {G6, 125, 42},
 {F6, 188, 104},
 {G6, 125, 63},
 {A6, 229, 292},
 {A6, 188, 125},
 {A6, 125, 83},
 {A6, 188, 146},
 {G6, 125, 42},
 {F6, 208, 104},
 {G6, 125, 63},
 {A6, 229, 292},
 {A5, 146, 104},
 {B5, 125, 63},
 {Cb6, 229, 354},
 {A5, 125, 125},
 {Fb6, 125, 42},
 {E6, 938, 83},
 {D6, 354, 604},
 {D6, 0, 0}
};

// Non-blocking player state (static inside function)
void playMidiNonBlocking(const int notes[][3], size_t len) {
  static size_t idx = 0;              // current note index
  static bool noteOn = false;         // true while current note is sounding
  static unsigned long stateStart = 0;// timestamp when current on/off started
  static unsigned long onDur = 0;     // milliseconds to sound current note
  static unsigned long offDur = 0;    // milliseconds to wait after sounding

  unsigned long now = millis();

  // wrap-around behavior to repeat the song (matches original loop behavior)
  if (idx >= len) {
    idx = 0;
    noteOn = false;
    stateStart = 0;
  }

  // If we are currently sounding a note, check if its on-duration elapsed
  if (noteOn) {
    if (now - stateStart >= onDur) {
      // stop the note
      ledcWrite(CH, 0);
      noteOn = false;
      stateStart = now; // start counting the off-duration
      // offDur was already loaded when the note started
      // if offDur == 0, we'll advance idx immediately on next check
    }
    return; // still inside on-duration (or just turned off and waiting)
  }

  // If we're in the off-duration for the current note index
  if (stateStart != 0 && !noteOn) {
    if (now - stateStart >= offDur) {
      // finished the off gap for current note, move to next note
      idx++;
      stateStart = 0; // signal ready to start next note
    }
    return;
  }

  // If stateStart == 0 and noteOn == false -> start (or immediately skip) the note at idx
  // Load durations for this note
  onDur  = notes[idx][1];
  offDur = notes[idx][2];

  if (onDur > 0) {
    // start sounding this note
    ledcSetup(CH, notes[idx][0], RES);
    ledcWrite(CH, DUTY);
    noteOn = true;
    stateStart = now; // mark when we started the note
  } else {
    // zero on-duration: ensure output is off and wait offDur (or advance immediately if offDur==0)
    ledcWrite(CH, 0);
    noteOn = false;
    stateStart = now;
    if (offDur == 0) {
      // nothing to wait, advance to next note immediately
      idx++;
      stateStart = 0;
    }
  }
}

void setup() {
  // attach pin to PWM channel
  ledcAttachPin(PIN, CH);
  // initialize channel with harmless frequency (optional)
  // ledcSetup(CH, 1000, RES);
}

void loop() {
  // call non-blocking player frequently
  playMidiNonBlocking(midi1, ARRAY_LEN(midi1));

  // --- put other non-blocking application code here ---
  // e.g. read sensors, handle buttons, update displays, etc.
}
