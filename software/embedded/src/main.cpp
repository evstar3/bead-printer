#include "Adafruit_AS726x.h"
#include "Arduino.h"
#include "pins_arduino.h"
#include <Wire.h>

#include "Servo.h"


/*
Note:
X = horizontal axis
Y = vertical axis
*/

// pins

#define DROP_SERVO_PIN 5
#define PHOTORESISTOR_PIN A0
#define STEP_EN_PIN 29
#define GEAR_STEP_PIN 6
#define GEAR_DIR_PIN 7
#define X_STEP_PIN 10
#define X_DIR_PIN 11
#define X_STOP_PIN 3
#define Y_STEP_PIN 8
#define Y_DIR_PIN 9
#define Y_STOP_PIN 2

// physical constants

#define REJECT_X 100     // TODO: figure out actual value
#define MM_PER_BEAD 6    // TODO: figure out actual value
#define BEAD_OFFSET_X 10 // TODO: figure out actual value
#define BEAD_OFFSET_Y 10 // TODO: figure out actual value

#define HOMING_STEP_DELAY_US 1000
#define STEP_DELAY_US 1000

#define AXIS_STEPS_PER_REV 200
#define AXIS_MM_PER_REV 5
#define AXIS_STEPS_PER_MM (AXIS_STEPS_PER_REV / AXIS_MM_PER_REV)

#define NEUTRAL_ANGLE 180

// globals

Servo DropServo;
Adafruit_AS726x ColorSensor;
int8_t CurrentX = 0; // by grid index
int8_t CurrentY = 0; // by grid index
bool IsHomed = false;

// function prototypes

void endstopInit();
void stepperInit();
void servoInit();
void ColorSensorInit();
void photoresistorInit();

void setBeadGearStepper(bool on);
void homeAxes();
void moveTo(double x, double y);
inline void moveToBead(uint8_t x, uint8_t y);
void dropRoutine();
void parseSerial();

// function definitions

void endstopInit() {
    pinMode(X_STOP_PIN, INPUT_PULLUP);
    pinMode(Y_STOP_PIN, INPUT_PULLUP);
}

void stepperInit() {
    // configure stepper pins as output
    pinMode(STEP_EN_PIN, OUTPUT);

    pinMode(GEAR_DIR_PIN, OUTPUT);
    pinMode(GEAR_STEP_PIN, OUTPUT);

    pinMode(X_DIR_PIN, OUTPUT);
    pinMode(X_STEP_PIN, OUTPUT);

    pinMode(Y_DIR_PIN, OUTPUT);
    pinMode(Y_STEP_PIN, OUTPUT);

    // initialize stepper pins
    digitalWrite(STEP_EN_PIN, LOW);

    digitalWrite(GEAR_DIR_PIN, HIGH);
    digitalWrite(X_DIR_PIN, LOW);
    digitalWrite(Y_DIR_PIN, LOW);

    digitalWrite(GEAR_STEP_PIN, LOW);
    digitalWrite(X_STEP_PIN, LOW);
    digitalWrite(Y_DIR_PIN, LOW);
}

void servoInit() {
    DropServo.attach(DROP_SERVO_PIN);
    DropServo.write(NEUTRAL_ANGLE);
}

void ColorSensorInit() {
    while (!ColorSensor.begin());

    ColorSensor.drvOn(); // super bright white LED
    ColorSensor.indicateLED(true);
}

void photoresistorInit() {
    pinMode(PHOTORESISTOR_PIN, INPUT);
}

