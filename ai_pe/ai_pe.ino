#include <Arduino.h>

// --- Pin Definitions ---
constexpr uint8_t PIN_IN1          = 2;
constexpr uint8_t PIN_IN2          = 3;
constexpr uint8_t PIN_MOSFET       = 5;
constexpr uint8_t PIN_ENA          = 4;

constexpr uint8_t PIN_LIMIT_TOP    = 6;
constexpr uint8_t PIN_LIMIT_BOTTOM = 7;

constexpr uint8_t PIN_BTN_RAISE    = 8;
constexpr uint8_t PIN_BTN_LOWER    = 9;
constexpr uint8_t PIN_BTN_CONTINUE = 11;
constexpr uint8_t PIN_BTN_RELEASE  = 10;

// --- System States ---
enum class SystemMode {
    MANUAL,
    AUTO_RAISING,
    AUTO_UNWINDING,
    AUTO_LOWERING
};

SystemMode currentMode = SystemMode::MANUAL;
unsigned long unwindStartTime = 0;
constexpr unsigned long UNWIND_DURATION_MS = 500;

// Motor Brake State for Manual Coast vs Brake toggle
bool isBraked = false;

// Debounce Tracking for Toggle Buttons
bool lastContinueState = HIGH;
bool lastReleaseState  = HIGH;
unsigned long lastDebounceTime = 0;
constexpr unsigned long DEBOUNCE_DELAY_MS = 50;

// --- Helper Functions for Hardware Control ---

void stopMotor() {
    digitalWrite(PIN_ENA, LOW);
    digitalWrite(PIN_IN1, LOW);
    digitalWrite(PIN_IN2, LOW);
}

void brakeMotor() {
    // L298N Electronic Brake: Both inputs HIGH with ENA HIGH shorts motor terminals
    digitalWrite(PIN_IN1, HIGH);
    digitalWrite(PIN_IN2, HIGH);
    digitalWrite(PIN_ENA, HIGH);
}

void motorRaise() {
    digitalWrite(PIN_MOSFET, LOW); // Prevent shorting generator path during powered drive
    digitalWrite(PIN_IN1, HIGH);
    digitalWrite(PIN_IN2, LOW);
    digitalWrite(PIN_ENA, HIGH);
}

void motorLower() {
    digitalWrite(PIN_MOSFET, LOW);
    digitalWrite(PIN_IN1, LOW);
    digitalWrite(PIN_IN2, HIGH);
    digitalWrite(PIN_ENA, HIGH);
}

void enableGeneration() {
    stopMotor();                   // High-Z / Disable H-Bridge
    digitalWrite(PIN_MOSFET, HIGH); // Engage N-FET to route energy to LED
}

void disableGeneration() {
    digitalWrite(PIN_MOSFET, LOW);
}

// Switches wired between GND and Pin (Active LOW)
bool isTopLimitReached() {
    return digitalRead(PIN_LIMIT_TOP) == LOW;
}

bool isBottomLimitReached() {
    return digitalRead(PIN_LIMIT_BOTTOM) == LOW;
}

void setup() {
    // Outputs
    pinMode(PIN_IN1, OUTPUT);
    pinMode(PIN_IN2, OUTPUT);
    pinMode(PIN_ENA, OUTPUT);
    pinMode(PIN_MOSFET, OUTPUT);

    // Inputs with Internal Pull-ups
    pinMode(PIN_LIMIT_TOP, INPUT_PULLUP);
    pinMode(PIN_LIMIT_BOTTOM, INPUT_PULLUP);
    pinMode(PIN_BTN_RAISE, INPUT_PULLUP);
    pinMode(PIN_BTN_LOWER, INPUT_PULLUP);
    pinMode(PIN_BTN_CONTINUE, INPUT_PULLUP);
    pinMode(PIN_BTN_RELEASE, INPUT_PULLUP);

    // Safe initial state
    stopMotor();
    disableGeneration();
}

void loop() {
    // Read raw button states (Active LOW)
    bool rawRaise    = digitalRead(PIN_BTN_RAISE) == LOW;
    bool rawLower    = digitalRead(PIN_BTN_LOWER) == LOW;
    bool rawContinue = digitalRead(PIN_BTN_CONTINUE);
    bool rawRelease  = digitalRead(PIN_BTN_RELEASE);

    // --- Debounce & Mode Overrides ---
    if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY_MS) {
        // Continue Button: Toggle Auto Control vs Manual Override
        if (rawContinue == LOW && lastContinueState == HIGH) {
            lastDebounceTime = millis();
            if (currentMode == SystemMode::MANUAL) {
                currentMode = SystemMode::AUTO_RAISING;
                disableGeneration();
                motorRaise();
            } else {
                currentMode = SystemMode::MANUAL;
            }
        }
        lastContinueState = rawContinue;

        // Release/Brake Button Toggle (Only acts in Manual mode)
        if (rawRelease == LOW && lastReleaseState == HIGH) {
            lastDebounceTime = millis();
            if (currentMode == SystemMode::MANUAL) {
                isBraked = !isBraked;
            }
        }
        lastReleaseState = rawRelease;
    }

    // Direct Manual Hold Overrides: Pressing Raise or Lower drops Auto state immediately
    if (rawRaise || rawLower) {
        currentMode = SystemMode::MANUAL;
    }

    // --- State Machine Execution ---
    switch (currentMode) {
        case SystemMode::AUTO_RAISING:
            if (isTopLimitReached()) {
                // Top hit: Unwind driven for 500ms to break static friction
                motorLower();
                unwindStartTime = millis();
                currentMode = SystemMode::AUTO_UNWINDING;
            } else {
                motorRaise();
            }
            break;

        case SystemMode::AUTO_UNWINDING:
            if (millis() - unwindStartTime >= UNWIND_DURATION_MS) {
                // Done unwinding: Cut bridge, switch to MOSFET generation
                enableGeneration();
                currentMode = SystemMode::AUTO_LOWERING;
            }
            break;

        case SystemMode::AUTO_LOWERING:
            if (isBottomLimitReached()) {
                // Bottom hit: Loop cycle back to raise
                disableGeneration();
                motorRaise();
                currentMode = SystemMode::AUTO_RAISING;
            } else {
                enableGeneration();
            }
            break;

        case SystemMode::MANUAL:
            disableGeneration();

            if (rawRaise && !isTopLimitReached()) {
                motorRaise();
            } else if (rawLower && !isBottomLimitReached()) {
                motorLower();
            } else {
                // Idle manual state
                if (isBraked) {
                    brakeMotor();
                } else {
                    stopMotor();
                }
            }
            break;
    }
}
