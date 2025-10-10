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
volatile bool systemLockout = false; // System lockout state
float distance = 0.0;           // Measured distance
const float DETECTION_RANGE = 20.0; // Detection threshold in cm
const int DOOR_OPEN_TIME = 5000; // 5 seconds door open time

// False trigger detection variables
unsigned int falseTriggerCount = 0;
const unsigned int MAX_FALSE_TRIGGERS = 3;

// Rapid button press detection - FIXED
unsigned long lastButtonReleaseTime = 0;
const unsigned long RAPID_PRESS_THRESHOLD = 500; // Time between release and next press
bool buttonPressed = false;

// Sensor stability detection
unsigned long lastDistanceChangeTime = 0;
float lastStableDistance = 0.0;
bool lastDistanceState = false; // false = above 20cm, true = below 20cm
const unsigned long SENSOR_STABILITY_THRESHOLD = 200; // 200ms

// Display and timing variables
unsigned long lastDisplayTime = 0;
const unsigned long DISPLAY_INTERVAL = 1000; // Update display every 1 second
unsigned long lastDistanceLog = 0;
const unsigned long DISTANCE_LOG_INTERVAL = 1000; // Log distance every 1 second
unsigned long lockoutStartTime = 0;
const unsigned long LOCKOUT_DURATION = 10000; // 10 seconds lockout

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
const int WARNING_FREQ = 440;   // 440Hz 5-second timer lapsed warning tone
const int WARNING_FREQ_2 = 550; // 550Hz door close attempt warning tone
const int WARNING_FREQ_3 = 660; // 660Hz false trigger alert
const int WARNING_FREQ_4 = 880; // 880Hz lockout alert

// --------- FUNCTION PROTOTYPES --------
float measureDistance();
void openDoor();
void closeDoor();
void startWarningBuzzer();
void stopWarningBuzzer();
void shortBeep();
void falseTriggerAlert();
void lockoutAlert();
void checkFalseTriggers();
bool readButton();
void handleButtonPress();
void exitToAutoMode();
void enterLockoutMode();
void exitLockoutMode();
void displaySystemStatus();
void logDistanceReading();
void logEvent(const char* event, const char* details = "");
void logWarning(const char* warning);
void logError(const char* error);
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
  
  // Initialize distance state
  lastStableDistance = measureDistance();
  lastDistanceState = (lastStableDistance <= DETECTION_RANGE);
  
  // Configure hardware timer for automatic door closing
  doorTimer = timerBegin(0, 80, true); // Timer 0, prescaler 80 (1MHz), count up
  timerAttachInterrupt(doorTimer, &onTimer, true); // Edge triggered
  timerAlarmWrite(doorTimer, DOOR_OPEN_TIME * 1000, false); // 5 seconds in microseconds
  timerAlarmEnable(doorTimer); // Start timer (but it won't trigger until armed)
  
  Serial.println("\n================================================");
  Serial.println("    ENHANCED SMART DOOR SYSTEM INITIALIZED");
  Serial.println("================================================");
  logEvent("SYSTEM", "Smart Door System Started - False Trigger Protection Active");
  displaySystemStatus();
  Serial.println("Waiting for object detection within 20cm...");
  Serial.println("------------------------------------------------");
}

