#include <M5Unified.h>
#include <LittleFS.h>
#include <math.h>

// =====================================================
// ATOM ELITE - JANUS ATMOSPHERE PACK v8.0.4
// Atmosphere Pack: station variants, escape pods, contraband checks, jump tunnel, dock melody, dock/star fixes.
// =====================================================

#define SCREEN_W 128
#define SCREEN_H 128

#define SAVE_PILOT_FILE "/pilot.dat"
#define SAVE_AGENT_FILE "/agent.dat"
#define SAVE_LEARN_FILE "/learn.dat"
#define SAVE_BEST_AGENT_FILE "/agent_best.dat"

#define BASE_CARGO 20
#define EXPANDED_CARGO 40

#define MAX_SYSTEMS 64
#define MAX_MARKET_ITEMS 8
#define MAX_ENEMIES 8
#define MAX_TRAFFIC 6
#define STAR_COUNT 32

struct Vec3 { float x, y, z; };
struct Edge { uint8_t a, b; };

struct StarSystem {
  char name[16];
  uint8_t economy;
  uint8_t government;
  uint8_t techLevel;
  uint8_t danger;
  uint8_t x;
  uint8_t y;
};

struct Commodity {
  const char* name;
  int basePrice;
  int gradient;
};

enum AgentMode {
  MODE_DOCKED = 0,
  MODE_LAUNCH = 1,
  MODE_PATROL = 2,
  MODE_COMBAT = 3,
  MODE_RETURN = 4
};

enum UiMode {
  UI_FLIGHT = 0,
  UI_MAP = 1,
  UI_MISSION = 2,
  UI_WITCHSPACE = 3,
  UI_STATUS = 4,
  UI_MARKET = 5,
  UI_EQUIP = 6,
  UI_LOCAL = 7,
  UI_JUMP = 8
};

enum LegalStatus {
  LEGAL_CLEAN = 0,
  LEGAL_OFFENDER = 1,
  LEGAL_FUGITIVE = 2
};

enum EquipmentFlags {
  EQ_NONE              = 0,
  EQ_FUEL_SCOOP        = 1 << 0,
  EQ_GAL_HYPERDRIVE    = 1 << 1,
  EQ_CARGO_EXPANSION   = 1 << 2,
  EQ_MILITARY_LASER    = 1 << 3,
  EQ_DOCK_COMPUTER     = 1 << 4,
  EQ_ECM               = 1 << 5
};

enum DamageFlags {
  DMG_NONE    = 0,
  DMG_LASER   = 1 << 0,
  DMG_ENGINE  = 1 << 1,
  DMG_SCANNER = 1 << 2
};

enum MissionStage {
  MISSION_NONE = 0,
  MISSION_CONSTRICTOR = 1,
  MISSION_THARGOID_DOCS = 2
};

enum ShipType {
  SHIP_COBRA = 0,
  SHIP_MAMBA = 1,
  SHIP_TRANSPORT = 2,
  SHIP_VIPER = 3,
  SHIP_SIDEWINDER = 4,
  SHIP_THARGOID = 5,
  SHIP_THARGON = 6,
  SHIP_CONSTRICTOR = 7,
  SHIP_ADDER = 8,
  SHIP_ASP = 9,
  SHIP_PYTHON = 10,
  SHIP_ANACONDA = 11
};

enum ShipRole {
  ROLE_TRADER = 0,
  ROLE_PIRATE = 1,
  ROLE_POLICE = 2,
  ROLE_HUNTER = 3,
  ROLE_THARGOID = 4,
  ROLE_THARGON = 5,
  ROLE_BOSS = 6
};

enum StationServiceStep {
  SERVICE_IDLE = 0,
  SERVICE_MARKET = 1,
  SERVICE_EQUIP = 2,
  SERVICE_STATUS = 3,
  SERVICE_LAUNCH = 4
};

enum DockPhase {
  DOCK_NONE = 0,
  DOCK_APPROACH = 1,
  DOCK_ALIGN = 2,
  DOCK_TUNNEL = 3
};

enum StationType {
  STATION_CORIOLIS = 0,
  STATION_DODECA = 1
};

struct PassengerJob {
  bool active;
  uint8_t targetSystem;
  int reward;
};

enum PilotArchetype {
  ARCHETYPE_BALANCED = 0,
  ARCHETYPE_TRADER = 1,
  ARCHETYPE_HUNTER = 2,
  ARCHETYPE_COWARD = 3,
  ARCHETYPE_PIRATE = 4
};

struct Debris {
  Vec3 pos;
  Vec3 vel;
  uint16_t ttl;
  bool alive;
};

struct CargoCanister {
  Vec3 pos;
  Vec3 vel;
  uint8_t item;
  uint16_t ttl;
  bool alive;
};

struct EscapePod {
  Vec3 pos;
  Vec3 vel;
  uint16_t ttl;
  bool alive;
};

enum DeathCause {
  DEATH_NONE = 0,
  DEATH_ENEMY = 1,
  DEATH_COLLISION = 2,
  DEATH_SUN = 3,
  DEATH_POLICE = 4,
  DEATH_DOCK = 5
};

enum PilotGoal {
  GOAL_NONE = 0,
  GOAL_TRADE = 1,
  GOAL_DOCK = 2,
  GOAL_REFUEL = 3,
  GOAL_FIGHT = 4,
  GOAL_ESCAPE = 5,
  GOAL_MISSION = 6,
  GOAL_UPGRADE = 7
};

struct DeathRecord {
  uint8_t system;
  uint8_t cause;
  float shieldAtDeath;
  float energyAtDeath;
  float aggression;
  float caution;
  uint8_t cargoCount;
  uint16_t bounty;
};

struct LearnState {
  DeathRecord deathMemory[16];
  uint8_t deathIndex;
  float systemDeathPenalty[MAX_SYSTEMS];
  float bestFitness;
  uint16_t totalDeaths;
};

struct PilotState {
  uint8_t currentSystem;
  uint8_t galaxyIndex;
  int credits;
  int fuel;
  uint8_t cargoType[EXPANDED_CARGO];
  int cargoBuyPrice[EXPANDED_CARGO];
  uint8_t cargoCount;
  int totalProfit;
  int score;
  int kills;
  bool docked;
  uint8_t legalStatus;
  uint16_t bounty;
  uint32_t equipmentFlags;
  uint8_t missionStage;
  uint8_t missionProgress;
  uint8_t missiles;
  uint8_t energyMode;
  uint8_t damagedFlags;
  bool missileIncoming;
  uint8_t escapePods;
  PassengerJob passengerJob;
  int ecmCooldown;
};

struct AgentState {
  float w[12];
  float slimeTrace[12];
  float bias;
  float lr;
  float slimeThreshold;
  uint32_t decisions;
  AgentMode mode;
  float aggression;
  float caution;

  float traderInstinct;   // 0..1
  float riskTolerance;    // 0..1
  float lawfulness;       // 0..1
  float curiosity;        // 0..1
  uint8_t archetype;
};

struct ShipModel {
  const Vec3* verts;
  const Edge* edges;
  uint8_t vertCount;
  uint8_t edgeCount;
};

struct SpaceShip {
  ShipType type;
  uint8_t role;
  Vec3 pos;
  Vec3 vel;
  float hp;
  bool alive;
  bool hostile;
};

struct ShipStats {
  float hp;
  float maxSpeed;
  float turnRate;
  uint8_t lawRisk;
};

static inline float clampf(float v, float mn, float mx) {
  return (v < mn) ? mn : ((v > mx) ? mx : v);
}

static inline int clampi(int v, int mn, int mx) {
  return (v < mn) ? mn : ((v > mx) ? mx : v);
}

float frandRange(float a, float b) {
  return a + ((float)random(0, 10000) / 10000.0f) * (b - a);
}

uint16_t dist2D(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
  int dx = (int)x2 - (int)x1;
  int dy = (int)y2 - (int)y1;
  return (uint16_t)sqrtf((float)(dx * dx + dy * dy));
}

uint16_t colorDim(uint16_t c, float f) {
  f = clampf(f, 0.10f, 1.00f);
  uint8_t r = (uint8_t)(((c >> 11) & 0x1F) * f);
  uint8_t g = (uint8_t)(((c >> 5) & 0x3F) * f);
  uint8_t b = (uint8_t)((c & 0x1F) * f);
  return (r << 11) | (g << 5) | b;
}

// ---------------- PALETTE ----------------
const uint16_t UI_AMBER_BRIGHT = 0xFD20;
const uint16_t UI_AMBER = 0xFC60;
const uint16_t UI_AMBER_DARK = 0xA320;
const uint16_t UI_REDGRID = 0xC800;
const uint16_t UI_DIM = 0x7BEF;
const uint16_t UI_SOFT = 0xBDF7;

// -----------------------------------------------------
// GLOBALS
// -----------------------------------------------------
StarSystem galaxy[MAX_SYSTEMS];

Commodity marketItems[MAX_MARKET_ITEMS] = {
  {"Food",      18, -2},
  {"Textiles",  20, -1},
  {"Liquor",    30, -3},
  {"Luxuries",  90,  8},
  {"Computers", 80, 15},
  {"Machinery", 55,  6},
  {"Alloys",    40,  1},
  {"Medicine",  65,  7}
};

PilotState pilot;
AgentState janus;

SpaceShip enemies[MAX_ENEMIES];
SpaceShip traffic[MAX_TRAFFIC];
Vec3 stars[STAR_COUNT];
Debris debris[24];
CargoCanister canisters[10];
EscapePod escapePods[6];
LearnState learnState;
bool justDied = false;
DeathCause lastDeathCause = DEATH_NONE;
unsigned long survivalStartMs = 0;
PilotGoal currentGoal = GOAL_NONE;
bool alreadyJumpedThisDock = false;
StationType currentStationType = STATION_CORIOLIS;
int dockAlignTicks = 0;
unsigned long lastPoliceScanRoll = 0;

UiMode uiMode = UI_FLIGHT;
StationServiceStep stationStep = SERVICE_IDLE;

String statusLine = "BOOT";

bool paused = false;
bool laserFiring = false;
bool soundEnabled = true;
bool holdActionTriggered = false;
bool inWitchSpace = false;
bool stationFlowActive = false;
bool jumpInProgress = false;
DockPhase dockPhase = DOCK_NONE;

unsigned long btnPressStart = 0;
unsigned long lastLogicTick = 0;
unsigned long lastRenderTick = 0;
unsigned long lastSaveTick = 0;
unsigned long lastAgentTick = 0;
unsigned long lastEventRoll = 0;

int patrolTimer = 0;
int launchTimer = 0;
int returnTimer = 0;
int dockTimer = 0;
int witchTimer = 0;
int stationStepTimer = 0;
int jumpTimer = 0;
int tunnelTimer = 0;
uint8_t jumpTargetSystem = 0;

uint8_t brightnessIndex = 2;
const uint8_t brightnessLevels[4] = {32, 96, 160, 255};

float shipYaw = 0.0f;
float shipPitch = 0.0f;
float shipRoll = 0.0f;
float shipSpeed = 0.0f;
float shipEnergy = 100.0f;
float shipShield = 100.0f;
float laserHeat = 0.0f;

float sensorBiasYaw = 0.0f;
float sensorBiasAggro = 0.0f;
float sensorBiasCaution = 0.0f;

Vec3 stationPos = {0.0f, 0.0f, 250.0f};
Vec3 planetPos  = {0.0f, 0.0f, 320.0f};
Vec3 sunPos     = {80.0f, -40.0f, 420.0f};
float stationSpin = 0.0f;

// -----------------------------------------------------
// MODELS
// -----------------------------------------------------
const Vec3 cobraVerts[] = {
  {0, 0, 18}, {14, 5, -8}, {14, -5, -8}, {-14, 5, -8}, {-14, -5, -8}, {0, 9, -6}, {0, -9, -6}
};
const Edge cobraEdges[] = {
  {0,1},{0,2},{0,3},{0,4},{1,5},{3,5},{2,6},{4,6},{1,2},{3,4},{5,6}
};

const Vec3 mambaVerts[] = {
  {0, 0, 20}, {16, 0, -10}, {-16, 0, -10}, {0, 8, -8}, {0, -8, -8}, {8, 0, 2}, {-8, 0, 2}
};
const Edge mambaEdges[] = {
  {0,1},{0,2},{0,3},{0,4},{1,3},{1,4},{2,3},{2,4},{5,3},{6,4},{5,6}
};

const Vec3 transportVerts[] = {
  {0,0,22},{18,8,-16},{18,-8,-16},{-18,8,-16},{-18,-8,-16},{0,11,-10},{0,-11,-10},{0,0,-22}
};
const Edge transportEdges[] = {
  {0,1},{0,2},{0,3},{0,4},{1,5},{3,5},{2,6},{4,6},{1,2},{3,4},{5,7},{6,7}
};

const Vec3 viperVerts[] = {
  {0,0,16},{12,4,-6},{12,-4,-6},{-12,4,-6},{-12,-4,-6},{0,6,-4},{0,-6,-4}
};
const Edge viperEdges[] = {
  {0,1},{0,2},{0,3},{0,4},{1,5},{3,5},{2,6},{4,6},{1,2},{3,4},{5,6}
};

const Vec3 sidewinderVerts[] = {
  {0,0,14},{10,4,-5},{10,-4,-5},{-10,4,-5},{-10,-4,-5},{0,0,-10}
};
const Edge sidewinderEdges[] = {
  {0,1},{0,2},{0,3},{0,4},{1,2},{3,4},{1,5},{2,5},{3,5},{4,5}
};

const Vec3 thargoidVerts[] = {
  {12,0,0},{8,8,0},{0,12,0},{-8,8,0},{-12,0,0},{-8,-8,0},{0,-12,0},{8,-8,0},
  {0,0,10},{0,0,-10}
};
const Edge thargoidEdges[] = {
  {0,1},{1,2},{2,3},{3,4},{4,5},{5,6},{6,7},{7,0},
  {0,8},{2,8},{4,8},{6,8},
  {0,9},{2,9},{4,9},{6,9}
};

const Vec3 thargonVerts[] = {
  {0,0,8},{6,0,-4},{-6,0,-4},{0,6,-4},{0,-6,-4}
};
const Edge thargonEdges[] = {
  {0,1},{0,2},{0,3},{0,4},{1,3},{1,4},{2,3},{2,4}
};

const Vec3 constrictorVerts[] = {
  {0,0,24},{18,6,-10},{18,-6,-10},{-18,6,-10},{-18,-6,-10},{0,10,-8},{0,-10,-8},{0,0,-20}
};
const Edge constrictorEdges[] = {
  {0,1},{0,2},{0,3},{0,4},{1,5},{3,5},{2,6},{4,6},{1,2},{3,4},{5,7},{6,7}
};

const Vec3 adderVerts[] = {
  {0,0,15},{11,4,-6},{11,-4,-6},{-11,4,-6},{-11,-4,-6},{0,0,-12}
};
const Edge adderEdges[] = {
  {0,1},{0,2},{0,3},{0,4},{1,2},{3,4},{1,5},{2,5},{3,5},{4,5}
};

const Vec3 aspVerts[] = {
  {0,0,22},{15,7,-10},{15,-7,-10},{-15,7,-10},{-15,-7,-10},{0,12,-5},{0,-12,-5},{0,0,-18}
};
const Edge aspEdges[] = {
  {0,1},{0,2},{0,3},{0,4},{1,5},{3,5},{2,6},{4,6},{1,2},{3,4},{5,7},{6,7}
};

const Vec3 pythonVerts[] = {
  {0,0,26},{20,8,-16},{20,-8,-16},{-20,8,-16},{-20,-8,-16},{0,12,-10},{0,-12,-10},{0,0,-24}
};
const Edge pythonEdges[] = {
  {0,1},{0,2},{0,3},{0,4},{1,5},{3,5},{2,6},{4,6},{1,2},{3,4},{5,7},{6,7}
};

const Vec3 anacondaVerts[] = {
  {0,0,30},{24,10,-20},{24,-10,-20},{-24,10,-20},{-24,-10,-20},{0,14,-12},{0,-14,-12},{0,0,-30}
};
const Edge anacondaEdges[] = {
  {0,1},{0,2},{0,3},{0,4},{1,5},{3,5},{2,6},{4,6},{1,2},{3,4},{5,7},{6,7}
};

const Vec3 coriolisVerts[] = {
  {0,0,30},{30,30,0},{-30,30,0},{30,-30,0},{-30,-30,0},{0,0,-30}
};
const Edge coriolisEdges[] = {
  {0,1},{0,2},{0,3},{0,4},{1,2},{1,3},{2,4},{3,4},{5,1},{5,2},{5,3},{5,4}
};

