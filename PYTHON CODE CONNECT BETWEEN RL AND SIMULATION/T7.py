import numpy as np
import serial
import time
import tensorflow as tf
from tensorflow.keras import Model
from tensorflow.keras.layers import Dense, Input, Lambda, Add
from tensorflow.keras.optimizers import Adam
import tensorflow.keras.backend as K
import os
import csv
import datetime

MODEL_PATH = r"D:\t7\best_model.keras"
COM_PORT   = "COM5"
BAUD_RATE  = 9600

MAX_CARS         = 300.0
MAX_WAIT         = 120.0
N_SIGNALS        = 4
MIN_GREEN_STEPS  = 1
MAX_GREEN_STEPS  = 10
STATE_DIM        = N_SIGNALS * 2 + 1
N_ACTIONS        = N_SIGNALS
CARS_PER_PHASE   = 15
SMALL_LANE_LIMIT = 25
STARVATION_LIMIT = 110.0

# --- yellow / transition timing (visual on the dashboard) ---
YELLOW_TIME    = 3.0   # seconds the outgoing GREEN lane stays YELLOW (about to stop)
CLEARANCE_TIME = 1.5   # seconds of ALL-RED transition before the next lane goes green

# ==============================================================================
# NEW: DATA LOGGING — save every reading from the Arduino into a file
#      that opens directly in Excel, inside the same folder as this script.
#      This is the data you will reuse later to retrain a better model.
# ==============================================================================
# Save next to this .py file (same project folder). Falls back to current folder.
try:
    BASE_DIR = os.path.dirname(os.path.abspath(__file__))
except NameError:
    BASE_DIR = os.getcwd()

LOG_CSV   = os.path.join(BASE_DIR, "traffic_data_log.csv")    # always written (safe, live)
LOG_XLSX  = os.path.join(BASE_DIR, "traffic_data_log.xlsx")   # real Excel file (refreshed)
SAVE_EVERY = 25   # refresh the .xlsx file every 25 decisions (and once more on exit)

LOG_HEADER = ["Timestamp",
              "S1_Cars", "S2_Cars", "S3_Cars", "S4_Cars",
              "S1_Wait", "S2_Wait", "S3_Wait", "S4_Wait",
              "CurrentGreen", "ChosenAction", "TotalCars", "MaxWait", "DecisionNo"]

# create the CSV with a header only if it does not exist yet
# (so running the program again APPENDS more data instead of erasing it)
if (not os.path.exists(LOG_CSV)) or os.path.getsize(LOG_CSV) == 0:
    with open(LOG_CSV, "w", newline="", encoding="utf-8") as f:
        csv.writer(f).writerow(LOG_HEADER)

try:
    import pandas as pd
    _HAS_PANDAS = True
except Exception:
    _HAS_PANDAS = False

_excel_warned = False

def log_decision(counts, waits, current_green, action, decision_no):
    """Append one row (one Arduino reading + the model's decision) to the CSV."""
    row = [datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
           int(counts[0]), int(counts[1]), int(counts[2]), int(counts[3]),
           round(float(waits[0]), 1), round(float(waits[1]), 1),
           round(float(waits[2]), 1), round(float(waits[3]), 1),
           int(current_green), int(action),
           int(np.sum(counts)), round(float(np.max(waits)), 1), int(decision_no)]
    with open(LOG_CSV, "a", newline="", encoding="utf-8") as f:
        csv.writer(f).writerow(row)

def export_excel():
    """Rewrite the .xlsx from the full CSV so Excel always has all the data."""
    global _excel_warned
    if not _HAS_PANDAS:
        if not _excel_warned:
            print("ℹ Excel (.xlsx) export needs pandas. The CSV file opens in Excel anyway.")
            _excel_warned = True
        return
    try:
        pd.read_csv(LOG_CSV).to_excel(LOG_XLSX, index=False)
    except Exception as e:
        if not _excel_warned:
            print("ℹ Could not write .xlsx (run: pip install openpyxl). CSV is saved fine. ", e)
            _excel_warned = True

