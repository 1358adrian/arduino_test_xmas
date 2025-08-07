// Pin assignments
const int ldrPin = A1;      // Pin connected to LDR
const int potPin = A0;
const int redPin = 11;
const int yellowPin = 10;
const int greenPin = 9;
const int buttonPin = 13;
const int emergencyPin = 2; // Emergency button pin
const int rgbPin = 6;

const int minValDelay = 0;
const int maxValDelay = 10230;
const int minValRgb = 100;  // Adjust this value as needed; 50 recommended
const int maxValRgb = 800;   // Adjust this value as needed; 600 recommended

// Threshold value for light level
int threshold = 700;  // Adjust this value as needed; 500 recommended

unsigned long previousMillis = 0;
bool redLedState = LOW;  // Initial state of the LED (LOW)
bool ledOn = false;   // Indicates whether the LED is on or off
bool restartSequence = false; // Flag to indicate sequence restart
bool emergencyMode = false; // Flag to indicate emergency mode
unsigned long emergencyStartTime = 0;

uint64_t getMillisEmergency = 0;

// Pin definitions for 7 segment
const int dataPin = 5;    // DS Pin of 74HC595
const int latchPin = 4;   // ST_CP Pin of 74HC595
const int clockPin = 3;   // SH_CP Pin of 74HC595

const int digit1 = 12;
const int digit2 = 8;
const int digit3 = 7;

const uint8_t symbols[] = {
  0b00111111,  // 0
  0b00000110,  // 1
  0b01011011,  // 2
  0b01001111,  // 3
  0b01100110,  // 4
  0b01101101,  // 5
  0b01111101,  // 6
  0b00100111,  // 7
  0b01111111,  // 8
  0b01101111,   // 9
};

int count = 0; // for Emergency Mode number of blinks

void displayCount(int num) {
  digitalWrite(digit1, HIGH);
  digitalWrite(digit2, HIGH);
  digitalWrite(digit3, LOW);
  digitalWrite(latchPin, LOW);  // Disable the latch to prevent updates
  shiftOut(dataPin, clockPin, MSBFIRST, symbols[num % 10]);
  digitalWrite(latchPin, HIGH); // Enable the latch to update the LEDs
  digitalWrite(latchPin, LOW);  // Disable the latch to prevent updates
  shiftOut(dataPin, clockPin, MSBFIRST, 0b00000000);
  digitalWrite(latchPin, HIGH); // Enable the latch to update the LEDs
  
  digitalWrite(digit1, HIGH);
  digitalWrite(digit2, LOW);
  digitalWrite(digit3, HIGH);
  digitalWrite(latchPin, LOW);  // Disable the latch to prevent updates
  shiftOut(dataPin, clockPin, MSBFIRST, symbols[(num / 10) % 10]);
  digitalWrite(latchPin, HIGH); // Enable the latch to update the LEDs
  digitalWrite(latchPin, LOW);  // Disable the latch to prevent updates
  shiftOut(dataPin, clockPin, MSBFIRST, 0b00000000);
  digitalWrite(latchPin, HIGH); // Enable the latch to update the LEDs

  digitalWrite(digit1, LOW);
  digitalWrite(digit2, HIGH);
  digitalWrite(digit3, HIGH);
  digitalWrite(latchPin, LOW);  // Disable the latch to prevent updates
  shiftOut(dataPin, clockPin, MSBFIRST, symbols[(num / 100) % 10]);
  digitalWrite(latchPin, HIGH); // Enable the latch to update the LEDs
  digitalWrite(latchPin, LOW);  // Disable the latch to prevent updates
  shiftOut(dataPin, clockPin, MSBFIRST, 0b00000000);
  digitalWrite(latchPin, HIGH); // Enable the latch to update the LEDs
}
// -----------------------------------------------

void emergencyISR() {
  emergencyMode = true;
  emergencyStartTime = millis();
}

