#include <Arduino.h> // dont forget to add this in PlatformIO!

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

void playMidi(int pin, const int notes[][3], size_t len){
 for (int i = 0; i < len; i++) {
    tone(pin, notes[i][0]);
    delay(notes[i][1]);
    noTone(pin);
    delay(notes[i][2]);
  }
}
// Generated using https://github.com/ShivamJoker/MIDI-to-Arduino

// main.ino or main.cpp
void setup() {
  // put your setup code here, to run once:
  // play midi by passing pin no., midi, midi len
  //playMidi(11, midi1, ARRAY_LEN(midi1));
}

void loop() {
  // put your main code here, to run repeatedly:
  playMidi(3, midi1, ARRAY_LEN(midi1));
}