const Vec3 dodecaVerts[] = {
  {0,0,28}, {18,0,10}, {-18,0,10}, {0,18,10}, {0,-18,10},
  {18,18,-10}, {18,-18,-10}, {-18,18,-10}, {-18,-18,-10}, {0,0,-28}
};
const Edge dodecaEdges[] = {
  {0,1},{0,2},{0,3},{0,4},
  {1,5},{1,6},{3,5},{4,6},
  {2,7},{2,8},{3,7},{4,8},
  {5,9},{6,9},{7,9},{8,9},
  {5,7},{6,8}
};

ShipModel shipModels[] = {
  {cobraVerts,       cobraEdges,       7, 11},
  {mambaVerts,       mambaEdges,       7, 11},
  {transportVerts,   transportEdges,   8, 12},
  {viperVerts,       viperEdges,       7, 11},
  {sidewinderVerts,  sidewinderEdges,  6, 10},
  {thargoidVerts,    thargoidEdges,   10, 16},
  {thargonVerts,     thargonEdges,     5,  8},
  {constrictorVerts, constrictorEdges, 8, 12},
  {adderVerts,       adderEdges,       6, 10},
  {aspVerts,         aspEdges,         8, 12},
  {pythonVerts,      pythonEdges,      8, 12},
  {anacondaVerts,    anacondaEdges,    8, 12}
};

ShipStats shipStats[] = {
  {55.0f, 0.26f, 0.045f, 15},
  {48.0f, 0.31f, 0.055f, 20},
  {70.0f, 0.18f, 0.025f,  5},
  {45.0f, 0.28f, 0.050f,  0},
  {30.0f, 0.30f, 0.060f, 20},
  {70.0f, 0.22f, 0.040f, 80},
  {18.0f, 0.34f, 0.070f, 80},
  {110.0f,0.29f, 0.050f, 90},
  {36.0f, 0.27f, 0.050f, 10},
  {72.0f, 0.28f, 0.050f, 25},
  {95.0f, 0.21f, 0.030f, 10},
  {125.0f,0.17f, 0.022f,  5}
};

ShipModel coriolisStationModel = {coriolisVerts, coriolisEdges, 6, 12};
ShipModel dodecaStationModel = {dodecaVerts, dodecaEdges, 10, 18};

// -----------------------------------------------------
// SOUND INIT
// -----------------------------------------------------
void initStackedSpeaker() {
  // Official M5 guidance for AtomS3R + Atomic Echo/Voice Base uses
  // M5.config().external_speaker.atomic_echo = 1 before M5.begin().
  // We still configure Speaker afterwards so tones are ready in sketches
  // that rely on M5Unified tone playback.
  auto spk_cfg = M5.Speaker.config();
  spk_cfg.sample_rate = 16000;
  spk_cfg.stereo = false;
  spk_cfg.buzzer = false;
  spk_cfg.use_dac = false;
  spk_cfg.pin_data_out = 5;
  spk_cfg.pin_ws = 6;
  spk_cfg.pin_bck = 8;
  M5.Speaker.config(spk_cfg);
  M5.Speaker.begin();
  M5.Speaker.setVolume(110);
}

void clearKnownStaleFiles() {
  const char* stale[] = {
    "/agent_tmp.dat",
    "/agent_bad.dat",
    "/candidate.dat",
    "/brain_old.dat",
    "/brain_tmp.dat",
    "/agent_prev.dat"
  };
  for (size_t i = 0; i < sizeof(stale) / sizeof(stale[0]); ++i) {
    if (LittleFS.exists(stale[i])) LittleFS.remove(stale[i]);
  }
}

void saveLearnState() {
  File f = LittleFS.open(SAVE_LEARN_FILE, "w");
  if (f) {
    f.write((const uint8_t*)&learnState, sizeof(learnState));
    f.close();
  }
}

void loadLearnState() {
  memset(&learnState, 0, sizeof(learnState));
  if (!LittleFS.exists(SAVE_LEARN_FILE)) return;
  File f = LittleFS.open(SAVE_LEARN_FILE, "r");
  if (f && f.size() == sizeof(learnState)) {
    f.read((uint8_t*)&learnState, sizeof(learnState));
  }
  if (f) f.close();
}

float computeRunFitness() {
  float survivalSec = (millis() - survivalStartMs) / 1000.0f;
  float score = 0.0f;
  score += survivalSec * 1.5f;
  score += pilot.kills * 18.0f;
  score += pilot.credits * 0.05f;
  score += pilot.totalProfit * 0.03f;
  score -= pilot.bounty * 0.2f;
  return score;
}

void saveBestAgentIfImproved() {
  float fitness = computeRunFitness();
  if (fitness <= learnState.bestFitness) return;

  learnState.bestFitness = fitness;
  File f = LittleFS.open(SAVE_BEST_AGENT_FILE, "w");
  if (f) {
    f.write((const uint8_t*)&janus, sizeof(janus));
    f.close();
  }
  saveLearnState();
}

void loadBestAgentIfAvailable() {
  if (!LittleFS.exists(SAVE_BEST_AGENT_FILE)) return;
  File f = LittleFS.open(SAVE_BEST_AGENT_FILE, "r");
  if (f && f.size() == sizeof(janus)) {
    f.read((uint8_t*)&janus, sizeof(janus));
  }
  if (f) f.close();
}

void registerDeath(DeathCause cause) {
  DeathRecord &d = learnState.deathMemory[learnState.deathIndex];
  d.system = pilot.currentSystem;
  d.cause = (uint8_t)cause;
  d.shieldAtDeath = shipShield;
  d.energyAtDeath = shipEnergy;
  d.aggression = janus.aggression;
  d.caution = janus.caution;
  d.cargoCount = pilot.cargoCount;
  d.bounty = pilot.bounty;

  learnState.deathIndex = (learnState.deathIndex + 1) % 16;
  learnState.totalDeaths++;
  lastDeathCause = cause;
  justDied = true;

  if (pilot.currentSystem < MAX_SYSTEMS) {
    learnState.systemDeathPenalty[pilot.currentSystem] =
      clampf(learnState.systemDeathPenalty[pilot.currentSystem] + 0.25f, 0.0f, 5.0f);
  }

  saveBestAgentIfImproved();
  saveLearnState();
}

void analyzeAndAdapt() {
  int valid = 0;
  int enemyDeaths = 0;
  int collisionDeaths = 0;
  int sunDeaths = 0;
  int dockDeaths = 0;

  for (int i = 0; i < 16; ++i) {
    if (learnState.deathMemory[i].cause == DEATH_NONE) continue;
    valid++;
    if (learnState.deathMemory[i].cause == DEATH_ENEMY) enemyDeaths++;
    if (learnState.deathMemory[i].cause == DEATH_COLLISION) collisionDeaths++;
    if (learnState.deathMemory[i].cause == DEATH_SUN) sunDeaths++;
    if (learnState.deathMemory[i].cause == DEATH_DOCK) dockDeaths++;
  }

  if (valid < 2) return;

  if (enemyDeaths > valid / 3) {
    janus.aggression *= 0.88f;
    janus.caution += 0.08f;
  }
  if (collisionDeaths > valid / 4) {
    janus.caution += 0.10f;
    shipSpeed = clampf(shipSpeed * 0.9f, 0.08f, 0.25f);
  }
  if (sunDeaths > 0) {
    janus.caution += 0.06f;
  }
  if (dockDeaths > 0) {
    janus.caution += 0.05f;
  }

  janus.aggression = clampf(janus.aggression, 0.12f, 0.95f);
  janus.caution = clampf(janus.caution, 0.12f, 0.98f);
}

void onRespawnLearning() {
  analyzeAndAdapt();
  mutatePersonalityAfterDeath(lastDeathCause);
  // tiny mutation, but only around current/best brain
  for (int i = 0; i < 12; ++i) {
    janus.w[i] += frandRange(-0.015f, 0.015f);
  }
  janus.aggression += frandRange(-0.02f, 0.02f);
  janus.caution += frandRange(-0.02f, 0.02f);
  janus.aggression = clampf(janus.aggression, 0.12f, 0.95f);
  janus.caution = clampf(janus.caution, 0.12f, 0.98f);
  janus.traderInstinct = clampf(janus.traderInstinct, 0.0f, 1.0f);
  janus.riskTolerance = clampf(janus.riskTolerance, 0.0f, 1.0f);
  janus.lawfulness = clampf(janus.lawfulness, 0.0f, 1.0f);
  janus.curiosity = clampf(janus.curiosity, 0.0f, 1.0f);
  pilot.damagedFlags = DMG_NONE;
  pilot.escapePods = 0;
  justDied = false;
  survivalStartMs = millis();
}

// -----------------------------------------------------
// AUDIO
// -----------------------------------------------------
void playToneSafe(uint16_t freq, uint16_t dur) {
  if (!soundEnabled) return;
  M5.Speaker.tone(freq, dur);
}

void playLaserSound() { playToneSafe(1800, 30); }
void playHitSound()   { playToneSafe(320, 40); }
void playKillSound()  { playToneSafe(900, 80); }
void playDockSound()  { playToneSafe(600, 60); }
void playJumpSound()  { playToneSafe(1200, 120); }
void playAlertSound() { playToneSafe(240, 150); }

// -----------------------------------------------------
// LEGAL / RANK
// -----------------------------------------------------
const char* getRankName(int kills) {
  if (kills < 8) return "HARMLESS";
  if (kills < 16) return "M.HARM";
  if (kills < 32) return "POOR";
  if (kills < 64) return "AVERAGE";
  if (kills < 128) return "ABVAVG";
  if (kills < 256) return "COMP";
  if (kills < 512) return "DANGER";
  if (kills < 1024) return "DEADLY";
  return "ELITE";
}

const char* getLegalName(uint8_t st) {
  if (st == LEGAL_CLEAN) return "CLEAN";
  if (st == LEGAL_OFFENDER) return "OFFND";
  return "FUG";
}

void addCrime(uint16_t amount) {
  pilot.bounty += amount;
  if (pilot.bounty >= 80) pilot.legalStatus = LEGAL_FUGITIVE;
  else if (pilot.bounty >= 20) pilot.legalStatus = LEGAL_OFFENDER;
  else pilot.legalStatus = LEGAL_CLEAN;
}

void decayLegalStatusInDock() {
  if (pilot.bounty > 0 && pilot.docked) {
    pilot.bounty = (pilot.bounty >= 5) ? (pilot.bounty - 5) : 0;
  }

  if (pilot.bounty == 0) pilot.legalStatus = LEGAL_CLEAN;
  else if (pilot.bounty < 80) pilot.legalStatus = LEGAL_OFFENDER;
  else pilot.legalStatus = LEGAL_FUGITIVE;
}

// -----------------------------------------------------
// ECONOMY / EQUIPMENT
// -----------------------------------------------------

bool hasEquipment(uint32_t flag) {
  return (pilot.equipmentFlags & flag) != 0;
}

bool isDamaged(uint8_t flag) {
  return (pilot.damagedFlags & flag) != 0;
}

void damageRandomSubsystem() {
  int roll = random(0, 3);
  if (roll == 0) pilot.damagedFlags |= DMG_LASER;
  else if (roll == 1) pilot.damagedFlags |= DMG_ENGINE;
  else pilot.damagedFlags |= DMG_SCANNER;
}

void repairAllSystems() {
  pilot.damagedFlags = DMG_NONE;
}

int cargoLimit() {
  return hasEquipment(EQ_CARGO_EXPANSION) ? EXPANDED_CARGO : BASE_CARGO;
}

void buyEquipment(uint32_t flag, int cost) {
  if (hasEquipment(flag)) return;
  if (pilot.credits < cost) return;
  pilot.credits -= cost;
  pilot.equipmentFlags |= flag;

  if (flag == EQ_FUEL_SCOOP) statusLine = "FUEL SCOOP";
  else if (flag == EQ_GAL_HYPERDRIVE) statusLine = "GAL DRIVE";
  else if (flag == EQ_CARGO_EXPANSION) statusLine = "CARGO EXP";
  else if (flag == EQ_MILITARY_LASER) statusLine = "MIL LASER";
  else if (flag == EQ_DOCK_COMPUTER) statusLine = "DOCK COMP";
  else if (flag == EQ_ECM) statusLine = "ECM FIT";
}

void autoBuyEquipment() {
  if (!hasEquipment(EQ_FUEL_SCOOP) && pilot.credits >= 150) buyEquipment(EQ_FUEL_SCOOP, 150);
  if (!hasEquipment(EQ_CARGO_EXPANSION) && pilot.credits >= 220) buyEquipment(EQ_CARGO_EXPANSION, 220);
  if (!hasEquipment(EQ_MILITARY_LASER) && pilot.credits >= 400) buyEquipment(EQ_MILITARY_LASER, 400);
  if (!hasEquipment(EQ_GAL_HYPERDRIVE) && pilot.credits >= 600 && pilot.kills > 25) buyEquipment(EQ_GAL_HYPERDRIVE, 600);
}

int getCommodityPrice(uint8_t item, const StarSystem &sys) {
  int p = marketItems[item].basePrice + marketItems[item].gradient * sys.economy;
  p += (sys.techLevel - 7);
  p += ((7 - sys.government) / 2);
  p += random(-3, 4);
  return clampi(p, 2, 200);
}

bool buyCommodity(uint8_t item) {
  if (pilot.cargoCount >= cargoLimit()) return false;
  int price = getCommodityPrice(item, galaxy[pilot.currentSystem]);
  if (pilot.credits < price) return false;
  pilot.credits -= price;
  pilot.cargoType[pilot.cargoCount] = item;
  pilot.cargoBuyPrice[pilot.cargoCount] = price;
  pilot.cargoCount++;
  return true;
}

int sellAllCargoHere() {
  int earned = 0;
  StarSystem &sys = galaxy[pilot.currentSystem];

  for (int i = pilot.cargoCount - 1; i >= 0; --i) {
    uint8_t item = pilot.cargoType[i];
    int price = getCommodityPrice(item, sys);
    earned += price;
    pilot.credits += price;
    pilot.totalProfit += (price - pilot.cargoBuyPrice[i]);
  }
  pilot.cargoCount = 0;
  return earned;
}

int fuelCostTo(uint8_t dst) {
  uint16_t d = dist2D(galaxy[pilot.currentSystem].x, galaxy[pilot.currentSystem].y,
                      galaxy[dst].x, galaxy[dst].y);
  return clampi((int)(d / 12), 1, 20);
}

int bestCommodityMarginHere(uint8_t target) {
  int bestScore = -9999;
  for (uint8_t i = 0; i < MAX_MARKET_ITEMS; ++i) {
    int buyP = getCommodityPrice(i, galaxy[pilot.currentSystem]);
    int sellP = getCommodityPrice(i, galaxy[target]);
    int score = sellP - buyP;
    if (score > bestScore) bestScore = score;
  }
  return bestScore;
}

void refuelAtStation() {
  int need = 70 - pilot.fuel;
  if (need <= 0) return;
  int pricePer = 2;
  int maxBuy = pilot.credits / pricePer;
  int buy = (need < maxBuy) ? need : maxBuy;
  if (buy > 0) {
    pilot.credits -= buy * pricePer;
    pilot.fuel += buy;
  }
}

void updateFuelScoop() {
  if (!hasEquipment(EQ_FUEL_SCOOP)) return;
  if (pilot.fuel >= 70) return;
  if (pilot.docked) return;

  float dx = sunPos.x, dy = sunPos.y, dz = sunPos.z;
  float dist = sqrtf(dx * dx + dy * dy + dz * dz);

  if (dist < 185.0f && dist > 100.0f) {
    pilot.fuel = clampi(pilot.fuel + 1, 0, 70);
    laserHeat = clampf(laserHeat + 2.0f, 0.0f, 100.0f);
    shipSpeed = clampf(shipSpeed - 0.001f, 0.08f, 0.25f);
    statusLine = "SCOOP ACTIVE";
    playToneSafe(880, 20);
  } else if (dist <= 100.0f) {
    shipShield -= 0.9f;
    shipEnergy -= 0.5f;
    laserHeat = clampf(laserHeat + 4.0f, 0.0f, 100.0f);
    statusLine = "SUN HEAT";
  }
}

