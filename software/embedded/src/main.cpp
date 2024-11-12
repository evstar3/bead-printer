#include "Adafruit_AS726x.h"
#include "Arduino.h"
#include "pins_arduino.h"
#include <Wire.h>

#include "Servo.h"

// # pins
// ## steppers
#define DMODE0_PIN 26
#define DMODE1_PIN 27
#define DMODE2_PIN 28
#define STEPEN_PIN 29
#define DIR0_PIN 30
#define DIR1_PIN 31
#define DIR2_PIN 32
#define STEP0_PIN 9  // bead gear
#define STEP1_PIN 10 // x axis
#define STEP2_PIN 11 // y axis
// ## servos
#define SERVO0_PIN 5
#define SERVO1_PIN 6

// # physical constants
#define NEUTRAL_ANGLE 0 // TODO: figure out actual value
#define DROP_ANGLE 180  // TODO: figure out actual value
#define REJECT_X 10     // TODO: figure out actual value
#define REJECT_Y 10     // TODO: figure out actual value
#define MM_PER_BEAD 6   // TODO: figure out actual value
#define STEP_DELAY_MS 5 // TODO: figure out actual value
// 5mm per revolution, 1.8 degrees per step => 200 steps per revolution
// => 40 steps per mm
#define STEPS_PER_MM 40

// # objects
Servo servo0;
Adafruit_AS726x colorSensor;

// # global state
int8_t currentX = 0,
       currentY = 0; // bead indices (not more than 60? in each direction)

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
  digitalWrite(STEPEN_PIN, HIGH);

  // set DIRs all 0
  digitalWrite(DIR0_PIN, LOW);
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

  // sweep twice?
  int pos = 0;
  for (pos = 0; pos <= 180; pos += 1) { // goes from 0 degrees to 180 degrees
    // in steps of 1 degree
    servo0.write(pos); // tell servo to go to position in variable 'pos'
    delay(15);         // waits 15 ms for the servo to reach the position
  }
  for (pos = 180; pos >= 0; pos -= 1) { // goes from 180 degrees to 0 degrees
    servo0.write(pos); // tell servo to go to position in variable 'pos'
    delay(15);         // waits 15 ms for the servo to reach the position
  }

  // Set initial servo position
  servo0.write(NEUTRAL_ANGLE);
}

void toggleBeadGearStepper(bool on) {
  // digitalWrite(13, on);
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
    // 16383 gives 1 Hz
    // 1637 gives 10 Hz
    // 162 gives 100 Hz
    OCR1A = 1637; // Set compare match register for 10 Hz increments
    // OCR1A = 162; // Set compare match register for 10 Hz increments
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

void colorSensorInit() {
  // Initialize color sensor
  // begin and make sure we can talk to the sensor
  if (!colorSensor.begin()) {
    // Serial.println("could not connect to sensor! Please check your wiring.");
    while (1)
      ;
  }

  // colorSensor.drvOn();
  colorSensor.indicateLED(true);
}

// operates on bead indices
void moveTo(uint8_t x, uint8_t y) {
  // Check if there's any movement needed
  if (x == currentX && y == currentY) {
    return;
  }

  // Calculate the number of steps for each axis
  int16_t stepsX = (x - currentX) * MM_PER_BEAD * STEPS_PER_MM;
  int16_t stepsY = (y - currentY) * MM_PER_BEAD * STEPS_PER_MM;

  // Adjust direction if needed (uncomment to reverse direction)
  // stepsX *= -1;
  // stepsY *= -1;

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

    moveTo(x, y);

    dropRoutine();

    break;
  }
  // 3: reject bead
  case 3:
    moveTo(REJECT_X, REJECT_Y);
    dropRoutine();
    break;
  }
}

volatile bool beadGearStepValue = 0;
// void updateBeadGearStepper() {
ISR(TIMER1_COMPA_vect) {
  digitalWrite(STEP0_PIN, beadGearStepValue);
  digitalWrite(13, beadGearStepValue);
  beadGearStepValue = !beadGearStepValue;
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  // Initialize serial communication
  Serial.begin(115200);
  delay(1000);
  Serial.println("hello");

  // Initialize stepper motors
  stepperInit();

  // Initialize servo
  // servoInit();

  // Initialize color sensor
  // colorSensorInit();

  Serial.println("Setup done!");
}

void loop() {
  // float colorSensorData[AS726x_NUM_CHANNELS];
  //
  // colorSensor.startMeasurement(); // begin a measurement
  // // TODO: ^^ ideally it's in continuous mode?
  //
  // // wait till data is available
  // while (!colorSensor.dataReady())
  //   delay(5);
  //
  // colorSensor.readCalibratedValues(colorSensorData);
  //
  // // send the color values as raw bytes (6 x f32)
  // Serial.write((uint8_t *)colorSensorData, sizeof(float) *
  // AS726x_NUM_CHANNELS);

  // this blocks on a response
  parseSerial();
}