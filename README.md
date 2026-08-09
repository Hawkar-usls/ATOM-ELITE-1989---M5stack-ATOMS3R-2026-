<div align="center">

# ATOM ELITE
### Retro space game for M5Stack ATOMS3R

`embedded game` · `procedural systems` · `adaptive in-game agent`

</div>

ATOM ELITE is a creative embedded game inspired by classic space-trading and wireframe-combat games. It runs on M5Stack ATOMS3R-family hardware and includes a small adaptive in-game agent named Janus.

It is a **game/firmware project**, not an AI benchmark or research claim.

Machine-readable status: [`PROJECT_STATUS.json`](PROJECT_STATUS.json)

## Features

- procedural multi-galaxy game world;
- trading, cargo and equipment systems;
- wireframe space combat;
- missions, stations, docking and jump sequences;
- IMU-driven input;
- persistent save data;
- adaptive game-agent state saved between sessions.

The adaptive agent changes game behavior from stored outcomes and state. Terms such as `learning`, `personality` or `brain` describe game mechanics; they do not establish AGI, consciousness or general learning capability.

## Boundary

```text
PROJECT_CLASS = CREATIVE_EMBEDDED_GAME
FLAGSHIP_RESEARCH = FALSE
SCIENTIFIC_AI_BENCHMARK = FALSE
AGI_OR_CONSCIOUSNESS = NOT_CLAIMED
```

References to **Elite** describe creative inspiration. Third-party names, game rights, libraries and assets remain with their respective owners.

## Hardware / run

- M5Stack ATOMS3R-family device;
- compatible audio hardware if sound is desired;
- Arduino IDE with `M5Unified`;
- LittleFS enabled as required by the sketch.

Open the current `.ino`, select the documented board/flash layout and upload. Review the exact sketch and local hardware configuration before flashing.

## License

See the repository license for code authored here; third-party components retain their own licenses.
