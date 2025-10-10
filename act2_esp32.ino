#include <Arduino.h>
#include <ESP32Servo.h>

// --------- PIN CONFIGURATION ----------
const int TRIG_PIN     = 14; // HC-SR04 trigger
const int ECHO_PIN     = 27; // HC-SR04 echo
const int SERVO_PIN    = 26; // Servo PWM pin
const int GREEN_LED    = 12; // Door open indicator
const int RED_LED      = 2;  // Door closed indicator
const int BUZZER_PIN   = 25; // Piezo buzzer
const int BUTTON_PIN   = 13; // Push button (use internal pull-up)

// --------- OBJECT DECLARATIONS --------
Servo doorServo;

// --------- GLOBAL VARIABLES -----------
volatile bool timerFlag = false; // Timer interrupt flag
volatile bool doorOpen = false;  // Door state
volatile bool obstacleDetected = false; // Obstacle warning state
volatile bool manualMode = false; // Manual control mode
float distance = 0.0;           // Measured distance
const float DETECTION_RANGE = 20.0; // Detection threshold in cm
const int DOOR_OPEN_TIME = 5000; // 5 seconds door open time

// Button debouncing variables
bool lastButtonState = HIGH;
bool buttonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// --------- TIMER CONFIGURATION --------
hw_timer_t *doorTimer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

// --------- BUZZER CONFIGURATION --------
const int BUZZER_CHANNEL = 2;
const int WARNING_FREQ = 440; // 440Hz warning tone
const int WARNING_FREQ_2 = 550; // 550Hz warning tone

// --------- FUNCTION PROTOTYPES --------
float measureDistance();
void openDoor();
void closeDoor();
void startWarningBuzzer();
void stopWarningBuzzer();
void shortBeep();
bool readButton();
void handleButtonPress();
void exitToAutoMode();
void IRAM_ATTR onTimer();

void setup() {
  Serial.begin(115200);
  
  // Initialize I/O pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP); // Internal pull-up resistor
  
  // Initialize servo
  doorServo.attach(SERVO_PIN);
  
  // Initialize buzzer with LEDC
  ledcSetup(BUZZER_CHANNEL, WARNING_FREQ, 8);
  ledcAttachPin(BUZZER_PIN, BUZZER_CHANNEL);
  
  // Initialize door state (closed)
  closeDoor();
  
  // Configure hardware timer for automatic door closing
  doorTimer = timerBegin(0, 80, true); // Timer 0, prescaler 80 (1MHz), count up
  timerAttachInterrupt(doorTimer, &onTimer, true); // Edge triggered
  timerAlarmWrite(doorTimer, DOOR_OPEN_TIME * 1000, false); // 5 seconds in microseconds
  timerAlarmEnable(doorTimer); // Start timer (but it won't trigger until armed)
  
  Serial.println("Enhanced Smart Door System with Smart Mode Switching");
  Serial.println("Modes: Auto (object <20cm) | Manual (object >20cm + button)");
  Serial.println("Waiting for object detection within 20cm...");
}

void loop() {
  // Measure distance continuously
  distance = measureDistance();
  
  // Read and handle button press
  if (readButton()) {
    handleButtonPress();
  }
  
  // Check if object detected in manual mode - exit to auto mode
  if (manualMode && distance <= DETECTION_RANGE && distance > 0) {
    Serial.println("Object detected in manual mode - Switching to auto mode");
    exitToAutoMode();
  }
  
  // Automatic mode: Check for object detection to open door
  if (!manualMode && distance <= DETECTION_RANGE && distance > 0) {
    if (!doorOpen && !obstacleDetected) {
      Serial.println("Object detected! Opening door...");
      openDoor();
      
      // Arm the timer for automatic closing
      portENTER_CRITICAL(&timerMux);
      timerFlag = false;
      portEXIT_CRITICAL(&timerMux);
      
      timerWrite(doorTimer, 0); // Reset timer counter
      timerAlarmEnable(doorTimer); // Enable timer interrupt
    }
  }
  
  // Check timer flag for automatic door closing (only in auto mode)
  portENTER_CRITICAL(&timerMux);
  if (timerFlag && doorOpen && !manualMode) {
    timerFlag = false;
    portEXIT_CRITICAL(&timerMux);
    
    // Check if obstacle is still present before closing
    float currentDistance = measureDistance();
    if (currentDistance <= DETECTION_RANGE && currentDistance > 0) {
      // Obstacle still detected - keep door open and sound warning
      obstacleDetected = true;
      Serial.println("OBSTACLE DETECTED! Cannot close door - Warning buzzer activated");
      startWarningBuzzer();
      
      // Reset timer to check again in 2 seconds
      timerWrite(doorTimer, 0);
      timerAlarmEnable(doorTimer);
    } else {
      // No obstacle - safe to close door
      Serial.println("Timer expired! Closing door...");
      closeDoor();
      obstacleDetected = false;
      timerAlarmDisable(doorTimer); // Disable timer until next opening
    }
  } else {
    portEXIT_CRITICAL(&timerMux);
  }
  
  // Handle obstacle warning state
  if (obstacleDetected && !manualMode) {
    // Continuously check if obstacle has cleared
    float currentDistance = measureDistance();
    
    if (currentDistance > DETECTION_RANGE || currentDistance <= 0) {
      // Obstacle cleared - stop warning and close door
      Serial.println("Obstacle cleared! Closing door...");
      stopWarningBuzzer();
      closeDoor();
      obstacleDetected = false;
      timerAlarmDisable(doorTimer);
    } else {
      // Obstacle still present - continue warning
      // Blink green LED to indicate warning state
      static unsigned long lastBlink = 0;
      static bool ledState = false;
      
      if (millis() - lastBlink > 500) { // Blink every 500ms
        ledState = !ledState;
        digitalWrite(GREEN_LED, ledState);
        lastBlink = millis();
      }
    }
  }
  
  // Small delay to prevent excessive measurements
  delay(100);
}