// -----------------------------------------------------
// SAVE / LOAD
// -----------------------------------------------------
void defaultPilot() {
  memset(&pilot, 0, sizeof(pilot));
  pilot.currentSystem = 7;
  pilot.galaxyIndex = 0;
  pilot.credits = 1000;
  pilot.fuel = 70;
  pilot.docked = true;
  pilot.legalStatus = LEGAL_CLEAN;
  pilot.bounty = 0;
  pilot.equipmentFlags = EQ_NONE;
  pilot.missionStage = MISSION_NONE;
  pilot.missionProgress = 0;
  pilot.missiles = 2;
  pilot.energyMode = 0;
  pilot.damagedFlags = DMG_NONE;
  pilot.escapePods = 0;
  pilot.missileIncoming = false;
  pilot.passengerJob.active = false;
  pilot.passengerJob.targetSystem = 0;
  pilot.passengerJob.reward = 0;
  pilot.ecmCooldown = 0;
  pilot.escapePods = 0;
}


const char* getArchetypeName(uint8_t a) {
  switch (a) {
    case ARCHETYPE_TRADER: return "TRADER";
    case ARCHETYPE_HUNTER: return "HUNTER";
    case ARCHETYPE_COWARD: return "COWARD";
    case ARCHETYPE_PIRATE: return "PIRATE";
    default: return "BALANCED";
  }
}

bool hasVisitedSystem(uint8_t sys) {
  return learnState.systemDeathPenalty[sys] > 0.0f || sys == pilot.currentSystem;
}

void setArchetype(PilotArchetype a) {
  janus.archetype = (uint8_t)a;
  if (a == ARCHETYPE_TRADER) {
    janus.traderInstinct = 0.90f; janus.riskTolerance = 0.30f; janus.lawfulness = 0.80f; janus.curiosity = 0.40f;
  } else if (a == ARCHETYPE_HUNTER) {
    janus.traderInstinct = 0.20f; janus.riskTolerance = 0.80f; janus.lawfulness = 0.30f; janus.curiosity = 0.70f;
  } else if (a == ARCHETYPE_COWARD) {
    janus.traderInstinct = 0.55f; janus.riskTolerance = 0.10f; janus.lawfulness = 0.90f; janus.curiosity = 0.20f;
  } else if (a == ARCHETYPE_PIRATE) {
    janus.traderInstinct = 0.30f; janus.riskTolerance = 0.90f; janus.lawfulness = 0.10f; janus.curiosity = 0.60f;
  } else {
    janus.traderInstinct = 0.50f; janus.riskTolerance = 0.50f; janus.lawfulness = 0.50f; janus.curiosity = 0.50f;
  }
}

void mutatePersonalityAfterDeath(DeathCause cause) {
  if (cause == DEATH_POLICE) janus.lawfulness = clampf(janus.lawfulness + 0.08f, 0.0f, 1.0f);
  if (cause == DEATH_ENEMY) janus.riskTolerance = clampf(janus.riskTolerance - 0.06f, 0.0f, 1.0f);
  if (cause == DEATH_COLLISION || cause == DEATH_DOCK) janus.riskTolerance = clampf(janus.riskTolerance - 0.04f, 0.0f, 1.0f);
  if (cause == DEATH_SUN) janus.curiosity = clampf(janus.curiosity - 0.03f, 0.0f, 1.0f);

  janus.traderInstinct = clampf(janus.traderInstinct + frandRange(-0.02f, 0.02f), 0.0f, 1.0f);
  janus.riskTolerance = clampf(janus.riskTolerance + frandRange(-0.02f, 0.02f), 0.0f, 1.0f);
  janus.lawfulness = clampf(janus.lawfulness + frandRange(-0.02f, 0.02f), 0.0f, 1.0f);
  janus.curiosity = clampf(janus.curiosity + frandRange(-0.02f, 0.02f), 0.0f, 1.0f);
}

void rewardPersonalityFromSuccess() {
  if (pilot.totalProfit > 300) janus.traderInstinct = clampf(janus.traderInstinct + 0.02f, 0.0f, 1.0f);
  if (pilot.kills > 8) janus.riskTolerance = clampf(janus.riskTolerance + 0.02f, 0.0f, 1.0f);
  if (pilot.bounty == 0) janus.lawfulness = clampf(janus.lawfulness + 0.01f, 0.0f, 1.0f);
}
void defaultAgent() {
  memset(&janus, 0, sizeof(janus));
  float base[12] = {
    0.30f, -0.25f, 0.20f, 0.15f,
    0.20f,  0.10f, -0.20f, -0.20f,
    0.15f,  0.05f,  0.18f, -0.10f
  };
  for (int i = 0; i < 12; ++i) {
    janus.w[i] = base[i];
    janus.slimeTrace[i] = 1.0f;
  }
  janus.bias = 0.0f;
  janus.lr = 0.015f;
  janus.slimeThreshold = 0.22f;
  janus.decisions = 0;
  janus.mode = MODE_DOCKED;
  janus.aggression = 0.5f;
  janus.caution = 0.5f;
  setArchetype((PilotArchetype)random(0, 5));
}

void saveState() {
  File f = LittleFS.open(SAVE_PILOT_FILE, "w");
  if (f) {
    f.write((const uint8_t*)&pilot, sizeof(pilot));
    f.close();
  }
  File g = LittleFS.open(SAVE_AGENT_FILE, "w");
  if (g) {
    g.write((const uint8_t*)&janus, sizeof(janus));
    g.close();
  }
}

void loadState() {
  if (!LittleFS.exists(SAVE_PILOT_FILE)) {
    defaultPilot();
  } else {
    File f = LittleFS.open(SAVE_PILOT_FILE, "r");
    if (f && f.size() == sizeof(pilot)) {
      f.read((uint8_t*)&pilot, sizeof(pilot));
      f.close();
    } else {
      defaultPilot();
    }
  }

  if (!LittleFS.exists(SAVE_AGENT_FILE)) {
    defaultAgent();
  } else {
    File g = LittleFS.open(SAVE_AGENT_FILE, "r");
    if (g && g.size() == sizeof(janus)) {
      g.read((uint8_t*)&janus, sizeof(janus));
      g.close();
    } else {
      defaultAgent();
    }
  }
}

// -----------------------------------------------------
// GALAXY
// -----------------------------------------------------
uint16_t nextSeed(uint16_t &a, uint16_t &b, uint16_t &c) {
  uint16_t t = a + b + c;
  a = b; b = c; c = t;
  return t;
}

void generateGalaxy(uint8_t galaxyNumber) {
  uint16_t s0 = 0x5A4A + galaxyNumber * 0x1111;
  uint16_t s1 = 0x0248 + galaxyNumber * 0x0101;
  uint16_t s2 = 0xB753 + galaxyNumber * 0x0222;

  const char* sA[] = {"La","Ti","Re","Ve","Zo","Qu","Ce","Ma","Or","Er"};
  const char* sB[] = {"ve","ra","ti","an","or","us","es","on","is","ar"};

  for (int i = 0; i < MAX_SYSTEMS; ++i) {
    StarSystem &sys = galaxy[i];
    uint16_t n1 = nextSeed(s0, s1, s2);
    uint16_t n2 = nextSeed(s0, s1, s2);
    uint16_t n3 = nextSeed(s0, s1, s2);
    uint16_t n4 = nextSeed(s0, s1, s2);

    sys.x = (n1 >> 8) & 0xFF;
    sys.y = (n2 >> 8) & 0xFF;
    sys.economy = n3 & 7;
    sys.government = (n4 >> 3) & 7;
    sys.techLevel = 1 + ((sys.economy ^ sys.government) & 7) + (n2 & 3);
    if (sys.techLevel > 15) sys.techLevel = 15;
    sys.danger = (7 - sys.government + (sys.economy < 3 ? 2 : 0)) & 7;

    int a = (n1 >> 8) % 10;
    int b = (n2 >> 8) % 10;
    int c = (n3 >> 8) % 10;

    snprintf(sys.name, sizeof(sys.name), "%s%s%s",
             sA[a], sB[b],
             (c % 3 == 0) ? "on" : ((c % 3 == 1) ? "ar" : ""));
  }
}

void galacticJump() {
  if (!hasEquipment(EQ_GAL_HYPERDRIVE)) return;
  pilot.galaxyIndex = (pilot.galaxyIndex + 1) & 7;
  generateGalaxy(pilot.galaxyIndex);
  selectStationType();
  pilot.currentSystem = random(0, MAX_SYSTEMS);
  pilot.fuel = 70;
  pilot.docked = true;
  pilot.equipmentFlags &= ~EQ_GAL_HYPERDRIVE;
  alreadyJumpedThisDock = false;
  statusLine = "GAL JUMP";
  playJumpSound();
}

bool jumpToSystem(uint8_t dst) {
  if (dst >= MAX_SYSTEMS || dst == pilot.currentSystem) return false;
  int cost = fuelCostTo(dst);
  if (pilot.fuel < cost) return false;

  pilot.fuel -= cost;
  pilot.currentSystem = dst;
  pilot.docked = true;
  statusLine = String("JUMP ") + galaxy[dst].name;
  playJumpSound();

  if (pilot.missionStage == MISSION_THARGOID_DOCS && random(0, 100) < 45) {
    inWitchSpace = true;
    witchTimer = 120;
    uiMode = UI_WITCHSPACE;
    pilot.docked = false;
    statusLine = "WITCHSPACE";
  }
  return true;
}

// -----------------------------------------------------
// MISSIONS
// -----------------------------------------------------
void updateMissionsOnDock() {
  if (pilot.missionStage == MISSION_NONE && pilot.kills >= 12) {
    pilot.missionStage = MISSION_CONSTRICTOR;
    pilot.missionProgress = 0;
    statusLine = "MISSION READY";
  }
  if (pilot.missionStage == MISSION_CONSTRICTOR && pilot.kills >= 20) {
    pilot.missionStage = MISSION_THARGOID_DOCS;
    pilot.missionProgress = 0;
    statusLine = "NEW MISSION";
  }
}

// -----------------------------------------------------
// SENSOR
// -----------------------------------------------------
void updateSensorInfluence() {
  float gx = 0, gy = 0, gz = 0;
  M5.Imu.getGyroData(&gx, &gy, &gz);

  sensorBiasYaw = clampf(gz * 0.015f, -0.25f, 0.25f);
  sensorBiasAggro = clampf(fabsf(gx) * 0.02f, 0.0f, 0.30f);
  sensorBiasCaution = clampf(fabsf(gy) * 0.02f, 0.0f, 0.30f);

  janus.aggression = clampf(0.35f + sensorBiasAggro, 0.2f, 0.9f);
  janus.caution = clampf(0.35f + sensorBiasCaution, 0.2f, 0.9f);
}

void resetDebris() {
  for (int i = 0; i < 24; ++i) {
    debris[i].alive = false;
    debris[i].ttl = 0;
    debris[i].pos = {0,0,0};
    debris[i].vel = {0,0,0};
  }
}

void resetCanisters() {
  for (int i = 0; i < 10; ++i) {
    canisters[i].alive = false;
    canisters[i].ttl = 0;
    canisters[i].item = 0;
    canisters[i].pos = {0,0,0};
    canisters[i].vel = {0,0,0};
  }
}

void spawnDebrisBurst(const Vec3 &origin) {
  for (int i = 0; i < 24; ++i) {
    if (!debris[i].alive) {
      debris[i].alive = true;
      debris[i].ttl = 18 + random(0, 20);
      debris[i].pos = origin;
      debris[i].vel = {frandRange(-1.1f, 1.1f), frandRange(-0.9f, 0.9f), frandRange(-0.4f, 0.8f)};
      if (random(0, 100) < 65) continue;
      else return;
    }
  }
}

void spawnCargoCanister(const Vec3 &origin, uint8_t item) {
  for (int i = 0; i < 10; ++i) {
    if (!canisters[i].alive) {
      canisters[i].alive = true;
      canisters[i].ttl = 500;
      canisters[i].item = item;
      canisters[i].pos = origin;
      canisters[i].vel = {frandRange(-0.15f, 0.15f), frandRange(-0.10f, 0.10f), frandRange(-0.05f, 0.08f)};
      return;
    }
  }
}

void updateDebris() {
  for (int i = 0; i < 24; ++i) {
    if (!debris[i].alive) continue;
    debris[i].pos.x += debris[i].vel.x;
    debris[i].pos.y += debris[i].vel.y;
    debris[i].pos.z += debris[i].vel.z - shipSpeed * 1.2f;
    if (debris[i].ttl > 0) debris[i].ttl--;
    if (debris[i].ttl == 0 || debris[i].pos.z < 3 || debris[i].pos.z > 260) debris[i].alive = false;
  }
}

void updateCanisters() {
  for (int i = 0; i < 10; ++i) {
    if (!canisters[i].alive) continue;
    canisters[i].pos.x += canisters[i].vel.x;
    canisters[i].pos.y += canisters[i].vel.y;
    canisters[i].pos.z += canisters[i].vel.z - shipSpeed * 1.0f;
    if (canisters[i].ttl > 0) canisters[i].ttl--;
    float d = sqrtf(canisters[i].pos.x*canisters[i].pos.x + canisters[i].pos.y*canisters[i].pos.y + canisters[i].pos.z*canisters[i].pos.z);
    if (d < 12.0f && pilot.cargoCount < cargoLimit()) {
      pilot.cargoType[pilot.cargoCount++] = canisters[i].item;
      canisters[i].alive = false;
      statusLine = "SALVAGE";
      playToneSafe(700, 35);
    }
    if (canisters[i].ttl == 0 || canisters[i].pos.z < 3 || canisters[i].pos.z > 260) canisters[i].alive = false;
  }
}

void beginVisibleJump(uint8_t dst) {
  jumpTargetSystem = dst;
  jumpTimer = 60;
  jumpInProgress = true;
  uiMode = UI_MAP;
  statusLine = "SYSTEM SEL";
}

bool completeVisibleJump() {
  if (jumpTargetSystem >= MAX_SYSTEMS || jumpTargetSystem == pilot.currentSystem) {
    jumpInProgress = false;
    uiMode = UI_MARKET;
    statusLine = "JUMP FAIL";
    stationFlowActive = true;
    stationStep = SERVICE_MARKET;
    stationStepTimer = 1;
    return false;
  }

  int cost = fuelCostTo(jumpTargetSystem);
  if (pilot.fuel < cost) {
    jumpInProgress = false;
    uiMode = UI_MARKET;
    statusLine = "NO FUEL";
    stationFlowActive = true;
    stationStep = SERVICE_MARKET;
    stationStepTimer = 1;
    return false;
  }

  pilot.fuel -= cost;
  pilot.currentSystem = jumpTargetSystem;
  pilot.docked = true;
  jumpInProgress = false;
  uiMode = UI_MARKET;
  if (jumpTargetSystem < MAX_SYSTEMS) statusLine = String("ARRIVED ") + galaxy[jumpTargetSystem].name;
  else statusLine = "ARRIVED";
  playJumpSound();
  selectStationType();

  stationFlowActive = true;
  stationStep = SERVICE_MARKET;
  stationStepTimer = 1;

  if (pilot.missionStage == MISSION_THARGOID_DOCS && random(0, 100) < 35) {
    inWitchSpace = true;
    witchTimer = 100;
    uiMode = UI_WITCHSPACE;
    pilot.docked = false;
    stationFlowActive = false;
    statusLine = "WITCHSPACE";
  }
  return true;
}

void beginDockRun() {
  if (dockPhase != DOCK_NONE) return;
  dockPhase = DOCK_APPROACH;
  tunnelTimer = 0;
  if (stationPos.z < 90.0f) stationPos.z = 120.0f;
  statusLine = "DOCK APP";
}

// -----------------------------------------------------
// WORLD OBJECTS
// -----------------------------------------------------


bool scannerOperational() {
  return !isDamaged(DMG_SCANNER);
}

void maybeGeneratePassengerJob() {
  if (pilot.passengerJob.active) return;
  if (random(0, 100) >= 28) return;
  uint8_t target = random(0, MAX_SYSTEMS);
  if (target == pilot.currentSystem) target = (target + 1) % MAX_SYSTEMS;
  pilot.passengerJob.active = true;
  pilot.passengerJob.targetSystem = target;
  pilot.passengerJob.reward = 140 + fuelCostTo(target) * 18 + random(0, 80);
}

void processPassengerJobAtStation() {
  if (!pilot.passengerJob.active) {
    maybeGeneratePassengerJob();
    return;
  }
  if (pilot.currentSystem == pilot.passengerJob.targetSystem) {
    pilot.credits += pilot.passengerJob.reward;
    statusLine = "PASSENGER OK";
    pilot.passengerJob.active = false;
    pilot.passengerJob.targetSystem = 0;
    pilot.passengerJob.reward = 0;
  }
}

