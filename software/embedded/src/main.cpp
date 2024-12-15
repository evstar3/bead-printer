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
// #define SERVO1_PIN 6

// # physical constants
#define NEUTRAL_ANGLE 180
#define DROP_ANGLE 48
#define REJECT_X 100     // TODO: figure out actual value
#define MM_PER_BEAD 6    // TODO: figure out actual value
#define BEAD_OFFSET_X 10 // TODO: figure out actual value
#define BEAD_OFFSET_Y 10 // TODO: figure out actual value
#define HOMING_STEP_DELAY_US 1000
#define STEP_DELAY_US 1000
// 5mm per revolution, 1.8 degrees per step => 200 steps per revolution
// => 40 steps per mm
#define STEPS_PER_MM 40
// #define FLIP_X
// #define FLIP_Y
#define BEAD_PHOTORESISTOR_THRESHOLD 500 // TODO: figure out actual value

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
  // pinMode(DMODE0_PIN, OUTPUT);
  // pinMode(DMODE1_PIN, OUTPUT);
  // pinMode(DMODE2_PIN, OUTPUT);
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
  // digitalWrite(DMODE0_PIN, LOW);
  // digitalWrite(DMODE1_PIN, LOW);
  // digitalWrite(DMODE2_PIN, HIGH);

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

  colorSensor.drvOn(); // white LED
  colorSensor.indicateLED(true);

  // set up photoresistor
  pinMode(PHOTORESISTOR_PIN, INPUT);
  // default to 10 bit resolution
  for (int i = 0; i < 10; i++) {
    analogRead(PHOTORESISTOR_PIN);
    delay(100);
  }
}

