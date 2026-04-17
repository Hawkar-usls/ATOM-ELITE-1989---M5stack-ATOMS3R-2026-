# ATOM ELITE — JANUS ATMOSPHERE PACK v8.0.4

**A retro space trading & combat simulator inspired by Elite (1984)**  
**Running natively on M5Stack Atom Elite** with an **adaptive AI pilot named Janus**.

![M5Stack Atom Elite]
<img width="1920" height="2560" alt="Elite" src="https://github.com/user-attachments/assets/caee3e32-a127-4c13-94b1-ec584105b4c5" />

## ✨ Features

- **Procedural 8-galaxy universe** (64 systems per galaxy)
- **Full trading system** with dynamic prices and cargo
- **Real-time 3D wireframe combat** (12 ship types)
- **Two station types**: Coriolis & Dodecahedron
- **Missions**: Constrictor Hunt + Thargoid Documents
- **Escape pods**, **cargo canisters**, **debris**, **police scans**, **contraband checks**
- **Fuel scooping**, **ECM**, **military laser**, **galactic hyperdrive**
- **Dock computer** with authentic tunnel animation + melody
- **Witchspace jumps** with Thargoid ambush chance

### 🧠 JANUS — The Adaptive AI Pilot
- Neural-style reinforcement learning agent
- Learns from every death (16 death memory slots)
- Evolves **personality traits**: Trader / Hunter / Coward / Pirate / Balanced
- Uses M5Stack IMU tilt for real-time aggression/caution bias
- Saves best brain (`agent_best.dat`) and improves over time

## 🎮 Controls

| Button          | Action                              |
|-----------------|-------------------------------------|
| **BtnA short**  | Cycle brightness (4 levels)        |
| **BtnA long**   | Toggle sound on/off                |
| **BtnA very long** (>2s) | Cycle UI screens (Flight → Status → Market → Equip → Local → Map → Mission) |

**In-flight:**
- Tilt the device → influences ship yaw/pitch and Janus aggression/caution

## 🛠 Hardware Requirements

- **M5Stack Atom Elite** (S3)
- **Atomic Echo / Voice Base** (for speaker — recommended)
- LittleFS enabled in Arduino IDE

## 📥 Installation

1. Download the latest release or clone the repo
2. Open `ATOM-ELITE-v8.0.4 - Janus.ino` in **Arduino IDE**
3. Install required libraries:
   - `M5Unified`
4. Select board: **M5Stack AtomS3R**
5. Set **Flash Size** to at least **4MB with 1.5MB LittleFS**
6. Upload

First boot will create save files (`pilot.dat`, `agent.dat`, `learn.dat`, `agent_best.dat`).

## 📜 Changelog (v8.0.4)

- New station variants (Coriolis + Dodeca)
- Escape pods & rescue mechanics
- Contraband/customs checks
- Improved jump tunnel + docking melody
- Dock/star collision fixes
- Better Janus learning & personality system
- UI polish and performance tweaks

## 📄 License

This project is open-source under the **MIT License**.  
Feel free to fork, modify, and fly it into the black.

---

**Made with ❤️ for the M5Stack retro gaming community**

---

### Tips for you (the author):

- Replace the image link with an actual screenshot/GIF of the game in action.
- Add a **Releases** section on GitHub with the compiled `.bin` file for easy flashing.
- If you want, I can also create a shorter version or a version with more technical details.

Would you like me to also generate a **short version** (for the repo description) or a **CHANGELOG.md**? Just say the word! 🚀