void updateECMCooldown() {
  if (pilot.ecmCooldown > 0) pilot.ecmCooldown--;
}

void attemptECM() {
  if (!hasEquipment(EQ_ECM)) return;
  if (pilot.ecmCooldown > 0) return;
  if (shipEnergy < 10.0f) return;
  shipEnergy -= 10.0f;
  pilot.ecmCooldown = 90;
  pilot.missileIncoming = false;
  statusLine = "ECM";
  playToneSafe(520, 50);
}

int nearestEnemyIndexInSight() {
  int idx = -1;
  float bestZ = 999999.0f;
  for (int i = 0; i < MAX_ENEMIES; ++i) {
    if (!enemies[i].alive) continue;
    if (enemies[i].pos.z > 0.0f && enemies[i].pos.z < bestZ) {
      bestZ = enemies[i].pos.z;
      idx = i;
    }
  }
  return idx;
}

void processPoliceScanInFlight() {
  if (pilot.docked) return;
  if (millis() - lastPoliceScanRoll < 5000) return;
  lastPoliceScanRoll = millis();

  if (galaxy[pilot.currentSystem].government <= 4) return;

  bool policePresent = false;
  for (int i = 0; i < MAX_ENEMIES; ++i) {
    if (enemies[i].alive && enemies[i].role == ROLE_POLICE) {
      policePresent = true;
      break;
    }
  }

  if (!policePresent && random(0, 100) >= 14) return;

  int illegalFound = 0;
  for (int i = 0; i < pilot.cargoCount; ++i) {
    if (isIllegalCommodity(pilot.cargoType[i], galaxy[pilot.currentSystem])) illegalFound++;
  }
  if (pilot.escapePods > 0 && galaxy[pilot.currentSystem].government > 4) illegalFound += pilot.escapePods;

  if (illegalFound > 0) {
    pilot.bounty += 20 + illegalFound * 10;
    pilot.legalStatus = (pilot.bounty >= 80) ? LEGAL_FUGITIVE : LEGAL_OFFENDER;
    statusLine = "POLICE SCAN";
    spawnPoliceWing();
  } else {
    statusLine = "SCAN CLEAR";
  }
}

void drawCargoScanInfo() {
  if (!scannerOperational()) return;
  int idx = nearestEnemyIndexInSight();
  if (idx < 0) return;
  if (enemies[idx].pos.z > 50.0f) return;

  const char* cargoHint = "EMPTY";
  if (enemies[idx].role == ROLE_TRADER) cargoHint = "TRADE";
  else if (enemies[idx].type == SHIP_TRANSPORT || enemies[idx].type == SHIP_PYTHON || enemies[idx].type == SHIP_ANACONDA) cargoHint = "HEAVY";
  else if (enemies[idx].role == ROLE_POLICE) cargoHint = "POLICE";
  else if (enemies[idx].role == ROLE_PIRATE) cargoHint = "PLUNDER";

  M5.Display.setTextColor(UI_AMBER_BRIGHT, TFT_BLACK);
  M5.Display.setCursor(2, 90);
  M5.Display.print("SCAN ");
  M5.Display.print(cargoHint);
}
ShipModel& getCurrentStationModel() {
  return (currentStationType == STATION_DODECA) ? dodecaStationModel : coriolisStationModel;
}

void selectStationType() {
  currentStationType = (galaxy[pilot.currentSystem].techLevel >= 8) ? STATION_CORIOLIS : STATION_DODECA;
}

void resetEscapePods() {
  for (int i = 0; i < 6; ++i) {
    escapePods[i].alive = false;
    escapePods[i].ttl = 0;
    escapePods[i].pos = {0,0,0};
    escapePods[i].vel = {0,0,0};
  }
}

void spawnEscapePod(const Vec3 &origin) {
  for (int i = 0; i < 6; ++i) {
    if (!escapePods[i].alive) {
      escapePods[i].alive = true;
      escapePods[i].ttl = 700;
      escapePods[i].pos = origin;
      escapePods[i].vel = {frandRange(-0.12f, 0.12f), frandRange(-0.08f, 0.08f), frandRange(-0.03f, 0.06f)};
      return;
    }
  }
}

void updateEscapePods() {
  for (int i = 0; i < 6; ++i) {
    if (!escapePods[i].alive) continue;
    escapePods[i].pos.x += escapePods[i].vel.x;
    escapePods[i].pos.y += escapePods[i].vel.y;
    escapePods[i].pos.z += escapePods[i].vel.z - shipSpeed * 0.9f;
    if (escapePods[i].ttl > 0) escapePods[i].ttl--;
    float d = sqrtf(escapePods[i].pos.x*escapePods[i].pos.x + escapePods[i].pos.y*escapePods[i].pos.y + escapePods[i].pos.z*escapePods[i].pos.z);
    if (d < 12.0f) {
      pilot.escapePods++;
      escapePods[i].alive = false;
      statusLine = "POD RECOVER";
      playToneSafe(720, 40);
    }
    if (escapePods[i].ttl == 0 || escapePods[i].pos.z < 3 || escapePods[i].pos.z > 260) {
      escapePods[i].alive = false;
    }
  }
}

bool isIllegalCommodity(uint8_t item, const StarSystem &sys) {
  // strict systems treat liquor and luxuries as contraband
  if (sys.government > 4) {
    if (item == 2 || item == 3) return true; // Liquor, Luxuries
  }
  return false;
}

void processContrabandCheck() {
  if (galaxy[pilot.currentSystem].government <= 4) return;
  if (random(0, 100) >= 20) return;

  int illegalFound = 0;
  for (int i = 0; i < pilot.cargoCount; ++i) {
    if (isIllegalCommodity(pilot.cargoType[i], galaxy[pilot.currentSystem])) illegalFound++;
  }
  if (pilot.escapePods > 0) illegalFound += pilot.escapePods;

  if (illegalFound > 0) {
    pilot.bounty += 50 + illegalFound * 20;
    pilot.legalStatus = (pilot.bounty >= 80) ? LEGAL_FUGITIVE : LEGAL_OFFENDER;
    statusLine = "CUSTOMS!";
    playToneSafe(240, 80);
  } else {
    statusLine = "CLEARANCE";
  }
}

void processRescuedPodsAtStation() {
  if (pilot.escapePods == 0) return;
  if (galaxy[pilot.currentSystem].government > 4) {
    // lawful surrender
    int reward = pilot.escapePods * (50 + random(0, 40));
    pilot.credits += reward;
    statusLine = "POD REWARD";
  } else {
    // black market sale in rough systems
    int reward = pilot.escapePods * (120 + random(0, 60));
    pilot.credits += reward;
    pilot.bounty += pilot.escapePods * 8;
    if (pilot.bounty >= 20 && pilot.legalStatus == LEGAL_CLEAN) pilot.legalStatus = LEGAL_OFFENDER;
    statusLine = "BLACK SALE";
  }
  pilot.escapePods = 0;
}

void playDockMelody() {
  if (!hasEquipment(EQ_DOCK_COMPUTER)) return;
  if (!soundEnabled) return;
  // short docking-computer melody nod, intentionally lightweight
  playToneSafe(523, 40);
  delay(8);
  playToneSafe(659, 40);
  delay(8);
  playToneSafe(784, 50);
}

void drawJumpTunnelScreen() {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(UI_AMBER_BRIGHT, TFT_BLACK);
  int cx = SCREEN_W / 2;
  int cy = SCREEN_H / 2;
  for (int i = 0; i < 26; ++i) {
    float a = (i / 26.0f) * 6.2831853f + (millis() * 0.0025f);
    int len = 10 + ((i * 7 + jumpTimer * 3) % 54);
    int x2 = cx + (int)(cosf(a) * len);
    int y2 = cy + (int)(sinf(a) * len);
    M5.Display.drawLine(cx, cy, x2, y2, (i % 2) ? UI_SOFT : UI_AMBER_BRIGHT);
  }
  M5.Display.setCursor(2, 2);
  M5.Display.print("HYPERSPACE");
  M5.Display.setCursor(2, 118);
  if (jumpTargetSystem < MAX_SYSTEMS) M5.Display.printf("%s", galaxy[jumpTargetSystem].name);
  else M5.Display.print("HYPER");
}

void resetStars() {
  for (int i = 0; i < STAR_COUNT; ++i) {
    stars[i].x = frandRange(-140, 140);
    stars[i].y = frandRange(-140, 140);
    stars[i].z = frandRange(-200, 200);
    if (fabsf(stars[i].z) < 12.0f) stars[i].z = (stars[i].z < 0 ? -24.0f : 24.0f);
  }
}

void resetEnemies() {
  for (int i = 0; i < MAX_ENEMIES; ++i) {
    enemies[i].alive = false;
    enemies[i].hp = 0.0f;
    enemies[i].type = SHIP_COBRA;
    enemies[i].role = ROLE_PIRATE;
    enemies[i].pos = {0,0,0};
    enemies[i].vel = {0,0,0};
    enemies[i].hostile = true;
  }
}

void initTraffic() {
  for (int i = 0; i < MAX_TRAFFIC; ++i) {
    traffic[i].alive = true;
    traffic[i].hostile = false;
    traffic[i].role = ROLE_TRADER;

    int rr = random(0, 100);
    if (rr < 25) traffic[i].type = SHIP_ADDER;
    else if (rr < 45) traffic[i].type = SHIP_TRANSPORT;
    else if (rr < 70) traffic[i].type = SHIP_COBRA;
    else if (rr < 85) traffic[i].type = SHIP_PYTHON;
    else traffic[i].type = SHIP_ANACONDA;

    traffic[i].hp = 1.0f;
    traffic[i].pos = {frandRange(-70, 70), frandRange(-35, 35), frandRange(80, 200)};
    traffic[i].vel = {frandRange(-0.05f, 0.05f), frandRange(-0.03f, 0.03f), frandRange(-0.10f, -0.03f)};
  }
}

void updateTraffic() {
  for (int i = 0; i < MAX_TRAFFIC; ++i) {
    if (!traffic[i].alive) continue;
    traffic[i].pos.x += traffic[i].vel.x;
    traffic[i].pos.y += traffic[i].vel.y;
    traffic[i].pos.z += traffic[i].vel.z - shipSpeed * 1.5f;

    if (traffic[i].pos.z < 20 || traffic[i].pos.z > 220) {
      traffic[i].alive = true;
      traffic[i].hostile = false;
      traffic[i].role = ROLE_TRADER;
      int rr = random(0, 100);
      if (rr < 25) traffic[i].type = SHIP_ADDER;
      else if (rr < 45) traffic[i].type = SHIP_TRANSPORT;
      else if (rr < 70) traffic[i].type = SHIP_COBRA;
      else if (rr < 85) traffic[i].type = SHIP_PYTHON;
      else traffic[i].type = SHIP_ANACONDA;
      traffic[i].hp = 1.0f;
      traffic[i].pos = {frandRange(-70, 70), frandRange(-35, 35), 200.0f};
      traffic[i].vel = {frandRange(-0.05f, 0.05f), frandRange(-0.03f, 0.03f), frandRange(-0.10f, -0.03f)};
    }
  }
}

void spawnPoliceWing() {
  for (int i = 0; i < MAX_ENEMIES; ++i) {
    if (!enemies[i].alive) {
      enemies[i].alive = true;
      enemies[i].hostile = true;
      enemies[i].role = ROLE_POLICE;
      enemies[i].type = SHIP_VIPER;
      enemies[i].hp = shipStats[SHIP_VIPER].hp;
      enemies[i].pos = {frandRange(-30, 30), frandRange(-14, 14), frandRange(105, 135)};
      enemies[i].vel = {frandRange(-0.09f, 0.09f), frandRange(-0.05f, 0.05f), frandRange(-0.18f, -0.06f)};
    }
  }
  statusLine = "POLICE";
  playAlertSound();
}

void spawnSingleEnemy() {
  for (int i = 0; i < MAX_ENEMIES; ++i) {
    if (!enemies[i].alive) {
      enemies[i].alive = true;
      enemies[i].hostile = true;

      if (pilot.legalStatus != LEGAL_CLEAN && random(0, 100) < 35) {
        enemies[i].role = ROLE_POLICE;
        enemies[i].type = SHIP_VIPER;
      } else {
        int rr = random(0, 100);
        if (rr < 35) {
          enemies[i].role = ROLE_PIRATE;
          enemies[i].type = SHIP_SIDEWINDER;
        } else if (rr < 55) {
          enemies[i].role = ROLE_HUNTER;
          enemies[i].type = SHIP_MAMBA;
        } else if (rr < 75) {
          enemies[i].role = ROLE_PIRATE;
          enemies[i].type = SHIP_COBRA;
        } else if (rr < 90) {
          enemies[i].role = ROLE_HUNTER;
          enemies[i].type = SHIP_ASP;
        } else {
          enemies[i].role = ROLE_PIRATE;
          enemies[i].type = SHIP_PYTHON;
        }
      }

      if (pilot.missionStage == MISSION_CONSTRICTOR && pilot.kills > 15 && random(0, 100) < 15) {
        enemies[i].role = ROLE_BOSS;
        enemies[i].type = SHIP_CONSTRICTOR;
      }

      enemies[i].hp = shipStats[enemies[i].type].hp;
      enemies[i].pos = {frandRange(-55, 55), frandRange(-30, 30), frandRange(100, 170)};
      enemies[i].vel = {frandRange(-0.12f, 0.12f), frandRange(-0.08f, 0.08f), frandRange(-0.25f, -0.05f)};
      break;
    }
  }
}

void spawnThargoidEncounter() {
  resetEnemies();
  for (int i = 0; i < 2 && i < MAX_ENEMIES; ++i) {
    enemies[i].alive = true;
    enemies[i].hostile = true;
    enemies[i].role = ROLE_THARGOID;
    enemies[i].type = SHIP_THARGOID;
    enemies[i].hp = shipStats[SHIP_THARGOID].hp;
    enemies[i].pos = {frandRange(-28, 28), frandRange(-12, 12), frandRange(105, 135)};
    enemies[i].vel = {frandRange(-0.05f, 0.05f), frandRange(-0.05f, 0.05f), frandRange(-0.18f, -0.06f)};
  }
  statusLine = "THARGOID";
  playAlertSound();
}

void maybeSpawnThargons(const Vec3 &origin) {
  for (int i = 0; i < MAX_ENEMIES; ++i) {
    if (!enemies[i].alive) {
      enemies[i].alive = true;
      enemies[i].hostile = true;
      enemies[i].role = ROLE_THARGON;
      enemies[i].type = SHIP_THARGON;
      enemies[i].hp = shipStats[SHIP_THARGON].hp;
      enemies[i].pos = {origin.x + frandRange(-12,12), origin.y + frandRange(-12,12), origin.z + frandRange(-6,6)};
      enemies[i].vel = {frandRange(-0.12f, 0.12f), frandRange(-0.12f, 0.12f), frandRange(-0.18f, -0.05f)};
      return;
    }
  }
}

void launchScene() {
  pilot.docked = false;
  stationFlowActive = false;
  stationStep = SERVICE_IDLE;
  uiMode = UI_FLIGHT;
  dockPhase = DOCK_NONE;
  currentGoal = GOAL_NONE;
  alreadyJumpedThisDock = false;
  selectStationType();

  shipSpeed = 0.10f;
  shipYaw = 0.0f;
  shipPitch = 0.0f;
  shipRoll = 0.0f;
  laserHeat = 0.0f;

  resetStars();
  resetEnemies();
  resetDebris();
  resetCanisters();
  resetEscapePods();
  initTraffic();
  pilot.passengerJob.active = false;
  pilot.passengerJob.targetSystem = 0;
  pilot.passengerJob.reward = 0;

  stationPos = {0.0f, 0.0f, 320.0f};
  planetPos = {0.0f, 0.0f, 320.0f};
  sunPos = {80.0f, -40.0f, 420.0f};

  launchTimer = 50;
  patrolTimer = 0;
  returnTimer = 0;
  dockTimer = 0;

  janus.mode = MODE_LAUNCH;
  statusLine = "LAUNCH";
}

// -----------------------------------------------------
// DOCK FLOW
// -----------------------------------------------------
void beginStationFlow() {
  stationFlowActive = true;
  stationStep = SERVICE_MARKET;
  stationStepTimer = 40;
  uiMode = UI_MARKET;
  statusLine = "MARKET";
}