void setBeadGearStepper(bool set_on) {
    // disable global interrupts
    cli();

    if (set_on) {
        // Set compare match register to desired timer count
        // f = 16 MHz / (prescaler * (1 + OCR1A))
        // => OCR1A = 16 Mhz / F / prescaler - 1
        //     ^^     ^ XTAL runs at 16*1000^2 so use that
        // 15624 gives 1 Hz
        // 1561 gives 10 Hz
        // 1040 gives 15 Hz
        // 155 gives 100 Hz
        OCR1A = 1040; // Set compare match register for 15 Hz increments

        // turn on CTC mode (Clear Timer on Compare match)
        TCCR1B |= (1 << WGM12);

        // set CS12 and CS10 bits for 1024 prescaler
        TCCR1B |= (1 << CS12) | (1 << CS10);

        // enable timer compare interrupt
        TIMSK1 |= (1 << OCIE1A);
    } else {
        // clear current timer configs
        TCCR1A = 0;
        TCCR1B = 0;
        TCNT1 = 0;

        // disable the interrupt
        TIMSK1 &= ~(1 << OCIE1A);
    }

    // reenable global interrupts
    sei();
}

void homeAxes() {
    int axis_pins[2][3] = {
        {X_DIR_PIN, X_STEP_PIN, X_STOP_PIN},
        {Y_DIR_PIN, Y_STEP_PIN, Y_STOP_PIN}
    };

    for (uint8_t axis = 0; axis < 2; axis++) {
        int dir_pin = axis_pins[axis][0];
        int step_pin = axis_pins[axis][1];
        int stop_pin = axis_pins[axis][2];

        // move at normal speed
        digitalWrite(dir_pin, HIGH);
        while (digitalRead(stop_pin) == HIGH) {
            digitalWrite(step_pin, HIGH);
            delayMicroseconds(HOMING_STEP_DELAY_US);
            digitalWrite(step_pin, LOW);
            delayMicroseconds(HOMING_STEP_DELAY_US);
        }

        // now back off 5mm
        digitalWrite(dir_pin, LOW);
        for (int i = 0; i < 5 * AXIS_STEPS_PER_MM; i++) {
            digitalWrite(step_pin, HIGH);
            delayMicroseconds(HOMING_STEP_DELAY_US);
            digitalWrite(step_pin, LOW);
            delayMicroseconds(HOMING_STEP_DELAY_US);
        }

        // move at half speed
        digitalWrite(dir_pin, HIGH);
        while (digitalRead(stop_pin) == HIGH) {
            digitalWrite(step_pin, HIGH);
            delayMicroseconds(HOMING_STEP_DELAY_US * 2);
            digitalWrite(step_pin, LOW);
            delayMicroseconds(HOMING_STEP_DELAY_US * 2);
        }
    }

    IsHomed = true;
}

// operates on real world coordinates (mm)
void moveTo(double x, double y) {
    if (!IsHomed)
        return;

    if (x == CurrentX && y == CurrentY)
        return;

    if (x < 0 || y < 0)
        return;

    int16_t x_steps = (CurrentX - x) * AXIS_STEPS_PER_MM;
    int16_t y_steps = (CurrentY - y) * AXIS_STEPS_PER_MM;

    // set directions
    digitalWrite(X_DIR_PIN, x_steps > 0);
    digitalWrite(Y_DIR_PIN, y_steps > 0);

    // handle both axes simulataneously
    while (x_steps || y_steps) {
        if (x_steps)
            digitalWrite(X_STEP_PIN, HIGH);
        if (y_steps)
            digitalWrite(Y_STEP_PIN, HIGH);

        delayMicroseconds(STEP_DELAY_US);

        if (x_steps)
            digitalWrite(X_STEP_PIN, LOW);
        if (y_steps)
            digitalWrite(Y_STEP_PIN, LOW);

        delayMicroseconds(STEP_DELAY_US);

        if (x_steps > 0)
            x_steps--;
        else if (x_steps < 0)
            x_steps++;

        if (y_steps > 0)
            y_steps--;
        else if (y_steps < 0)
            y_steps++;
    }

    CurrentX = x;
    CurrentY = y;
}

// converts bead indices to coordinates
inline void moveToBead(uint8_t x, uint8_t y) {
    moveTo(x * MM_PER_BEAD + BEAD_OFFSET_X, y * MM_PER_BEAD + BEAD_OFFSET_Y);
}