void emergencyBlink() {
  bool emergencyFlag = false;
  while (millis() - emergencyStartTime < 60000) { // Blink for 1 minute
    count++;
    getMillisEmergency = millis();
    while(true) {
      digitalWrite(redPin, HIGH);
      displayCount(count);
      if((millis() - getMillisEmergency) >= 250) break;
      if (digitalRead(buttonPin) == HIGH) emergencyFlag = true;
      if (digitalRead(buttonPin) == LOW && emergencyFlag == true) break;
    }
    if (digitalRead(buttonPin) == LOW && emergencyFlag == true) break;
    getMillisEmergency = millis();
    while(true) {
      digitalWrite(redPin, LOW);
      displayCount(count);
      if((millis() - getMillisEmergency) >= 250) break;
      if (digitalRead(buttonPin) == HIGH) emergencyFlag = true;
      if (digitalRead(buttonPin) == LOW && emergencyFlag == true) break;
    }
    if (digitalRead(buttonPin) == LOW && emergencyFlag == true) break;
  }
  count = 0;
  emergencyMode = false;
  emergencyFlag = false;
  digitalWrite(latchPin, LOW);  // Disable the latch to prevent updates
  shiftOut(dataPin, clockPin, MSBFIRST, 0b00000000);
  digitalWrite(latchPin, HIGH); // Enable the latch to update the LEDs
}

// -------------------------------------------
bool buttonReleaseMem = false;
bool manualMode = false;
bool manualExitFlag = false;

unsigned long getMillisHere;

void waitForButtonRelease() {
    getMillisHere = millis();
    while (digitalRead(buttonPin) == LOW || digitalRead(buttonPin) == HIGH) {
        if (digitalRead(buttonPin) == HIGH && !buttonReleaseMem) {
            buttonReleaseMem = true;
            continue;
        }
        if (digitalRead(buttonPin) == LOW && buttonReleaseMem) return;
        if (digitalRead(buttonPin) == LOW && !buttonReleaseMem && millis() - getMillisHere >= 1000) {
            manualMode = false;
            manualExitFlag = true;
            return;
        }
        if (emergencyMode) return;
    }
}

void buttonOverrideManualMode() {
    waitForButtonRelease();
    
    buttonReleaseMem = !buttonReleaseMem;
    getMillisHere = millis();
    waitForButtonRelease();
}

void waitFor(unsigned long duration) {
  unsigned long startTime = millis();
  while (millis() - startTime < duration) {
    if (digitalRead(buttonPin) == LOW && manualMode == false && manualExitFlag == false) {
      manualMode = true;
      return;
    }
    if (digitalRead(buttonPin) == LOW && manualMode == false && manualExitFlag == true) continue;
    if (digitalRead(buttonPin) == HIGH && manualMode == false && manualExitFlag == true) {
      manualExitFlag = false;
      continue;
    }
    if (emergencyMode) return; // if emergencyISR() was triggered here in this loop
  }
}

// ---------------------------------------
void lightSequence() {
  digitalWrite(latchPin, LOW);  // Disable the latch to prevent updates
  shiftOut(dataPin, clockPin, MSBFIRST, 0b00000000);
  digitalWrite(latchPin, HIGH); // Enable the latch to update the LEDs
  while (true) {
    serialPrintLdrAndPot();
    if (emergencyMode) return; // if emergencyISR() was triggered here in this loop

    int ldrValue = analogRead(ldrPin);
    if (ldrValue < threshold) return; // Return to redLightBlink(); see void loop()

    // Update RGB brightness first before executing below
    int brightness = map(ldrValue, minValRgb, maxValRgb, 255, 0);
    if (ldrValue < minValRgb) analogWrite(rgbPin, 255);
    else if (ldrValue > maxValRgb) analogWrite(rgbPin, 0);
    else analogWrite(rgbPin, brightness);

    digitalWrite(redPin, LOW);
    int potValue = analogRead(potPin);
    int delayTime = map(potValue, 0, 1023, minValDelay, maxValDelay);
    Serial.println("Green Light ON");
    digitalWrite(greenPin, HIGH);
    (manualMode == false) ? waitFor(delayTime) : buttonOverrideManualMode();
    digitalWrite(greenPin, LOW);
    Serial.println("Green Light OFF");

    if (emergencyMode) return; // if emergencyISR() was triggered here in this loop
    Serial.println("Yellow Light ON");
    digitalWrite(yellowPin, HIGH);
    (manualMode == false) ? waitFor(1000) : buttonOverrideManualMode();
    digitalWrite(yellowPin, LOW);
    Serial.println("Yellow Light OFF");

    if (emergencyMode) return; // if emergencyISR() was triggered here in this loop
    Serial.println("Red Light ON");
    digitalWrite(redPin, HIGH);
    (manualMode == false) ? waitFor(delayTime) : buttonOverrideManualMode();
    digitalWrite(redPin, LOW);
    Serial.println("Red Light OFF");
  }
}

