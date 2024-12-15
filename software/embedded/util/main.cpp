#include "Adafruit_AS726x.h"
#include "Arduino.h"
#include "pins_arduino.h"
#include <Wire.h>

#include "Servo.h"

// # pins
// ## photoresistor
// this gets dealt with as if it's part of the color sensor
#define PHOTORESISTOR_PIN A0
// ## steppers
#define DMODE0_PIN 26
#define DMODE1_PIN 27
#define DMODE2_PIN 28
#define STEPEN_PIN 29
#define STEP0_PIN 6 // bead gear
#define DIR0_PIN 7
#define STEP1_PIN 10 // x axis
#define DIR1_PIN 11
#define XSTOP_PIN 3
#define STEP2_PIN 8 // y axis
#define DIR2_PIN 9
#define YSTOP_PIN 2
// ## servos
#define SERVO0_PIN 5
#define SERVO1_PIN 6

// # physical constants
#define NEUTRAL_ANGLE 180
#define DROP_ANGLE 40
#define REJECT_X 10      // TODO: figure out actual value
#define REJECT_Y 10      // TODO: figure out actual value
#define MM_PER_BEAD 6    // TODO: figure out actual value
#define BEAD_OFFSET_X 10 // TODO: figure out actual value
#define BEAD_OFFSET_Y 10 // TODO: figure out actual value
#define STEP_DELAY_MS 5  // TODO: figure out actual value
// 5mm per revolution, 1.8 degrees per step => 200 steps per revolution
// => 40 steps per mm
#define STEPS_PER_MM 40
// #define FLIP_X
// #define FLIP_Y

// # objects
Servo servo0;
Adafruit_AS726x colorSensor;

// # global state
int8_t currentX = 0,
       currentY = 0; // bead indices (not more than 60? in each direction)
bool homed = false; // this is also gated by `bool homed` as EN can only be HIGH
                    // if that's the case

// # function declarations
void endstopInit();
void stepperInit();
void servoInit();
void colorSensorInit();
void toggleBeadGearStepper(bool on);
void homeAxes();
void moveTo(double x, double y);
inline void moveToBead(uint8_t x, uint8_t y);
void dropRoutine();
void parseSerial();

void endstopInit() {
  pinMode(XSTOP_PIN, INPUT_PULLUP);
  pinMode(YSTOP_PIN, INPUT_PULLUP);
}

void stepperInit() {
  // Configure stepper motor pins as output
  pinMode(DMODE0_PIN, OUTPUT);
  pinMode(DMODE1_PIN, OUTPUT);
  pinMode(DMODE2_PIN, OUTPUT);
  pinMode(STEPEN_PIN, OUTPUT);
  pinMode(DIR0_PIN, OUTPUT);
  pinMode(DIR1_PIN, OUTPUT);
  pinMode(DIR2_PIN, OUTPUT);
  pinMode(STEP0_PIN, OUTPUT);
  pinMode(STEP1_PIN, OUTPUT);
  pinMode(STEP2_PIN, OUTPUT);

  // set DMODEs pins
  // per: https://www.pololu.com/product/2973 `Step (and microstep) size`
  // (full steps (for now at least))
  digitalWrite(DMODE0_PIN, LOW);
  digitalWrite(DMODE1_PIN, LOW);
  digitalWrite(DMODE2_PIN, HIGH);

  // set STEPEN to 1
  digitalWrite(STEPEN_PIN, LOW);

  // set DIRs all 0
  digitalWrite(DIR0_PIN, HIGH);
  digitalWrite(DIR1_PIN, LOW);
  digitalWrite(DIR2_PIN, LOW);

  // set STEP to 0
  // triggered by rising edge
  digitalWrite(STEP0_PIN, LOW);
  digitalWrite(STEP1_PIN, LOW);
  digitalWrite(STEP2_PIN, LOW);
}

void servoInit() {
  // Attach the servo object to the pin
  servo0.attach(SERVO0_PIN);

  // Set initial servo position
  servo0.write(NEUTRAL_ANGLE);
}