void toggleBeadGearStepper(bool setOn) {
  digitalWrite(13, setOn);
  cli(); // Disable global interrupts
  if (setOn) {
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

    digitalWrite(axisConfig[0], HIGH);
    while (digitalRead(axisConfig[2]) == HIGH) {
      digitalWrite(axisConfig[1], HIGH);
      delayMicroseconds(HOMING_STEP_DELAY_US);
      digitalWrite(axisConfig[1], LOW);
      delayMicroseconds(HOMING_STEP_DELAY_US);
    }
    // now back off 5mm
    digitalWrite(axisConfig[0], LOW);
    for (int i = 0; i < 5 * STEPS_PER_MM; i++) {
      digitalWrite(axisConfig[1], HIGH);
      delayMicroseconds(HOMING_STEP_DELAY_US);
      digitalWrite(axisConfig[1], LOW);
      delayMicroseconds(HOMING_STEP_DELAY_US);
    }
    digitalWrite(axisConfig[0], HIGH);
    while (digitalRead(axisConfig[2]) == HIGH) {
      digitalWrite(axisConfig[1], HIGH);
      delayMicroseconds(HOMING_STEP_DELAY_US * 2);
      digitalWrite(axisConfig[1], LOW);
      delayMicroseconds(HOMING_STEP_DELAY_US * 2);
    }
  }

  homed = true;

  // go to neutral position
  moveTo(50, 50);
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
  // Adjust direction if needed (change #define's to reverse direction)
  int16_t stepsX = (currentX - x) * STEPS_PER_MM;
  int16_t stepsY = (currentY - y) * STEPS_PER_MM;

  // Perform the movement
  // handle both axes simulataneously
  digitalWrite(DIR1_PIN, stepsX > 0);
  digitalWrite(DIR2_PIN, stepsY > 0);
  while (stepsX != 0 && stepsY != 0) {
    // steps
    digitalWrite(STEP1_PIN, HIGH);
    digitalWrite(STEP2_PIN, HIGH);
    // wait a bit
    delayMicroseconds(STEP_DELAY_US);
    // reset
    digitalWrite(STEP1_PIN, LOW);
    digitalWrite(STEP2_PIN, LOW);
    // wait a bit
    delayMicroseconds(STEP_DELAY_US);
    // change counter
    stepsX -= stepsX > 0 ? 1 : -1;
    stepsY -= stepsY > 0 ? 1 : -1;
  }
  // x axis only
  digitalWrite(DIR1_PIN, stepsX > 0);
  while (stepsX != 0) {
    digitalWrite(STEP1_PIN, HIGH);
    delayMicroseconds(STEP_DELAY_US);
    digitalWrite(STEP1_PIN, LOW);
    delayMicroseconds(STEP_DELAY_US);
    stepsX -= stepsX > 0 ? 1 : -1;
  }
  // y axis only
  digitalWrite(DIR2_PIN, stepsY > 0);
  while (stepsY != 0) {
    digitalWrite(STEP2_PIN, HIGH);
    delayMicroseconds(STEP_DELAY_US);
    digitalWrite(STEP2_PIN, LOW);
    delayMicroseconds(STEP_DELAY_US);
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

  uint8_t buf = Serial.peek();

  if (buf >= 'a' && buf <= 'z') {
    // this means buf is a a-z character
    String command = Serial.readStringUntil('\n');

    if (command == "home") {
      homeAxes();
      return;
    } else if (command == "drop") {
      dropRoutine();
      return;
    } else if (command == "start") {
      toggleBeadGearStepper(true);
      return;
    } else if (command == "stop") {
      toggleBeadGearStepper(false);
      return;
    } else if (command.startsWith("move ")) {
      // move to (x, y) mm
      // move 10 10

      // remove the "move " part
      command.remove(0, 5);

      // split the string into x and y
      int spaceIndex = command.indexOf(' ');
      if (spaceIndex != -1) {
        String xStr = command.substring(0, spaceIndex);
        String yStr = command.substring(spaceIndex + 1);

        // convert to double
        double x = xStr.toDouble();
        double y = yStr.toDouble();

        moveTo(x, y);
        return;
      }
    } else if (command.startsWith("moveBead ")) {
      // move to (x, y) bead indices
      // moveBead 10 10

      // remove the "moveBead " part
      command.remove(0, 9);

      // split the string into x and y
      int spaceIndex = command.indexOf(' ');
      if (spaceIndex != -1) {
        String xStr = command.substring(0, spaceIndex);
        String yStr = command.substring(spaceIndex + 1);

        // convert to uint8_t
        uint8_t x = xStr.toInt();
        uint8_t y = yStr.toInt();

        moveToBead(x, y);
        return;
      }
    } else if (command == "reject") {
      moveTo(REJECT_X, currentY);
      dropRoutine();
      return;
    }
  }

  buf = Serial.read();

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
    // i'm too lazy to write a new function just to move on the x axis
    // so this'll work.
    moveTo(REJECT_X, currentY);
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
  delay(2000);
  Serial.println("hello");

  // configure endstops
  // endstopInit();

  // Initialize stepper motors
  // stepperInit();

  // Initialize servo
  // servoInit();

  // Initialize color sensor
  colorSensorInit();

  // Home X and Y axes
  // homeAxes();

  Serial.println("Setup done!");
  toggleBeadGearStepper(true);
}

void loop() {
  // float colorSensorData[AS726x_NUM_CHANNELS];
  //
  // // read analog value from photoresistor
  // // while it's larger than the threshold, keep looping
  // // (lack of bead means more light hits it)
  // while (analogRead(PHOTORESISTOR_PIN) > BEAD_PHOTORESISTOR_THRESHOLD)
  //   delay(5);
  //
  // // read color sensor data, store in sendBuf
  // colorSensor.startMeasurement();
  //
  // // wait till data is available
  // while (!colorSensor.dataReady())
  //   delay(5);
  // colorSensor.readCalibratedValues(colorSensorData);
  //
  // // send the color values as raw bytes (6 x f32)
  // Serial.write((uint8_t *)colorSensorData, sizeof(colorSensorData));

  // this blocks on a response
  // parseSerial();

  int av = analogRead(PHOTORESISTOR_PIN);
  Serial.println(av);
  Serial.println(av < 300 || av > 400);
  delay(200);
}
