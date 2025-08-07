#include <Servo.h>
#include <Stepper.h>

// ----- Hardware Definitions & Constants -----
#define STEPS_PER_REV 2048
#define IR_SENSOR_PIN 2         // Interrupt pin for object detection
#define LDR_PIN A0              // LDR analog pin for brightness detection
#define ULTRASONIC_TRIG 6       // Ultrasonic sensor trigger pin
#define ULTRASONIC_ECHO 7       // Ultrasonic sensor echo pin
#define POTENTIOMETER_PIN A1    // Potentiometer for threshold adjustment
#define SERVO_PIN 3             // Servo motor control pin
#define BUZZER_PIN 13           // Buzzer pin
#define LED_RED 12              // LED for dark objects
#define LED_YELLOW 4            // LED for bright-large objects
#define LED_GREEN 5             // LED for bright-small objects

// ----- Create Motor Objects -----
Stepper stepperMotor(STEPS_PER_REV, 8, 9, 10, 11);
Servo myServo;

// ----- Global Variables -----
volatile bool objectDetected = false; // Set true in ISR

// Define a state machine for non-blocking operations:
enum SortingState { IDLE, ROTATE, SERVO_PUSH, SERVO_RETRACT, COMPLETE };
SortingState currentState = IDLE;

// Timing variables:
unsigned long stateStartTime = 0;
unsigned long lastStepTime = 0;
const unsigned long stepInterval = 1;      // 10ms between stepper motor steps
const unsigned long pushDelay     = 500;      // Servo push duration in ms
const unsigned long retractDelay  = 500;      // Servo retract duration in ms
const unsigned long buzzerDelay   = 300;      // Buzzer alert duration in ms

// Sorting variables:
int binNumber = 0;        // Bin index (1 to 4)
int targetPos = 0;        // Target position in stepper steps (0, 50, 100, or 150)
int stepsToMove = 0;      // Steps remaining to rotate the platform
int stepDirection = 1;    // 1 for forward, -1 for reverse rotation
int currentPosition = 0;  // Current stepper position (in steps)

// ----- Function Prototypes -----
void IRsensorInterrupt();
float measureDistance();