void colorSensorInit() {
  // Initialize color sensor
  // begin and make sure we can talk to the sensor
  if (!colorSensor.begin()) {
    // Serial.println("could not connect to sensor! Please check your wiring.");
    while (1)
      ;
  }

  colorSensor.drvOn();
  colorSensor.indicateLED(true);

  // set up photoresistor
  pinMode(PHOTORESISTOR_PIN, INPUT);
  // default to 10 bit resolution
  for (int i = 0; i < 10; i++) {
    analogRead(PHOTORESISTOR_PIN);
    delay(10);
  }
}

void toggleBeadGearStepper(bool on) {
  digitalWrite(13, on);
  cli(); // Disable global interrupts
  if (on) {
    // Initialize Timer1 for 100 Hz interrupts
    // Clear configs
    TCCR1A = 0; // Set entire TCCR1A register to 0
    TCCR1B = 0; // Same for TCCR1B
    TCNT1 = 0;  // Initialize counter value to 0

    // Set compare match register to desired timer count
    // f = 16 MHz / (prescaler * (1 + OCR1A))
    // => OCR1A = 16 Mhz / F / prescaler - 1
    //     ^^     ^ XTAL runs at 16*1000^2 so use that
    // 15624 gives 1 Hz
    // 1561 gives 10 Hz
    // 1040 gives 15 Hz
    // 155 gives 100 Hz
    OCR1A = 1040; // Set compare match register for 15 Hz increments

    // Turn on CTC mode (Clear Timer on Compare match)
    TCCR1B |= (1 << WGM12);
    // Set CS12 and CS10 bits for 1024 prescaler
    TCCR1B |= (1 << CS12) | (1 << CS10);
    // Enable timer compare interrupt
    TIMSK1 |= (1 << OCIE1A);

  } else {
    // disable the timer
    // clear configs
    TCCR1A = 0;               // Set entire TCCR1A register to 0
    TCCR1B = 0;               // Same for TCCR1B
    TCNT1 = 0;                // Initialize counter value to 0
    TIMSK1 &= ~(1 << OCIE1A); // disable the interrupt
  }
  sei(); // Reenable global interrupts
}

void homeAxes() {
  // double array where [0] = (DIR1_PIN, STEP1_PIN, XSTOP_PIN), [1] = (DIR2_PIN,
  // STEP2_PIN, YSTOP_PIN)
  int axesConfig[2][3] = {{DIR1_PIN, STEP1_PIN, XSTOP_PIN},
                          {DIR2_PIN, STEP2_PIN, YSTOP_PIN}};

  for (uint8_t axis = 0; axis < 2; axis++) {
    int *axisConfig = axesConfig[axis];
    // we want to home at normal speed and then half speed
    for (uint8_t delayFactor = 1; delayFactor <= 2; delayFactor++) {
      uint8_t waitTime = STEP_DELAY_MS * delayFactor;

      digitalWrite(axisConfig[0], HIGH);
      while (digitalRead(axisConfig[2]) == HIGH) {
        digitalWrite(axisConfig[1], HIGH);
        delay(waitTime);
        digitalWrite(axisConfig[1], LOW);
        delay(waitTime);
      }
      // now back off 5mm
      digitalWrite(axisConfig[0], LOW);
      for (int i = 0; i < 5 * STEPS_PER_MM; i++) {
        digitalWrite(axisConfig[1], HIGH);
        delay(waitTime);
        digitalWrite(axisConfig[1], LOW);
        delay(waitTime);
      }
    }
  }

  // now update variables
  currentX = 5;
  currentY = 5;
  homed = true;

  // go to neutral position
  moveToBead(0, 0);
}