def build_dueling_dqn():
    inp = Input(shape=(STATE_DIM,), name="state_input")
    x   = Dense(256, activation="relu")(inp)
    x   = Dense(256, activation="relu")(x)
    x   = Dense(128, activation="relu")(x)
    v   = Dense(64,  activation="relu")(x)
    v   = Dense(1,   name="value")(v)
    a   = Dense(64,  activation="relu")(x)
    a   = Dense(N_ACTIONS, name="advantage")(a)
    q   = Add(name="Q_values")([
        v,
        Lambda(
            lambda t: t - K.mean(t, axis=1, keepdims=True),
            output_shape=lambda s: s
        )(a)
    ])
    model = Model(inputs=inp, outputs=q, name="DuelingDQN")
    model.compile(optimizer=Adam(1e-4), loss="huber")
    return model

def clear():
    os.system('cls' if os.name == 'nt' else 'clear')

def draw_bar(value, max_val, width=10, fill="█", empty="░"):
    filled = int((value / max_val) * width) if max_val > 0 else 0
    filled = min(filled, width)
    return fill * filled + empty * (width - filled)

def print_dashboard(counts, waits, current_green, action, decision_count,
                    phase="green", yellow_lane=-1):
    """
    phase:
      "green"  -> normal view, `action` lane is GREEN
      "yellow" -> `yellow_lane` is YELLOW (about to stop), everything else RED
      "allred" -> transition: every lane RED (clearance time)
    """
    clear()
    print("╔══════════════════════════════════════════════════════════╗")
    print("║         SMART TRAFFIC LIGHT  —  AI DECISION             ║")
    print("╠══════════════════════════════════════════════════════════╣")
    print(f"║  Decision #{decision_count:<5}                                        ║")
    print("╠══════════════╦══════════════╦══════════════╦════════════╣")
    print("║    LANE      ║    CARS      ║  WAIT TIME   ║   STATUS   ║")
    print("╠══════════════╬══════════════╬══════════════╬════════════╣")
    for i in range(N_SIGNALS):
        if phase == "yellow" and i == yellow_lane:
            status = "🟡 YELLOW "
        elif phase == "green" and i == action:
            status = "🟢 GREEN  "
        else:
            status = "🔴 RED    "
        cars_bar = draw_bar(counts[i], MAX_CARS, width=10)
        cars_val = f"{int(counts[i]):3d} {cars_bar}"
        wait_bar = draw_bar(waits[i], MAX_WAIT, width=10)
        wait_val = f"{waits[i]:5.1f}s {wait_bar}"
        print(f"║  Signal S{i+1}   ║ {cars_val:<12} ║ {wait_val:<12} ║ {status} ║")
    print("╠══════════════╩══════════════╩══════════════╩════════════╣")
    total_cars = int(np.sum(counts))
    max_wait   = float(np.max(waits))
    if phase == "yellow":
        print(f"║  🟡 Signal S{yellow_lane+1} stopping...  ➜ next: Signal S{action+1}          ║")
    elif phase == "allred":
        print(f"║  ⏳ Transition (all red)...  ➜ next: Signal S{action+1}           ║")
    else:
        print(f"║  🎯 AI chose  : Signal S{action+1}                              ║")
    print(f"║  🚗 Total cars: {total_cars:<5}  |  ⏱ Max wait: {max_wait:5.1f}s            ║")
    print("╚══════════════════════════════════════════════════════════╝")

print("Building model...")
model = build_dueling_dqn()
model(tf.zeros((1, STATE_DIM)))
print("Loading weights...")
model.load_weights(MODEL_PATH)
print("Model loaded ✅")
print(f"Logging data to: {LOG_CSV}")

ser = None
while ser is None:
    try:
        ser = serial.Serial(COM_PORT, BAUD_RATE, timeout=1)
    except Exception as e:
        print("Waiting for COM port...", e)
        time.sleep(2)

time.sleep(2)
print("Serial connected ✅")
print("Starting...")

green_steps    = 0
decision_count = 0

