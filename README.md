# 🚦 Smart Traffic Light Control System

![Arduino](https://img.shields.io/badge/Platform-Arduino-blue)
![Language](https://img.shields.io/badge/Language-C%2FC%2B%2B-green)
![Simulation](https://img.shields.io/badge/Simulation-Proteus-orange)
![Status](https://img.shields.io/badge/Project-Completed-success)

---

## 📍 Overview
A real-time **adaptive traffic light control system** developed using Arduino to optimize traffic flow across a four-way intersection.

This system replaces traditional fixed-timing signals with a **dynamic scheduling algorithm** that adjusts in real-time based on traffic demand, improving efficiency and fairness.

---

## 🖼️ Project Preview

### 🔹 System Logic Flowchart
![Flowchart](logic_flowchart.png)

### 🔹 Arduino Wiring
![Wiring](arduino_wiring.png)

### 🔹 Full Intersection Simulation
![Simulation](full_intersection_view.png)

### 🔹 Signal Details
![Green Signal](green_signal_detail.png)
![Red Signal](red_signal_detail.png)

### 🔹 Serial Monitor Output
![Serial Output](serial_monitor_output.png)

---

## 🚀 Key Highlights
- Developed a **real-time adaptive traffic control algorithm**  
- Implemented **dynamic green light timing** based on vehicle count  
- Designed a **fair scheduling system (Round Robin + Priority Boosting)**  
- Built a complete **embedded system using Arduino**  
- Simulated realistic traffic scenarios using keypad input  

---

## 🧠 Core Logic & Algorithms

### 🔹 Priority Formula
```
Priority = Cars + Waiting Cycles
```

### 🔹 Scheduling Strategy
- Round Robin baseline  
- Priority override for congested lanes  
- Forced scheduling for long-waiting lanes  

### 🔹 Adaptive Timing
- Green light duration increases with traffic load  
- Constrained between minimum and maximum thresholds  

---

## 🛠 Technologies & Tools
- **Embedded Systems:** Arduino (C/C++)  
- **Simulation:** Proteus  
- **Hardware:** LEDs, Keypad  
- **Debugging:** Serial Monitor  

---

## 🔌 System Architecture
- 4 independent traffic lanes  
- Input: Keypad (vehicle simulation)  
- Output: LED signals  
- State Machine:
  - Idle → Green → Transition → Next Lane  

---

## ⚙️ Key Parameters
| Parameter | Value |
|----------|------|
| Minimum Green Time | 3000 ms |
| Maximum Green Time | 12000 ms |
| Time per Vehicle | 120 ms |
| Switch Delay | 2000 ms |
| Vehicles per Cycle | 10 |

---

## 📊 Sample Output (Serial Monitor)
```
S1 C=100 P=100 W=0 | S2 C=0 P=0 W=0 | S3 C=3 P=3 W=0 | S4 C=11 P=11 W=0 || GREEN=S1 | RR=S2
```

---

## 📁 Project Structure
```
Code/           → Arduino source code  
Simulation/     → Proteus files  
Docs/           → Diagrams & images  
README.md  
```

---

## 🎯 Achievements
- Improved traffic flow efficiency compared to static systems  
- Ensured fairness across all lanes (no starvation)  
- Applied **real-world scheduling algorithms in embedded systems**  

---

## 🔮 Future Enhancements
- Real sensors (IR / Ultrasonic) instead of keypad  
- AI-based traffic prediction  
- LCD dashboard  
- Pedestrian crossing system  

---

## 👨‍💻 Author
Developed as part of an **Embedded Systems / Smart Systems project**.