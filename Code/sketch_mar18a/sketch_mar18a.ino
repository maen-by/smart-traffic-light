const int NUM_LANES = 4;

// ==================== LED Pins ====================
const int ledPins[NUM_LANES] = {9, 10, 11, 12};

// ==================== Keypad Pins ====================
// حسب توصيلك:
const int C2_PIN = 2;   // غير مستخدم الآن
const int C1_PIN = 3;   // column 1 = ADD ONLY

const int rowPins[NUM_LANES] = {4, 5, 6, 7};   // R1, R2, R3, R4

// ==================== State ====================
long cnt[NUM_LANES] = {0, 0, 0, 0};
long waitCycles[NUM_LANES] = {0, 0, 0, 0};

int currentLane = -1;
bool laneRunning = false;

bool inTransition = false;
unsigned long transitionStart = 0;
int pendingLane = -1;

int rrPointer = 0;

unsigned long greenStart = 0;
unsigned long greenDuration = 0;

// ==================== Keypad debounce ====================
const unsigned long keypadDebounceMs = 180;
unsigned long lastKeyTime = 0;
bool keyHeld = false;

// ==================== Scheduling Parameters ====================
const unsigned long minGreenMs = 3000;
const unsigned long maxGreenMs = 12000;
const unsigned long perCarMs   = 120;

const int servedCarsPerTurn = 10;
const unsigned long switchDelayMs = 2000;

const int minCarsToServeLane    = 3;
const int forceServeAfterCycles = 3;

// ==================== Print Tracking ====================
bool lastPrintedInTransition = false;
int  lastPrintedCurrentLane = -99;
int  lastPrintedPendingLane = -99;
long lastPrintedCounts[NUM_LANES] = {-1, -1, -1, -1};

// ======================================================
// Display helpers
// ======================================================
long getPriorityValue(int lane) {
  if (cnt[lane] <= 0) return 0;
  return cnt[lane] + waitCycles[lane];
}

void printSnapshot() {
  Serial.println();

  Serial.print("S1 C="); Serial.print(cnt[0]);
  Serial.print(" P=");   Serial.print(getPriorityValue(0));
  Serial.print(" W=");   Serial.print(waitCycles[0]);

  Serial.print(" | S2 C="); Serial.print(cnt[1]);
  Serial.print(" P=");      Serial.print(getPriorityValue(1));
  Serial.print(" W=");      Serial.print(waitCycles[1]);

  Serial.print(" | S3 C="); Serial.print(cnt[2]);
  Serial.print(" P=");      Serial.print(getPriorityValue(2));
  Serial.print(" W=");      Serial.print(waitCycles[2]);

  Serial.print(" | S4 C="); Serial.print(cnt[3]);
  Serial.print(" P=");      Serial.print(getPriorityValue(3));
  Serial.print(" W=");      Serial.print(waitCycles[3]);

  Serial.print(" || ");

  if (inTransition) {
    Serial.print("TRANS->S");
    Serial.print(pendingLane + 1);
  } else if (currentLane == -1) {
    Serial.print("NONE");
  } else {
    Serial.print("GREEN=S");
    Serial.print(currentLane + 1);
  }

  Serial.print(" | RR=S");
  Serial.println(rrPointer + 1);
}

void printStateIfChanged() {
  bool changed = false;

  if (inTransition != lastPrintedInTransition) changed = true;
  if (currentLane != lastPrintedCurrentLane) changed = true;
  if (pendingLane != lastPrintedPendingLane) changed = true;

  if (!changed) return;

  lastPrintedInTransition = inTransition;
  lastPrintedCurrentLane = currentLane;
  lastPrintedPendingLane = pendingLane;

  printSnapshot();
}

void printCountsIfChanged() {
  bool changed = false;

  for (int i = 0; i < NUM_LANES; i++) {
    if (cnt[i] != lastPrintedCounts[i]) {
      changed = true;
      break;
    }
  }

  if (!changed) return;

  for (int i = 0; i < NUM_LANES; i++) {
    lastPrintedCounts[i] = cnt[i];
  }

  printSnapshot();
}

// ======================================================
// LEDs
// ======================================================
void allLedsOff() {
  for (int i = 0; i < NUM_LANES; i++) {
    digitalWrite(ledPins[i], LOW);
  }
}

// ======================================================
// Keypad helpers
// الصفوف غير النشطة تكون Hi-Z
// ======================================================
void releaseAllRows() {
  for (int i = 0; i < NUM_LANES; i++) {
    pinMode(rowPins[i], INPUT);
  }
}

void activateRow(int r) {
  pinMode(rowPins[r], OUTPUT);
  digitalWrite(rowPins[r], LOW);
}

// ======================================================
// Keypad scan
// الآن نستخدم C1 فقط
// R1+C1 -> S1 add
// R2+C1 -> S2 add
// R3+C1 -> S3 add
// R4+C1 -> S4 add
// ======================================================
bool scanKeypadAddOnly(int &lane) {
  releaseAllRows();

  for (int r = 0; r < NUM_LANES; r++) {
    releaseAllRows();
    activateRow(r);
    delayMicroseconds(80);

    if (digitalRead(C1_PIN) == LOW) {
      lane = r;
      releaseAllRows();
      return true;
    }
  }

  releaseAllRows();
  return false;
}

void readKeypadAndUpdateCounts() {
  int lane;
  bool pressed = scanKeypadAddOnly(lane);

  if (pressed) {
    if (!keyHeld && (millis() - lastKeyTime >= keypadDebounceMs)) {
      keyHeld = true;
      lastKeyTime = millis();

      // زيادة فقط
      cnt[lane]++;

      printCountsIfChanged();
    }
  } else {
    keyHeld = false;
  }
}