void updateStationFlow() {
  if (!stationFlowActive || !pilot.docked) return;

  if (stationStepTimer > 0) {
    stationStepTimer--;
    return;
  }

  switch (stationStep) {
    case SERVICE_MARKET:
      sellAllCargoHere();
      processContrabandCheck();
      processRescuedPodsAtStation();
      processPassengerJobAtStation();
      refuelAtStation();
      uiMode = UI_MARKET;
      statusLine = "MARKET";
      stationStep = SERVICE_EQUIP;
      stationStepTimer = 40;
      break;

    case SERVICE_EQUIP:
      autoBuyEquipment();
      janusRestockMissiles();
      if (pilot.damagedFlags != DMG_NONE && pilot.credits >= 100) {
        pilot.credits -= 100;
        pilot.damagedFlags = DMG_NONE;
        statusLine = "REPAIRED";
      }
      uiMode = UI_EQUIP;
      statusLine = "EQUIP";
      stationStep = SERVICE_STATUS;
      stationStepTimer = 40;
      break;

    case SERVICE_STATUS:
      updateMissionsOnDock();
      decayLegalStatusInDock();
      uiMode = UI_STATUS;
      statusLine = "STATUS";
      stationStep = SERVICE_LAUNCH;
      stationStepTimer = 30;
      break;

    case SERVICE_LAUNCH:
      stationFlowActive = false;
      stationStep = SERVICE_IDLE;
      uiMode = UI_FLIGHT;
      dockTimer = 0;   // let janusDockedDecision run next, do NOT restart station flow
      statusLine = "LAUNCH PREP";
      break;

    default:
      stationFlowActive = false;
      stationStep = SERVICE_IDLE;
      uiMode = UI_FLIGHT;
      break;
  }
}

// -----------------------------------------------------
// PROJECTION / DRAW
// -----------------------------------------------------

bool enemyScreenVisible(const SpaceShip &e, int &sx, int &sy, float &d) {
  return projectPoint(e.pos, sx, sy, d);
}

void drawEnemyEdgeMarker(const SpaceShip &e, uint16_t color) {
  int sx, sy; float d;
  if (!projectPoint(e.pos, sx, sy, d)) return;
  int mx = sx;
  int my = sy;
  if (mx < 2) mx = 2;
  if (mx > SCREEN_W - 3) mx = SCREEN_W - 3;
  if (my < 24) my = 24;
  if (my > 95) my = 95;
  M5.Display.drawRect(mx - 1, my - 1, 3, 3, color);
}
bool projectPoint(const Vec3 &p, int &sx, int &sy, float &depth) {
  float cosy = cosf(shipYaw), siny = sinf(shipYaw);
  float cosp = cosf(shipPitch), sinp = sinf(shipPitch);
  float cosr = cosf(shipRoll), sinr = sinf(shipRoll);

  float x1 = p.x * cosy - p.z * siny;
  float z1 = p.x * siny + p.z * cosy;
  float y1 = p.y;

  float y2 = y1 * cosp - z1 * sinp;
  float z2 = y1 * sinp + z1 * cosp;
  float x2 = x1;

  float x3 = x2 * cosr - y2 * sinr;
  float y3 = x2 * sinr + y2 * cosr;
  float z3 = z2;

  depth = z3;
  if (z3 < 5.0f) return false;

  float scale = 90.0f / z3;
  sx = (int)(SCREEN_W * 0.5f + x3 * scale);
  sy = (int)(SCREEN_H * 0.42f + y3 * scale);

  return (sx >= -20 && sx < SCREEN_W + 20 && sy >= -20 && sy < SCREEN_H + 20);
}

void drawWireModel(const ShipModel &model, const Vec3 &pos, uint16_t color) {
  int px[20], py[20];
  float pz[20];
  bool ok[20];

  for (int i = 0; i < model.vertCount; ++i) {
    Vec3 world = {model.verts[i].x + pos.x, model.verts[i].y + pos.y, model.verts[i].z + pos.z};
    ok[i] = projectPoint(world, px[i], py[i], pz[i]);
  }

  for (int i = 0; i < model.edgeCount; ++i) {
    int a = model.edges[i].a, b = model.edges[i].b;
    if (ok[a] && ok[b]) {
      float zAvg = (pz[a] + pz[b]) * 0.5f;
      float shade = clampf(1.2f - zAvg / 260.0f, 0.2f, 1.0f);
      M5.Display.drawLine(px[a], py[a], px[b], py[b], colorDim(color, shade));
    }
  }
}

void drawCrosshair() {
  int cx = SCREEN_W / 2;
  int cy = SCREEN_H / 2 - 2;
  M5.Display.drawLine(cx - 5, cy, cx - 1, cy, TFT_WHITE);
  M5.Display.drawLine(cx + 1, cy, cx + 5, cy, TFT_WHITE);
  M5.Display.drawLine(cx, cy - 5, cx, cy - 1, TFT_WHITE);
  M5.Display.drawLine(cx, cy + 1, cx, cy + 5, TFT_WHITE);
}

void drawStars() {
  for (int i = 0; i < STAR_COUNT; ++i) {
    int sx, sy; float d;
    if (projectPoint(stars[i], sx, sy, d)) {
      M5.Display.drawPixel(sx, sy, colorDim(TFT_WHITE, clampf(1.2f - d / 240.0f, 0.2f, 1.0f)));
    }
  }
}

void drawPlanetAndSun() {
  int sx, sy; float d;
  if (projectPoint(planetPos, sx, sy, d)) {
    int r = clampi((int)(60.0f / d), 2, 14);
    M5.Display.fillCircle(sx, sy, r, TFT_BLUE);
    M5.Display.drawCircle(sx, sy, r, TFT_WHITE);
  }
  if (projectPoint(sunPos, sx, sy, d)) {
    int r = clampi((int)(80.0f / d), 2, 12);
    M5.Display.fillCircle(sx, sy, r, TFT_YELLOW);
  }
}

void drawClassicEliteScanner() {
  int cx = 64;
  int cy = 116;
  int rx = 34;
  int ry = 9;
  bool scannerOk = scannerOperational();

  M5.Display.drawEllipse(cx, cy + 2, rx + 3, ry + 2, UI_DIM);
  M5.Display.drawEllipse(cx, cy, rx, ry, UI_REDGRID);
  M5.Display.drawFastHLine(cx - rx, cy, rx * 2, UI_REDGRID);
  M5.Display.drawLine(cx, cy - ry, cx, cy + ry, UI_REDGRID);
  M5.Display.drawEllipse(cx, cy, rx / 2, ry / 2, UI_REDGRID);
  M5.Display.drawLine(cx - rx / 2, cy - ry + 1, cx - rx / 2, cy + ry - 1, UI_REDGRID);
  M5.Display.drawLine(cx + rx / 2, cy - ry + 1, cx + rx / 2, cy + ry - 1, UI_REDGRID);

  for (int i = 0; i < MAX_ENEMIES; ++i) {
    if (!enemies[i].alive) continue;
    int px = cx + (int)(enemies[i].pos.x * 0.22f);
    int py = cy + (int)((enemies[i].pos.z - 110.0f) * -0.020f);

    if (scannerOk && px > cx - rx + 1 && px < cx + rx - 1 && py > cy - ry + 1 && py < cy + ry - 1) {
      uint16_t c = TFT_GREEN;
      if (enemies[i].role == ROLE_POLICE) c = TFT_YELLOW;
      else if (enemies[i].role == ROLE_THARGOID || enemies[i].role == ROLE_THARGON) c = TFT_MAGENTA;
      else if (enemies[i].role == ROLE_BOSS) c = TFT_CYAN;
      else c = TFT_RED;
      M5.Display.drawPixel(px, py, c);
    }
  }

  for (int i = 0; i < MAX_TRAFFIC; ++i) {
    if (!traffic[i].alive) continue;
    int px = cx + (int)(traffic[i].pos.x * 0.14f);
    int py = cy + (int)((traffic[i].pos.z - 110.0f) * -0.014f);

    if (scannerOk && px > cx - rx + 1 && px < cx + rx - 1 && py > cy - ry + 1 && py < cy + ry - 1) {
      M5.Display.drawPixel(px, py, TFT_GREEN);
    }
  }

  if (scannerOk) M5.Display.drawPixel(cx, cy, TFT_GREEN);
  else M5.Display.drawFastHLine(cx - 5, cy, 10, TFT_RED);
}

void drawClassicEliteBottomHud() {
  int y = 104;

  M5.Display.drawRect(0, y, 20, 24, UI_DIM);
  M5.Display.setTextColor(UI_AMBER_BRIGHT, TFT_BLACK);

  M5.Display.setCursor(2, y + 2);   M5.Display.print("FS");
  M5.Display.setCursor(2, y + 8);   M5.Display.print("AS");
  M5.Display.setCursor(2, y + 14);  M5.Display.print("FU");
  M5.Display.setCursor(2, y + 20);  M5.Display.print("LT");

  M5.Display.drawRect(10, y + 2, 8, 3, UI_AMBER_DARK);
  M5.Display.fillRect(11, y + 3, (int)(6 * shipShield / 100.0f), 1, UI_AMBER_BRIGHT);

  M5.Display.drawRect(10, y + 8, 8, 3, UI_AMBER_DARK);
  M5.Display.fillRect(11, y + 9, (int)(6 * shipEnergy / 100.0f), 1, UI_AMBER_BRIGHT);

  M5.Display.drawRect(10, y + 14, 8, 3, UI_AMBER_DARK);
  M5.Display.fillRect(11, y + 15, (int)(6 * pilot.fuel / 70.0f), 1, UI_AMBER_BRIGHT);

  M5.Display.drawRect(10, y + 20, 8, 3, UI_AMBER_DARK);
  M5.Display.fillRect(11, y + 21, (int)(6 * (100.0f - laserHeat) / 100.0f), 1, UI_AMBER_BRIGHT);

  drawClassicEliteScanner();

  int rx = 108;
  M5.Display.drawRect(rx, y, 20, 24, UI_DIM);

  M5.Display.setCursor(rx + 2, y + 2);   M5.Display.print("CG");
  M5.Display.setCursor(rx + 2, y + 8);   M5.Display.print("BO");
  M5.Display.setCursor(rx + 2, y + 14);  M5.Display.print("RL");
  M5.Display.setCursor(rx + 2, y + 20);  M5.Display.print("EQ");

  M5.Display.setCursor(rx + 12, y + 2);  M5.Display.print(pilot.cargoCount);
  M5.Display.setCursor(rx + 12, y + 8);  M5.Display.print((pilot.bounty > 99) ? 99 : pilot.bounty);
  M5.Display.setCursor(rx + 12, y + 14); M5.Display.print(getLegalName(pilot.legalStatus));
  M5.Display.setCursor(rx + 12, y + 20); M5.Display.print(hasEquipment(EQ_MILITARY_LASER) ? "L" : "-");
}




void drawEliteHUD() {
  // More compact amber top HUD, visually aligned with the lower amber panel.
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(UI_AMBER_BRIGHT, TFT_BLACK);

  String left1 = String("G") + String(pilot.galaxyIndex + 1) + " " + String(galaxy[pilot.currentSystem].name);
  String modeName = "DCK";
  if (janus.mode == MODE_LAUNCH) modeName = "LCH";
  else if (janus.mode == MODE_PATROL) modeName = "CRS";
  else if (janus.mode == MODE_COMBAT) modeName = "CMB";
  else if (janus.mode == MODE_RETURN) modeName = "RTN";

  String left2 = String(getLegalName(pilot.legalStatus)) + " " + modeName;
  String left3 = String(getRankName(pilot.kills));
  String right1 = "CR";
  String right2 = String(pilot.credits);
  String right3 = "FU";
  String right4 = String(pilot.fuel);

  // tighter spacing: 4 compact rows instead of the older larger spread
  M5.Display.setCursor(1, 1);   M5.Display.print(left1);
  M5.Display.setCursor(1, 8);   M5.Display.print(left2);
  M5.Display.setCursor(1, 15);  M5.Display.print(left3);
  M5.Display.setCursor(1, 22);  M5.Display.print("K:"); M5.Display.print(pilot.kills);

  auto drawRight = [](int y, const String& s, uint16_t color) {
    M5.Display.setTextColor(color, TFT_BLACK);
    int16_t w = M5.Display.textWidth(s.c_str());
    int x = SCREEN_W - 1 - w;
    if (x < 0) x = 0;
    M5.Display.setCursor(x, y);
    M5.Display.print(s);
  };

  drawRight(1,  right1, UI_AMBER_BRIGHT);
  drawRight(8,  right2, UI_AMBER_BRIGHT);
  drawRight(15, right3, UI_AMBER_BRIGHT);
  drawRight(22, right4, UI_AMBER_BRIGHT);

  drawClassicEliteBottomHud();
  drawCargoScanInfo();

  M5.Display.setTextColor(UI_AMBER_BRIGHT, TFT_BLACK);
  M5.Display.setCursor(2, 98);
  M5.Display.printf("%s", statusLine.c_str());
}




void drawGalaxyMapScreen() {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(UI_AMBER_BRIGHT, TFT_BLACK);
  M5.Display.setTextSize(1);

  M5.Display.setCursor(2, 2);
  M5.Display.printf("GALACTIC CHART %d", pilot.galaxyIndex + 1);

  for (int i = 0; i < MAX_SYSTEMS; ++i) {
    int sx = 6 + (galaxy[i].x * 116) / 255;
    int sy = 16 + (galaxy[i].y * 70) / 255;
    uint16_t col = (i == pilot.currentSystem) ? TFT_WHITE : UI_SOFT;
    M5.Display.drawPixel(sx, sy, col);
    if (i == pilot.currentSystem) M5.Display.drawCircle(sx, sy, 3, UI_SOFT);
    if (jumpInProgress && i == jumpTargetSystem) M5.Display.drawCircle(sx, sy, 5, TFT_GREEN);
  }

  M5.Display.drawRect(0, 88, 128, 40, UI_DIM);
  if (jumpInProgress) {
    M5.Display.setCursor(2, 92);  M5.Display.printf("CUR %s", galaxy[pilot.currentSystem].name);
    M5.Display.setCursor(2, 100); M5.Display.printf("TGT %s", galaxy[jumpTargetSystem].name);
    M5.Display.setCursor(2, 108); M5.Display.printf("LAW %s", getLegalName(pilot.legalStatus));
    M5.Display.setCursor(2, 116); M5.Display.printf("ETA %d", jumpTimer);
  } else {
    M5.Display.setCursor(2, 92);  M5.Display.printf("SYS  %s", galaxy[pilot.currentSystem].name);
    M5.Display.setCursor(2, 100); M5.Display.printf("LAW  %s", getLegalName(pilot.legalStatus));
    M5.Display.setCursor(2, 108); M5.Display.printf("RANK %s", getRankName(pilot.kills));
    M5.Display.setCursor(2, 116); M5.Display.printf("SAFE %.1f", learnState.systemDeathPenalty[pilot.currentSystem]);
  }
}

