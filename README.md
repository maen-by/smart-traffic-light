<div align="center">

# 🚦 Adaptive Traffic Light Control System

### A self-learning traffic signal controller that beats classical algorithms

**A Dueling Double DQN agent controls a real 4-lane intersection (Arduino + sensors), learning to cut congestion and waiting time — and outperforming Fixed-Time, FCFS, and Priority control in every traffic scenario tested.**

![License](https://img.shields.io/badge/License-MIT-green.svg)
![Python](https://img.shields.io/badge/Python-3.10+-blue.svg)
![TensorFlow](https://img.shields.io/badge/TensorFlow-2.x-orange.svg)
![Arduino](https://img.shields.io/badge/Hardware-Arduino-00979D.svg)
![Reinforcement Learning](https://img.shields.io/badge/RL-Dueling%20Double%20DQN-purple.svg)

<img src="assets/full_intersection.png" alt="4-lane intersection" width="70%">

</div>

---

## 📍 Overview

Traditional traffic lights run on fixed timers or simple rules that ignore real demand: a green light can stay on for an empty lane while cars pile up elsewhere. This project replaces that logic with a **reinforcement-learning agent** that reads live traffic from each lane and decides, second by second, **which lane should get the green** to keep the whole intersection flowing.

The system has two parts working together:

- **Hardware (Arduino)** — sensors count vehicles and measure waiting time on four lanes, and drive the red / yellow / green lights.
- **Brain (Python + Dueling Double DQN)** — receives the live state over a serial link, chooses the next green lane, and sends the command back to the Arduino.

The agent was trained entirely in a custom simulator with **domain randomization** (five different traffic patterns) and then deployed on the physical intersection. It also **logs every real decision to a file**, so the model can be retrained later on real-world data to become even better.

---

## 🚀 Key Highlights

- 🧠 **Dueling Double DQN** with **n-step returns**, **Prioritized Experience Replay**, and **teacher-guided exploration** — a modern, stable RL pipeline.
- 🏆 **Beats all three classical baselines** (Fixed-Time, FCFS, Priority) on the combined efficiency score in **all 5 traffic scenarios**.
- ⚖️ **Built-in fairness guard** (anti-starvation, min/max green) so no lane is ever ignored — the agent stays efficient *and* fair.
- 🟡 **Realistic signal timing** on deployment: green → **yellow warning** → **all-red clearance** → next green.
- 💾 **Self-improving** — logs live traffic data to CSV/Excel for future retraining.
- 🔌 **Runs on real hardware** — Arduino + sensors + LEDs, talking to Python over serial.

---

## 📊 Results

All policies were evaluated on the **same** simulated intersections across five traffic patterns. The metric is the **performance score** (the combined objective that balances throughput, waiting time, and signal switching — higher is better).

<div align="center">
<img src="assets/performance_score.png" alt="Performance score by scenario" width="80%">
</div>

| Scenario      | Fixed-Time | FCFS  | Priority | **RL (this project)** | Improvement\* |
|---------------|:----------:|:-----:|:--------:|:---------------------:|:-------------:|
| Balanced      | 427.4      | 427.4 | 417.8    | **447.2**             | **+4.6%**     |
| Light         | 21.7       | 18.6  | 13.2     | **43.5**              | **+100.5%**   |
| Asymmetric    | 343.7      | 364.6 | 449.1    | **464.9**             | **+3.5%**     |
| One jammed    | 266.9      | 266.4 | 264.5    | **288.4**             | **+8.1%**     |
| Bursty        | 604.1      | 604.2 | 601.2    | **610.3**             | **+1.0%**     |

<sub>\*Improvement of the RL model over the **best** traditional method in each scenario. The RL agent wins **5 / 5** scenarios.</sub>

**What stands out:** on the hardest case — the **unbalanced (asymmetric)** intersection — the RL agent doesn't just win, it beats the aggressive *Priority* policy on **congestion, waiting time, and throughput at the same time**, without starving the quieter lanes.

<div align="center">
<img src="assets/rl_vs_others.png" alt="RL vs others" width="49%">
<img src="assets/how_much_better.png" alt="How much better" width="49%">
<img src="assets/total_cars_over_time.png" alt="Total cars over time" width="49%">
<img src="assets/overall_balance.png" alt="Overall balance radar" width="49%">
</div>

> ℹ️ **Honest note:** the RL model wins on the **overall balanced objective**, which is what it optimizes. It is not the single best on *every* isolated metric — predictable cyclers like Fixed-Time can show a lower *maximum* wait under light traffic. The agent wins because it balances flow, waiting, and fairness better than any single rule.

---

## ⚙️ How It Works

### Live control loop

```
Sensors (4 lanes) ──► Arduino (Master) ──serial──► Python program ──► RL model
       ▲                    │                                            │
       │                    │  set lights (🟢 green / 🟡 yellow / 🔴 red) │  chosen lane
       └──── traffic ───────┴────────────◄───────────────────────────────┘
```

<div align="center">
<img src="assets/flowchart.png" alt="System flowchart" width="55%">
</div>

Each cycle:

1. The Arduino reads **vehicle count** and **waiting time** for each of the 4 lanes.
2. It sends the state to Python over serial; the agent builds a normalized 9-value state vector.
3. The **Dueling DQN** scores every lane; a **safety guard** then enforces fairness rules.
4. The chosen lane goes through **yellow → all-red → green**, and the command is sent to the Arduino.
5. Every decision is **saved to a data file** for later retraining.

### Decision priority (safety guard on top of the network)

<div align="center">
<img src="assets/decision_chart.png" alt="Decision logic" width="45%">
</div>

The network proposes an action, but these rules always take priority — guaranteeing safe, fair operation:

1. **All lanes empty?** → all-red.
2. **A lane with very few cars?** → serve it immediately.
3. **A lane waited too long?** → force-serve it (anti-starvation).
4. **Below minimum green time?** → keep the current green.
5. **Above maximum green time?** → force a switch.
6. **Otherwise** → take the **model's** highest-scoring lane.

---

## 🧠 The Reinforcement-Learning Model

| Component        | Choice |
|------------------|--------|
| Algorithm        | Dueling **Double** DQN |
| State (9 inputs) | 4 normalized car counts + 4 normalized wait times + current green lane |
| Actions (4)      | Which lane receives the green |
| Network          | Dense 256 → 256 → 128, split into Value + Advantage heads (Q = V + (A − mean A)) |
| Stability        | **n-step returns (n = 3)**, **Prioritized Experience Replay**, soft target updates |
| Exploration      | **Teacher-guided** (a 1-step greedy oracle bootstraps early learning) |
| Reward           | `passed_cars − 0.01 × total_wait − 2 × switch` |
| Training         | 3500 episodes with **domain randomization** over 5 traffic scenarios |
| Fairness         | Anti-starvation guard (force-serve a lane after a max wait) |

---

## 🔌 Hardware

- **Arduino** (Master) + lane controllers
- **Ultrasonic / IR sensors** for vehicle detection
- **Red, Yellow, Green LEDs** per lane, resistors, breadboard, jumper wires
- Serial link (9600 baud) between Arduino and the Python controller

<div align="center">
<img src="assets/arduino_wiring.png" alt="Arduino wiring" width="60%">
</div>

---

## 💻 Installation & Usage

### Requirements

```bash
pip install tensorflow numpy pyserial pandas matplotlib openpyxl
```

### 1) Train the model (simulation)

Open the training notebook and **Run All**. It trains the Dueling DQN on the randomized simulator and saves `best_model.keras`.

### 2) Deploy on the intersection

Edit the two settings at the top of the controller script, then run it:

```python
MODEL_PATH = r"path/to/best_model.keras"   # your trained model
COM_PORT   = "COM5"                         # your Arduino serial port
```

```bash
python controller.py
```

A live dashboard shows each lane's cars, waiting time, and the current 🟢/🟡/🔴 status.

---

## 💾 Data Logging & Retraining

During deployment, every reading from the Arduino plus the chosen action is appended to **`traffic_data_log.csv`** (and a `.xlsx` copy) in the project folder. This builds a dataset of **real-world traffic** that can later be used to retrain the agent — closing the loop from *simulation* to *reality* and producing an even stronger model over time.

---

## 📁 Project Structure

```
Adaptive-Traffic-Light-Control-System/
├── Code/                 # Arduino (C/C++) firmware for the intersection
├── RL model/             # Training notebook + trained model (best_model.keras)
├── Simulation/           # The custom traffic simulator
├── Connection/           # Python code linking the RL model to the hardware
├── Docs/                 # Diagrams, charts, and figures
├── assets/               # Images used in this README
├── comparison.ipynb      # Benchmark: RL vs classical algorithms + charts
├── LICENSE               # MIT
└── README.md
```

---

## 🛠️ Tech Stack

**Reinforcement Learning:** TensorFlow / Keras · NumPy
**Hardware:** Arduino · C/C++ · sensors & LEDs
**Tooling:** PySerial · Pandas · Matplotlib · Jupyter

---

## 🔮 Future Enhancements

- Computer-vision vehicle counting (cameras instead of point sensors)
- Emergency-vehicle priority mode
- Multi-intersection coordination across a corridor
- Continual retraining from the logged real-world data
- Cloud dashboard for remote monitoring (MQTT / Firebase)

---

## 👤 Author

**maen-by** — [GitHub](https://github.com/maen-by)
Project: [Adaptive-Traffic-Light-Control-System](https://github.com/maen-by/Adaptive-Traffic-Light-Control-System)

## 📄 License

Released under the **MIT License** — see [LICENSE](LICENSE) for details.