// ======================================================
// Scheduling helpers
// ======================================================
unsigned long computeGreenTime(int lane) {
  unsigned long t = minGreenMs + (cnt[lane] * perCarMs);

  if (t > maxGreenMs) t = maxGreenMs;
  if (t < minGreenMs) t = minGreenMs;

  return t;
}

bool laneEligible(int lane) {
  if (cnt[lane] <= 0) return false;
  if (cnt[lane] >= minCarsToServeLane) return true;
  if (waitCycles[lane] >= forceServeAfterCycles) return true;
  return false;
}

int peekNextLaneRR(int startPointer) {
  for (int i = 0; i < NUM_LANES; i++) {
    if (cnt[i] > 0 && waitCycles[i] >= forceServeAfterCycles) {
      return i;
    }
  }

  for (int step = 0; step < NUM_LANES; step++) {
    int lane = (startPointer + step) % NUM_LANES;
    if (laneEligible(lane)) return lane;
  }

  for (int step = 0; step < NUM_LANES; step++) {
    int lane = (startPointer + step) % NUM_LANES;
    if (cnt[lane] > 0) return lane;
  }

  return -1;
}

void commitRrPointerAfterChoosing(int chosenLane) {
  if (chosenLane >= 0) {
    rrPointer = (chosenLane + 1) % NUM_LANES;
  }
}

bool shouldFinishCurrentLaneFirst(int lane) {
  if (lane == -1) return false;
  if (cnt[lane] <= 0) return false;

  if (cnt[lane] <= servedCarsPerTurn) return true;

  unsigned long remainingClearTime = cnt[lane] * perCarMs;
  if (remainingClearTime <= switchDelayMs) return true;

  return false;
}

void updateWaitCyclesAfterTurn(int servedLane) {
  for (int i = 0; i < NUM_LANES; i++) {
    if (i == servedLane) {
      waitCycles[i] = 0;
    } else {
      if (cnt[i] > 0) waitCycles[i]++;
      else waitCycles[i] = 0;
    }
  }
}

// ======================================================
// State transitions
// ======================================================
void startLane(int lane) {
  currentLane = lane;
  laneRunning = true;
  inTransition = false;
  pendingLane = -1;

  greenStart = millis();
  greenDuration = computeGreenTime(lane);

  allLedsOff();
  digitalWrite(ledPins[lane], HIGH);

  printStateIfChanged();
}

void continueSameLane() {
  if (currentLane == -1) return;

  digitalWrite(ledPins[currentLane], HIGH);
  greenStart = millis();
  greenDuration = computeGreenTime(currentLane);
}

void beginTransition(int nextLane) {
  allLedsOff();
  currentLane = -1;
  laneRunning = false;
  inTransition = true;
  pendingLane = nextLane;
  transitionStart = millis();

  printStateIfChanged();
}

// ======================================================
// Main decision logic
// ======================================================
void finishCurrentTurn() {
  if (currentLane == -1) return;

  int servedLane = currentLane;

  if (cnt[servedLane] > 0) {
    cnt[servedLane] -= servedCarsPerTurn;
    if (cnt[servedLane] < 0) cnt[servedLane] = 0;
  }

  updateWaitCyclesAfterTurn(servedLane);
  printCountsIfChanged();

  if (cnt[servedLane] > 0 && shouldFinishCurrentLaneFirst(servedLane)) {
    currentLane = servedLane;
    laneRunning = true;
    inTransition = false;
    pendingLane = -1;
    continueSameLane();
    return;
  }

  int nextLane = peekNextLaneRR(rrPointer);

  if (nextLane == -1) {
    allLedsOff();
    currentLane = -1;
    laneRunning = false;
    inTransition = false;
    pendingLane = -1;
    printStateIfChanged();
    return;
  }

  // إذا الإشارة القادمة هي نفسها الحالية -> تبقى خضراء
  if (nextLane == servedLane && cnt[servedLane] > 0) {
    currentLane = servedLane;
    laneRunning = true;
    inTransition = false;
    pendingLane = -1;
    commitRrPointerAfterChoosing(nextLane);
    continueSameLane();
    return;
  }

  commitRrPointerAfterChoosing(nextLane);
  beginTransition(nextLane);
}

void scheduleIfNeeded() {
  if (laneRunning || inTransition) return;

  int nextLane = peekNextLaneRR(rrPointer);

  if (nextLane == -1) {
    allLedsOff();
    currentLane = -1;
    laneRunning = false;
    inTransition = false;
    pendingLane = -1;
    printStateIfChanged();
    return;
  }

  commitRrPointerAfterChoosing(nextLane);
  startLane(nextLane);
}

// ======================================================
// Setup / Loop
// ======================================================
void setup() {
  Serial.begin(9600);

  // الصفوف تبدأ Hi-Z
  releaseAllRows();

  // C1 فقط مستخدم للزيادة
  pinMode(C1_PIN, INPUT_PULLUP);

  // C2 موجود لكنه غير مستخدم
  pinMode(C2_PIN, INPUT_PULLUP);

  // LEDs
  for (int i = 0; i < NUM_LANES; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
    lastPrintedCounts[i] = cnt[i];
  }

  printSnapshot();
}

void loop() {
  readKeypadAndUpdateCounts();

  if (!laneRunning && !inTransition) {
    scheduleIfNeeded();
  }

  if (laneRunning && (millis() - greenStart >= greenDuration)) {
    finishCurrentTurn();
  }

  if (inTransition && (millis() - transitionStart >= switchDelayMs)) {
    startLane(pendingLane);
  }
}