try:
    while True:
        try:
            line = ser.readline().decode(errors="ignore").strip()

            # تجاهل الأسطر القديمة المتراكمة، خذ الأحدث فقط
            while ser.in_waiting > 0:
                newer = ser.readline().decode(errors="ignore").strip()
                if newer:
                    line = newer

            if not line or not line.startswith("S,"):
                if "ALL_EMPTY" in line:
                    clear()
                    print("╔══════════════════════════════════════╗")
                    print("║   ✅  ALL LANES EMPTY — ALL RED!     ║")
                    print("╚══════════════════════════════════════╝")
                    green_steps    = 0
                    decision_count = 0
                continue

            parts = line.split(",")
            if len(parts) != 10:
                continue

            counts        = np.array(list(map(float, parts[1:5])), dtype=np.float32)
            waits         = np.array(list(map(float, parts[5:9])), dtype=np.float32)
            current_green = int(parts[9])

            if np.sum(counts) == 0:
                continue

            # وقت انتظار الاشارة الخضراء = صفر (بدون نصف)
            for i in range(N_SIGNALS):
                if i == current_green:
                    waits[i] = 0.0

            norm_cars  = np.clip(counts / MAX_CARS, 0, 1)
            norm_wait  = np.clip(waits  / MAX_WAIT, 0, 1)
            green_feat = np.array([(current_green + 1) / N_SIGNALS], dtype=np.float32)
            state      = np.concatenate([norm_cars, norm_wait, green_feat])
            state_input = tf.constant(np.expand_dims(state, axis=0), dtype=tf.float32)

            q_values = model(state_input, training=False).numpy()[0]
            masked_q = q_values.copy()

            # اخفاء الاشارات الفاضية
            for i in range(N_SIGNALS):
                if counts[i] <= 0:
                    masked_q[i] = -1e9

            # المسارات الصغيرة تُخدم فوراً
            forced = -1
            for i in range(N_SIGNALS):
                if 0 < counts[i] < SMALL_LANE_LIMIT*2:
                    if forced == -1 or counts[i] < counts[forced]:
                        forced = i

            # قاعدة أمان: منع تجويع المسارات
            starving = -1
            for i in range(N_SIGNALS):
                if counts[i] > 0 and waits[i] >= STARVATION_LIMIT:
                    if starving == -1 or waits[i] > waits[starving]:
                        starving = i

            if forced >= 0:
                action      = forced
                green_steps = 0
            elif starving >= 0:
                action      = starving
                green_steps = 0
            else:
                # MIN GREEN
                if current_green >= 0 and green_steps < MIN_GREEN_STEPS:
                    action = current_green
                else:
                    # MAX GREEN
                    if current_green >= 0 and green_steps >= MAX_GREEN_STEPS:
                        masked_q[current_green] = -1e9
                    action = int(np.argmax(masked_q))

            # اذا الاشارة الحالية فاضية بدل فورا
            if counts[action] <= 0 or (current_green >= 0 and counts[current_green] == 0):
                non_empty = [i for i in range(N_SIGNALS) if counts[i] > 0]
                if non_empty:
                    action = max(non_empty, key=lambda i: waits[i])
                green_steps = 0

            # ============================================================
            # YELLOW + TRANSITION when switching from an active green
            #   green lane -> 🟡 YELLOW (about to stop) -> 🔴 all-red -> 🟢 new green
            # ============================================================
            switching = (action != current_green and current_green >= 0
                         and counts[current_green] > 0)
            if switching:
                # 🟡 warn the lane that is currently green that it will close
                print_dashboard(counts, waits, current_green, action, decision_count,
                                phase="yellow", yellow_lane=current_green)
                # (optional) if your Arduino has a yellow command, you could send it here:
                # ser.write(f"Y,{current_green}\n".encode())
                time.sleep(YELLOW_TIME)

                # 🔴 all-red clearance — the transition time between signals
                print_dashboard(counts, waits, current_green, action, decision_count,
                                phase="allred")
                time.sleep(CLEARANCE_TIME)

            if action == current_green:
                green_steps += 1
            else:
                green_steps = 0

            ser.write(f"D,{action}\n".encode())
            decision_count += 1

            # ---- SAVE THIS READING + DECISION TO THE DATA FILE ----
            log_decision(counts, waits, current_green, action, decision_count)
            if decision_count % SAVE_EVERY == 0:
                export_excel()

            # 🟢 new lane goes green
            print_dashboard(counts, waits, current_green, action, decision_count,
                            phase="green")

        except Exception as e:
            print("Loop Error:", e)
            time.sleep(0.1)

except KeyboardInterrupt:
    print("\nStopped.")

finally:
    export_excel()   # final Excel save so nothing is lost
    print(f"💾 Data saved: {LOG_CSV}")
    if _HAS_PANDAS:
        print(f"💾 Excel file: {LOG_XLSX}")
    try:
        ser.close()
        print("Serial closed ✅")
    except:
        pass