void drawMissionScreen() {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(UI_AMBER_BRIGHT, TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(2, 2);
  M5.Display.print("MISSION");

  if (pilot.missionStage == MISSION_CONSTRICTOR) {
    M5.Display.setCursor(2, 16); M5.Display.print("HUNT CONSTRICTOR");
    M5.Display.setCursor(2, 28); M5.Display.print("FIND & DESTROY");
    M5.Display.setCursor(2, 40); M5.Display.printf("STATE:%d", pilot.missionProgress);
  } else if (pilot.missionStage == MISSION_THARGOID_DOCS) {
    M5.Display.setCursor(2, 16); M5.Display.print("THARGOID DOCS");
    M5.Display.setCursor(2, 28); M5.Display.print("EXPECT AMBUSH");
  } else {
    M5.Display.setCursor(2, 16); M5.Display.print("NO MISSION");
  }
}

void drawWitchSpaceScreen() {
  M5.Display.fillScreen(TFT_BLACK);
  for (int i = 0; i < STAR_COUNT; ++i) {
    int x = random(0, SCREEN_W);
    int y = random(0, SCREEN_H);
    int len = random(3, 12);
    M5.Display.drawLine(x, y, x, y + len, TFT_MAGENTA);
  }
  M5.Display.setTextColor(UI_AMBER_BRIGHT, TFT_BLACK);
  M5.Display.setCursor(20, 56);
  M5.Display.print("WITCH-SPACE");
}

void drawStatusScreen() {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(UI_AMBER_BRIGHT, TFT_BLACK);
  M5.Display.setTextSize(1);

  M5.Display.setCursor(2, 2);   M5.Display.print("STATUS");
  M5.Display.setCursor(2, 14);  M5.Display.printf("SYS  %s", galaxy[pilot.currentSystem].name);
  M5.Display.setCursor(2, 24);  M5.Display.printf("GAL  %d", pilot.galaxyIndex + 1);
  M5.Display.setCursor(2, 34);  M5.Display.printf("RANK %s", getRankName(pilot.kills));
  M5.Display.setCursor(2, 44);  M5.Display.printf("LAW  %s", getLegalName(pilot.legalStatus));
  M5.Display.setCursor(2, 54);  M5.Display.printf("BNTY %d", pilot.bounty);
  M5.Display.setCursor(2, 64);  M5.Display.printf("KILL %d", pilot.kills);
  M5.Display.setCursor(2, 74);  M5.Display.printf("CR   %d", pilot.credits);
  M5.Display.setCursor(2, 84);  M5.Display.printf("PRFT %d", pilot.totalProfit);
  M5.Display.setCursor(2, 94);  M5.Display.printf("CG   %d/%d", pilot.cargoCount, cargoLimit());
  M5.Display.setCursor(2, 104); M5.Display.printf("MIS  %d", pilot.missiles);
  M5.Display.setCursor(2, 114); M5.Display.printf("DMG  %02X", pilot.damagedFlags);
}

void drawMarketScreen() {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(UI_AMBER_BRIGHT, TFT_BLACK);
  M5.Display.setTextSize(1);

  M5.Display.setCursor(2, 2); M5.Display.print("MARKET");
  for (int i = 0; i < MAX_MARKET_ITEMS; ++i) {
    M5.Display.setCursor(2, 14 + i * 10);
    M5.Display.printf("%-8s %3d", marketItems[i].name, getCommodityPrice(i, galaxy[pilot.currentSystem]));
  }
  M5.Display.setCursor(2, 118);
  M5.Display.printf("%d cr", pilot.credits);
}

void drawEquipScreen() {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(UI_AMBER_BRIGHT, TFT_BLACK);
  M5.Display.setTextSize(1);

  M5.Display.setCursor(2, 2);  M5.Display.print("EQUIPMENT");
  M5.Display.setCursor(2, 16); M5.Display.printf("SCOOP  %s", hasEquipment(EQ_FUEL_SCOOP) ? "FIT" : "NONE");
  M5.Display.setCursor(2, 28); M5.Display.printf("GALDRV %s", hasEquipment(EQ_GAL_HYPERDRIVE) ? "FIT" : "NONE");
  M5.Display.setCursor(2, 40); M5.Display.printf("CARGO  %s", hasEquipment(EQ_CARGO_EXPANSION) ? "FIT" : "STD");
  M5.Display.setCursor(2, 52); M5.Display.printf("LASER  %s", hasEquipment(EQ_MILITARY_LASER) ? "MIL" : "PULSE");
  M5.Display.setCursor(2, 64); M5.Display.printf("DOCK   %s", hasEquipment(EQ_DOCK_COMPUTER) ? "FIT" : "NONE");
  M5.Display.setCursor(2, 118); M5.Display.printf("%d cr", pilot.credits);
}

void drawDockedScreen() {
  M5.Display.fillScreen(TFT_BLACK);
  drawStars();
  stationSpin += 0.015f;
  float ox = sinf(stationSpin) * 4.0f;
  float oy = cosf(stationSpin * 0.7f) * 3.0f;
  drawWireModel(getCurrentStationModel(), {ox, oy, 140}, TFT_WHITE);
  drawCrosshair();
  drawEliteHUD();
}

void drawFlightScreen() {
  M5.Display.fillScreen(TFT_BLACK);
  drawStars();
  drawPlanetAndSun();

  if (stationPos.z < 360.0f) {
    Vec3 liveStation = stationPos;
    liveStation.x = sinf(stationSpin) * 6.0f;
    liveStation.y = cosf(stationSpin * 0.7f) * 5.0f;
    drawWireModel(getCurrentStationModel(), liveStation, TFT_WHITE);

    if (dockPhase == DOCK_TUNNEL) {
      int cx = SCREEN_W / 2;
      int cy = SCREEN_H / 2;
      int s = 12 + tunnelTimer;
      M5.Display.drawRect(cx - s, cy - s, s * 2, s * 2, UI_SOFT);
      M5.Display.drawRect(cx - s / 2, cy - s / 2, s, s, UI_AMBER_BRIGHT);
    }
  }

  for (int i = 0; i < 24; ++i) {
    if (!debris[i].alive) continue;
    int sx, sy; float d;
    if (projectPoint(debris[i].pos, sx, sy, d)) {
      M5.Display.drawLine(sx - 2, sy, sx + 2, sy, TFT_YELLOW);
      M5.Display.drawLine(sx, sy - 2, sx, sy + 2, TFT_RED);
    }
  }

  for (int i = 0; i < 10; ++i) {
    if (!canisters[i].alive) continue;
    int sx, sy; float d;
    if (projectPoint(canisters[i].pos, sx, sy, d)) {
      M5.Display.drawRect(sx - 2, sy - 2, 5, 5, TFT_GREEN);
    }
  }

  for (int i = 0; i < 6; ++i) {
    if (!escapePods[i].alive) continue;
    int sx, sy; float d;
    if (projectPoint(escapePods[i].pos, sx, sy, d)) {
      M5.Display.drawCircle(sx, sy, 2, TFT_CYAN);
    }
  }

  for (int i = 0; i < MAX_TRAFFIC; ++i) {
    if (traffic[i].alive) drawWireModel(shipModels[traffic[i].type], traffic[i].pos, UI_DIM);
  }

  for (int i = 0; i < MAX_ENEMIES; ++i) {
    if (!enemies[i].alive) continue;
    uint16_t col = TFT_RED;
    if (enemies[i].role == ROLE_POLICE) col = TFT_YELLOW;
    else if (enemies[i].role == ROLE_THARGOID || enemies[i].role == ROLE_THARGON) col = TFT_MAGENTA;
    else if (enemies[i].role == ROLE_BOSS) col = TFT_CYAN;

    int esx, esy; float ed;
    if (enemyScreenVisible(enemies[i], esx, esy, ed)) {
      drawWireModel(shipModels[enemies[i].type], enemies[i].pos, col);
    } else if (enemies[i].pos.z > 0.0f) {
      drawEnemyEdgeMarker(enemies[i], col);
    }
  }

  if (laserFiring) {
    int cx = SCREEN_W / 2;
    int cy = SCREEN_H / 2 - 2;
    M5.Display.drawLine(cx - 10, SCREEN_H - 18, cx - 1, cy, TFT_RED);
    M5.Display.drawLine(cx + 10, SCREEN_H - 18, cx + 1, cy, TFT_RED);
  }

  drawCrosshair();
  drawEliteHUD();
}


void drawLocalMapScreen() {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(UI_AMBER_BRIGHT, TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(2, 2); M5.Display.print("LOCAL MAP");

  int cx = 64;
  int cy = 64;
  M5.Display.drawCircle(cx, cy, 2, TFT_WHITE); // ship

  int px = cx + (int)clampf(planetPos.x * 0.15f, -40.0f, 40.0f);
  int py = cy + (int)clampf(planetPos.z * -0.08f, -40.0f, 40.0f);
  int sx = cx + (int)clampf(sunPos.x * 0.10f, -50.0f, 50.0f);
  int sy = cy + (int)clampf(sunPos.z * -0.06f, -50.0f, 50.0f);
  int stx = cx + (int)clampf(stationPos.x * 0.15f, -50.0f, 50.0f);
  int sty = cy + (int)clampf(stationPos.z * -0.08f, -50.0f, 50.0f);

  M5.Display.fillCircle(px, py, 4, TFT_BLUE);
  M5.Display.fillCircle(sx, sy, 3, TFT_YELLOW);
  M5.Display.drawRect(stx - 2, sty - 2, 5, 5, TFT_WHITE);

  M5.Display.setCursor(2, 110); M5.Display.printf("M:%d ECM:%s", pilot.missiles, hasEquipment(EQ_ECM) ? "Y" : "N");
  M5.Display.setCursor(2, 118); M5.Display.printf("DMG:%02X EM:%d", pilot.damagedFlags, pilot.energyMode);
}

void renderTick() {
  if (uiMode == UI_JUMP || jumpInProgress) { drawJumpTunnelScreen(); return; }
  if (uiMode == UI_WITCHSPACE) { drawWitchSpaceScreen(); return; }
  if (uiMode == UI_MAP)        { drawGalaxyMapScreen(); return; }
  if (uiMode == UI_MISSION)    { drawMissionScreen(); return; }
  if (uiMode == UI_STATUS)     { drawStatusScreen(); return; }
  if (uiMode == UI_MARKET)     { drawMarketScreen(); return; }
  if (uiMode == UI_EQUIP)      { drawEquipScreen(); return; }
  if (uiMode == UI_LOCAL)      { drawLocalMapScreen(); return; }

  if (pilot.docked) drawDockedScreen();
  else drawFlightScreen();
}

// -----------------------------------------------------
// JANUS BRAIN
// -----------------------------------------------------
void buildFeatures(float f[12]) {
  StarSystem &sys = galaxy[pilot.currentSystem];
  int aliveEnemies = 0;
  for (int i = 0; i < MAX_ENEMIES; ++i) if (enemies[i].alive) aliveEnemies++;

  f[0]  = aliveEnemies / 8.0f;
  f[1]  = 1.0f - (pilot.fuel / 70.0f);
  f[2]  = clampf(pilot.credits / 6000.0f, 0.0f, 1.5f);
  f[3]  = sys.danger / 7.0f;
  f[4]  = 1.0f - (pilot.cargoCount / (float)cargoLimit());
  f[5]  = sys.techLevel / 15.0f;
  f[6]  = 1.0f - shipShield / 100.0f;
  f[7]  = 1.0f - shipEnergy / 100.0f;
  f[8]  = sensorBiasAggro;
  f[9]  = random(0, 100) / 100.0f;
  f[10] = janus.aggression;
  f[11] = janus.caution;
}

float janusEval(const float f[12]) {
  float s = janus.bias;
  for (int i = 0; i < 12; ++i) s += janus.w[i] * f[i] * janus.slimeTrace[i];
  return s;
}

void quantizeJanusWeights() {
  for (int i = 0; i < 12; ++i) {
    int8_t q = (int8_t)clampi((int)roundf(janus.w[i] * 127.0f), -127, 127);
    janus.w[i] = q / 127.0f;
  }
  int8_t qb = (int8_t)clampi((int)roundf(janus.bias * 127.0f), -127, 127);
  janus.bias = qb / 127.0f;
}

void trainJanus(const float f[12], float reward) {
  float survivalSec = (millis() - survivalStartMs) / 1000.0f;
  reward += survivalSec * 0.002f;
  reward += pilot.kills * 0.01f;
  reward += pilot.credits * 0.0001f;
  if (justDied) reward -= 1.5f;

  float pred = janusEval(f);
  float error = reward - pred;
  float bond = expf(-fabsf(error));

  for (int i = 0; i < 12; ++i) {
    janus.w[i] += janus.lr * error * f[i] * bond;
    janus.slimeTrace[i] = 0.9f * janus.slimeTrace[i] + 0.1f * bond;
    if (janus.slimeTrace[i] < janus.slimeThreshold) janus.w[i] *= 0.85f;
  }

  janus.bias += janus.lr * error;
  quantizeJanusWeights();
}


bool hasAllCoreEquipment() {
  return hasEquipment(EQ_FUEL_SCOOP) &&
         hasEquipment(EQ_CARGO_EXPANSION) &&
         hasEquipment(EQ_MILITARY_LASER) &&
         hasEquipment(EQ_DOCK_COMPUTER) &&
         hasEquipment(EQ_ECM);
}


void janusRestockMissiles() {
  if (pilot.missiles >= 4) return;
  const int missileCost = 30;
  while (pilot.missiles < 4 && pilot.credits >= missileCost) {
    pilot.credits -= missileCost;
    pilot.missiles++;
    statusLine = "MISSILE BUY";
  }
}

float getEquipmentUtility(uint32_t equipment) {
  switch (equipment) {
    case EQ_FUEL_SCOOP:
      return 0.5f + (1.0f - pilot.fuel / 70.0f) * 1.5f;
    case EQ_CARGO_EXPANSION:
      return (pilot.cargoCount > BASE_CARGO * 0.7f) ? 2.0f : 0.3f;
    case EQ_MILITARY_LASER:
      return (pilot.kills * 0.05f) + (galaxy[pilot.currentSystem].danger * 0.35f);
    case EQ_DOCK_COMPUTER:
      return 1.4f + (learnState.totalDeaths > 2 ? 0.5f : 0.0f);
    case EQ_GAL_HYPERDRIVE:
      return (pilot.galaxyIndex == 0 && pilot.kills > 30) ? 1.2f : 0.1f;
    case EQ_ECM:
      return pilot.legalStatus != LEGAL_CLEAN || pilot.kills > 20 ? 1.1f : 0.25f;
    default:
      return 0.0f;
  }
}

void janusBuyEquipment() {
  struct Option {
    uint32_t flag;
    int cost;
    float utility;
  };

  Option options[6] = {
    {EQ_FUEL_SCOOP,      150, getEquipmentUtility(EQ_FUEL_SCOOP)},
    {EQ_CARGO_EXPANSION, 220, getEquipmentUtility(EQ_CARGO_EXPANSION)},
    {EQ_MILITARY_LASER,  400, getEquipmentUtility(EQ_MILITARY_LASER)},
    {EQ_DOCK_COMPUTER,   180, getEquipmentUtility(EQ_DOCK_COMPUTER)},
    {EQ_GAL_HYPERDRIVE,  600, getEquipmentUtility(EQ_GAL_HYPERDRIVE)},
    {EQ_ECM,             260, getEquipmentUtility(EQ_ECM)}
  };

  for (int i = 0; i < 6; ++i) {
    for (int j = i + 1; j < 6; ++j) {
      if (options[j].utility > options[i].utility) {
        Option t = options[i];
        options[i] = options[j];
        options[j] = t;
      }
    }
  }

  for (int i = 0; i < 6; ++i) {
    if (hasEquipment(options[i].flag)) continue;
    if (pilot.credits < options[i].cost) continue;
    if (options[i].utility < 0.8f) continue;
    buyEquipment(options[i].flag, options[i].cost);
    break;
  }
}


void janusPlanTrade(uint8_t target) {
  int capacity = cargoLimit() - pilot.cargoCount;
  if (capacity <= 0) return;

  struct ItemScore {
    uint8_t item;
    int score;
    int price;
  };
  ItemScore scored[MAX_MARKET_ITEMS];

  for (uint8_t i = 0; i < MAX_MARKET_ITEMS; ++i) {
    int buyHere = getCommodityPrice(i, galaxy[pilot.currentSystem]);
    int sellThere = getCommodityPrice(i, galaxy[target]);
    int score = sellThere - buyHere;

    score += (int)(janus.traderInstinct * 6.0f);
    if (janus.lawfulness > 0.7f && isIllegalCommodity(i, galaxy[target])) score -= 25;
    if (janus.lawfulness < 0.3f && isIllegalCommodity(i, galaxy[target])) score += 8;
    if (janus.curiosity > 0.7f && !hasVisitedSystem(target)) score += 4;

    // Safe explicit assignment instead of brace-init on embedded toolchains.
    scored[i].item = i;
    scored[i].score = score;
    scored[i].price = buyHere;
  }

  // Stable selection-style sort, avoids fragile index mistakes.
  for (int i = 0; i < MAX_MARKET_ITEMS - 1; ++i) {
    for (int j = i + 1; j < MAX_MARKET_ITEMS; ++j) {
      if (scored[j].score > scored[i].score) {
        ItemScore temp = scored[i];
        scored[i] = scored[j];
        scored[j] = temp;
      }
    }
  }

  int willingness = 1 + (int)(janus.traderInstinct * 3.0f);
  for (int n = 0; n < MAX_MARKET_ITEMS && capacity > 0; ++n) {
    if (scored[n].score <= 0) continue;

    int desired = 1 + willingness + (int)random(0, 3);
    int amount = (capacity < desired) ? capacity : desired;

    while (amount > 0 && capacity > 0 && pilot.credits >= scored[n].price) {
      if (!buyCommodity(scored[n].item)) break;
      capacity--;
      amount--;
    }
  }
}

float calculateThreat(const SpaceShip& enemy) {
  float threat = 0.0f;
  float distance = sqrtf(enemy.pos.x * enemy.pos.x + enemy.pos.y * enemy.pos.y + enemy.pos.z * enemy.pos.z);
  threat += shipStats[enemy.type].hp * 0.35f;
  threat += 80.0f / (distance + 10.0f);
  threat += 140.0f / (fabsf(enemy.pos.z) + 20.0f);   // frontal closing danger
  threat -= fabsf(enemy.pos.x) * 0.15f;              // side targets less urgent
  if (enemy.role == ROLE_POLICE) threat *= 1.3f;
  if (enemy.role == ROLE_HUNTER) threat *= 1.2f;
  if (enemy.role == ROLE_BOSS) threat *= 2.0f;
  threat += (1.0f - enemy.hp / max(1.0f, shipStats[enemy.type].hp)) * 20.0f;
  return threat;
}


void updateGoal() {
  int aliveEnemies = 0;
  for (int i = 0; i < MAX_ENEMIES; ++i) if (enemies[i].alive) aliveEnemies++;

  if (pilot.missileIncoming) currentGoal = GOAL_ESCAPE;
  else if (pilot.passengerJob.active && pilot.currentSystem != pilot.passengerJob.targetSystem && pilot.fuel > 8 && aliveEnemies == 0) currentGoal = GOAL_TRADE;
  else if (aliveEnemies > 0 && shipShield > 40.0f * (1.0f - janus.riskTolerance * 0.3f)) currentGoal = GOAL_FIGHT;
  else if (shipShield < 28.0f + (1.0f - janus.riskTolerance) * 22.0f || shipEnergy < 25.0f) currentGoal = GOAL_ESCAPE;
  else if (pilot.fuel < (12.0f - janus.riskTolerance * 3.0f) && hasEquipment(EQ_FUEL_SCOOP)) currentGoal = GOAL_REFUEL;
  else if (pilot.cargoCount >= cargoLimit() * (0.8f - janus.traderInstinct * 0.2f)) currentGoal = GOAL_DOCK;
  else if (pilot.credits > 300 && random(0,100) < (10 + (int)(janus.curiosity * 20.0f))) currentGoal = GOAL_UPGRADE;
  else currentGoal = GOAL_TRADE;

  if (currentGoal == GOAL_FIGHT) pilot.energyMode = 2;
  else if (currentGoal == GOAL_ESCAPE) pilot.energyMode = 3;
  else if (currentGoal == GOAL_DOCK) pilot.energyMode = 1;
  else pilot.energyMode = 0;
}

uint8_t pickBestTradeItem(uint8_t target) {
  StarSystem &from = galaxy[pilot.currentSystem];
  StarSystem &to   = galaxy[target];

  int bestScore = -9999;
  uint8_t bestItem = 0;

  for (uint8_t i = 0; i < MAX_MARKET_ITEMS; ++i) {
    int buyP = getCommodityPrice(i, from);
    int sellP = getCommodityPrice(i, to);
    int score = sellP - buyP - fuelCostTo(target);
    if (score > bestScore) {
      bestScore = score;
      bestItem = i;
    }
  }
  return bestItem;
}


uint8_t pickTradeTarget() {
  int bestScore = -9999;
  uint8_t bestTarget = pilot.currentSystem;

  for (uint8_t i = 0; i < MAX_SYSTEMS; ++i) {
    if (i == pilot.currentSystem) continue;
    int fuelCost = fuelCostTo(i);
    if (fuelCost > pilot.fuel) continue;

    int score = bestCommodityMarginHere(i) - fuelCost * 2 - galaxy[i].danger;
    score -= (int)(learnState.systemDeathPenalty[i] * 40.0f * (1.0f + janus.riskTolerance * 0.5f));

    // Trader prefers safe systems and earlier selling.
    score += (int)(janus.traderInstinct * 12.0f * (7 - galaxy[i].danger));

    // Lawful pilot avoids police trouble when dirty.
    if (janus.lawfulness > 0.7f && pilot.legalStatus != LEGAL_CLEAN && galaxy[i].government > 4) {
      score -= 50;
    }

    // Curious pilot likes unseen systems.
    if (janus.curiosity > 0.7f && !hasVisitedSystem(i)) {
      score += 20;
    }

    // Pirate / risk-tolerant prefers rougher worlds if already criminal.
    if (janus.lawfulness < 0.3f && pilot.legalStatus != LEGAL_CLEAN) {
      score += (int)(galaxy[i].danger * 3);
      score -= (int)(galaxy[i].government * 3);
    }

    if (score > bestScore) {
      bestScore = score;
      bestTarget = i;
    }
  }
  return bestTarget;
}

// -----------------------------------------------------
// GAME LOGIC
// -----------------------------------------------------
void janusPrepareLaunch() {
  float f[12];
  buildFeatures(f);

  if (hasEquipment(EQ_GAL_HYPERDRIVE) && pilot.kills > 25 && random(0, 100) < 10) {
    galacticJump();
  }

  uint8_t target = pickTradeTarget();
  uint8_t item   = pickBestTradeItem(target);

  if (pilot.cargoCount < cargoLimit() && pilot.credits > 50) {
    buyCommodity(item);
  }

  jumpToSystem(target);

  shipEnergy = clampf(shipEnergy + 4.0f, 0.0f, 100.0f);
  shipShield = clampf(shipShield + 4.0f, 0.0f, 100.0f);

  launchScene();
  trainJanus(f, 0.12f);
  saveBestAgentIfImproved();
  janus.decisions++;
}

void janusCombatDecision() {
  float f[12];
  buildFeatures(f);
  laserFiring = false;

  int targetIndex = -1;
  float maxThreat = -1.0f;
  for (int i = 0; i < MAX_ENEMIES; ++i) {
    if (!enemies[i].alive || enemies[i].pos.z <= 10.0f) continue;

    int visx, visy; float visd;
    bool visible = enemyScreenVisible(enemies[i], visx, visy, visd);

    float threat = calculateThreat(enemies[i]);
    if (visible) threat += 18.0f;  // prefer what is actually on screen
    if (threat > maxThreat) {
      maxThreat = threat;
      targetIndex = i;
    }
  }

  if (laserHeat >= 90) statusLine = "LASER HOT";
  else if (isDamaged(DMG_LASER)) statusLine = "LASER DMG";

  if (pilot.missileIncoming) {
    if (hasEquipment(EQ_ECM) && pilot.ecmCooldown == 0 && shipEnergy >= 10.0f) {
      attemptECM();
    } else if (hasEquipment(EQ_ECM) && pilot.ecmCooldown > 0) {
      statusLine = "ECM COOL";
    } else {
      shipShield -= 14.0f;
      shipEnergy -= 4.0f;
      pilot.missileIncoming = false;
      statusLine = "MISSILE HIT";
      playHitSound();
    }
  }

  if (targetIndex < 0) {
    janus.mode = MODE_RETURN;
    returnTimer = 140;
    statusLine = "AREA CLEAR";
    trainJanus(f, 0.08f);
    janus.decisions++;
    return;
  }

  SpaceShip &e = enemies[targetIndex];

  float targetYaw = atan2f(e.pos.x, e.pos.z);
  float targetPitch = -atan2f(e.pos.y, e.pos.z);

  shipYaw += clampf(targetYaw * (shipStats[e.type].turnRate + janus.aggression * 0.02f) + sensorBiasYaw * 0.25f, -0.05f, 0.05f);
  shipPitch += clampf(targetPitch * 0.10f, -0.04f, 0.04f);
  shipRoll = clampf(-targetYaw * 0.8f, -0.5f, 0.5f);
  shipSpeed = clampf(shipSpeed + 0.002f, 0.10f, 0.40f);

  int tsx, tsy; float td;
  bool targetVisible = enemyScreenVisible(e, tsx, tsy, td);
  bool inCross = targetVisible && fabsf(e.pos.x) < 8 && fabsf(e.pos.y) < 8 && e.pos.z < 120;

  if (pilot.missiles > 0 &&
      (e.type == SHIP_CONSTRICTOR || e.type == SHIP_ANACONDA || e.type == SHIP_PYTHON || e.role == ROLE_THARGOID) &&
      (targetVisible || (e.pos.z < 130 && fabsf(e.pos.x) < 16 && fabsf(e.pos.y) < 16))) {
    e.hp -= 42.0f;
    pilot.missiles--;
    statusLine = "MISSILE";
    playToneSafe(180, 80);
    trainJanus(f, 0.05f);
  }

  if (!targetVisible) {
    statusLine = "TRACKING";
  }

  if (inCross && laserHeat < 90 && !isDamaged(DMG_LASER)) {
    laserFiring = true;
    laserHeat += 8.0f;
    float laserDamage = hasEquipment(EQ_MILITARY_LASER) ? 14.0f : 8.0f;
    if (e.type == SHIP_TRANSPORT || e.type == SHIP_PYTHON || e.type == SHIP_ANACONDA || e.type == SHIP_CONSTRICTOR) laserDamage -= 2.0f;
    e.hp -= laserDamage;
    statusLine = "FIRE";
    playLaserSound();
    trainJanus(f, 0.04f);
  }

  float breakShield = 20.0f + janus.caution * 20.0f;
  if (shipShield < breakShield || shipEnergy < breakShield) {
    janus.mode = MODE_RETURN;
    returnTimer = 140;
    statusLine = "BREAK";
    trainJanus(f, -0.20f);
  } else {
    trainJanus(f, 0.02f);
  }

  janus.decisions++;
}

void updateDockApproach() {
  if (dockPhase == DOCK_NONE) {
    beginDockRun();
    dockAlignTicks = 0;
    return;
  }

  float dx = -stationPos.x;
  float dy = -stationPos.y;

  if (dockPhase == DOCK_APPROACH) {
    shipYaw += clampf(dx * 0.0008f, -0.020f, 0.020f);
    shipPitch += clampf(-dy * 0.0008f, -0.020f, 0.020f);
    shipRoll *= 0.96f;
    stationPos.x *= 0.96f;
    stationPos.y *= 0.96f;
    stationPos.z -= clampf(shipSpeed * 2.0f, 0.25f, 0.8f);

    if (fabsf(stationPos.x) < 9.0f && fabsf(stationPos.y) < 9.0f && stationPos.z < 95.0f) {
      dockPhase = DOCK_ALIGN;
      dockAlignTicks = 0;
      statusLine = "DOCK ALIGN";
    }
    return;
  }

  if (dockPhase == DOCK_ALIGN) {
    float alignFactor = hasEquipment(EQ_DOCK_COMPUTER) ? 0.88f : 0.97f;
    shipYaw *= alignFactor;
    shipPitch *= alignFactor;
    shipRoll *= 0.95f;
    stationPos.z -= hasEquipment(EQ_DOCK_COMPUTER) ? 0.8f : 0.35f;

    float slotPhase = fabsf(sinf(stationSpin));
    float alignmentError = fabsf(stationPos.x) + fabsf(stationPos.y) + fabsf(shipYaw) * 90.0f + fabsf(shipPitch) * 90.0f;
    float entryThreshold = hasEquipment(EQ_DOCK_COMPUTER) ? 11.0f : 6.5f;
    bool spinWindowOk = hasEquipment(EQ_DOCK_COMPUTER) ? true : (slotPhase < 0.28f);

    if (alignmentError < entryThreshold && stationPos.z < 68.0f && spinWindowOk) {
      dockPhase = DOCK_TUNNEL;
      tunnelTimer = 0;
      statusLine = "DOCK ENTRY";
      return;
    }

    if (stationPos.z < 52.0f && (!spinWindowOk || alignmentError > entryThreshold * 1.7f)) {
      dockPhase = DOCK_APPROACH;
      stationPos.z = 132.0f;
      shipShield -= hasEquipment(EQ_DOCK_COMPUTER) ? 1.0f : 3.0f;
      registerDeath(DEATH_DOCK);
      statusLine = "ALIGN FAIL";
      return;
    }
    return;
  }

  if (dockPhase == DOCK_TUNNEL) {
    tunnelTimer++;
    shipYaw *= 0.90f;
    shipPitch *= 0.90f;
    shipRoll *= 0.90f;
    if (tunnelTimer > 32) {
      pilot.docked = true;
      dockPhase = DOCK_NONE;
      stationPos = {0, 0, 250};
      shipSpeed = 0.0f;
      resetEnemies();
      resetDebris();
      janus.mode = MODE_DOCKED;
      dockTimer = 12;
      stationFlowActive = false;
      stationStep = SERVICE_IDLE;
      statusLine = "DOCK OK";
      playDockSound();
    }
  }
}


void janusDockedDecision() {
  if (alreadyJumpedThisDock || jumpInProgress) return;

  float f[12];
  buildFeatures(f);
  updateGoal();

  decayLegalStatusInDock();
  updateMissionsOnDock();

  // Sell only profitable cargo first, keep the rest for future systems.
  StarSystem &sys = galaxy[pilot.currentSystem];
  for (int i = pilot.cargoCount - 1; i >= 0; --i) {
    uint8_t item = pilot.cargoType[i];
    int sellPrice = getCommodityPrice(item, sys);
    if (sellPrice >= pilot.cargoBuyPrice[i]) {
      pilot.credits += sellPrice;
      pilot.totalProfit += (sellPrice - pilot.cargoBuyPrice[i]);
      for (int j = i; j < pilot.cargoCount - 1; ++j) {
        pilot.cargoType[j] = pilot.cargoType[j + 1];
        pilot.cargoBuyPrice[j] = pilot.cargoBuyPrice[j + 1];
      }
      pilot.cargoCount--;
    }
  }

  refuelAtStation();
  janusBuyEquipment();
  janusRestockMissiles();

  if (pilot.damagedFlags != DMG_NONE && pilot.credits >= 100) {
    pilot.credits -= 100;
    pilot.damagedFlags = DMG_NONE;
    statusLine = "REPAIRED";
  }

  if (hasEquipment(EQ_GAL_HYPERDRIVE) && pilot.kills > 25 && pilot.credits > 800 && pilot.galaxyIndex < 7) {
    if (pilot.credits > 1200 || learnState.systemDeathPenalty[pilot.currentSystem] > 1.2f) {
      galacticJump();
      alreadyJumpedThisDock = false;
      trainJanus(f, 0.08f);
      rewardPersonalityFromSuccess();
      saveBestAgentIfImproved();
      janus.decisions++;
      return;
    }
  }

  uint8_t target = pickTradeTarget();

  // If there is no better reachable system, do not sit in dock forever.
  if (target == pilot.currentSystem || fuelCostTo(target) > pilot.fuel) {
    launchScene();
    alreadyJumpedThisDock = false;
    trainJanus(f, 0.03f);
    saveBestAgentIfImproved();
    janus.decisions++;
    return;
  }

  janusPlanTrade(target);
  beginVisibleJump(target);

  // If for any reason jump did not actually start, force launch so Janus leaves the dock.
  if (!jumpInProgress) {
    launchScene();
    alreadyJumpedThisDock = false;
    trainJanus(f, 0.02f);
    saveBestAgentIfImproved();
    janus.decisions++;
    return;
  }

  alreadyJumpedThisDock = true;

  shipEnergy = clampf(shipEnergy + 4.0f, 0.0f, 100.0f);
  shipShield = clampf(shipShield + 4.0f, 0.0f, 100.0f);

  trainJanus(f, 0.12f);
  rewardPersonalityFromSuccess();
  saveBestAgentIfImproved();
  janus.decisions++;
}

void janusSpaceDecision() {
  if (dockPhase != DOCK_NONE) {
    updateDockApproach();
    return;
  }

  if (janus.mode == MODE_LAUNCH) {
    shipSpeed = clampf(shipSpeed + 0.005f, 0.10f, 0.22f);
    stationPos.z += 2.0f;
    stationPos.x *= 0.96f;
    stationPos.y *= 0.96f;
    shipYaw *= 0.94f;
    shipPitch *= 0.94f;
    shipRoll *= 0.94f;
    statusLine = "LAUNCH";
    if (--launchTimer <= 0 || stationPos.z > 420.0f) {
      janus.mode = MODE_PATROL;
      patrolTimer = 320 + random(0, 180);
      currentGoal = GOAL_NONE;
      statusLine = "CRUISE";
    }
    return;
  }

  if (janus.mode == MODE_PATROL) {
    shipYaw += sensorBiasYaw * 0.025f + frandRange(-0.003f, 0.003f);
    shipPitch += frandRange(-0.0015f, 0.0015f);
    shipRoll = clampf(shipRoll * 0.97f + frandRange(-0.006f, 0.006f), -0.22f, 0.22f);
    shipSpeed = clampf(shipSpeed + frandRange(-0.001f, 0.002f), 0.10f, 0.21f);

    if (hasEquipment(EQ_FUEL_SCOOP) && pilot.fuel < 58) {
      float targetYaw = atan2f(sunPos.x, sunPos.z);
      float targetPitch = -atan2f(sunPos.y, sunPos.z);
      shipYaw += clampf(targetYaw * 0.03f, -0.02f, 0.02f);
      shipPitch += clampf(targetPitch * 0.03f, -0.02f, 0.02f);
      statusLine = "SUN SEEK";
    } else if (random(0, 100) < 5) {
      planetPos.z -= 2.0f;
      statusLine = "PLANET";
    }

    stationPos.z += 0.5f;
    if (stationPos.z > 420.0f) stationPos.z = 420.0f;

    int contactChance = 2 + galaxy[pilot.currentSystem].danger;
    if (shipShield > 45.0f && shipEnergy > 45.0f && random(0, 100) < contactChance) {
      spawnSingleEnemy();
      janus.mode = MODE_COMBAT;
      statusLine = "CONTACT";
      return;
    }

    if (pilot.legalStatus != LEGAL_CLEAN && shipShield > 40.0f && random(0, 100) < 4) {
      spawnPoliceWing();
      janus.mode = MODE_COMBAT;
      statusLine = "POLICE";
      return;
    }

    bool lowResources = (shipShield < 35.0f || shipEnergy < 35.0f || pilot.fuel < 12);
    bool bored = (patrolTimer <= 0 && random(0, 100) < 15);
    patrolTimer--;

    if (lowResources || bored) {
      janus.mode = MODE_RETURN;
      returnTimer = 220;
      beginDockRun();
      statusLine = "RETURN";
    } else {
      statusLine = "CRUISE";
    }
    return;
  }

  if (janus.mode == MODE_COMBAT) {
    janusCombatDecision();
    if (shipShield < 30 || shipEnergy < 30) {
      janus.mode = MODE_RETURN;
      returnTimer = 220;
      beginDockRun();
      statusLine = "BREAK";
    }
    return;
  }

  if (janus.mode == MODE_RETURN) {
    shipSpeed = clampf(shipSpeed * 0.995f, 0.08f, 0.18f);
    updateDockApproach();
  }
}

void updateIntersystemEvents() {
  if (pilot.docked) return;
  if (millis() - lastEventRoll < 4000) return;

  lastEventRoll = millis();
  int roll = random(0, 100);

  if (roll < 8) {
    spawnSingleEnemy();
    statusLine = "AMBUSH";
  } else if (roll < 12 && pilot.legalStatus != LEGAL_CLEAN) {
    spawnPoliceWing();
  } else if (roll < 18) {
    statusLine = "TRAFFIC";
  }
}

// -----------------------------------------------------
// WORLD UPDATE
// -----------------------------------------------------
void updateStars() {
  for (int i = 0; i < STAR_COUNT; ++i) {
    stars[i].z -= shipSpeed * 4.0f;
    stars[i].x -= shipYaw * 1.5f;
    stars[i].y -= shipPitch * 1.2f;
    if (stars[i].z < -200.0f || stars[i].z > 200.0f) {
      stars[i].x = frandRange(-140, 140);
      stars[i].y = frandRange(-140, 140);
      stars[i].z = frandRange(-200, 200);
      if (fabsf(stars[i].z) < 12.0f) stars[i].z = (stars[i].z < 0 ? -24.0f : 24.0f);
    }
  }
}

void updatePlanetSun() {
  planetPos.z -= shipSpeed * 2.5f;
  sunPos.z -= shipSpeed * 1.5f;
  if (planetPos.z < 120) planetPos.z = 320;
  if (sunPos.z < 180) sunPos.z = 420;
}

void updateEnemies() {
  for (int i = 0; i < MAX_ENEMIES; ++i) {
    if (!enemies[i].alive) continue;

    enemies[i].pos.x += enemies[i].vel.x;
    enemies[i].pos.y += enemies[i].vel.y;
    enemies[i].pos.z += enemies[i].vel.z - shipSpeed * 2.5f;

    enemies[i].vel.x += clampf(-enemies[i].pos.x * 0.0008f, -0.02f, 0.02f);
    enemies[i].vel.y += clampf(-enemies[i].pos.y * 0.0008f, -0.02f, 0.02f);

    bool enemyAligned = fabsf(enemies[i].pos.x) < 10 && fabsf(enemies[i].pos.y) < 8 && enemies[i].pos.z < 100;
    if ((enemies[i].type == SHIP_PYTHON || enemies[i].type == SHIP_ANACONDA || enemies[i].type == SHIP_CONSTRICTOR) &&
        enemies[i].pos.z < 140 && !pilot.missileIncoming && random(0, 100) < 3) {
      pilot.missileIncoming = true;
      statusLine = "MISSILE LOCK";
    }
    if (enemyAligned && random(0, 100) < 20) {
      float hit = 2.0f + galaxy[pilot.currentSystem].danger * 0.25f;
      if (enemies[i].role == ROLE_POLICE) hit += 1.0f;
      if (enemies[i].role == ROLE_HUNTER) hit += 0.7f;
      if (enemies[i].role == ROLE_THARGOID) hit += 1.2f;
      shipShield -= hit;
      shipEnergy -= 1.0f;
      if (random(0,100) < 6) damageRandomSubsystem();
      statusLine = "HIT";
      playHitSound();
    }

    if (enemies[i].hp <= 0.0f) {
      Vec3 deathPos = enemies[i].pos;
      uint8_t deathRole = enemies[i].role;
      enemies[i].alive = false;
      pilot.kills++;
      pilot.score += 50;
      statusLine = "KILL";
      playKillSound();

      if (deathRole == ROLE_THARGOID) maybeSpawnThargons(deathPos);
      if (deathRole == ROLE_POLICE || deathRole == ROLE_TRADER) {
        addCrime(25);
        spawnPoliceWing();
      }
      if (deathRole == ROLE_BOSS && pilot.missionStage == MISSION_CONSTRICTOR) {
        pilot.missionProgress = 1;
        statusLine = "MISSION DONE";
      }
    }

    if (enemies[i].pos.z < 5 || enemies[i].pos.z > 260) {
      enemies[i].alive = false;
    }
  }

  int aliveCount = 0;
  for (int i = 0; i < MAX_ENEMIES; ++i) if (enemies[i].alive) aliveCount++;

  if (aliveCount == 0 && janus.mode == MODE_COMBAT) {
    janus.mode = MODE_RETURN;
    returnTimer = 80;
    statusLine = "CLEAR";
  }
}

void applyAvoidance(Vec3 objPos, float strength) {
  float dist = sqrtf(objPos.x * objPos.x + objPos.y * objPos.y + objPos.z * objPos.z);
  if (dist < 0.001f || dist > 70.0f) return;

  if (objPos.z > 0.0f) {
    shipYaw += clampf((-objPos.x / dist) * strength, -0.02f, 0.02f);
    shipPitch += clampf((objPos.y / dist) * strength, -0.02f, 0.02f);
  }

  if (dist < 12.0f) {
    shipShield -= 6.0f;
    shipEnergy -= 3.0f;
    if (random(0,100) < 10) damageRandomSubsystem();
    shipYaw += 0.03f;
    shipRoll += 0.05f;
    statusLine = "COLLISION";
  }
}

void updateCollisionAvoidance() {
  if (pilot.docked) return;
  if (dockPhase != DOCK_NONE) return;
  if (janus.mode == MODE_RETURN) return;

  if (stationPos.z > 0.0f && stationPos.z < 90.0f) applyAvoidance(stationPos, 0.010f);
  if (planetPos.z > 0.0f && planetPos.z < 110.0f) applyAvoidance(planetPos, 0.020f);
  if (sunPos.z > 0.0f && sunPos.z < 95.0f) applyAvoidance(sunPos, 0.018f);

  for (int i = 0; i < MAX_TRAFFIC; ++i) {
    if (traffic[i].alive && traffic[i].pos.z > 0.0f && traffic[i].pos.z < 60.0f) applyAvoidance(traffic[i].pos, 0.016f);
  }
  for (int i = 0; i < MAX_ENEMIES; ++i) {
    if (enemies[i].alive && enemies[i].pos.z > 0.0f && enemies[i].pos.z < 60.0f) applyAvoidance(enemies[i].pos, 0.012f);
  }
  for (int i = 0; i < 10; ++i) {
    if (canisters[i].alive && canisters[i].pos.z > 0.0f && canisters[i].pos.z < 40.0f) applyAvoidance(canisters[i].pos, 0.010f);
  }
}

void updateShipState() {
  float eRegen = 0.08f;
  float sRegen = 0.05f;
  if (pilot.energyMode == 1) sRegen = 0.09f;
  if (pilot.energyMode == 2) eRegen = 0.11f;
  if (pilot.energyMode == 3 && !isDamaged(DMG_ENGINE)) shipSpeed = clampf(shipSpeed + 0.002f, 0.10f, 0.30f);
  if (isDamaged(DMG_ENGINE)) shipSpeed = clampf(shipSpeed, 0.08f, 0.18f);

  shipEnergy = clampf(shipEnergy + eRegen, 0, 100);
  shipShield = clampf(shipShield + sRegen, 0, 100);
  laserHeat = clampf(laserHeat - 1.8f, 0, 100);
  stationSpin += 0.012f;

  if (shipShield <= 0 || shipEnergy <= 0) {
    pilot.score -= 30;
    pilot.docked = true;
    stationPos = {0, 0, 250};
    resetEnemies();
    shipSpeed = 0;
    shipEnergy = 100;
    shipShield = 100;
    janus.mode = MODE_DOCKED;
    dockTimer = 12;
    stationFlowActive = false;
    stationStep = SERVICE_IDLE;
    statusLine = "RECOVER";
  }
}

// -----------------------------------------------------
// INPUT
// -----------------------------------------------------
void applyBrightness() {
  M5.Display.setBrightness(brightnessLevels[brightnessIndex]);
}

void handleButton() {
  if (M5.BtnA.wasPressed()) {
    btnPressStart = millis();
    holdActionTriggered = false;
  }

  if (M5.BtnA.isPressed() && !holdActionTriggered) {
    unsigned long held = millis() - btnPressStart;

    if (held > 2000) {
      if      (uiMode == UI_FLIGHT) uiMode = UI_STATUS;
      else if (uiMode == UI_STATUS) uiMode = UI_MARKET;
      else if (uiMode == UI_MARKET) uiMode = UI_EQUIP;
      else if (uiMode == UI_EQUIP)  uiMode = UI_LOCAL;
      else if (uiMode == UI_LOCAL)  uiMode = UI_MAP;
      else if (uiMode == UI_MAP)    uiMode = UI_MISSION;
      else uiMode = UI_FLIGHT;

      holdActionTriggered = true;
      statusLine = "UI MODE";
      return;
    }

    if (held > 900) {
      soundEnabled = !soundEnabled;
      holdActionTriggered = true;
      statusLine = soundEnabled ? "SOUND ON" : "SOUND OFF";
      if (soundEnabled) playToneSafe(1000, 80);
    }
  }

  if (M5.BtnA.wasReleased()) {
    unsigned long held = millis() - btnPressStart;
    if (!holdActionTriggered && held < 900) {
      brightnessIndex = (brightnessIndex + 1) & 0x03;
      applyBrightness();
      statusLine = "BRIGHT";
    }
  }
}

// -----------------------------------------------------
// MAIN TICK
// -----------------------------------------------------
void logicTick() {
  updateSensorInfluence();
  laserFiring = false;

  if (jumpInProgress) {
    if (jumpTimer > 0) jumpTimer--;
    else completeVisibleJump();
    return;
  }

  if (inWitchSpace) {
    if (witchTimer > 0) witchTimer--;
    else {
      inWitchSpace = false;
      uiMode = UI_FLIGHT;
      janus.mode = MODE_COMBAT;
      spawnThargoidEncounter();
    }
    return;
  }

  if (pilot.docked) {
    if (!jumpInProgress && uiMode == UI_MAP) uiMode = UI_MARKET;
    updateStationFlow();

    if (!stationFlowActive) {
      if (dockTimer > 0) {
        dockTimer--;
        if (dockTimer == 0 && millis() - lastAgentTick > 150) {
          lastAgentTick = millis();
          if (!stationFlowActive) beginStationFlow();
        }
      } else if (millis() - lastAgentTick > 180) {
        lastAgentTick = millis();
        if (!stationFlowActive) janusDockedDecision();
      }
    }
  } else {
    // Critical fix: while launching, do NOT let updateGoal pull Janus back into docking logic.
    if (janus.mode == MODE_LAUNCH) {
      if (millis() - lastAgentTick > 120) {
        lastAgentTick = millis();
        janusSpaceDecision();
      }

      updateStars();
      updatePlanetSun();
      updateTraffic();
      updateEnemies();
      updateDebris();
      updateCanisters();
      updateEscapePods();
      updateFuelScoop();
      updateECMCooldown();
      processPoliceScanInFlight();
      updateCollisionAvoidance();
      updateIntersystemEvents();
      updateShipState();
      return;
    }

    updateGoal();

    if (millis() - lastAgentTick > 220) {
      lastAgentTick = millis();

      switch (currentGoal) {
        case GOAL_ESCAPE:
          janus.mode = MODE_RETURN;
          returnTimer = 160;
          updateDockApproach();
          shipSpeed = clampf(shipSpeed + 0.01f, 0.15f, 0.40f);
          break;

        case GOAL_FIGHT:
          janus.mode = MODE_COMBAT;
          janusCombatDecision();
          break;

        case GOAL_REFUEL: {
          float targetYaw = atan2f(sunPos.x, sunPos.z);
          float targetPitch = -atan2f(sunPos.y, sunPos.z);
          shipYaw += clampf(targetYaw * 0.03f, -0.02f, 0.02f);
          shipPitch += clampf(targetPitch * 0.03f, -0.02f, 0.02f);
          shipSpeed = clampf(shipSpeed, 0.10f, 0.18f);
          statusLine = "SUN SKIM";
          break;
        }

        case GOAL_DOCK:
          janus.mode = MODE_RETURN;
          updateDockApproach();
          break;

        default:
          janusSpaceDecision();
          break;
      }
    }

    updateStars();
    updatePlanetSun();
    updateTraffic();
    updateEnemies();
    updateDebris();
    updateCanisters();
    updateEscapePods();
    updateFuelScoop();
    updateECMCooldown();
    processPoliceScanInFlight();
    updateCollisionAvoidance();
    updateIntersystemEvents();
    updateShipState();
  }
}

// -----------------------------------------------------
// SETUP / LOOP
// -----------------------------------------------------
void setup() {
  auto cfg = M5.config();
  cfg.external_speaker.atomic_echo = 1;
  M5.begin(cfg);
  initStackedSpeaker();

  Serial.begin(115200);
  delay(100);

  LittleFS.begin(true);
  clearKnownStaleFiles();
  randomSeed((uint32_t)esp_random());

  loadState();
  pilot.passengerJob.active = false;
  pilot.passengerJob.targetSystem = 0;
  pilot.passengerJob.reward = 0;
  pilot.ecmCooldown = 0;
  loadLearnState();
  loadBestAgentIfAvailable();
  generateGalaxy(pilot.galaxyIndex);
  selectStationType();

  M5.Display.setRotation(0);
  applyBrightness();

  resetStars();
  resetEnemies();
  resetDebris();
  resetCanisters();
  resetEscapePods();
  initTraffic();
  pilot.passengerJob.active = false;
  pilot.passengerJob.targetSystem = 0;
  pilot.passengerJob.reward = 0;

  stationPos = {0.0f, 0.0f, 250.0f};
  planetPos  = {0.0f, 0.0f, 320.0f};
  sunPos     = {80.0f, -40.0f, 420.0f};

  shipEnergy = 100.0f;
  shipShield = 100.0f;
  laserHeat  = 0.0f;
  shipSpeed  = 0.12f;

  pilot.docked = false;
  launchScene();

  survivalStartMs = millis();
  statusLine = "JANUS LIVE";
  renderTick();
}

void loop() {
  M5.update();
  handleButton();

  if (paused) {
    if (millis() - lastRenderTick > 120) {
      lastRenderTick = millis();
      renderTick();
    }
    delay(20);
    return;
  }

  if (millis() - lastLogicTick > 33) {
    lastLogicTick = millis();
    logicTick();
  }

  if (millis() - lastRenderTick > 50) {
    lastRenderTick = millis();
    renderTick();
  }

  if (millis() - lastSaveTick > 5000) {
    lastSaveTick = millis();
    saveState();
    saveLearnState();
  }

  delay(5);
}