// Timer Interrupt Service Routine
void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&timerMux);
  timerFlag = true;
  portEXIT_CRITICAL_ISR(&timerMux);
}

// Function to measure distance using ultrasonic sensor
float measureDistance() {
  // Clear trigger pin
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  
  // Send 10us pulse to trigger pin
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  // Read echo pin - returns sound wave travel time in microseconds
  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // Timeout after 30ms
  
  // Calculate distance in cm
  float calculatedDistance = duration * 0.0343 / 2;
  
  // Filter out invalid readings
  if (calculatedDistance <= 0 || calculatedDistance > 400) {
    return 400.0; // Return maximum range for invalid readings
  }
  
  return calculatedDistance;
}

// Function to open the door
void openDoor() {
  doorServo.write(180); // Rotate servo to 180 degrees (open)
  digitalWrite(GREEN_LED, HIGH); // Turn on green LED
  digitalWrite(RED_LED, LOW);    // Turn off red LED
  doorOpen = true;
  obstacleDetected = false;
  
  Serial.println("Door opened - Green LED ON");
}

// Function to close the door
void closeDoor() {
  doorServo.write(0); // Rotate servo to 0 degrees (closed)
  digitalWrite(GREEN_LED, LOW);  // Turn off green LED
  digitalWrite(RED_LED, HIGH);   // Turn on red LED
  doorOpen = false;
  obstacleDetected = false;
  stopWarningBuzzer();
  
  Serial.println("Door closed - Red LED ON");
}

// Function to start warning buzzer
void startWarningBuzzer() {
  ledcWriteTone(BUZZER_CHANNEL, WARNING_FREQ);
  Serial.println("Warning buzzer activated (440Hz)");
}

// Function to stop warning buzzer
void stopWarningBuzzer() {
  ledcWriteTone(BUZZER_CHANNEL, 0);
  Serial.println("Warning buzzer deactivated");
}

// Function for short beep alert
void shortBeep() {
  ledcWriteTone(BUZZER_CHANNEL, WARNING_FREQ_2);
  delay(200); // 200ms beep
  ledcWriteTone(BUZZER_CHANNEL, 0);
  Serial.println("Short beep alert (550Hz)");
}

// Function to read button with debouncing
bool readButton() {
  bool reading = digitalRead(BUTTON_PIN);
  
  // Check if button state changed (due to noise or pressing)
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    // Whatever the reading is at, it's been there longer than the debounce delay
    if (reading != buttonState) {
      buttonState = reading;
      
      // If the button was pressed (LOW because of pull-up)
      if (buttonState == LOW) {
        lastButtonState = reading;
        return true;
      }
    }
  }
  
  lastButtonState = reading;
  return false;
}

// Function to handle button press logic
void handleButtonPress() {
  // If in AUTO mode and object is detected nearby (<20cm), deny manual operation
  if (!manualMode && distance <= DETECTION_RANGE && distance > 0) {
    Serial.println("Button denied: Auto mode active with object nearby");
    shortBeep();
    return;
  }
  
  // Button only works when object is >20cm OR in manual mode
  if (distance > DETECTION_RANGE || manualMode) {
    if (!doorOpen) {
      // Closed state: Open door and enter manual mode
      Serial.println("Button pressed: Opening door manually");
      openDoor();
      manualMode = true;
      timerAlarmDisable(doorTimer); // Disable auto-close timer in manual mode
    } else {
      // Opened state: Close door and toggle manual mode
      Serial.println("Button pressed: Closing door manually");
      closeDoor();
      manualMode = true; // Stay in manual mode for next toggle
    }
  } else if (doorOpen && distance <= DETECTION_RANGE) {
    // Object was detected and door opened automatically, but object removed before timer
    // Button press closes door immediately
    Serial.println("Button pressed: Closing door immediately (object cleared)");
    closeDoor();
    manualMode = true; // Enter manual mode after button intervention
    timerAlarmDisable(doorTimer); // Disable the running timer
  }
  
  Serial.print("Manual mode: ");
  Serial.println(manualMode ? "ON" : "OFF");
}

// Function to exit manual mode and return to auto mode
void exitToAutoMode() {
  manualMode = false;
  Serial.println("Exiting manual mode - Returning to auto mode");
  
  // Ensure door is open when object is detected
  if (!doorOpen) {
    openDoor();
  }
  
  // Arm the timer for automatic closing
  portENTER_CRITICAL(&timerMux);
  timerFlag = false;
  portEXIT_CRITICAL(&timerMux);
  
  timerWrite(doorTimer, 0); // Reset timer counter
  timerAlarmEnable(doorTimer); // Enable timer interrupt
  
  Serial.println("Auto mode reactivated - Door will close automatically in 5 seconds");
}
