#include <Arduino.h>

#include "note_define.h"
#include "pitches.h"
#include "note_define2.h"
#include "pitches2.h"
#include "note_define3.h"
#include "pitches3.h"
#include "note_define4.h"
#include "pitches4.h"

// Resolution of duty cycle (bits)
#define LEDC_RESOLUTION 8  // 8-bit = 0–255 duty range
#define DUTY 127
#define NOTEOFF 0

// Pin assignments (change to match your wiring)
#define BUZZER1_PIN  25
#define BUZZER2_PIN  26
#define BUZZER3_PIN  27
#define BUZZER4_PIN  13

// LEDC channels (each buzzer needs its own channel)
#define BUZZER1_CH   0
#define BUZZER2_CH   2
#define BUZZER3_CH   4
#define BUZZER4_CH   6

void playMidi(const uint32_t notes[][3], size_t len,
              const uint32_t notes2[][3], size_t len2,
              const uint32_t notes3[][3], size_t len3,
              const uint32_t notes4[][3], size_t len4
              ){
  uint32_t channel1Count = 0;
  uint32_t countNote = 0;
  uint32_t countRest = 0;

  uint32_t channel2Count = 0;
  uint32_t countNote2 = 0;
  uint32_t countRest2 = 0;

  uint32_t channel3Count = 0;
  uint32_t countNote3 = 0;
  uint32_t countRest3 = 0;

  uint32_t channel4Count = 0;
  uint32_t countNote4 = 0;
  uint32_t countRest4 = 0;

  do {
    // Channel 1 (only read if channel1Count < len)
    if (channel1Count < len) {
      uint32_t pitch  = notes[channel1Count][0];
      uint32_t nTicks = notes[channel1Count][1];
      uint32_t rTicks = notes[channel1Count][2];

      if (countNote < nTicks) {
        if (pitch != 0) {
          ledcSetup(BUZZER1_CH, pitch, LEDC_RESOLUTION);
          ledcWrite(BUZZER1_CH, DUTY);
        }
        else {
          ledcSetup(BUZZER1_CH, NOTEOFF, LEDC_RESOLUTION);
          ledcWrite(BUZZER1_CH, DUTY);
        };
        countNote++;
      }
      else if (countRest < rTicks) {
        ledcSetup(BUZZER1_CH, NOTEOFF, LEDC_RESOLUTION);
        ledcWrite(BUZZER1_CH, DUTY);
        countRest++;
      }
      else {
        countNote = 0;
        countRest = 0;
        channel1Count++;
      }
    } else {
        ledcSetup(BUZZER1_CH, NOTEOFF, LEDC_RESOLUTION);
        ledcWrite(BUZZER1_CH, DUTY);
    }

    // Channel 2
    if (channel2Count < len2) {
      uint32_t pitch  = notes2[channel2Count][0];
      uint32_t nTicks = notes2[channel2Count][1];
      uint32_t rTicks = notes2[channel2Count][2];

      if (countNote2 < nTicks) {
        if (pitch != 0) {
          ledcSetup(BUZZER2_CH, pitch, LEDC_RESOLUTION);
          ledcWrite(BUZZER2_CH, DUTY);
        }
        else {
          ledcSetup(BUZZER2_CH, NOTEOFF, LEDC_RESOLUTION);
          ledcWrite(BUZZER2_CH, DUTY);
        }
        countNote2++;
      }
      else if (countRest2 < rTicks) {
        ledcSetup(BUZZER2_CH, NOTEOFF, LEDC_RESOLUTION);
        ledcWrite(BUZZER2_CH, DUTY);
        countRest2++;
      }
      else {
        countNote2 = 0;
        countRest2 = 0;
        channel2Count++;
      }
    } else {
      ledcSetup(BUZZER2_CH, NOTEOFF, LEDC_RESOLUTION);
      ledcWrite(BUZZER2_CH, DUTY);
    }

    // Channel 3
    if (channel3Count < len3) {
      uint32_t pitch  = notes3[channel3Count][0];
      uint32_t nTicks = notes3[channel3Count][1];
      uint32_t rTicks = notes3[channel3Count][2];

      if (countNote3 < nTicks) {
        if (pitch != 0) {
          ledcSetup(BUZZER3_CH, pitch, LEDC_RESOLUTION);
          ledcWrite(BUZZER3_CH, DUTY);
        }
        else {
          ledcSetup(BUZZER3_CH, NOTEOFF, LEDC_RESOLUTION);
          ledcWrite(BUZZER3_CH, DUTY);
        }
        countNote3++;
      }
      else if (countRest3 < rTicks) {
        ledcSetup(BUZZER3_CH, NOTEOFF, LEDC_RESOLUTION);
        ledcWrite(BUZZER3_CH, DUTY);
        countRest3++;
      }
      else {
        countNote3 = 0;
        countRest3 = 0;
        channel3Count++;
      }
    } else {
      ledcSetup(BUZZER3_CH, NOTEOFF, LEDC_RESOLUTION);
      ledcWrite(BUZZER3_CH, DUTY);
    }

    // Channel 4
    if (channel4Count < len4) {
      uint32_t pitch  = notes4[channel4Count][0];
      uint32_t nTicks = notes4[channel4Count][1];
      uint32_t rTicks = notes4[channel4Count][2];

      if (countNote4 < nTicks) {
        if (pitch != 0) {
          ledcSetup(BUZZER4_CH, pitch, LEDC_RESOLUTION);
          ledcWrite(BUZZER4_CH, DUTY);
        }
        else {
          ledcSetup(BUZZER4_CH, NOTEOFF, LEDC_RESOLUTION);
          ledcWrite(BUZZER4_CH, DUTY);
        }
        countNote4++;
      }
      else if (countRest4 < rTicks) {
        ledcSetup(BUZZER4_CH, NOTEOFF, LEDC_RESOLUTION);
        ledcWrite(BUZZER4_CH, DUTY);
        countRest4++;
      }
      else {
        countNote4 = 0;
        countRest4 = 0;
        channel4Count++;
      }
    } else {
      ledcSetup(BUZZER4_CH, NOTEOFF, LEDC_RESOLUTION);
      ledcWrite(BUZZER4_CH, DUTY);
    }

    delayMicroseconds(1000); // Tempo adjustment in case of simulation lag
  } while (channel1Count < len && channel2Count < len2
          && channel3Count < len3 && channel4Count < len4);

  ledcSetup(BUZZER1_CH, NOTEOFF, LEDC_RESOLUTION);
  ledcWrite(BUZZER1_CH, DUTY);
  ledcSetup(BUZZER2_CH, NOTEOFF, LEDC_RESOLUTION);
  ledcWrite(BUZZER2_CH, DUTY);
  ledcSetup(BUZZER3_CH, NOTEOFF, LEDC_RESOLUTION);
  ledcWrite(BUZZER3_CH, DUTY);
  ledcSetup(BUZZER4_CH, NOTEOFF, LEDC_RESOLUTION);
  ledcWrite(BUZZER4_CH, DUTY);
}

// main.ino or main.cpp
void setup() {
  // initialize buzzer pins
  ledcAttachPin(BUZZER1_PIN, BUZZER1_CH);
  ledcAttachPin(BUZZER2_PIN, BUZZER2_CH);
  ledcAttachPin(BUZZER3_PIN, BUZZER3_CH);
  ledcAttachPin(BUZZER4_PIN, BUZZER4_CH);

  // play midi by passing arrays and their lengths
  playMidi(midi1, ARRAY_LEN(midi1), midi2, ARRAY_LEN2(midi2),
           midi3, ARRAY_LEN3(midi3), midi4, ARRAY_LEN4(midi4));
}

void loop() {
  // nothing to do here
}