void loop() {
  // Check if system is in lockout mode
  if (systemLockout) {
    if (millis() - lockoutStartTime >= LOCKOUT_DURATION) {
      exitLockoutMode();
    } else {
      // Continue lockout - blink both LEDs to indicate lockout
      static unsigned long lastLockoutBlink = 0;
      static bool lockoutLedState = false;
      
      if (millis() - lastLockoutBlink > 500) {
        lockoutLedState = !lockoutLedState;
        digitalWrite(GREEN_LED, lockoutLedState);
        digitalWrite(RED_LED, lockoutLedState);
        lastLockoutBlink = millis();
      }
      return; // Skip all other processing during lockout
    }
  }
  
  // Measure distance continuously
  distance = measureDistance();
  
  // Check for false triggers
  checkFalseTriggers();
  
  // Log distance readings periodically
  logDistanceReading();
  
  // Display system status periodically
  if (millis() - lastDisplayTime >= DISPLAY_INTERVAL) {
    displaySystemStatus();
    lastDisplayTime = millis();
  }
  
  // Read and handle button press
  if (readButton()) {
    handleButtonPress();
  }
  
  // Check if object detected in manual mode - exit to auto mode
  if (manualMode && distance <= DETECTION_RANGE && distance > 0) {
    logEvent("MODE SWITCH", "Object detected in manual mode - Switching to auto mode");
    exitToAutoMode();
  }
  
  // Automatic mode: Check for object detection to open door
  if (!manualMode && distance <= DETECTION_RANGE && distance > 0) {
    if (!doorOpen && !obstacleDetected) {
      logEvent("AUTO TRIGGER", "Object detected! Opening door...");
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
      logWarning("OBSTACLE DETECTED! Cannot close door - Warning buzzer activated");
      startWarningBuzzer();
      
      // Reset timer to check again in 2 seconds
      timerWrite(doorTimer, 0);
      timerAlarmEnable(doorTimer);
    } else {
      // No obstacle - safe to close door
      logEvent("TIMER", "Timer expired! Closing door...");
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
      logEvent("SAFETY", "Obstacle cleared! Closing door...");
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
  if (systemLockout) return; // Ignore during lockout
  
  doorServo.write(180); // Rotate servo to 180 degrees (open)
  digitalWrite(GREEN_LED, HIGH); // Turn on green LED
  digitalWrite(RED_LED, LOW);    // Turn off red LED
  doorOpen = true;
  obstacleDetected = false;
  
  logEvent("DOOR", "Door opened - Green LED ON");
}

// Function to close the door
void closeDoor() {
  if (systemLockout) return; // Ignore during lockout
  
  doorServo.write(0); // Rotate servo to 0 degrees (closed)
  digitalWrite(GREEN_LED, LOW);  // Turn off green LED
  digitalWrite(RED_LED, HIGH);   // Turn on red LED
  doorOpen = false;
  obstacleDetected = false;
  stopWarningBuzzer();
  
  logEvent("DOOR", "Door closed - Red LED ON");
}

// Function to start warning buzzer
void startWarningBuzzer() {
  ledcWriteTone(BUZZER_CHANNEL, WARNING_FREQ);
  logEvent("BUZZER", "Warning buzzer activated (440Hz)");
}

// Function to stop warning buzzer
void stopWarningBuzzer() {
  ledcWriteTone(BUZZER_CHANNEL, 0);
  logEvent("BUZZER", "Warning buzzer deactivated");
}

// Function for short beep alert
void shortBeep() {
  ledcWriteTone(BUZZER_CHANNEL, WARNING_FREQ_2);
  delay(200); // 200ms beep
  ledcWriteTone(BUZZER_CHANNEL, 0);
  logEvent("ALERT", "Short beep alert (550Hz) - Button denied");
}

// Function for false trigger alert
void falseTriggerAlert() {
  ledcWriteTone(BUZZER_CHANNEL, WARNING_FREQ_3);
  delay(300); // 300ms beep
  ledcWriteTone(BUZZER_CHANNEL, 0);
  Serial.print("⚠️ [WARNING] ");
  Serial.println("False trigger detected! Count: " + String(falseTriggerCount) + "/" + String(MAX_FALSE_TRIGGERS));
}

// Function for lockout alert
void lockoutAlert() {
  ledcWriteTone(BUZZER_CHANNEL, WARNING_FREQ_4);
  logError("SYSTEM LOCKOUT ACTIVATED - Ignoring all inputs for 10 seconds");
}

// Function to check for false triggers
void checkFalseTriggers() {
  // Check for inconsistent ultrasonic sensor readings
  bool currentDistanceState = (distance <= DETECTION_RANGE && distance > 0);
  
  if (currentDistanceState != lastDistanceState) {
    if (lastDistanceChangeTime > 0 && (millis() - lastDistanceChangeTime) < SENSOR_STABILITY_THRESHOLD) {
      // Rapid state change detected
      falseTriggerCount++;
      logWarning("Sensor instability detected - Rapid state change");
      falseTriggerAlert();
    }
    lastDistanceState = currentDistanceState;
    lastDistanceChangeTime = millis();
  }
  
  // Check if false trigger threshold reached
  if (falseTriggerCount >= MAX_FALSE_TRIGGERS) {
    enterLockoutMode();
  }
}

// Function to read button with debouncing and rapid press detection - FIXED
bool readButton() {
  if (systemLockout) return false; // Ignore buttons during lockout
  
  bool reading = digitalRead(BUTTON_PIN);
  
  // Check if button state changed (due to noise or pressing)
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    // Whatever the reading is at, it's been there longer than the debounce delay
    if (reading != buttonState) {
      buttonState = reading;
      
      if (buttonState == LOW) {
        // Button pressed (falling edge)
        lastButtonState = reading;
        buttonPressed = true;
        
        // Check for rapid press: time between release and next press < 100ms
        if (lastButtonReleaseTime > 0 && (millis() - lastButtonReleaseTime) < RAPID_PRESS_THRESHOLD) {
          falseTriggerCount++;
          logWarning("Rapid button press detected");
          falseTriggerAlert();
        }
        
        return true;
      } else {
        // Button released (rising edge)
        lastButtonReleaseTime = millis();
        buttonPressed = false;
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
    logWarning("Button denied: Auto mode active with object nearby");
    shortBeep();
    return;
  }
  
  // Button only works when object is >20cm OR in manual mode
  if (distance > DETECTION_RANGE || manualMode) {
    if (!doorOpen) {
      // Closed state: Open door and enter manual mode
      logEvent("BUTTON", "Opening door manually");
      openDoor();
      manualMode = true;
      timerAlarmDisable(doorTimer); // Disable auto-close timer in manual mode
    } else {
      // Opened state: Close door and toggle manual mode
      logEvent("BUTTON", "Closing door manually");
      closeDoor();
      manualMode = true; // Stay in manual mode for next toggle
    }
  } else if (doorOpen && distance <= DETECTION_RANGE) {
    // Object was detected and door opened automatically, but object removed before timer
    // Button press closes door immediately
    logEvent("BUTTON", "Closing door immediately (object cleared)");
    closeDoor();
    manualMode = true; // Enter manual mode after button intervention
    timerAlarmDisable(doorTimer); // Disable the running timer
  }
  
  logEvent("MODE", manualMode ? "Manual mode ON" : "Auto mode ON");
}

// Function to exit manual mode and return to auto mode
void exitToAutoMode() {
  manualMode = false;
  logEvent("MODE SWITCH", "Exiting manual mode - Returning to auto mode");
  
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
  
  logEvent("TIMER", "Auto mode reactivated - Door will close automatically in 5 seconds");
}

// Function to enter lockout mode
void enterLockoutMode() {
  systemLockout = true;
  lockoutStartTime = millis();
  falseTriggerCount = 0; // Reset counter
  lockoutAlert();
  
  // Stop any ongoing operations
  timerAlarmDisable(doorTimer);
}

// Function to exit lockout mode
void exitLockoutMode() {
  systemLockout = false;
  stopWarningBuzzer();
  
  // Reset all false trigger detection variables
  falseTriggerCount = 0;
  lastButtonReleaseTime = 0;
  lastDistanceChangeTime = 0;
  
  logEvent("SYSTEM", "Lockout period ended. Resuming normal operation.");
  
  // Restore LED states based on door state
  if (doorOpen) {
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(RED_LED, LOW);
  } else {
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, HIGH);
  }
}

// Function to display comprehensive system status
void displaySystemStatus() {
  Serial.println("\n=== SYSTEM STATUS ===");
  Serial.print("Mode: ");
  Serial.println(manualMode ? "MANUAL" : "AUTO");
  Serial.print("Door: ");
  Serial.println(doorOpen ? "OPEN" : "CLOSED");
  Serial.print("Obstacle: ");
  Serial.println(obstacleDetected ? "DETECTED" : "CLEAR");
  Serial.print("Lockout: ");
  Serial.println(systemLockout ? "ACTIVE" : "INACTIVE");
  Serial.print("False Triggers: ");
  Serial.print(falseTriggerCount);
  Serial.print("/");
  Serial.println(MAX_FALSE_TRIGGERS);
  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");
  Serial.print("Detection Range: ");
  Serial.print(DETECTION_RANGE);
  Serial.println(" cm");
  Serial.print("Timer: ");
  Serial.println(timerAlarmEnabled(doorTimer) ? "ACTIVE" : "INACTIVE");
  if (systemLockout) {
    Serial.print("Lockout Time Remaining: ");
    Serial.print((LOCKOUT_DURATION - (millis() - lockoutStartTime)) / 1000);
    Serial.println(" seconds");
  }
  Serial.println("===================");
}

// Function to log distance readings
void logDistanceReading() {
  if (millis() - lastDistanceLog >= DISTANCE_LOG_INTERVAL) {
    Serial.print("[SENSOR] Distance: ");
    Serial.print(distance);
    Serial.print(" cm - ");
    if (distance <= DETECTION_RANGE && distance > 0) {
      Serial.println("OBJECT DETECTED");
    } else if (distance > DETECTION_RANGE) {
      Serial.println("CLEAR");
    } else {
      Serial.println("INVALID READING");
    }
    lastDistanceLog = millis();
  }
}

// Function to log general events
void logEvent(const char* event, const char* details) {
  Serial.print("[");
  Serial.print(event);
  Serial.print("] ");
  Serial.println(details);
}

// Function to log warnings
void logWarning(const char* warning) {
  Serial.print("⚠️ [WARNING] ");
  Serial.println(warning);
}

// Function to log errors
void logError(const char* error) {
  Serial.print("🚨 [ERROR] ");
  Serial.println(error);
}