// --------------------------------------------
void serialPrintLdrAndPot() {
  int potValue = analogRead(potPin);
  int ldrValue = analogRead(ldrPin);
  int delayTime = map(potValue, 0, 1023, minValDelay, maxValDelay);
  int brightness = map(ldrValue, minValRgb, maxValRgb, 255, 0);

  Serial.print("Potentiometer Value: ");
  Serial.print(potValue);
  Serial.print(" - Mapped Delay Time: ");
  Serial.println(delayTime);
  Serial.print("LDR Value: ");
  Serial.print(ldrValue);
  Serial.print(" - Mapped Brightness Value: ");
  Serial.println(brightness);
}

unsigned long memoryMillisForSerialPrint = 0; 

void redLightBlink() {
  digitalWrite(latchPin, LOW);  // Disable the latch to prevent updates
  shiftOut(dataPin, clockPin, MSBFIRST, 0b00000000);
  digitalWrite(latchPin, HIGH); // Enable the latch to update the LEDs
  unsigned long currentMillis = millis();
  int intervalRedLightBlink = 1000;
  if (ledOn && currentMillis - previousMillis >= intervalRedLightBlink) {
    redLedState = LOW;
    ledOn = false;
    previousMillis = currentMillis;
  }
  if (!ledOn && currentMillis - previousMillis >= intervalRedLightBlink) {
    redLedState = HIGH;
    ledOn = true;
    previousMillis = currentMillis;
  }
  digitalWrite(redPin, redLedState);

  // ------------------------------------
  unsigned long getMillisForSerialPrint = millis();

  if (getMillisForSerialPrint - memoryMillisForSerialPrint >= intervalRedLightBlink * 2) {
    memoryMillisForSerialPrint = getMillisForSerialPrint;

    int potValue = analogRead(potPin);
    int delayTime = map(potValue, 0, 1023, minValDelay, maxValDelay);

    serialPrintLdrAndPot();
  }
}

void setup() {
  pinMode(redPin, OUTPUT);
  pinMode(yellowPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(rgbPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(emergencyPin, INPUT_PULLUP);
  pinMode(ldrPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(emergencyPin), emergencyISR, FALLING);

  // Set pins as outputs for 3-digit seven segment
  pinMode(dataPin, OUTPUT);
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);

  pinMode(digit1, OUTPUT);
  pinMode(digit2, OUTPUT);
  pinMode(digit3, OUTPUT);

  // Start serial monitor debugging at 9600 baud
  Serial.begin(9600);
}

void loop() {
  if (emergencyMode) {
    emergencyBlink();
    return;
  }
  int potValue = analogRead(potPin);
  int delayTime = map(potValue, 0, 1023, minValDelay, maxValDelay);

  int ldrValue = analogRead(ldrPin);
  int brightness = map(ldrValue, minValRgb, maxValRgb, 255, 0);
  if (ldrValue < minValRgb) analogWrite(rgbPin, 255);
  else if (ldrValue > maxValRgb) analogWrite(rgbPin, 0);
  else analogWrite(rgbPin, brightness);

  // Check if the LDR value is equal or more than the threshold
  (ldrValue >= threshold) ? lightSequence() : redLightBlink();
}