// operates on real world coordinates (mm)
void moveTo(double x, double y) {
  // make sure homed first (should always be the case)
  if (!homed)
    return;

  // Check if there's any movement needed
  if (x == currentX && y == currentY) {
    return;
  }

  // prevent negative (x,y)
  if (x < 0 || y < 0) {
    return;
  }

  // Calculate the number of steps for each axis
  int16_t stepsX = (x - currentX) * STEPS_PER_MM;
  int16_t stepsY = (y - currentY) * STEPS_PER_MM;

  // Adjust direction if needed (uncomment to reverse direction)
#ifdef FLIP_X
  stepsX *= -1;
#endif
#ifdef FLIP_Y
  stepsY *= -1;
#endif

  // Perform the movement
  // handle both axes simulataneously
  digitalWrite(DIR1_PIN, stepsX > 0);
  digitalWrite(DIR2_PIN, stepsY > 0);
  while (stepsX != 0 && stepsY != 0) {
    // steps
    digitalWrite(STEP1_PIN, HIGH);
    digitalWrite(STEP2_PIN, HIGH);
    // wait a bit
    delay(STEP_DELAY_MS);
    // reset
    digitalWrite(STEP1_PIN, LOW);
    digitalWrite(STEP2_PIN, LOW);
    // change counter
    stepsX -= stepsX > 0 ? 1 : -1;
    stepsY -= stepsY > 0 ? 1 : -1;
  }
  // x axis only
  digitalWrite(DIR1_PIN, stepsX > 0);
  while (stepsX != 0) {
    digitalWrite(STEP1_PIN, HIGH);
    delay(STEP_DELAY_MS);
    digitalWrite(STEP1_PIN, LOW);
    stepsX -= stepsX > 0 ? 1 : -1;
  }
  // y axis only
  digitalWrite(DIR2_PIN, stepsY > 0);
  while (stepsY != 0) {
    digitalWrite(STEP2_PIN, HIGH);
    delay(STEP_DELAY_MS);
    digitalWrite(STEP2_PIN, LOW);
    stepsY -= stepsY > 0 ? 1 : -1;
  }

  // Update current position
  currentX = x;
  currentY = y;
}

// converts bead indices to coordinates
inline void moveToBead(uint8_t x, uint8_t y) {
  moveTo(x * MM_PER_BEAD + BEAD_OFFSET_X, y * MM_PER_BEAD + BEAD_OFFSET_Y);
}

void dropRoutine() {
  // make sure at neutral
  servo0.write(NEUTRAL_ANGLE);
  delay(100);

  // trigger drop
  servo0.write(DROP_ANGLE);
  delay(1000);

  // go back to neutral
  servo0.write(NEUTRAL_ANGLE);
  delay(100);
}

void parseSerial() {
  // wait for a byte
  while (!Serial.available())
    ;

  uint8_t buf = Serial.read();

  switch (buf) {
  // 0: start bead gear
  case 0:
    toggleBeadGearStepper(true);
    break;
  // 1: stop bead gear
  case 1:
    toggleBeadGearStepper(false);
    break;
    // 2: move to <x, y [bead indices]>, drop bead
  case 2: {
    while (Serial.available() < 2)
      ;

    // these are bead indices
    uint8_t x = Serial.read();
    uint8_t y = Serial.read();

    moveToBead(x, y);

    dropRoutine();

    break;
  }
  // 3: reject bead
  case 3:
    moveToBead(REJECT_X, REJECT_Y);
    dropRoutine();
    break;
  default:
    break;
  }
}

volatile bool beadGearStepValue = 0;
ISR(TIMER1_COMPA_vect) {
  digitalWrite(STEP0_PIN, beadGearStepValue);
  // digitalWrite(13, beadGearStepValue);
  beadGearStepValue = !beadGearStepValue;
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  // Initialize serial communication
  Serial.begin(115200);
  delay(1000);

  // configure endstops
  // endstopInit();

  // Initialize stepper motors
  // stepperInit();

  // Initialize servo
  servoInit();

  // Initialize color sensor
  colorSensorInit();

  // Home X and Y axes
  // homeAxes();

  Serial.println("test setup done!");
  // toggleBeadGearStepper(true);
}

void loop() {
  while (!Serial.available())
    ;
  String angle_str = Serial.readStringUntil('\n');
  if (angle_str) {
    int angle = angle_str.toInt();
    if (angle < 15)
      return;
    servo0.write(angle);
    delay(1000);
    servo0.write(170);
  }
}