void dropRoutine() {
    const int DROP_ANGLE = 65;

    // make sure at neutral
    DropServo.write(NEUTRAL_ANGLE);
    delay(500);

    // trigger drop
    DropServo.write(DROP_ANGLE);
    delay(1000);

    // go back to neutral
    DropServo.write(NEUTRAL_ANGLE);
    delay(500);
}

void parseSerial() {
    while (!Serial.available());

    uint8_t buf = Serial.peek();

    uint8_t x, y;

    if (buf >= 'a' && buf <= 'z') {
        String command = Serial.readStringUntil('\n');

        if (command == "home") {
            homeAxes();
        } else if (command == "drop") {
            dropRoutine();
        } else if (command == "start") {
            setBeadGearStepper(true);
        } else if (command == "stop") {
            setBeadGearStepper(false);
        } else if (command.startsWith("move ")) {
            // expecting "move <double> <double>"

            command.remove(0, strlen("move "));

            // split the string into x and y
            int space_index = command.indexOf(' ');
            if (space_index != -1) {
                String x_str = command.substring(0, space_index);
                String y_str = command.substring(space_index + 1);

                // convert to double
                double x = x_str.toDouble();
                double y = y_str.toDouble();

                moveTo(x, y);
            }
        } else if (command.startsWith("moveBead ")) {
            // expecting "moveBead <int> <int>"

            command.remove(0, strlen("moveBead "));

            // split the string into x and y
            int space_index = command.indexOf(' ');
            if (space_index != -1) {
                String x_str = command.substring(0, space_index);
                String y_str = command.substring(space_index + 1);

                x = x_str.toInt();
                y = y_str.toInt();

                moveToBead(x, y);
            }
        } else if (command == "reject") {
            moveTo(REJECT_X, CurrentY);
            dropRoutine();
        }

        return;
    }

    buf = Serial.read();

    switch (buf) {
        case 0:
            // start bead gear
            setBeadGearStepper(true);
            break;
        case 1:
            // stop bead gear
            setBeadGearStepper(false);
            break;
        case 2:
            // 2: move to <x, y [bead indices]>, drop bead
            while (Serial.available() < 2);

            // these are bead indices
            x = Serial.read();
            y = Serial.read();

            moveToBead(x, y);
            dropRoutine();

            break;
        case 3:
            // 3: reject bead
            moveTo(REJECT_X, CurrentY);
            dropRoutine();
            break;
        default:
            break;
    }
}

volatile bool beadGearStepValue = 0;
ISR(TIMER1_COMPA_vect) {
    digitalWrite(GEAR_STEP_PIN, beadGearStepValue);
    beadGearStepValue = !beadGearStepValue;
}

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);

    Serial.begin(115200);

    endstopInit();
    stepperInit();
    servoInit();
    ColorSensorInit();
    photoresistorInit();
    homeAxes();

    setBeadGearStepper(true);
}

void loop() {
    // float ColorSensorData[AS726x_NUM_CHANNELS];
    //
    // // read analog value from photoresistor
    // // while it's larger than the threshold, keep looping
    // // (lack of bead means more light hits it)
    // while (analogRead(PHOTORESISTOR_PIN) > BEAD_PHOTORESISTOR_THRESHOLD)
    //   delay(5);
    //
    // // read color sensor data, store in sendBuf
    // ColorSensor.startMeasurement();
    //
    // // wait till data is available
    // while (!ColorSensor.dataReady())
    //   delay(5);
    // ColorSensor.readCalibratedValues(ColorSensorData);
    //
    // // send the color values as raw bytes (6 x f32)
    // Serial.write((uint8_t *)ColorSensorData, sizeof(ColorSensorData));

    // this blocks on a response
    // parseSerial();

    int av = analogRead(PHOTORESISTOR_PIN);
    Serial.print(av);
    Serial.print(' ');
    Serial.println(av < 300 || av > 400);
    delay(200);
}
