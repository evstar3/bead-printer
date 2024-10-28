#include "Adafruit_AS726x.h"
#include "Arduino.h"
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
#define STEP0_PIN 9
#define STEP1_PIN 10
#define STEP2_PIN 11
// ## servos
#define SERVO0_PIN 5
#define SERVO1_PIN 6

// # physical constants
#define NEUTRAL_ANGLE 0 // TODO: figure out actual value
#define DROP_ANGLE 180  // TODO: figure out actual value
#define DROP_X 10       // TODO: figure out actual value
#define DROP_Y 10       // TODO: figure out actual value

// # objects
Servo servo0;
// Servo servo1;
Adafruit_AS726x colorSensor;

// # global state
bool beadGearOn = false;
int8_t currentX = 0, currentY = 0;

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

  // set STEPEN to 0
  digitalWrite(STEPEN_PIN, LOW);

  // set DIRs all 0
  digitalWrite(DIR0_PIN, LOW);
  digitalWrite(DIR1_PIN, LOW);
  digitalWrite(DIR2_PIN, LOW);

  // set STEP to 0
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

  // colorSensor.drvOn();
  colorSensor.indicateLED(true);
}

// operates on bead indices
void moveTo(uint8_t x, uint8_t y) {
  // TODO: this
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
    beadGearOn = true;
    break;
  // 1: stop bead gear
  case 1:
    beadGearOn = false;
    break;
    // 2: move to <x, y [bead indices]>, drop bead
  case 2: {
    if (Serial.available() != 2) {
      break;
    }

    uint8_t x = Serial.read();
    uint8_t y = Serial.read();

    moveTo(x, y);

    dropRoutine();

    break;
  }
  // 3: reject bead
  case 3:
    moveTo(DROP_X, DROP_Y);
    dropRoutine();
    break;
  }
}

void colorSensorTask() {

  float data[AS726x_NUM_CHANNELS];

  // colorSensor.startMeasurement(); // begin a measurement
  // TODO: ^^ ideally it's in continuous mode?

  // wait till data is available
  while (!colorSensor.dataReady())
    delay(5);

  colorSensor.readCalibratedValues(data);

  // send the color values as raw bytes (6 x f32)
  Serial.write((uint8_t *)data, sizeof(float) * AS726x_NUM_CHANNELS);
}

// TODO: this needs to be a timer interrupt
void updateBeadGearStepper() {
  // TODO: this
}

void setup() {
  // Initialize serial communication
  Serial.begin(115200);

  // Initialize stepper motors
  stepperInit();

  // Initialize servo
  servoInit();

  // Initialize color sensor
  colorSensorInit();
}

void loop() {
  float colorSensorData[AS726x_NUM_CHANNELS];

  while (true) {
    // colorSensor.startMeasurement(); // begin a measurement
    // TODO: ^^ ideally it's in continuous mode?

    // wait till data is available
    while (!colorSensor.dataReady())
      delay(5);

    colorSensor.readCalibratedValues(colorSensorData);

    // send the color values as raw bytes (6 x f32)
    Serial.write((uint8_t *)colorSensorData,
                 sizeof(float) * AS726x_NUM_CHANNELS);

    // this blocks on a response
    parseSerial();
  }
}
