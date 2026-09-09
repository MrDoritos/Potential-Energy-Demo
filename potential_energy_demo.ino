enum Inputs : int {
  LIMIT_TOP=6,
  LIMIT_BOTTOM=7,
  BUTTON_TOP=8,
  BUTTON_BOTTOM=9,
  BUTTON_RELEASE=10,
  BUTTON_CONTINUE=11,
  INPUTS_END=12,
};

enum Outputs : int {
  L298_IN1=2,
  L298_IN2=3,
  L298_ENABLE=4,
  Q_LED=5,
  OUTPUTS_END=6,
};

const int TOP_DIR_PIN = L298_IN1;
const int BOTTOM_DIR_PIN = L298_IN2;

struct DemoControl {
  bool bL298_IN1 = false;
  bool bL298_IN2 = false;
  bool bL298_ENABLE = false;
  bool bQ_LED = false;

  enum State : int {
    RELEASED=1,
    BRAKE=2,
    MOVE_TOP=4,
    MOVE_BOTTOM=8,
    FALL=16,
    REWIND=32
  };

  State state{RELEASED};
  
  bool isLimitTop() { return !digitalRead(LIMIT_TOP); }
  bool isLimitBottom() { return !digitalRead(LIMIT_BOTTOM); }
  bool isButtonTop() { return !digitalRead(BUTTON_TOP); }
  bool isButtonBottom() { return !digitalRead(BUTTON_BOTTOM); }
  bool isButtonRelease() { return !digitalRead(BUTTON_RELEASE); }
  bool isButtonContinue() { return !digitalRead(BUTTON_CONTINUE); }
  bool hasState(State st) {
    return (state & st) == st;
  }

  void init() { }

  void movementHalt() {
    analogWrite(L298_IN1, 0);
    analogWrite(L298_IN2, 0);
    bL298_IN1 = false;
    bL298_IN2 = false;
  }

  void setMovementState(bool enable) {
    if (getQLEDState()) {
      setQLEDState(false);
    }
    digitalWrite(L298_ENABLE, enable);
    bL298_ENABLE = enable;
  }

  void setQLEDState(bool enable) {
    if (getMovementState() && enable) { // Disable Motor Control
      movementHalt();
      setMovementState(false);
      state = RELEASED;
    }
    digitalWrite(Q_LED, enable);
    //analogWrite(Q_LED, enable ? 126 : 0);
    bQ_LED = enable;
  }

  bool getQLEDState() {
    return bQ_LED;
  }

  bool getMovementState() {
    return bL298_ENABLE;
  }

  void movementCheck() {
    if (hasState(RELEASED)) return;

    if (isLimitTop() && bL298_IN1) {
      movementHalt();
    }

    if (isLimitBottom() && bL298_IN2) { // Temporary for movement override. Will allow overrides to relieve tension.
      movementHalt();
    }
  }

  void buttonCheck() {
    if (isButtonTop()) {
      if (!isLimitTop()) {
        setMovementState(true);
        analogWrite(BOTTOM_DIR_PIN, 0);
        analogWrite(TOP_DIR_PIN, 255);
        bL298_IN2 = false;
        bL298_IN1 = true;
        state = BRAKE | MOVE_TOP;
      }
    } else
    if (isButtonBottom()) {
      if (!isLimitBottom()) {
        setMovementState(true);
        analogWrite(TOP_DIR_PIN, 0);
        analogWrite(BOTTOM_DIR_PIN, 255);
        bL298_IN1 = false;
        bL298_IN2 = true;
        state = BRAKE | MOVE_BOTTOM;
      }
    } else {
      if (hasState(MOVE_TOP) || hasState(MOVE_BOTTOM)) {
        analogWrite(TOP_DIR_PIN, 0);
        analogWrite(BOTTOM_DIR_PIN, 0);
        bL298_IN1 = false;
        bL298_IN2 = false;
        state = BRAKE;
      }
    }

    if (isButtonContinue()) {
      setMovementState(true);
      state = BRAKE;
    }

    if (isButtonRelease()) {
      if (getMovementState())
        setMovementState(false);
      if (!getQLEDState())
        setQLEDState(true);
      state = RELEASED;
    }
  }

  void update() {
    movementCheck();
    buttonCheck();
  }
};

DemoControl demo;

void setup() {
  Serial.begin(9600);
  for (int i = LIMIT_TOP; i < INPUTS_END; i++)
    pinMode(i, INPUT_PULLUP);
  for (int i = L298_IN1; i < OUTPUTS_END; i++)
    pinMode(i, OUTPUT);
  demo.init();
}

void loop() {
  demo.update();

  static long prev = 0;

  if (prev + 250 < millis()) {
    prev = millis();
    Serial.print("Inputs: ");
    for (int i = LIMIT_TOP; i < INPUTS_END; i++)
      Serial.print(digitalRead(i) ? 1 : 0);
    Serial.print(" Outputs: ");
    bool vars[] = {
      demo.bL298_IN1,
      demo.bL298_IN2,
      demo.bL298_ENABLE,
      demo.bQ_LED
    };
    for (int i = 0; i < 4; i++) {
      Serial.print(vars[i] ? 1 : 0);
    }
    Serial.println();
  }
}