void setup() {
  // Initialize serial communication
  Serial.begin(9600);

  // Set up pin modes
  pinMode(ULTRASONIC_TRIG, OUTPUT);
  pinMode(ULTRASONIC_ECHO, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(IR_SENSOR_PIN, INPUT_PULLUP);

  // Attach servo
  myServo.attach(SERVO_PIN);
  myServo.write(0); // Initial servo position (retracted)

  // Attach external interrupt for the IR sensor (object detection)
  attachInterrupt(digitalPinToInterrupt(IR_SENSOR_PIN), IRsensorInterrupt, FALLING);

  // Initialize stepper motor (you may adjust speed if needed)
  stepperMotor.setSpeed(11); // RPM (this is used by some libraries for delay calculation)

  Serial.println("System Initialized. Waiting for objects...");
}

void loop() {
  // --- Main State Machine ---
  switch (currentState) {
    case IDLE:
      // Wait for an object to be detected via the IR sensor interrupt
      if (objectDetected) {
        // Clear the detection flag
        objectDetected = false;

        // Turn on only red LED in IDLE enum with objectDetected = true
        digitalWrite(LED_RED, HIGH);
        digitalWrite(LED_YELLOW, LOW);
        digitalWrite(LED_GREEN, LOW);

        // --- Sensor Readings ---
        int brightness = analogRead(LDR_PIN);
        int potValue = analogRead(POTENTIOMETER_PIN);
        // Map potentiometer reading to a distance threshold (in cm)
        int distanceThreshold = map(potValue, 0, 1023, 10, 30); // 0 is 10 cm; 1023 is 30 cm
        float distance = measureDistance();

        Serial.print("Brightness: ");
        Serial.print(brightness);
        Serial.print(" | Distance: ");
        Serial.print(distance);
        Serial.print(" cm | Thresholds: ");
        Serial.print(distanceThreshold / 2);
        Serial.print(" cm and ");
        Serial.print(distanceThreshold);
        Serial.println(" cm");

        // --- Classification Logic ---
        // For demonstration, assume a fixed brightness threshold (512)
        if (brightness > 800) {
          // Bright object
          if (distance < distanceThreshold / 2)
            binNumber = 1;  // Bright & small
          else if (distance >= distanceThreshold / 2 && distance < distanceThreshold)
            binNumber = 2;  // Bright & medium
          else
            binNumber = 3;  // Bright & large
        } else {
          // Dark object
          if (distance < distanceThreshold / 2)
            binNumber = 4;  // Dark & small
          else if (distance >= distanceThreshold / 2 && distance < distanceThreshold)
            binNumber = 5;  // Dark & medium
          else
            binNumber = 6;  // Dark & large
        }

        Serial.print("Object classified to bin: ");
        Serial.println(binNumber);

        // --- Calculate Target Position for Stepper Motor ---
        // Assume 6 bins evenly spaced on a full revolution:
        // Bin 1: 0 steps, Bin 2: 33 steps, Bin 3: 66 steps, Bin 4: 100 steps, Bin 5: 133 steps, Bin 6: 166 steps
        targetPos = (binNumber - 1) * (STEPS_PER_REV / 6);
        stepsToMove = targetPos - currentPosition;
        // Determine direction (1 for forward, -1 for reverse)
        stepDirection = (stepsToMove >= 0) ? 32 : -32;
        stepsToMove = abs(stepsToMove);

        // Record state start time and move to ROTATE state
        lastStepTime = millis();
        currentState = ROTATE;
      } else {
        // Turn on only green LED in IDLE enum with objectDetected = false
        digitalWrite(LED_RED, LOW);
        digitalWrite(LED_YELLOW, LOW);
        digitalWrite(LED_GREEN, HIGH);
      }
      break;

    case ROTATE:
      // Non-blocking step-by-step rotation of the stepper motor
      if (stepsToMove > 0) {
        if (millis() - lastStepTime >= stepInterval) {
          stepperMotor.step(stepDirection); // Move one step
          currentPosition += (stepDirection);
          stepsToMove-=32;
          lastStepTime = millis();
        }
      } else {
        // Rotation complete: record time and move to SERVO_PUSH state
        stateStartTime = millis();
        currentState = SERVO_PUSH;
        Serial.println("Rotation complete. Activating servo to push object...");
      }
      break;

    case SERVO_PUSH:
      // Turn on only yellow LED during servo movements
      digitalWrite(LED_RED, LOW);
      digitalWrite(LED_YELLOW, HIGH);
      digitalWrite(LED_GREEN, LOW);  

      // Activate servo to push the object into the bin
      myServo.write(90); // Move servo to push position
      if (millis() - stateStartTime >= pushDelay) {
        // After the push delay, move to the next state
        stateStartTime = millis();
        currentState = SERVO_RETRACT;
      }
      break;

    case SERVO_RETRACT:
      // Retract the servo after pushing the object
      myServo.write(0); // Retract servo
      if (millis() - stateStartTime >= retractDelay) {
        // Move to COMPLETE state after retract delay
        stateStartTime = millis();
        currentState = COMPLETE;
      }
      break;

    case COMPLETE:
      // Signal sorting complete by activating the buzzer briefly
      tone(BUZZER_PIN, 1000, 400);
      if (millis() - stateStartTime >= buzzerDelay) {
        noTone(BUZZER_PIN);
        // Turn on only green LED in COMPLETE enum
        digitalWrite(LED_RED, LOW);
        digitalWrite(LED_YELLOW, LOW);
        digitalWrite(LED_GREEN, HIGH);

        Serial.println("Sorting cycle complete. Ready for next object.");
        currentState = IDLE;  // Return to idle and wait for next object
        // Clear the detection flag
        objectDetected = false;
      }
      break;
  }

  // Other tasks can be performed here in a non-blocking way...
}

// ----- Interrupt Service Routine -----
// This ISR is triggered when the IR sensor detects an object.
void IRsensorInterrupt() {
  objectDetected = true;
}

// ----- Function to measure distance using the ultrasonic sensor -----
float measureDistance() {
  // Ensure trigger is LOW
  digitalWrite(ULTRASONIC_TRIG, LOW);
  delayMicroseconds(2);
  // Trigger the ultrasonic pulse
  digitalWrite(ULTRASONIC_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC_TRIG, LOW);

  // Read the echo time (in microseconds)
  long duration = pulseIn(ULTRASONIC_ECHO, HIGH);
  // Calculate distance in centimeters:
  // Speed of sound ~0.034 cm/us; divide by 2 for the round-trip
  float distance = duration * 0.034 / 2;
  return distance;
}
