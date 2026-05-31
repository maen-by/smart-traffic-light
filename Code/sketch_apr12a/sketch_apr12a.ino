const int NUM_LANES = 4;
const int ledPins[NUM_LANES] = {9, 10, 11, 12};
const int rowPins[NUM_LANES] = {4, 5, 6, 7};
const int C1_PIN = 3;

long  cars[NUM_LANES]       = {200, 117, 29, 90};
float waits[NUM_LANES]      = {0, 0, 0, 0};
float waitOffset[NUM_LANES] = {0, 0, 0, 0};
unsigned long redStart[NUM_LANES];

const unsigned long PHASE_MS      = 2500;
const unsigned long TRANSITION_MS = 1000;
const int   CARS_PER_PHASE        = 15;
const float MAX_WAIT              = 120.0;
const float SIM_MULTIPLIER        = 10.0;

int  currentLane     = -1;
int  pendingLane     = -1;
bool laneRunning     = false;
bool inTransition    = false;
bool waitingDecision = false;

unsigned long phaseStart      = 0;
unsigned long transitionStart = 0;

void initRedTimers() {
  unsigned long now = millis();
  for (int i = 0; i < NUM_LANES; i++) {
    redStart[i]   = now;
    waitOffset[i] = 0;
  }
}

void allOff() {
  for (int i = 0; i < NUM_LANES; i++)
    digitalWrite(ledPins[i], LOW);
}

void sendState() {
  Serial.print("S,");
  for (int i = 0; i < NUM_LANES; i++) {
    Serial.print(cars[i]);
    Serial.print(",");
  }
  for (int i = 0; i < NUM_LANES; i++) {
    float w = waits[i];
    if (w > MAX_WAIT) w = MAX_WAIT;
    Serial.print(w, 1);
    if (i < NUM_LANES - 1) Serial.print(",");
  }
  Serial.print(",");
  Serial.println(currentLane);
}

void updateWaits() {
  unsigned long now = millis();

  int activeLanes = 0;
  for (int i = 0; i < NUM_LANES; i++)
    if (cars[i] > 0) activeLanes++;

  float multiplier = SIM_MULTIPLIER * ((float)activeLanes / NUM_LANES);
  if (multiplier < 1.0) multiplier = 1.0;

  for (int i = 0; i < NUM_LANES; i++) {
    if (i == currentLane) {
      waits[i] = 0;
    } else if (cars[i] > 0) {
      float elapsed = (now - redStart[i]) / 1000.0 * (CARS_PER_PHASE/2);
      waits[i] = waitOffset[i] + elapsed;
      if (waits[i] > MAX_WAIT) waits[i] = MAX_WAIT;
    } else {
      waits[i]      = 0;
      waitOffset[i] = 0;
      redStart[i]   = now;
    }
  }
}

void startLane(int lane) {
  if (lane == currentLane) {
    laneRunning     = true;
    waitingDecision = false;
    phaseStart      = millis();
    return;
  }

  if (currentLane != -1) {
    allOff();
    inTransition    = true;
    laneRunning     = false;
    waitingDecision = false;
    pendingLane     = lane;
    transitionStart = millis();
    return;
  }

  currentLane     = lane;
  laneRunning     = true;
  inTransition    = false;
  waitingDecision = false;
  pendingLane     = -1;

  unsigned long now = millis();
  waitOffset[lane] = 0;
  redStart[lane]   = now;
  waits[lane]      = 0;

  allOff();
  digitalWrite(ledPins[lane], HIGH);
  phaseStart = now;
}

void finishPhase() {
  int passed;
  if (cars[currentLane] <= CARS_PER_PHASE) {
    passed = cars[currentLane];
  } else {
    passed = CARS_PER_PHASE;
  }

  cars[currentLane] -= passed;
  if (cars[currentLane] < 0) cars[currentLane] = 0;

  laneRunning     = false;
  waitingDecision = true;

  bool allZero = true;
  for (int i = 0; i < NUM_LANES; i++) {
    if (cars[i] > 0) { allZero = false; break; }
  }

  if (allZero) {
    allOff();
    currentLane     = -1;
    waitingDecision = false;
    for (int i = 0; i < NUM_LANES; i++) {
      waits[i]      = 0;
      waitOffset[i] = 0;
    }
    Serial.println("ALL_EMPTY");
    return;
  }

  updateWaits();
  sendState();
}

void checkTransition() {
  if (!inTransition) return;

  if (millis() - transitionStart >= TRANSITION_MS) {
    inTransition = false;

    if (pendingLane >= 0 && cars[pendingLane] > 0) {
      int oldLane = currentLane;

      currentLane     = pendingLane;
      laneRunning     = true;
      waitingDecision = false;
      pendingLane     = -1;

      unsigned long now = millis();

      if (oldLane >= 0 && oldLane != currentLane) {
        redStart[oldLane]   = now;
        waitOffset[oldLane] = 0;
      }

      waitOffset[currentLane] = 0;
      redStart[currentLane]   = now;
      waits[currentLane]      = 0;

      allOff();
      digitalWrite(ledPins[currentLane], HIGH);
      phaseStart = now;
    } else {
      waitingDecision = true;
      sendState();
    }
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(C1_PIN, INPUT_PULLUP);
  for (int i = 0; i < NUM_LANES; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }
  initRedTimers();
  Serial.println("READY");

  delay(500);
  sendState();
  waitingDecision = true;
}

void loop() {
  updateWaits();
  checkTransition();

  if (!laneRunning && !inTransition && !waitingDecision) {
    bool hasCars = false;
    for (int i = 0; i < NUM_LANES; i++)
      if (cars[i] > 0) { hasCars = true; break; }
    if (hasCars) {
      sendState();
      waitingDecision = true;
    }
  }

  if (Serial.available()) {
    String data = Serial.readStringUntil('\n');
    data.trim();
    if (data.startsWith("D,")) {
      int lane = data.substring(2).toInt();
      if (lane >= 0 && lane < NUM_LANES && cars[lane] > 0) {
        startLane(lane);
      }
    }
  }

  if (laneRunning && (millis() - phaseStart >= PHASE_MS)) {
    finishPhase();
  }
}