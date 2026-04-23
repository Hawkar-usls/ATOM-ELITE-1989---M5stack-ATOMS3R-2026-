#include <M5Unified.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <math.h>

extern "C" {
  #include "driver/i2s_std.h"
}

// =====================================================
// ATOM ELITE - JANUS LEARNING PILOT EDITION v8.5.0 MESH ECHO COSMOSIM
// Travel appetite raised, visible laser/missile FX, real station UI flow,
// more aggressive trade/mission loops, blind-eye/beacon mesh telemetry,
// local Atomic Echo mic ambience bias, smoother 35-ish fps pacing.
// =====================================================

#define SCREEN_W 128
#define SCREEN_H 128
#define MESH_WIFI_SSID ""
#define MESH_WIFI_PASSWORD ""
#define MESH_URL_BLIND_EYE ""
#define MESH_URL_BEACON    ""

#define LOGIC_INTERVAL_MS 16
#define RENDER_INTERVAL_MS 12
#define PAUSED_RENDER_INTERVAL_MS 66
#define ECHO_MIC_BCLK_PIN 6
#define ECHO_MIC_WS_PIN   5
#define ECHO_MIC_DATA_PIN 7
#define ECHO_MIC_SAMPLE_RATE 16000
#define ECHO_MIC_FRAME_SAMPLES 128

#define SAVE_PILOT_FILE "/pilot.dat"
#define SAVE_AGENT_FILE "/agent.dat"
#define SAVE_LEARN_FILE "/learn.dat"
#define SAVE_WORLD_FILE "/world.dat"
#define SAVE_BEST_AGENT_FILE "/agent_best.dat"
#define WORLD_SAVE_VERSION 0x574C4432UL

#define BASE_CARGO 20
#define EXPANDED_CARGO 40

#define MAX_SYSTEMS 64
#define MAX_MARKET_ITEMS 8
#define MAX_ENEMIES 8
#define MAX_TRAFFIC 6
#define STAR_COUNT 32
#define PLANET_SCREEN_RADIUS 56
#define PLANET_RING_INNER_OFFSET 8
#define PLANET_RING_OUTER_OFFSET 18
#define COM_FOOD 0
#define COM_TEXTILES 1
#define COM_LIQUOR 2
#define COM_LUXURIES 3
#define COM_COMPUTERS 4
#define COM_MACHINERY 5
#define COM_ALLOYS 6
#define COM_MEDICINE 7

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
  UI_BAR = 8,
  UI_DOCK_INTERIOR = 9
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

enum CareerTrack {
  CAREER_TRADER = 0,
  CAREER_MECH_HAULER = 1,
  CAREER_MECH_MERC = 2
};

enum SurfaceBiome {
  BIOME_DUST = 0,
  BIOME_ICE = 1,
  BIOME_JUNGLE = 2,
  BIOME_VOLCANIC = 3
};

enum ShipFrameClass {
  FRAME_COBRA = 0,
  FRAME_FREIGHTER = 1,
  FRAME_CARRIER = 2
};

enum ShipDoctrine {
  DOCTRINE_TRADE = 0,
  DOCTRINE_SKIRMISH = 1,
  DOCTRINE_MINING = 2,
  DOCTRINE_LOGISTICS = 3,
  DOCTRINE_ASSAULT = 4,
  DOCTRINE_COMMAND = 5
};

enum SectorFaction {
  FACTION_CONSORT = 0,
  FACTION_UNION = 1,
  FACTION_WARDENS = 2,
  FACTION_THARGOID = 3
};

enum SurfaceObjective {
  SURFACE_RAID = 0,
  SURFACE_SIEGE = 1,
  SURFACE_EXTRACTION = 2
};

enum StellarPhenomenon {
  PHENOM_NORMAL = 0,
  PHENOM_PULSAR = 1,
  PHENOM_QUASAR = 2
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
  SERVICE_DOCK_INTERIOR = 1,
  SERVICE_MARKET = 2,
  SERVICE_EQUIP = 3,
  SERVICE_STATUS = 4,
  SERVICE_BAR = 5,
  SERVICE_LAUNCH = 6
};

enum DockPhase {
  DOCK_NONE = 0,
  DOCK_APPROACH = 1,
  DOCK_ALIGN = 2,
  DOCK_TUNNEL = 3
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

struct RemoteNodeState {
  bool online;
  unsigned long lastSeenMs;
  float shock;
  float magNorm;
  float gyroZ;
  float sync;
  float activity;
  float entropy;
  float m2r;
  float fit;
  char status[24];
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

struct BarMission {
  bool active;
  uint8_t type;         // 0 cargo, 1 hunt, 2 passenger, 3 mech haul, 4 surface war
  uint8_t targetSystem;
  uint8_t targetKills;
  uint8_t biome;
  uint8_t mechCount;
  uint8_t surfaceWaves;
  uint8_t objective;
  int reward;
  char title[24];
};

struct AsteroidRock {
  bool alive;
  Vec3 pos;
  Vec3 vel;
  float hp;
  float size;
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
  uint8_t careerTrack;
  uint8_t shipFrame;
  uint8_t drillTier;
  bool mechOwned;
  uint8_t mechLevel;
  uint8_t mechWeaponTier;
  uint16_t mechArmorMax;
  uint16_t mechAlloys;
  uint16_t mechServos;
  uint16_t mechCores;
  uint16_t mechWeaponParts;
  uint8_t mechHaulsCompleted;
  uint8_t surfaceOpsCompleted;
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

struct SurfaceOpState {
  bool active;
  uint8_t biome;
  uint8_t objective;
  uint8_t threat;
  uint8_t wavesRemaining;
  uint16_t mechArmor;
  uint16_t mechHeat;
  uint16_t payout;
  uint16_t salvageRoll;
  uint16_t tick;
};

struct WorldSaveBlob {
  uint32_t version;
  int8_t systemTradeFlux[MAX_SYSTEMS];
  int8_t systemRaidHeat[MAX_SYSTEMS];
  int8_t systemNeed[MAX_SYSTEMS];
  int8_t systemFactionPower[MAX_SYSTEMS];
  int8_t systemThargoidBlight[MAX_SYSTEMS];
  uint8_t systemFaction[MAX_SYSTEMS];
  BarMission barMission;
  SurfaceOpState surfaceOp;
  uint8_t uiMode;
  uint8_t stationStep;
  uint8_t dockPhase;
  uint8_t currentGoal;
  uint8_t jumpTargetSystem;
  uint8_t pendingJumpSystem;
  bool jumpInProgress;
  bool inWitchSpace;
  bool stationFlowActive;
  int patrolTimer;
  int launchTimer;
  int returnTimer;
  int dockTimer;
  int witchTimer;
  int stationStepTimer;
  int jumpTimer;
  int tunnelTimer;
  int launchLockout;
  float shipYaw;
  float shipPitch;
  float shipRoll;
  float shipSpeed;
  float shipEnergy;
  float shipShield;
  float laserHeat;
  Vec3 stationPos;
  Vec3 planetPos;
  Vec3 sunPos;
  float stationSpin;
  char statusText[32];
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


// ---------------- VISUAL STYLE HELPERS ----------------
static inline uint16_t mix565(uint16_t a, uint16_t b, float t) {
  t = clampf(t, 0.0f, 1.0f);
  int ar = (a >> 11) & 31, ag = (a >> 5) & 63, ab = a & 31;
  int br = (b >> 11) & 31, bg = (b >> 5) & 63, bb = b & 31;
  int rr = (int)(ar + (br - ar) * t);
  int rg = (int)(ag + (bg - ag) * t);
  int rb = (int)(ab + (bb - ab) * t);
  return (rr << 11) | (rg << 5) | rb;
}

static inline uint8_t procHash8(int x, int y, uint8_t seed = 0) {
  uint8_t v = (uint8_t)(x * 17 + y * 31 + seed * 13);
  v ^= (v >> 3);
  v = (uint8_t)(v * 17 + 23);
  return v;
}

static inline float procCell01(int x, int y, uint8_t seed = 0) {
  return (float)(procHash8(x, y, seed) & 0x0F) / 15.0f;
}

void fillDitherCircle(int cx, int cy, int r, uint16_t c1, uint16_t c2, uint8_t phase = 0) {
  if (r <= 0) return;
  int rr = r * r;
  for (int y = -r; y <= r; ++y) {
    int span = (int)sqrtf((float)(rr - y * y));
    float horizon = span / (float)r;
    float noise = procCell01(y + r, r, phase) * 0.16f;
    float mixT = clampf(0.22f + horizon * 0.60f + noise, 0.0f, 1.0f);
    uint16_t row = mix565(c2, c1, mixT);
    M5.Display.drawFastHLine(cx - span, cy + y, span * 2 + 1, row);

    int shadowStart = cx + span / 5;
    int shadowLen = cx + span - shadowStart + 1;
    if (shadowLen > 0) {
      M5.Display.drawFastHLine(shadowStart, cy + y, shadowLen, mix565(row, c2, 0.42f));
    }

    if (((y + phase) & 3) == 0 && span > 3) {
      int scatterW = clampi(span, 1, 96);
      int px = cx - span / 2 + (procHash8(y, r, phase) % scatterW);
      M5.Display.drawPixel(px, cy + y, mix565(c1, c2, 0.35f));
    }
  }
}

void fillDitherRect(int x, int y, int w, int h, uint16_t c1, uint16_t c2, uint8_t phase = 0) {
  if (w <= 0 || h <= 0) return;
  int innerW = clampi(w - 2, 0, SCREEN_W);
  int sparkW = clampi(w - 4, 1, 120);
  float invH = 1.0f / (float)clampi(h - 1, 1, 255);

  for (int yy = 0; yy < h; ++yy) {
    float t = yy * invH;
    float rib = (((yy + phase) & 3) == 0) ? 0.10f : 0.0f;
    float noise = procCell01(yy, w, phase) * 0.08f;
    uint16_t row = mix565(c2, c1, clampf(0.24f + (1.0f - t) * 0.56f + rib + noise, 0.0f, 1.0f));
    M5.Display.drawFastHLine(x, y + yy, w, row);

    if (((yy + phase) & 5) == 0 && innerW > 0) {
      M5.Display.drawFastHLine(x + 1, y + yy, innerW, colorDim(TFT_WHITE, 0.05f));
    }

    if (((yy + phase) & 3) == 1 && w > 8) {
      int px = x + 2 + (procHash8(yy, h, phase) % sparkW);
      M5.Display.drawPixel(px, y + yy, mix565(c1, c2, 0.25f));
    }
  }
}

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
int systemPrices[MAX_SYSTEMS][MAX_MARKET_ITEMS];
int8_t systemTradeFlux[MAX_SYSTEMS];
int8_t systemRaidHeat[MAX_SYSTEMS];
int8_t systemNeed[MAX_SYSTEMS];
int8_t systemFactionPower[MAX_SYSTEMS];
int8_t systemThargoidBlight[MAX_SYSTEMS];
uint8_t systemFaction[MAX_SYSTEMS];
uint8_t systemBiome[MAX_SYSTEMS];
uint8_t systemPhenomenon[MAX_SYSTEMS];
BarMission currentBarMission;
AsteroidRock asteroids[12];
SurfaceOpState surfaceOp;

SpaceShip enemies[MAX_ENEMIES];
SpaceShip traffic[MAX_TRAFFIC];
Vec3 stars[STAR_COUNT];
Debris debris[24];
CargoCanister canisters[10];
LearnState learnState;
bool justDied = false;
DeathCause lastDeathCause = DEATH_NONE;
unsigned long survivalStartMs = 0;
PilotGoal currentGoal = GOAL_NONE;
bool alreadyJumpedThisDock = false;

UiMode uiMode = UI_FLIGHT;
StationServiceStep stationStep = SERVICE_IDLE;

String statusLine = "BOOT";

bool paused = false;
bool laserFiring = false;
bool soundEnabled = true;
int screenFlashTimer = 0;
int hitShakeTimer = 0;
int dockCinemaPhase = 0;
int miningBeamPhase = 0;
bool unifiedMinimalFX = true;
float unifiedExposure = 1.0f;
String hudCacheLeft1, hudCacheLeft2, hudCacheLeft3, hudCacheLeft4;
String hudCacheRight1, hudCacheRight2, hudCacheRight3, hudCacheRight4;
uint32_t lastHudCompose = 0;
bool heavyPlanetView = false;
bool survivalFocusMode = false;
uint8_t frameParity = 0;
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
unsigned long lastGalaxyPulse = 0;
unsigned long lastVisualPhaseTick = 0;
unsigned long lastToneTick = 0;
unsigned long lastMicSampleTick = 0;

int patrolTimer = 0;
int launchTimer = 0;
int returnTimer = 0;
int dockTimer = 0;
int witchTimer = 0;
int stationStepTimer = 0;
int jumpTimer = 0;
int tunnelTimer = 0;
uint8_t jumpTargetSystem = 0;
uint8_t pendingJumpSystem = 255;
int launchLockout = 0;

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
float localMicRms = 0.0f;
float localMicMood = 0.0f;
bool localMicReady = false;
i2s_chan_handle_t echoMicRxHandle = nullptr;

Vec3 stationPos = {0.0f, 0.0f, 250.0f};
Vec3 planetPos  = {0.0f, 0.0f, 320.0f};
Vec3 sunPos     = {80.0f, -40.0f, 420.0f};
float stationSpin = 0.0f;

RemoteNodeState blindEyeState = {false, 0, 0, 0, 0, 0, 0, 0, 0, 0, "OFF"};
RemoteNodeState beaconState   = {false, 0, 0, 0, 0, 0, 0, 0, 0, 0, "OFF"};
unsigned long lastMeshPullTick = 0;
Vec3 lastLaserImpactPos = {0.0f, 0.0f, 0.0f};
Vec3 lastMissileTargetPos = {0.0f, 0.0f, 0.0f};
int laserBeamTimer = 0;
int missileTrailTimer = 0;
int mapBrowseTimer = 0;
float remoteExplorationBias = 0.0f;

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

ShipModel stationModel = {coriolisVerts, coriolisEdges, 6, 12};

// -----------------------------------------------------
// NETWORK / MESH TELEMETRY
// -----------------------------------------------------
bool ensureMeshWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  static unsigned long lastTry = 0;
  if (millis() - lastTry < 6000) return false;
  lastTry = millis();
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(MESH_WIFI_SSID, MESH_WIFI_PASSWORD);
  unsigned long until = millis() + 1600;
  while (WiFi.status() != WL_CONNECTED && millis() < until) delay(20);
  return WiFi.status() == WL_CONNECTED;
}

bool httpGetFast(const char* url, String& response, int timeoutMs = 350) {
  if (!ensureMeshWiFi()) return false;
  HTTPClient http;
  http.setConnectTimeout(timeoutMs);
  http.setTimeout(timeoutMs);
  if (!http.begin(url)) return false;
  int code = http.GET();
  response = (code > 0) ? http.getString() : "";
  http.end();
  return code >= 200 && code < 300;
}

JsonObject pickTelemetryPayload(StaticJsonDocument<2048>& doc) {
  JsonObject root = doc.as<JsonObject>();
  if (root.isNull()) return root;
  if (root.containsKey("payload") && root["payload"].is<JsonObject>()) {
    JsonObject p = root["payload"].as<JsonObject>();
    if (p.containsKey("data") && p["data"].is<JsonObject>()) return p["data"].as<JsonObject>();
    if (p.containsKey("payload") && p["payload"].is<JsonObject>()) return p["payload"].as<JsonObject>();
    return p;
  }
  if (root.containsKey("data") && root["data"].is<JsonObject>()) return root["data"].as<JsonObject>();
  return root;
}

float jsonFloatSafe(JsonObject obj, const char* key, float fallback) {
  if (obj.isNull() || !obj.containsKey(key) || obj[key].isNull()) return fallback;
  return obj[key].as<float>();
}

void decodeRemoteNode(RemoteNodeState &node, const String& body, bool isBeacon) {
  StaticJsonDocument<2048> doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) { node.online = false; return; }
  JsonObject obj = pickTelemetryPayload(doc);
  if (obj.isNull()) { node.online = false; return; }
  node.online = true;
  node.lastSeenMs = millis();
  node.shock = jsonFloatSafe(obj, isBeacon ? "eye_shock" : "shock", node.shock);
  node.magNorm = jsonFloatSafe(obj, isBeacon ? "eye_mag_norm" : "mag_norm", node.magNorm);
  node.gyroZ = jsonFloatSafe(obj, isBeacon ? "eye_gyro_z" : "gyro_z", node.gyroZ);
  node.sync = jsonFloatSafe(obj, isBeacon ? "eye_sync" : "sync_hint", node.sync);
  node.activity = jsonFloatSafe(obj, isBeacon ? "eye_activity" : "activity", node.activity);
  node.entropy = jsonFloatSafe(obj, "entropy", node.entropy);
  node.m2r = jsonFloatSafe(obj, "m2r", node.m2r);
  node.fit = jsonFloatSafe(obj, "fit", node.fit);
  const char* st = obj["status"] | (isBeacon ? "beacon" : "eye");
  strncpy(node.status, st, sizeof(node.status) - 1);
  node.status[sizeof(node.status) - 1] = 0;
}

void updateRemoteMeshInfluence() {
  if (millis() - lastMeshPullTick < 2200) return;
  lastMeshPullTick = millis();
  String body;
  if (httpGetFast(MESH_URL_BLIND_EYE, body, 420)) decodeRemoteNode(blindEyeState, body, false);
  if (httpGetFast(MESH_URL_BEACON, body, 420)) decodeRemoteNode(beaconState, body, true);

  if (blindEyeState.online && millis() - blindEyeState.lastSeenMs < 9000) {
    remoteExplorationBias = clampf(blindEyeState.activity * 0.03f + blindEyeState.magNorm * 0.0008f + blindEyeState.shock * 0.025f, 0.0f, 0.35f);
  } else if (beaconState.online && millis() - beaconState.lastSeenMs < 9000) {
    remoteExplorationBias = clampf(beaconState.sync * 0.12f + beaconState.entropy * 0.01f, 0.0f, 0.28f);
  } else {
    remoteExplorationBias *= 0.90f;
  }
}

float readLocalEchoMicRms() {
  if (!echoMicRxHandle) return 0.0f;

  int32_t samples[ECHO_MIC_FRAME_SAMPLES];
  size_t bytesRead = 0;
  esp_err_t err = i2s_channel_read(echoMicRxHandle, samples, sizeof(samples), &bytesRead, 2);
  if (err != ESP_OK || bytesRead == 0) return localMicRms * 0.96f;

  int count = bytesRead / sizeof(int32_t);
  if (count <= 0) return localMicRms * 0.96f;

  double sumSq = 0.0;
  for (int i = 0; i < count; ++i) {
    float v = (float)samples[i] / 2147483648.0f;
    sumSq += (double)v * (double)v;
  }
  return sqrt(sumSq / (double)count);
}

bool initLocalEchoMic() {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
  if (i2s_new_channel(&chan_cfg, NULL, &echoMicRxHandle) != ESP_OK) return false;

  i2s_std_config_t std_cfg = {
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(ECHO_MIC_SAMPLE_RATE),
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .bclk = (gpio_num_t)ECHO_MIC_BCLK_PIN,
      .ws = (gpio_num_t)ECHO_MIC_WS_PIN,
      .dout = I2S_GPIO_UNUSED,
      .din = (gpio_num_t)ECHO_MIC_DATA_PIN,
      .invert_flags = {
        .mclk_inv = false,
        .bclk_inv = false,
        .ws_inv = false,
      },
    },
  };

  if (i2s_channel_init_std_mode(echoMicRxHandle, &std_cfg) != ESP_OK) return false;
  if (i2s_channel_enable(echoMicRxHandle) != ESP_OK) return false;

  localMicRms = 0.0f;
  localMicMood = 0.0f;
  return true;
}

void updateLocalEchoMic() {
  if (!localMicReady) return;
  if (millis() - lastMicSampleTick < 120) return;
  lastMicSampleTick = millis();

  bool speakerBusy = soundEnabled && (millis() - lastToneTick < 180);
  if (speakerBusy) {
    localMicRms *= 0.94f;
    localMicMood *= 0.96f;
    return;
  }

  float rms = readLocalEchoMicRms();
  localMicRms = localMicRms * 0.72f + rms * 0.28f;
  float ambient = clampf(localMicRms * 18.0f, 0.0f, 1.2f);
  localMicMood = localMicMood * 0.82f + ambient * 0.18f;
}

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


bool canAffordAnyCommodityFor(uint8_t target) {
  for (uint8_t i = 0; i < MAX_MARKET_ITEMS; ++i) {
    int buyHere = getCommodityPrice(i, galaxy[pilot.currentSystem]);
    int sellThere = getCommodityPrice(i, galaxy[target]);
    if (sellThere > buyHere && pilot.credits >= buyHere) return true;
  }
  return false;
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
  // tiny mutation, but only around current/best brain
  for (int i = 0; i < 12; ++i) {
    janus.w[i] += frandRange(-0.015f, 0.015f);
  }
  janus.aggression += frandRange(-0.02f, 0.02f);
  janus.caution += frandRange(-0.02f, 0.02f);
  janus.aggression = clampf(janus.aggression, 0.12f, 0.95f);
  janus.caution = clampf(janus.caution, 0.12f, 0.98f);
  pilot.damagedFlags = DMG_NONE;
  justDied = false;
  survivalStartMs = millis();
}

// -----------------------------------------------------
// AUDIO
// -----------------------------------------------------
void playToneSafe(uint16_t freq, uint16_t dur) {
  if (!soundEnabled) return;
  lastToneTick = millis();
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

const char* getCareerName(uint8_t track) {
  if (track == CAREER_MECH_HAULER) return "HAULER";
  if (track == CAREER_MECH_MERC) return "MERC";
  return "TRADER";
}

const char* getBiomeName(uint8_t biome) {
  if (biome == BIOME_ICE) return "ICE";
  if (biome == BIOME_JUNGLE) return "JUNGLE";
  if (biome == BIOME_VOLCANIC) return "VOLCANIC";
  return "DUST";
}

const char* getPhenomenonName(uint8_t phenomenon) {
  if (phenomenon == PHENOM_PULSAR) return "PULSAR";
  if (phenomenon == PHENOM_QUASAR) return "QUASAR";
  return "STAR";
}

const char* getShipFrameName(uint8_t frame) {
  if (frame == FRAME_FREIGHTER) return "FREIGHTER";
  if (frame == FRAME_CARRIER) return "CARRIER";
  return "COBRA";
}

const char* getSurfaceObjectiveName(uint8_t objective) {
  if (objective == SURFACE_SIEGE) return "SIEGE";
  if (objective == SURFACE_EXTRACTION) return "EXTRACT";
  return "RAID";
}

bool canRunMechHauls() {
  return (pilot.shipFrame >= FRAME_FREIGHTER);
}

bool canRunSurfaceContracts() {
  return (pilot.mechOwned && pilot.mechLevel >= 1);
}

const char* getCareerObjective() {
  if (!canRunMechHauls()) return "BUILD FREIGHTER";
  if (pilot.mechHaulsCompleted < 2) return "RUN MECH HAULS";
  if (pilot.drillTier == 0) return "FIT DRILLS";
  if (pilot.drillTier < 3 && pilot.careerTrack == CAREER_MECH_HAULER) return "UPGRADE DRILLS";
  if (!pilot.mechOwned) return "FIT MECH BAY";
  if (pilot.surfaceOpsCompleted < 3) return "TAKE GROUND WAR";
  if (pilot.shipFrame != FRAME_CARRIER) return "UPGRADE CARRIER";
  return "SCALE WARZONES";
}

bool usingDualMiningRig() {
  if (pilot.drillTier == 0) return false;
  bool safeIndustrialWindow = (pilot.careerTrack == CAREER_MECH_HAULER &&
                               shipShield > 38.0f &&
                               shipEnergy > 42.0f &&
                               !surfaceOp.active);
  bool noHeavyLaser = ((pilot.equipmentFlags & EQ_MILITARY_LASER) == 0 && pilot.shipFrame >= FRAME_FREIGHTER);
  return (safeIndustrialWindow || noHeavyLaser);
}

bool isMechMissionType(uint8_t type) {
  return (type == 3 || type == 4);
}

bool isSurfaceMissionType(uint8_t type) {
  return (type == 4);
}

uint8_t currentShipMechCapacity() {
  uint8_t cap = 1;
  if (pilot.shipFrame == FRAME_CARRIER) cap = 7;
  else if (pilot.shipFrame == FRAME_FREIGHTER) cap = 4;
  if (pilot.drillTier > 0 && cap > 1) cap--;
  return cap;
}

const char* getFactionName(uint8_t faction) {
  if (faction == FACTION_UNION) return "UNION";
  if (faction == FACTION_WARDENS) return "WARDENS";
  if (faction == FACTION_THARGOID) return "THARGOID";
  return "CONSORT";
}

uint8_t pickCivilFactionForSystem(uint8_t idx) {
  if (galaxy[idx].danger > 5 || galaxy[idx].government < 3) return FACTION_WARDENS;
  if (galaxy[idx].economy < 3) return FACTION_UNION;
  if (galaxy[idx].techLevel > 9) return FACTION_CONSORT;
  return (galaxy[idx].economy > 4) ? FACTION_CONSORT : FACTION_UNION;
}

int getNearbyThargoidPressure(uint8_t idx) {
  int pressure = 0;
  for (int i = 0; i < MAX_SYSTEMS; ++i) {
    if (i == idx || systemThargoidBlight[i] < 6) continue;
    int d = dist2D(galaxy[idx].x, galaxy[idx].y, galaxy[i].x, galaxy[i].y);
    if (d < 52) pressure++;
  }
  return pressure;
}

uint8_t getShipDoctrine() {
  bool cargoExpanded = (pilot.equipmentFlags & EQ_CARGO_EXPANSION) != 0;
  bool militaryFit = (pilot.equipmentFlags & EQ_MILITARY_LASER) != 0;

  if (pilot.shipFrame == FRAME_COBRA) {
    if (militaryFit) return DOCTRINE_SKIRMISH;
    return DOCTRINE_TRADE;
  }
  if (pilot.shipFrame == FRAME_FREIGHTER) {
    if (pilot.drillTier >= 1) return DOCTRINE_MINING;
    if (pilot.mechOwned || cargoExpanded) return DOCTRINE_LOGISTICS;
    return militaryFit ? DOCTRINE_SKIRMISH : DOCTRINE_LOGISTICS;
  }
  if (pilot.mechOwned && militaryFit) return DOCTRINE_ASSAULT;
  if (pilot.mechOwned) return DOCTRINE_COMMAND;
  if (pilot.drillTier >= 2) return DOCTRINE_MINING;
  return DOCTRINE_COMMAND;
}

const char* getShipDoctrineName() {
  switch (getShipDoctrine()) {
    case DOCTRINE_SKIRMISH: return "SKIRM";
    case DOCTRINE_MINING: return "MINING";
    case DOCTRINE_LOGISTICS: return "LOGI";
    case DOCTRINE_ASSAULT: return "ASSAULT";
    case DOCTRINE_COMMAND: return "COMMAND";
    default: return "TRADE";
  }
}

uint16_t getShipTonnage() {
  int tonnage = 90;
  if (pilot.shipFrame == FRAME_FREIGHTER) tonnage = 320;
  else if (pilot.shipFrame == FRAME_CARRIER) tonnage = 780;
  if (pilot.equipmentFlags & EQ_CARGO_EXPANSION) tonnage += 36;
  if (pilot.equipmentFlags & EQ_MILITARY_LASER) tonnage += 28;
  tonnage += pilot.drillTier * 45;
  if (pilot.mechOwned) tonnage += 110;
  tonnage += pilot.missiles * 4;
  tonnage += currentShipMechCapacity() * 35;
  if (getShipDoctrine() == DOCTRINE_ASSAULT) tonnage += 40;
  else if (getShipDoctrine() == DOCTRINE_COMMAND) tonnage += 22;
  return tonnage;
}

const char* getShipBuildName() {
  bool cargoExpanded = (pilot.equipmentFlags & EQ_CARGO_EXPANSION) != 0;
  bool militaryFit = (pilot.equipmentFlags & EQ_MILITARY_LASER) != 0;

  if (pilot.shipFrame == FRAME_COBRA) {
    if (militaryFit && cargoExpanded) return "RAIDER";
    if (militaryFit) return "SKIRM";
    if (cargoExpanded) return "SMUGLER";
    return "RUNNER";
  }
  if (pilot.shipFrame == FRAME_FREIGHTER) {
    if (pilot.drillTier >= 3) return "OREBARGE";
    if (pilot.drillTier >= 1) return "PROSPECT";
    if (pilot.mechOwned) return "TENDER";
    if (cargoExpanded) return "BULKER";
    if (militaryFit) return "ESCORT";
    return "INDY";
  }
  if (pilot.mechOwned && militaryFit) return "SIEGE";
  if (pilot.mechOwned && pilot.surfaceOpsCompleted > 2) return "LEGION";
  if (pilot.mechOwned) return "LANDER";
  if (pilot.drillTier > 0) return "INDCORE";
  if (cargoExpanded) return "FLEET";
  return "HEAVY";
}

int getCommodityDoctrineBias(uint8_t item) {
  switch (getShipDoctrine()) {
    case DOCTRINE_TRADE:
      if (item == COM_LUXURIES) return 8;
      if (item == COM_COMPUTERS) return 6;
      if (item == COM_LIQUOR) return 4;
      return 0;
    case DOCTRINE_SKIRMISH:
      if (item == COM_LUXURIES) return 10;
      if (item == COM_COMPUTERS) return 8;
      if (item == COM_MACHINERY) return 4;
      return -1;
    case DOCTRINE_MINING:
      if (item == COM_ALLOYS) return 18;
      if (item == COM_MACHINERY) return 10;
      if (item == COM_MEDICINE) return -4;
      return 0;
    case DOCTRINE_LOGISTICS:
      if (item == COM_MEDICINE) return 14;
      if (item == COM_FOOD) return 10;
      if (item == COM_TEXTILES) return 8;
      if (item == COM_ALLOYS) return 6;
      return 0;
    case DOCTRINE_ASSAULT:
      if (item == COM_MACHINERY) return 12;
      if (item == COM_MEDICINE) return 8;
      if (item == COM_ALLOYS) return 6;
      return -1;
    case DOCTRINE_COMMAND:
      if (item == COM_COMPUTERS) return 12;
      if (item == COM_MACHINERY) return 10;
      if (item == COM_MEDICINE) return 6;
      return 0;
    default:
      return 0;
  }
}

int getFactionMarketBias(uint8_t idx, uint8_t item) {
  switch (systemFaction[idx]) {
    case FACTION_CONSORT:
      if (item == COM_LUXURIES) return 6;
      if (item == COM_COMPUTERS) return 4;
      return 0;
    case FACTION_UNION:
      if (item == COM_MACHINERY) return 5;
      if (item == COM_ALLOYS) return 4;
      return 0;
    case FACTION_WARDENS:
      if (item == COM_MEDICINE) return 6;
      if (item == COM_MACHINERY) return 3;
      if (item == COM_LUXURIES) return -4;
      return 0;
    case FACTION_THARGOID:
      if (item == COM_FOOD) return 8;
      if (item == COM_MEDICINE) return 10;
      if (item == COM_ALLOYS) return 6;
      if (item == COM_COMPUTERS) return -8;
      if (item == COM_LUXURIES) return -12;
      return 0;
    default:
      return 0;
  }
}

int getBiomeMarketBias(uint8_t idx, uint8_t item) {
  switch (systemBiome[idx]) {
    case BIOME_ICE:
      if (item == COM_FOOD) return 4;
      if (item == COM_MEDICINE) return 3;
      if (item == COM_ALLOYS) return -2;
      return 0;
    case BIOME_JUNGLE:
      if (item == COM_FOOD) return -4;
      if (item == COM_TEXTILES) return -3;
      if (item == COM_MEDICINE) return -2;
      return 0;
    case BIOME_VOLCANIC:
      if (item == COM_ALLOYS) return -5;
      if (item == COM_MACHINERY) return -3;
      if (item == COM_FOOD) return 4;
      return 0;
    default:
      if (item == COM_TEXTILES) return 2;
      return 0;
  }
}

int getPhenomenonMarketBias(uint8_t idx, uint8_t item) {
  if (systemPhenomenon[idx] == PHENOM_PULSAR) {
    if (item == COM_COMPUTERS) return 6;
    if (item == COM_MACHINERY) return 4;
    if (item == COM_MEDICINE) return 2;
  } else if (systemPhenomenon[idx] == PHENOM_QUASAR) {
    if (item == COM_COMPUTERS) return 10;
    if (item == COM_LUXURIES) return 6;
    if (item == COM_FOOD) return 4;
    if (item == COM_MEDICINE) return 5;
  }
  return 0;
}

int getSurfaceBiomeHeatBias(uint8_t biome) {
  if (biome == BIOME_ICE) return -4;
  if (biome == BIOME_JUNGLE) return 2;
  if (biome == BIOME_VOLCANIC) return 5;
  return 0;
}

int getSurfaceBiomeDefenseBias(uint8_t biome) {
  if (biome == BIOME_ICE) return 2;
  if (biome == BIOME_VOLCANIC) return -1;
  return 0;
}

int getSurfaceObjectiveOffenseBias(uint8_t objective) {
  if (objective == SURFACE_RAID) return 2;
  if (objective == SURFACE_SIEGE) return 1;
  return -1;
}

uint8_t pickCareerMissionType() {
  int roll = random(0, 100);
  uint8_t doctrine = getShipDoctrine();

  if (canRunSurfaceContracts()) {
    if ((doctrine == DOCTRINE_ASSAULT || doctrine == DOCTRINE_COMMAND) && roll < 58) return 4;
    if (pilot.surfaceOpsCompleted < 3 && roll < 42) return 4;
    if (pilot.shipFrame == FRAME_CARRIER && roll < 64) return 4;
    if (canRunMechHauls() && (doctrine == DOCTRINE_LOGISTICS || doctrine == DOCTRINE_MINING) && roll < 86) return 3;
    if (canRunMechHauls() && roll < 78) return 3;
  } else if (canRunMechHauls()) {
    if ((doctrine == DOCTRINE_LOGISTICS || doctrine == DOCTRINE_MINING) && roll < 70) return 3;
    if (pilot.mechHaulsCompleted < 2 && roll < 58) return 3;
    if (!pilot.mechOwned && roll < 74) return 3;
  }

  if (doctrine == DOCTRINE_SKIRMISH && roll < 54) return 1;
  if (roll < 38) return 0;
  if (roll < 68) return 1;
  return 2;
}

void seedRouteConductivity(uint8_t source, uint8_t sink, uint8_t amount) {
  if (source >= MAX_SYSTEMS || sink >= MAX_SYSTEMS) return;
  int x0 = galaxy[source].x;
  int y0 = galaxy[source].y;
  int x1 = galaxy[sink].x;
  int y1 = galaxy[sink].y;
  int span = abs(x1 - x0) + abs(y1 - y0) + 1;

  for (int i = 0; i < MAX_SYSTEMS; ++i) {
    int xs = galaxy[i].x;
    int ys = galaxy[i].y;
    int area = abs((x1 - x0) * (y0 - ys) - (x0 - xs) * (y1 - y0));
    int d0 = abs(xs - x0) + abs(ys - y0);
    int d1 = abs(xs - x1) + abs(ys - y1);
    if (area < span * 4 && d0 < span + 40 && d1 < span + 40) {
      systemTradeFlux[i] = clampi((int)systemTradeFlux[i] + amount, 0, 9);
    }
  }
}

void updateLivingGalaxy() {
  if (millis() - lastGalaxyPulse < 6000) return;
  lastGalaxyPulse = millis();

  uint8_t routeSource = random(0, MAX_SYSTEMS);
  uint8_t routeSink = random(0, MAX_SYSTEMS);
  if (routeSink == routeSource) routeSink = (routeSink + 1) % MAX_SYSTEMS;
  seedRouteConductivity(routeSource, routeSink, 2);

  for (int s = 0; s < MAX_SYSTEMS; ++s) {
    int raidDrift = (random(0, 3) - 1) + (galaxy[s].danger > 4 ? 1 : -1);
    int needDrift = (random(0, 3) - 1) + (galaxy[s].economy < 3 ? 1 : 0) + (systemRaidHeat[s] > 6 ? 1 : 0) - (systemTradeFlux[s] > 6 ? 1 : 0);
    int fluxDrift = (random(0, 3) - 1) + (galaxy[s].techLevel > 10 ? 1 : 0) - (systemRaidHeat[s] > 6 ? 2 : 0);
    int nearbyBlight = getNearbyThargoidPressure(s);
    int blightDrift = nearbyBlight + (systemFaction[s] == FACTION_THARGOID ? 2 : -1);
    if (systemPhenomenon[s] == PHENOM_PULSAR) blightDrift += 1;
    if (systemPhenomenon[s] == PHENOM_QUASAR) blightDrift += 2;
    if (systemTradeFlux[s] > 6) blightDrift -= 1;

    systemRaidHeat[s] = clampi((int)systemRaidHeat[s] + raidDrift, 0, 9);
    systemNeed[s] = clampi((int)systemNeed[s] + needDrift, 0, 9);
    systemTradeFlux[s] = clampi((int)systemTradeFlux[s] + fluxDrift, 0, 9);
    systemThargoidBlight[s] = clampi((int)systemThargoidBlight[s] + blightDrift, 0, 9);

    if (currentBarMission.active && currentBarMission.targetSystem == s) {
      if (currentBarMission.type == 3) systemNeed[s] = clampi((int)systemNeed[s] + 1, 0, 9);
      else if (currentBarMission.type == 4) {
        systemRaidHeat[s] = clampi((int)systemRaidHeat[s] + 1, 0, 9);
        systemThargoidBlight[s] = clampi((int)systemThargoidBlight[s] - 1, 0, 9);
      }
    }

    if (systemThargoidBlight[s] > 7) {
      systemFaction[s] = FACTION_THARGOID;
      systemFactionPower[s] = 9;
      systemTradeFlux[s] = clampi((int)systemTradeFlux[s] - 2, 0, 9);
      systemNeed[s] = clampi((int)systemNeed[s] + 2, 0, 9);
      systemRaidHeat[s] = clampi((int)systemRaidHeat[s] + 2, 0, 9);
      continue;
    }

    if (systemFaction[s] == FACTION_THARGOID && systemThargoidBlight[s] < 3) {
      systemFaction[s] = pickCivilFactionForSystem(s);
      systemFactionPower[s] = 4;
    }

    int controlDrift = (random(0, 3) - 1);
    if (systemFaction[s] == FACTION_CONSORT) controlDrift += (systemTradeFlux[s] > systemRaidHeat[s]) ? 1 : -1;
    else if (systemFaction[s] == FACTION_UNION) controlDrift += (systemBiome[s] == BIOME_VOLCANIC || galaxy[s].economy < 3) ? 1 : 0;
    else if (systemFaction[s] == FACTION_WARDENS) controlDrift += (systemRaidHeat[s] > 5 || galaxy[s].danger > 4) ? 1 : -1;
    controlDrift -= systemThargoidBlight[s] / 3;
    systemFactionPower[s] = clampi((int)systemFactionPower[s] + controlDrift, 0, 9);

    if (systemFactionPower[s] < 2) {
      systemFaction[s] = pickCivilFactionForSystem(s);
      systemFactionPower[s] = 4;
    }
  }
}

int getLiveMarketOffset(uint8_t idx, uint8_t item) {
  int flux = systemTradeFlux[idx] - 4;
  int raid = systemRaidHeat[idx] - 4;
  int need = systemNeed[idx] - 4;
  int offset = 0;

  switch (item) {
    case COM_FOOD:      offset = need + raid / 2; break;
    case COM_TEXTILES:  offset = need / 2 + raid / 2; break;
    case COM_LIQUOR:    offset = raid + need / 2; break;
    case COM_LUXURIES:  offset = flux - raid; break;
    case COM_COMPUTERS: offset = flux + galaxy[idx].techLevel / 3 - need / 2; break;
    case COM_MACHINERY: offset = flux + need / 2 + raid / 2; break;
    case COM_ALLOYS:    offset = raid + flux / 2; break;
    case COM_MEDICINE:  offset = need + raid; break;
    default:            offset = 0; break;
  }
  offset += getFactionMarketBias(idx, item);
  offset += getBiomeMarketBias(idx, item);
  offset += getPhenomenonMarketBias(idx, item);
  offset += systemThargoidBlight[idx] / 2;
  return offset;
}

const char* getLocalSectorStateName() {
  uint8_t sys = pilot.currentSystem;
  if (systemFaction[sys] == FACTION_THARGOID || systemThargoidBlight[sys] > 7) return "HIVE";
  if (systemThargoidBlight[sys] > 4) return "BLIGHT";
  if (systemFactionPower[sys] < 3) return "CONTEST";
  if (systemRaidHeat[sys] > 6) return "RAID";
  if (systemNeed[sys] > 6) return "NEED";
  if (systemTradeFlux[sys] > 6) return "CARAVAN";
  return "STABLE";
}

void awardSurfaceSalvage(uint8_t threat) {
  pilot.mechAlloys += 2 + threat;
  pilot.mechServos += 1 + threat / 2;
  pilot.mechCores += (threat > 2) ? 1 : 0;
  pilot.mechWeaponParts += 1 + threat;
}

void autoUpgradeShipFrame() {
  bool freighterReady = (pilot.totalProfit >= 260 || pilot.kills >= 18);
  bool carrierReady = (pilot.mechHaulsCompleted >= 4 || pilot.surfaceOpsCompleted >= 2);

  if (pilot.shipFrame == FRAME_COBRA && pilot.credits >= 2400 && freighterReady) {
    pilot.credits -= 1800;
    pilot.shipFrame = FRAME_FREIGHTER;
    statusLine = "FREIGHTER";
  } else if (pilot.shipFrame == FRAME_FREIGHTER && pilot.credits >= 5400 && carrierReady) {
    pilot.credits -= 4200;
    pilot.shipFrame = FRAME_CARRIER;
    statusLine = "CARRIER";
  }
}

void autoUpgradeMechBay() {
  if (!pilot.mechOwned && canRunMechHauls() && pilot.mechHaulsCompleted >= 2 && pilot.credits >= 900) {
    pilot.credits -= 700;
    pilot.mechOwned = true;
    pilot.mechLevel = 1;
    pilot.mechWeaponTier = 1;
    pilot.mechArmorMax = 120;
    statusLine = "MECH BAY";
  }

  if (!pilot.mechOwned) return;

  if (pilot.mechAlloys >= 18 && pilot.mechServos >= 10) {
    pilot.mechAlloys -= 18;
    pilot.mechServos -= 10;
    pilot.mechArmorMax = clampi(pilot.mechArmorMax + 16, 120, 260);
    statusLine = "MECH ARMOR";
  }

  if (pilot.mechWeaponParts >= 14) {
    pilot.mechWeaponParts -= 14;
    pilot.mechWeaponTier = clampi(pilot.mechWeaponTier + 1, 1, 5);
    statusLine = "MECH GUN";
  }

  if (pilot.mechCores >= 4) {
    pilot.mechCores -= 4;
    pilot.mechLevel = clampi(pilot.mechLevel + 1, 1, 9);
    statusLine = "MECH CORE";
  }
}

void autoUpgradeDrills() {
  if (!canRunMechHauls()) return;
  if (currentBarMission.active && currentBarMission.type == 3) return;

  if (pilot.drillTier == 0 && pilot.credits >= 650 && pilot.mechHaulsCompleted >= 1) {
    pilot.credits -= 450;
    pilot.drillTier = 1;
    statusLine = "DRILL I";
  } else if (pilot.drillTier == 1 && pilot.credits >= 950 && pilot.mechAlloys >= 10) {
    pilot.credits -= 550;
    pilot.mechAlloys -= 10;
    pilot.drillTier = 2;
    statusLine = "DRILL II";
  } else if (pilot.drillTier == 2 && pilot.credits >= 1500 && pilot.mechAlloys >= 20 && pilot.shipFrame == FRAME_CARRIER) {
    pilot.credits -= 900;
    pilot.mechAlloys -= 20;
    pilot.drillTier = 3;
    statusLine = "DRILL III";
  }
}

void beginSurfaceOperationFromBar() {
  if (!pilot.mechOwned) return;
  if (!isSurfaceMissionType(currentBarMission.type)) return;

  memset(&surfaceOp, 0, sizeof(surfaceOp));
  surfaceOp.active = true;
  surfaceOp.biome = currentBarMission.biome;
  surfaceOp.objective = currentBarMission.objective;
  surfaceOp.threat = clampi(currentBarMission.surfaceWaves, 1, 6);
  surfaceOp.wavesRemaining = currentBarMission.surfaceWaves;
  surfaceOp.mechArmor = clampi((int)pilot.mechArmorMax + pilot.mechLevel * 8, 80, 320);
  surfaceOp.mechHeat = 0;
  surfaceOp.payout = currentBarMission.reward;
  surfaceOp.salvageRoll = 0;
  surfaceOp.tick = 0;
  stationFlowActive = false;
  stationStep = SERVICE_IDLE;
  uiMode = UI_MISSION;
  statusLine = "DROP READY";
}

void completeSurfaceOperation(bool success) {
  if (success) {
    uint8_t sys = currentBarMission.targetSystem;
    pilot.credits += surfaceOp.payout;
    pilot.score += surfaceOp.payout / 3;
    pilot.kills += surfaceOp.threat;
    pilot.surfaceOpsCompleted = clampi((int)pilot.surfaceOpsCompleted + 1, 0, 255);
    if (surfaceOp.objective == SURFACE_RAID) pilot.mechWeaponParts += 2 + surfaceOp.threat;
    else if (surfaceOp.objective == SURFACE_SIEGE) pilot.mechAlloys += 3 + surfaceOp.threat;
    else {
      pilot.mechServos += 2 + surfaceOp.threat;
      pilot.mechCores += 1;
    }

    if (surfaceOp.biome == BIOME_ICE) pilot.mechCores += 1;
    else if (surfaceOp.biome == BIOME_JUNGLE) pilot.mechServos += 2;
    else if (surfaceOp.biome == BIOME_VOLCANIC) pilot.mechWeaponParts += 2;
    else pilot.mechAlloys += 2;
    systemRaidHeat[sys] = clampi((int)systemRaidHeat[sys] - 2, 0, 9);
    systemFactionPower[sys] = clampi((int)systemFactionPower[sys] + 2, 0, 9);
    systemThargoidBlight[sys] = clampi((int)systemThargoidBlight[sys] - (systemFaction[sys] == FACTION_THARGOID ? 2 : 1), 0, 9);
    if (systemFaction[sys] == FACTION_THARGOID && systemThargoidBlight[sys] < 3) {
      systemFaction[sys] = pickCivilFactionForSystem(sys);
      systemFactionPower[sys] = 4;
    }
    statusLine = "GROUND PAID";
  } else {
    uint8_t sys = currentBarMission.targetSystem;
    pilot.credits = clampi(pilot.credits - 120, 0, 999999);
    systemRaidHeat[sys] = clampi((int)systemRaidHeat[sys] + 1, 0, 9);
    systemThargoidBlight[sys] = clampi((int)systemThargoidBlight[sys] + 1, 0, 9);
    statusLine = "GROUND LOST";
  }

  surfaceOp.active = false;
  currentBarMission.active = false;
  generateBarMission();
  uiMode = UI_BAR;
}

void updateSurfaceOperation() {
  if (!surfaceOp.active) return;

  surfaceOp.tick++;
  if (surfaceOp.tick < 16) return;
  surfaceOp.tick = 0;

  int offense = 8 + pilot.mechLevel * 3 + pilot.mechWeaponTier * 3 + getSurfaceObjectiveOffenseBias(surfaceOp.objective);
  int defense = 4 + pilot.mechLevel * 2 + getSurfaceBiomeDefenseBias(surfaceOp.biome);
  if (surfaceOp.objective == SURFACE_SIEGE) defense += 2;
  else if (surfaceOp.objective == SURFACE_EXTRACTION) defense -= 1;
  int incoming = clampi(surfaceOp.threat * 4 + random(0, 7) - defense, 1, 18);

  if (surfaceOp.biome == BIOME_JUNGLE && random(0, 100) < 18) incoming += 3;
  surfaceOp.mechHeat = clampi((int)surfaceOp.mechHeat + 6 + surfaceOp.threat + getSurfaceBiomeHeatBias(surfaceOp.biome), 0, 100);
  if (surfaceOp.mechHeat > 74) incoming += 4;
  surfaceOp.mechArmor = (incoming >= (int)surfaceOp.mechArmor) ? 0 : (surfaceOp.mechArmor - incoming);

  if (random(0, 100) < clampi(35 + offense * 3 - surfaceOp.threat * 4, 28, 88)) {
    if (surfaceOp.wavesRemaining > 0) surfaceOp.wavesRemaining--;
    uint8_t salvageThreat = surfaceOp.threat;
    if (surfaceOp.objective == SURFACE_EXTRACTION) salvageThreat++;
    if (surfaceOp.biome == BIOME_JUNGLE) salvageThreat++;
    awardSurfaceSalvage(salvageThreat);
    surfaceOp.salvageRoll += salvageThreat;
    autoUpgradeMechBay();
    statusLine = "SURFACE CLR";
  } else {
    statusLine = "GROUND FIRE";
  }

  surfaceOp.mechHeat = clampi((int)surfaceOp.mechHeat - (7 + pilot.mechLevel), 0, 100);
  if (surfaceOp.mechArmor == 0) {
    completeSurfaceOperation(false);
  } else if (surfaceOp.wavesRemaining == 0) {
    completeSurfaceOperation(true);
  }
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

bool storeCargoUnit(uint8_t item, int bookPrice) {
  if (pilot.cargoCount >= cargoLimit()) return false;
  pilot.cargoType[pilot.cargoCount] = item;
  pilot.cargoBuyPrice[pilot.cargoCount] = bookPrice;
  pilot.cargoCount++;
  return true;
}

void registerMarketActivity(uint8_t item, bool soldHere) {
  uint8_t sys = pilot.currentSystem;
  if (soldHere) {
    systemNeed[sys] = clampi((int)systemNeed[sys] - 1, 0, 9);
    systemTradeFlux[sys] = clampi((int)systemTradeFlux[sys] + 1, 0, 9);
    if (item == COM_MEDICINE) systemRaidHeat[sys] = clampi((int)systemRaidHeat[sys] - 1, 0, 9);
  } else {
    systemNeed[sys] = clampi((int)systemNeed[sys] + 1, 0, 9);
    if (item == COM_ALLOYS || item == COM_MACHINERY) {
      systemTradeFlux[sys] = clampi((int)systemTradeFlux[sys] - 1, 0, 9);
    }
  }
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
  if (!hasEquipment(EQ_MILITARY_LASER) && pilot.credits >= 400 &&
      !(pilot.shipFrame >= FRAME_FREIGHTER && pilot.careerTrack == CAREER_MECH_HAULER && pilot.surfaceOpsCompleted == 0)) {
    buyEquipment(EQ_MILITARY_LASER, 400);
  }
  if (!hasEquipment(EQ_GAL_HYPERDRIVE) && pilot.credits >= 600 && pilot.kills > 25) buyEquipment(EQ_GAL_HYPERDRIVE, 600);
}


void generateSystemPrices() {
  for (int s = 0; s < MAX_SYSTEMS; ++s) {
    for (int i = 0; i < MAX_MARKET_ITEMS; ++i) {
      int p = marketItems[i].basePrice + marketItems[i].gradient * galaxy[s].economy;
      p += (galaxy[s].techLevel - 7);
      p += ((7 - galaxy[s].government) / 2);
      p += ((s * 13 + i * 7 + pilot.galaxyIndex * 11) % 5) - 2; // deterministic jitter
      systemPrices[s][i] = clampi(p, 2, 200);
    }
  }
}


void triggerHitFX(uint8_t strength);

void resetAsteroids() {
  for (int i = 0; i < 12; ++i) asteroids[i].alive = false;
}

void spawnAsteroidField() {
  int spawned = 0;
  for (int i = 0; i < 12; ++i) {
    asteroids[i].alive = true;
    asteroids[i].pos = {frandRange(-75, 75), frandRange(-35, 35), frandRange(120, 260)};
    asteroids[i].vel = {frandRange(-0.04f, 0.04f), frandRange(-0.03f, 0.03f), frandRange(-0.08f, -0.02f)};
    asteroids[i].hp = frandRange(18.0f, 40.0f);
    asteroids[i].size = frandRange(6.0f, 14.0f);
    spawned += 1;
    if (spawned >= 8 && random(0,100) < 60) break;
  }
}

void updateAsteroids() {
  for (int i = 0; i < 12; ++i) {
    if (!asteroids[i].alive) continue;
    asteroids[i].pos.x += asteroids[i].vel.x;
    asteroids[i].pos.y += asteroids[i].vel.y;
    asteroids[i].pos.z += asteroids[i].vel.z - shipSpeed * 2.4f;

    if (asteroids[i].pos.z < 8.0f) {
      shipShield -= 7.0f;
      shipEnergy -= 3.0f;
      shipYaw += frandRange(-0.08f, 0.08f);
      shipPitch += frandRange(-0.05f, 0.05f);
      statusLine = "ROCK IMPACT";
      triggerHitFX(4);
      asteroids[i].alive = false;
    }

    if (asteroids[i].pos.z < -20.0f) asteroids[i].alive = false;
  }
}

void drawAsteroids() {
  uint8_t biome = (galaxy[pilot.currentSystem].economy + galaxy[pilot.currentSystem].danger) % 3;
  for (int i = 0; i < 12; ++i) {
    if (!asteroids[i].alive) continue;
    int sx, sy; float d;
    if (!projectPoint(asteroids[i].pos, sx, sy, d)) continue;
    float presenceBoost = (dockPhase != DOCK_NONE) ? 0.85f : 1.0f;
    int r = clampi((int)(asteroids[i].size * 28.0f * presenceBoost / d), 3, 16);

    uint16_t c1 = 0x8410, c2 = 0x4208;
    if (biome == 1) { c1 = 0xA514; c2 = 0x528A; }      // iron/rust
    else if (biome == 2) { c1 = 0x7CDF; c2 = 0x39CF; } // icy/mineral

    int lobeX = sx - r / 3;
    int lobeY = sy + r / 5;
    int lobeR = clampi(r - 2, 2, 12);
    int capX = sx + r / 3;
    int capY = sy - r / 4;
    int capR = clampi(r / 2 + 1, 2, 8);

    fillDitherCircle(lobeX, lobeY, lobeR, c1, c2, (i + frameParity) & 3);
    fillDitherCircle(capX, capY, capR, mix565(c1, TFT_WHITE, 0.08f), c2, (i + 2) & 3);
    fillDitherCircle(sx, sy, r, mix565(c1, TFT_WHITE, 0.05f), c2, i & 1);

    M5.Display.drawCircle(sx, sy, r, colorDim(TFT_WHITE, 0.80f));
    M5.Display.drawCircle(lobeX, lobeY, lobeR, colorDim(TFT_WHITE, 0.45f));
    if (r > 5) M5.Display.drawCircle(capX, capY, capR, colorDim(TFT_WHITE, 0.30f));

    int ridgeX = sx - r / 2;
    int ridgeY = sy - r / 5;
    M5.Display.drawLine(ridgeX, ridgeY, sx + r / 3, sy + r / 4, colorDim(TFT_BLACK, 0.65f));
    if (r > 4) {
      M5.Display.drawLine(sx - r / 4, sy + r / 3, sx + r / 2, sy - r / 6, colorDim(TFT_BLACK, 0.45f));
      M5.Display.drawFastHLine(sx - r / 2, sy - r / 3, r, colorDim(TFT_WHITE, 0.16f));
      M5.Display.drawFastHLine(sx - r / 3, sy + r / 5, r - 1, colorDim(TFT_BLACK, 0.18f));
    }
    if (!heavyPlanetView && r > 6) {
      M5.Display.drawPixel(sx - r / 3, sy - r / 4, colorDim(TFT_WHITE, 0.75f));
      M5.Display.drawPixel(sx - r / 3 - 1, sy - r / 4 + 1, colorDim(TFT_WHITE, 0.35f));
    }
  }
}

void mineAsteroidsIfHit() {
  if (!laserFiring) return;
  miningBeamPhase = (miningBeamPhase + 1) & 7;
  for (int i = 0; i < 12; ++i) {
    if (!asteroids[i].alive) continue;
    if (fabsf(asteroids[i].pos.x) < 10.0f && fabsf(asteroids[i].pos.y) < 10.0f && asteroids[i].pos.z < 135.0f) {
      float miningDamage = usingDualMiningRig() ? (8.0f + pilot.drillTier * 4.5f) : (hasEquipment(EQ_MILITARY_LASER) ? 11.0f : 7.0f);
      asteroids[i].hp -= miningDamage;
      triggerHitFX(2);
      statusLine = usingDualMiningRig() ? "DUAL DRILL" : "MINING";
      if (asteroids[i].hp <= 0.0f) {
        spawnDebrisBurst(asteroids[i].pos);
        if (pilot.cargoCount < cargoLimit()) {
          int oreUnits = usingDualMiningRig() ? pilot.drillTier : 1;
          bool stored = false;
          for (int n = 0; n < oreUnits; ++n) {
            if (storeCargoUnit(COM_ALLOYS, 0)) stored = true;
          }
          if (stored) {
            if (usingDualMiningRig()) statusLine = String("ORE x") + String(oreUnits);
            else statusLine = "ORE MINED";
          }
        } else {
          spawnCargoCanister(asteroids[i].pos, COM_ALLOYS);
          statusLine = "ORE DROP";
          janus.mode = MODE_RETURN;
          returnTimer = 180;
        }
        asteroids[i].alive = false;
      }
      break;
    }
  }
}

void playLaserSoundProc() {
  if (!soundEnabled) return;
  M5.Speaker.tone(880, 12);
  M5.Speaker.tone(1160, 12);
  M5.Speaker.tone(1440, 8);
}

void playExplosionSoundProc() {
  if (!soundEnabled) return;
  M5.Speaker.tone(280, 22);
  M5.Speaker.tone(190, 26);
  M5.Speaker.tone(120, 30);
}

void playDockSoundProc() {
  if (!soundEnabled) return;
  M5.Speaker.tone(420, 22);
  M5.Speaker.tone(360, 24);
  M5.Speaker.tone(300, 28);
}

void playAlertSoundProc() {
  if (!soundEnabled) return;
  M5.Speaker.tone(760, 30);
  M5.Speaker.tone(760, 30);
}

void applyCRTEffect() {
  // disabled in anti-flicker pass
}



void applyFauxRetinaWeave() {
  // disabled in anti-flicker pass
}

void drawCockpitView() {
  uint16_t frame = colorDim(UI_SOFT, pilot.shipFrame == FRAME_CARRIER ? 0.28f : 0.18f);
  M5.Display.drawLine(0, 102, 18, 88, frame);
  M5.Display.drawLine(127, 102, 109, 88, frame);
  M5.Display.drawFastHLine(0, 105, SCREEN_W, frame);

  if (pilot.shipFrame >= FRAME_FREIGHTER) {
    uint16_t pod = colorDim(UI_AMBER_DARK, 0.42f);
    fillDitherRect(2, 84, 12, 8, pod, colorDim(UI_DIM, 0.10f), frameParity);
    fillDitherRect(114, 84, 12, 8, pod, colorDim(UI_DIM, 0.10f), frameParity);
    M5.Display.drawRect(2, 84, 12, 8, colorDim(TFT_WHITE, 0.35f));
    M5.Display.drawRect(114, 84, 12, 8, colorDim(TFT_WHITE, 0.35f));
    M5.Display.drawLine(13, 88, 23, 83, frame);
    M5.Display.drawLine(114, 88, 104, 83, frame);
  }

  if (pilot.shipFrame == FRAME_CARRIER) {
    M5.Display.drawFastHLine(34, 81, 60, colorDim(UI_SOFT, 0.30f));
    fillDitherRect(44, 77, 12, 5, colorDim(UI_SOFT, 0.26f), colorDim(UI_DIM, 0.08f), frameParity);
    fillDitherRect(72, 77, 12, 5, colorDim(UI_SOFT, 0.26f), colorDim(UI_DIM, 0.08f), frameParity);
    M5.Display.drawFastVLine(64, 72, 12, colorDim(UI_AMBER_BRIGHT, 0.22f));
  }
}

void drawDockInteriorScreen() {
  M5.Display.fillScreen(TFT_BLACK);
  for (int y = 0; y < SCREEN_H; ++y) {
    uint16_t c = mix565(TFT_BLACK, 0x0841, y / 160.0f);
    M5.Display.drawFastHLine(0, y, SCREEN_W, c);
  }

  for (int i = 0; i < 8; ++i) {
    int y = 10 + i * 11;
    M5.Display.drawFastHLine(10, y, 108, colorDim(UI_SOFT, 0.18f));
  }
  for (int i = 0; i < 6; ++i) {
    int x = 14 + i * 18;
    M5.Display.drawFastVLine(x, 10, 92, colorDim(UI_SOFT, 0.14f));
  }

  fillDitherRect(36, 58, 56, 18, colorDim(UI_SOFT, 0.28f), colorDim(UI_DIM, 0.12f), 0);
  M5.Display.drawRoundRect(36, 58, 56, 18, 3, colorDim(TFT_WHITE, 0.75f));
  M5.Display.drawFastHLine(28, 84, 72, colorDim(UI_AMBER_BRIGHT, 0.55f));

  fillDitherRect(46, 44, 34, 10, colorDim(UI_SOFT, 0.30f), colorDim(UI_DIM, 0.14f), 0);
  M5.Display.drawRoundRect(46, 44, 34, 10, 2, colorDim(TFT_WHITE, 0.7f));
  M5.Display.fillTriangle(80, 49, 88, 46, 88, 52, colorDim(UI_AMBER_BRIGHT, 0.60f));

  if (pilot.shipFrame >= FRAME_FREIGHTER) {
    fillDitherRect(16, 72, 16, 9, colorDim(UI_AMBER_DARK, 0.34f), colorDim(UI_DIM, 0.10f), 1);
    fillDitherRect(96, 72, 16, 9, colorDim(UI_AMBER_DARK, 0.34f), colorDim(UI_DIM, 0.10f), 2);
    M5.Display.drawRect(16, 72, 16, 9, colorDim(TFT_WHITE, 0.40f));
    M5.Display.drawRect(96, 72, 16, 9, colorDim(TFT_WHITE, 0.40f));
  }

  if (pilot.shipFrame == FRAME_CARRIER) {
    fillDitherRect(40, 28, 48, 8, colorDim(UI_SOFT, 0.24f), colorDim(UI_DIM, 0.08f), 3);
    M5.Display.drawFastHLine(36, 92, 56, colorDim(UI_AMBER_BRIGHT, 0.40f));
    M5.Display.drawFastVLine(52, 90, 6, colorDim(UI_SOFT, 0.36f));
    M5.Display.drawFastVLine(76, 90, 6, colorDim(UI_SOFT, 0.36f));
  }

  M5.Display.setTextColor(UI_AMBER_BRIGHT, TFT_BLACK);
  const char* phaseText = "STATION LOCK";
  const char* footerText = "SERVICING";
  if (dockCinemaPhase == 1) {
    phaseText = "FINAL APPROACH";
    footerText = "CLAMPING";
  } else if (dockCinemaPhase == 2) {
    phaseText = "LAUNCH TUBE";
    footerText = "CLEARANCE";
  }
  M5.Display.setCursor(2, 2); M5.Display.print(phaseText);
  M5.Display.setCursor(2, 98); M5.Display.printf("BUILD %s %ut", getShipBuildName(), getShipTonnage());
  M5.Display.setCursor(2, 108); M5.Display.printf("FRAME %s C%u", getShipFrameName(pilot.shipFrame), currentShipMechCapacity());
  M5.Display.setCursor(2, 118); M5.Display.print(footerText);
}

void generateBarMission() {
  currentBarMission.active = true;
  currentBarMission.mechCount = 0;
  currentBarMission.surfaceWaves = 0;
  currentBarMission.objective = SURFACE_RAID;
  currentBarMission.type = pickCareerMissionType();
  currentBarMission.targetSystem = random(0, MAX_SYSTEMS);
  if (currentBarMission.targetSystem == pilot.currentSystem) currentBarMission.targetSystem = (currentBarMission.targetSystem + 1) % MAX_SYSTEMS;
  currentBarMission.biome = systemBiome[currentBarMission.targetSystem];
  currentBarMission.targetKills = 2 + random(0, 4);
  currentBarMission.reward = 120 + fuelCostTo(currentBarMission.targetSystem) * 20 + random(0, 120);

  if (currentBarMission.type == 0) {
    currentBarMission.reward += systemNeed[currentBarMission.targetSystem] * 14 + systemThargoidBlight[currentBarMission.targetSystem] * 20;
    if (systemThargoidBlight[currentBarMission.targetSystem] > 4) snprintf(currentBarMission.title, sizeof(currentBarMission.title), "EVAC %s", galaxy[currentBarMission.targetSystem].name);
    else snprintf(currentBarMission.title, sizeof(currentBarMission.title), "FREIGHT %s", galaxy[currentBarMission.targetSystem].name);
  } else if (currentBarMission.type == 1) {
    currentBarMission.reward += systemThargoidBlight[currentBarMission.targetSystem] * 35;
    if (systemFaction[currentBarMission.targetSystem] == FACTION_THARGOID) snprintf(currentBarMission.title, sizeof(currentBarMission.title), "HUNT SWARM");
    else snprintf(currentBarMission.title, sizeof(currentBarMission.title), "HUNT %u PIR", currentBarMission.targetKills);
  } else if (currentBarMission.type == 2) {
    currentBarMission.reward += systemTradeFlux[currentBarMission.targetSystem] * 10 + systemFactionPower[currentBarMission.targetSystem] * 6;
    snprintf(currentBarMission.title, sizeof(currentBarMission.title), "PASSENGER %s", galaxy[currentBarMission.targetSystem].name);
  } else if (currentBarMission.type == 3) {
    currentBarMission.mechCount = clampi(1 + random(0, currentShipMechCapacity()) + galaxy[currentBarMission.targetSystem].danger / 3, 1, currentShipMechCapacity());
    currentBarMission.reward = 260 + currentBarMission.mechCount * 180 + fuelCostTo(currentBarMission.targetSystem) * 24 + galaxy[currentBarMission.targetSystem].danger * 45 + systemNeed[currentBarMission.targetSystem] * 18 + systemTradeFlux[currentBarMission.targetSystem] * 12 + systemThargoidBlight[currentBarMission.targetSystem] * 24;
    if (systemThargoidBlight[currentBarMission.targetSystem] > 4) snprintf(currentBarMission.title, sizeof(currentBarMission.title), "MECH RELIEF");
    else snprintf(currentBarMission.title, sizeof(currentBarMission.title), "MECH HAUL %u", currentBarMission.mechCount);
  } else {
    currentBarMission.surfaceWaves = clampi(2 + random(0, 2) + galaxy[currentBarMission.targetSystem].danger / 2 + systemRaidHeat[currentBarMission.targetSystem] / 3 + pilot.surfaceOpsCompleted / 2 + systemThargoidBlight[currentBarMission.targetSystem] / 3, 2, 6);
    currentBarMission.objective = random(0, 3);
    if (systemFaction[currentBarMission.targetSystem] == FACTION_THARGOID || systemThargoidBlight[currentBarMission.targetSystem] > 4) currentBarMission.objective = SURFACE_SIEGE;
    currentBarMission.reward = 420 + currentBarMission.surfaceWaves * 160 + fuelCostTo(currentBarMission.targetSystem) * 18 + galaxy[currentBarMission.targetSystem].danger * 70 + systemRaidHeat[currentBarMission.targetSystem] * 35 + systemThargoidBlight[currentBarMission.targetSystem] * 45 + (pilot.shipFrame == FRAME_CARRIER ? 120 : 0);
    if (systemFaction[currentBarMission.targetSystem] == FACTION_THARGOID) snprintf(currentBarMission.title, sizeof(currentBarMission.title), "GROUND PURGE");
    else snprintf(currentBarMission.title, sizeof(currentBarMission.title), "GROUND %s", getSurfaceObjectiveName(currentBarMission.objective));
  }
}

void processBarMissionOnDock() {
  if (!currentBarMission.active) {
    generateBarMission();
    return;
  }

  if (surfaceOp.active) return;

  if (currentBarMission.type == 1) {
    if (pilot.kills >= currentBarMission.targetKills) {
      pilot.credits += currentBarMission.reward;
      systemRaidHeat[pilot.currentSystem] = clampi((int)systemRaidHeat[pilot.currentSystem] - 1, 0, 9);
      if (systemFaction[pilot.currentSystem] == FACTION_THARGOID) {
        systemThargoidBlight[pilot.currentSystem] = clampi((int)systemThargoidBlight[pilot.currentSystem] - 1, 0, 9);
      }
      statusLine = "HUNT PAID";
      currentBarMission.active = false;
    }
  } else if (currentBarMission.type == 3) {
    if (pilot.currentSystem == currentBarMission.targetSystem && canRunMechHauls() && currentShipMechCapacity() >= currentBarMission.mechCount) {
      pilot.careerTrack = CAREER_MECH_HAULER;
      pilot.credits += currentBarMission.reward;
      pilot.score += currentBarMission.reward / 4;
      pilot.mechAlloys += currentBarMission.mechCount * 2;
      pilot.mechServos += 1 + currentBarMission.mechCount / 2;
      pilot.mechHaulsCompleted = clampi((int)pilot.mechHaulsCompleted + 1, 0, 255);
      systemNeed[pilot.currentSystem] = clampi((int)systemNeed[pilot.currentSystem] - 2, 0, 9);
      systemTradeFlux[pilot.currentSystem] = clampi((int)systemTradeFlux[pilot.currentSystem] + 2, 0, 9);
      systemFactionPower[pilot.currentSystem] = clampi((int)systemFactionPower[pilot.currentSystem] + 1, 0, 9);
      systemThargoidBlight[pilot.currentSystem] = clampi((int)systemThargoidBlight[pilot.currentSystem] - 1, 0, 9);
      statusLine = "HAUL PAID";
      currentBarMission.active = false;
    }
  } else if (currentBarMission.type == 4) {
    if (!pilot.mechOwned) {
      statusLine = "NEED MECH";
      return;
    }
    if (pilot.currentSystem == currentBarMission.targetSystem) {
      pilot.careerTrack = CAREER_MECH_MERC;
      beginSurfaceOperationFromBar();
      return;
    }
  } else {
    if (pilot.currentSystem == currentBarMission.targetSystem) {
      pilot.credits += currentBarMission.reward;
      if (currentBarMission.type == 0) {
        systemNeed[pilot.currentSystem] = clampi((int)systemNeed[pilot.currentSystem] - 1, 0, 9);
        systemTradeFlux[pilot.currentSystem] = clampi((int)systemTradeFlux[pilot.currentSystem] + 1, 0, 9);
        systemThargoidBlight[pilot.currentSystem] = clampi((int)systemThargoidBlight[pilot.currentSystem] - 1, 0, 9);
      } else if (currentBarMission.type == 2) {
        systemTradeFlux[pilot.currentSystem] = clampi((int)systemTradeFlux[pilot.currentSystem] + 2, 0, 9);
        systemFactionPower[pilot.currentSystem] = clampi((int)systemFactionPower[pilot.currentSystem] + 1, 0, 9);
      }
      statusLine = "MISSION PAID";
      currentBarMission.active = false;
    }
  }
}

int getCommodityPrice(uint8_t item, const StarSystem &sys) {
  int idx = (int)(&sys - galaxy);
  if (idx < 0 || idx >= MAX_SYSTEMS) {
    int p = marketItems[item].basePrice + marketItems[item].gradient * sys.economy;
    return clampi(p, 2, 200);
  }
  return clampi(systemPrices[idx][item] + getLiveMarketOffset(idx, item), 2, 240);
}

bool buyCommodity(uint8_t item) {
  if (pilot.cargoCount >= cargoLimit()) return false;
  int price = getCommodityPrice(item, galaxy[pilot.currentSystem]);
  if (pilot.credits < price) return false;
  pilot.credits -= price;
  if (storeCargoUnit(item, price)) {
    registerMarketActivity(item, false);
    return true;
  }
  pilot.credits += price;
  return false;
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
    registerMarketActivity(item, true);
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
  float stellarHazard = 1.0f;
  if (systemPhenomenon[pilot.currentSystem] == PHENOM_PULSAR) stellarHazard = 1.35f;
  else if (systemPhenomenon[pilot.currentSystem] == PHENOM_QUASAR) stellarHazard = 1.70f;

  if (dist < 185.0f && dist > 100.0f) {
    pilot.fuel = clampi(pilot.fuel + 1, 0, 70);
    laserHeat = clampf(laserHeat + 2.0f * stellarHazard, 0.0f, 100.0f);
    shipSpeed = clampf(shipSpeed - 0.001f, 0.08f, 0.25f);
    statusLine = (systemPhenomenon[pilot.currentSystem] == PHENOM_NORMAL) ? "SCOOP ACTIVE" : "HOT SCOOP";
    playToneSafe(880, 20);
  } else if (dist <= 100.0f) {
    shipShield -= 0.9f * stellarHazard;
    shipEnergy -= 0.5f * stellarHazard;
    laserHeat = clampf(laserHeat + 4.0f * stellarHazard, 0.0f, 100.0f);
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
  pilot.credits = 100000;
  pilot.fuel = 70;
  pilot.docked = true;
  pilot.legalStatus = LEGAL_CLEAN;
  pilot.bounty = 0;
  pilot.equipmentFlags = EQ_NONE;
  pilot.missionStage = MISSION_NONE;
  pilot.missionProgress = 0;
  pilot.missiles = 0;
  pilot.energyMode = 0;
  pilot.damagedFlags = DMG_NONE;
  pilot.missileIncoming = false;
  pilot.careerTrack = CAREER_TRADER;
  pilot.shipFrame = FRAME_COBRA;
  pilot.drillTier = 0;
  pilot.mechOwned = false;
  pilot.mechLevel = 0;
  pilot.mechWeaponTier = 0;
  pilot.mechArmorMax = 0;
  pilot.mechAlloys = 0;
  pilot.mechServos = 0;
  pilot.mechCores = 0;
  pilot.mechWeaponParts = 0;
  pilot.mechHaulsCompleted = 0;
  pilot.surfaceOpsCompleted = 0;
  memset(&surfaceOp, 0, sizeof(surfaceOp));
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
}

void saveWorldState() {
  WorldSaveBlob world = {};
  world.version = WORLD_SAVE_VERSION;
  memcpy(world.systemTradeFlux, systemTradeFlux, sizeof(systemTradeFlux));
  memcpy(world.systemRaidHeat, systemRaidHeat, sizeof(systemRaidHeat));
  memcpy(world.systemNeed, systemNeed, sizeof(systemNeed));
  memcpy(world.systemFactionPower, systemFactionPower, sizeof(systemFactionPower));
  memcpy(world.systemThargoidBlight, systemThargoidBlight, sizeof(systemThargoidBlight));
  memcpy(world.systemFaction, systemFaction, sizeof(systemFaction));
  world.barMission = currentBarMission;
  world.surfaceOp = surfaceOp;
  world.uiMode = (uint8_t)uiMode;
  world.stationStep = (uint8_t)stationStep;
  world.dockPhase = (uint8_t)dockPhase;
  world.currentGoal = (uint8_t)currentGoal;
  world.jumpTargetSystem = jumpTargetSystem;
  world.pendingJumpSystem = pendingJumpSystem;
  world.jumpInProgress = jumpInProgress;
  world.inWitchSpace = inWitchSpace;
  world.stationFlowActive = stationFlowActive;
  world.patrolTimer = patrolTimer;
  world.launchTimer = launchTimer;
  world.returnTimer = returnTimer;
  world.dockTimer = dockTimer;
  world.witchTimer = witchTimer;
  world.stationStepTimer = stationStepTimer;
  world.jumpTimer = jumpTimer;
  world.tunnelTimer = tunnelTimer;
  world.launchLockout = launchLockout;
  world.shipYaw = shipYaw;
  world.shipPitch = shipPitch;
  world.shipRoll = shipRoll;
  world.shipSpeed = shipSpeed;
  world.shipEnergy = shipEnergy;
  world.shipShield = shipShield;
  world.laserHeat = laserHeat;
  world.stationPos = stationPos;
  world.planetPos = planetPos;
  world.sunPos = sunPos;
  world.stationSpin = stationSpin;
  snprintf(world.statusText, sizeof(world.statusText), "%s", statusLine.c_str());

  File h = LittleFS.open(SAVE_WORLD_FILE, "w");
  if (h) {
    h.write((const uint8_t*)&world, sizeof(world));
    h.close();
  }
}

bool loadWorldState() {
  if (!LittleFS.exists(SAVE_WORLD_FILE)) return false;

  File h = LittleFS.open(SAVE_WORLD_FILE, "r");
  WorldSaveBlob world;
  if (!h || h.size() != sizeof(world)) {
    if (h) h.close();
    return false;
  }
  h.read((uint8_t*)&world, sizeof(world));
  h.close();
  if (world.version != WORLD_SAVE_VERSION) return false;

  memcpy(systemTradeFlux, world.systemTradeFlux, sizeof(systemTradeFlux));
  memcpy(systemRaidHeat, world.systemRaidHeat, sizeof(systemRaidHeat));
  memcpy(systemNeed, world.systemNeed, sizeof(systemNeed));
  memcpy(systemFactionPower, world.systemFactionPower, sizeof(systemFactionPower));
  memcpy(systemThargoidBlight, world.systemThargoidBlight, sizeof(systemThargoidBlight));
  memcpy(systemFaction, world.systemFaction, sizeof(systemFaction));
  currentBarMission = world.barMission;
  surfaceOp = world.surfaceOp;
  uiMode = (UiMode)clampi(world.uiMode, UI_FLIGHT, UI_DOCK_INTERIOR);
  stationStep = (StationServiceStep)clampi(world.stationStep, SERVICE_IDLE, SERVICE_LAUNCH);
  dockPhase = (DockPhase)clampi(world.dockPhase, DOCK_NONE, DOCK_TUNNEL);
  currentGoal = (PilotGoal)clampi(world.currentGoal, GOAL_NONE, GOAL_UPGRADE);
  jumpTargetSystem = world.jumpTargetSystem;
  pendingJumpSystem = world.pendingJumpSystem;
  jumpInProgress = world.jumpInProgress;
  inWitchSpace = world.inWitchSpace;
  stationFlowActive = world.stationFlowActive;
  patrolTimer = world.patrolTimer;
  launchTimer = world.launchTimer;
  returnTimer = world.returnTimer;
  dockTimer = world.dockTimer;
  witchTimer = world.witchTimer;
  stationStepTimer = world.stationStepTimer;
  jumpTimer = world.jumpTimer;
  tunnelTimer = world.tunnelTimer;
  launchLockout = world.launchLockout;
  shipYaw = world.shipYaw;
  shipPitch = world.shipPitch;
  shipRoll = world.shipRoll;
  shipSpeed = world.shipSpeed;
  shipEnergy = world.shipEnergy;
  shipShield = world.shipShield;
  laserHeat = world.laserHeat;
  stationPos = world.stationPos;
  planetPos = world.planetPos;
  sunPos = world.sunPos;
  stationSpin = world.stationSpin;
  statusLine = String(world.statusText);
  return true;
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
  saveWorldState();
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
    systemBiome[i] = (n1 + n3 + galaxyNumber) & 0x03;
    systemPhenomenon[i] = ((n4 & 0x1F) == 0) ? PHENOM_QUASAR : (((n3 & 0x0F) == 0) ? PHENOM_PULSAR : PHENOM_NORMAL);
    sys.techLevel = 1 + ((sys.economy ^ sys.government) & 7) + (n2 & 3);
    if (systemPhenomenon[i] == PHENOM_PULSAR) sys.techLevel += 1;
    if (systemPhenomenon[i] == PHENOM_QUASAR) sys.techLevel += 2;
    if (sys.techLevel > 15) sys.techLevel = 15;
    sys.danger = (7 - sys.government + (sys.economy < 3 ? 2 : 0)) & 7;
    if (systemBiome[i] == BIOME_VOLCANIC) sys.danger = clampi(sys.danger + 1, 0, 7);
    if (systemPhenomenon[i] != PHENOM_NORMAL) sys.danger = clampi(sys.danger + 1, 0, 7);
    systemTradeFlux[i] = clampi(3 + sys.techLevel / 4, 0, 9);
    systemRaidHeat[i] = clampi(sys.danger, 0, 9);
    systemNeed[i] = clampi((7 - sys.economy) + (sys.government < 3 ? 1 : 0), 0, 9);
    if (systemBiome[i] == BIOME_JUNGLE) systemNeed[i] = clampi((int)systemNeed[i] - 1, 0, 9);
    if (systemBiome[i] == BIOME_VOLCANIC) systemTradeFlux[i] = clampi((int)systemTradeFlux[i] + 1, 0, 9);
    if (systemPhenomenon[i] == PHENOM_PULSAR) systemRaidHeat[i] = clampi((int)systemRaidHeat[i] + 1, 0, 9);
    if (systemPhenomenon[i] == PHENOM_QUASAR) systemNeed[i] = clampi((int)systemNeed[i] + 1, 0, 9);
    systemFaction[i] = pickCivilFactionForSystem(i);
    systemFactionPower[i] = clampi(4 + sys.techLevel / 5 - sys.danger / 2, 1, 9);
    systemThargoidBlight[i] = (systemPhenomenon[i] == PHENOM_QUASAR && sys.danger > 4) ? 2 : ((n4 & 0x0300) == 0x0300 ? 1 : 0);

    int a = (n1 >> 8) % 10;
    int b = (n2 >> 8) % 10;
    int c = (n3 >> 8) % 10;

    snprintf(sys.name, sizeof(sys.name), "%s%s%s",
             sA[a], sB[b],
             (c % 3 == 0) ? "on" : ((c % 3 == 1) ? "ar" : ""));
  }
  generateSystemPrices();
}

void galacticJump() {
  if (!hasEquipment(EQ_GAL_HYPERDRIVE)) return;
  pilot.galaxyIndex = (pilot.galaxyIndex + 1) & 7;
  generateGalaxy(pilot.galaxyIndex);
  if (!currentBarMission.active) generateBarMission();
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
  uint8_t source = pilot.currentSystem;

  pilot.fuel -= cost;
  pilot.currentSystem = dst;
  pilot.docked = true;
  seedRouteConductivity(source, dst, 1 + (pilot.cargoCount > 0 ? 1 : 0));
  systemTradeFlux[dst] = clampi((int)systemTradeFlux[dst] + 1, 0, 9);
  systemFactionPower[dst] = clampi((int)systemFactionPower[dst] + 1, 0, 9);
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

  autoUpgradeShipFrame();
  autoUpgradeDrills();
  autoUpgradeMechBay();
}

// -----------------------------------------------------
// SENSOR
// -----------------------------------------------------
void updateSensorInfluence() {
  float gx = 0, gy = 0, gz = 0;
  M5.Imu.getGyroData(&gx, &gy, &gz);

  float remoteYaw = 0.0f;
  float remoteAggro = 0.0f;
  float remoteCaution = 0.0f;

  if (blindEyeState.online && millis() - blindEyeState.lastSeenMs < 9000) {
    remoteYaw += clampf(blindEyeState.gyroZ * 0.010f, -0.12f, 0.12f);
    remoteAggro += clampf(blindEyeState.activity * 0.010f + blindEyeState.shock * 0.008f, 0.0f, 0.22f);
    remoteCaution += clampf(blindEyeState.magNorm * 0.0012f, 0.0f, 0.18f);
  }
  if (beaconState.online && millis() - beaconState.lastSeenMs < 9000) {
    remoteAggro += clampf(beaconState.sync * 0.08f + beaconState.m2r * 0.05f, 0.0f, 0.16f);
    remoteCaution += clampf(beaconState.entropy * 0.012f, 0.0f, 0.16f);
  }

  float micCaution = clampf(localMicMood * 0.14f, 0.0f, 0.16f);
  float micAggro = clampf(localMicMood * 0.06f, 0.0f, 0.10f);
  float micExplore = clampf(localMicMood * 0.10f, 0.0f, 0.12f);

  sensorBiasYaw = clampf(gz * 0.015f + remoteYaw, -0.25f, 0.25f);
  sensorBiasAggro = clampf(fabsf(gx) * 0.02f + remoteAggro + micAggro, 0.0f, 0.38f);
  sensorBiasCaution = clampf(fabsf(gy) * 0.02f + remoteCaution + micCaution, 0.0f, 0.38f);

  float exploreBias = clampf(remoteExplorationBias + micExplore, 0.0f, 0.46f);
  janus.aggression = clampf(0.35f + sensorBiasAggro + exploreBias * 0.15f, 0.2f, 0.95f);
  janus.caution = clampf(0.35f + sensorBiasCaution, 0.2f, 0.98f);
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
      if (storeCargoUnit(canisters[i].item, 0)) {
        canisters[i].alive = false;
        statusLine = "SALVAGE";
        playToneSafe(700, 35);
      }
    }
    if (canisters[i].ttl == 0 || canisters[i].pos.z < 3 || canisters[i].pos.z > 260) canisters[i].alive = false;
  }
}

void beginVisibleJump(uint8_t dst) {
  jumpTargetSystem = dst;
  jumpTimer = 60;
  jumpInProgress = true;
  uiMode = UI_MAP;
  statusLine = "CHART LOCK";
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
  uint8_t source = pilot.currentSystem;

  pilot.fuel -= cost;
  pilot.currentSystem = jumpTargetSystem;
  pilot.docked = true;
  seedRouteConductivity(source, jumpTargetSystem, 1 + (pilot.cargoCount > 0 ? 1 : 0) + (currentBarMission.active ? 1 : 0));
  systemTradeFlux[jumpTargetSystem] = clampi((int)systemTradeFlux[jumpTargetSystem] + 1, 0, 9);
  systemFactionPower[jumpTargetSystem] = clampi((int)systemFactionPower[jumpTargetSystem] + 1, 0, 9);
  jumpInProgress = false;
  uiMode = UI_MARKET;
  statusLine = String("ARRIVED ") + galaxy[jumpTargetSystem].name;
  mapBrowseTimer = 22;
  playJumpSound();

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
  stationPos.x = frandRange(-28.0f, 28.0f);
  stationPos.y = frandRange(-16.0f, 16.0f);
  stationPos.z = 820.0f;
  shipSpeed = clampf(shipSpeed, 0.12f, 0.18f);
  statusLine = "APP 800M";
}

// -----------------------------------------------------
// WORLD OBJECTS
// -----------------------------------------------------
void resetStars() {
  for (int i = 0; i < STAR_COUNT; ++i) {
    stars[i].x = frandRange(-90, 90);
    stars[i].y = frandRange(-90, 90);
    stars[i].z = frandRange(20, 200);
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
      traffic[i].role = (systemRaidHeat[pilot.currentSystem] > 6 && random(0, 100) < 30) ? ROLE_POLICE : ROLE_TRADER;
      int rr = random(0, 100) + systemTradeFlux[pilot.currentSystem] * 2 - systemRaidHeat[pilot.currentSystem];
      if (rr < 25) traffic[i].type = SHIP_ADDER;
      else if (rr < 45) traffic[i].type = SHIP_TRANSPORT;
      else if (rr < 70) traffic[i].type = SHIP_COBRA;
      else if (rr < 85) traffic[i].type = SHIP_PYTHON;
      else traffic[i].type = SHIP_ANACONDA;
      traffic[i].hp = 1.0f;
      traffic[i].pos = {frandRange(-70, 70), frandRange(-35, 35), 200.0f};
      traffic[i].vel = {frandRange(-0.05f, 0.05f), frandRange(-0.03f, 0.03f), frandRange(-0.12f - systemTradeFlux[pilot.currentSystem] * 0.004f, -0.03f)};
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
      enemies[i].pos = {frandRange(-45, 45), frandRange(-20, 20), frandRange(110, 150)};
      enemies[i].vel = {frandRange(-0.09f, 0.09f), frandRange(-0.05f, 0.05f), frandRange(-0.18f, -0.06f)};
    }
  }
  statusLine = "POLICE";
  playAlertSoundProc();
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
    enemies[i].pos = {frandRange(-40, 40), frandRange(-20, 20), frandRange(110, 150)};
    enemies[i].vel = {frandRange(-0.05f, 0.05f), frandRange(-0.05f, 0.05f), frandRange(-0.18f, -0.06f)};
  }
  statusLine = "THARGOID";
  playAlertSoundProc();
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
  dockCinemaPhase = 0;
  dockPhase = DOCK_NONE;

  shipSpeed = 0.05f;
  shipYaw = 0.0f;
  shipPitch = 0.0f;
  shipRoll = 0.0f;
  laserHeat = 0.0f;

  resetStars();
  resetEnemies();
  resetDebris();
  resetCanisters();
  initTraffic();

  stationPos = {0.0f, 0.0f, 118.0f};
  planetPos = {10.0f, -10.0f, 208.0f};
  sunPos = {-78.0f, -30.0f, 520.0f};

  launchTimer = 84;
  launchLockout = 240;
  patrolTimer = 0;
  returnTimer = 0;
  dockTimer = 0;

  janus.mode = MODE_LAUNCH;
  statusLine = "LAUNCH TUBE";
}

// -----------------------------------------------------
// DOCK FLOW
// -----------------------------------------------------
void beginStationFlow() {
  stationFlowActive = true;
  stationStep = SERVICE_DOCK_INTERIOR;
  stationStepTimer = 24;
  uiMode = UI_DOCK_INTERIOR;
  dockCinemaPhase = 1;
  alreadyJumpedThisDock = false;
  statusLine = "HANGAR";
}

void updateStationFlow() {
  if (!stationFlowActive || !pilot.docked) return;

  if (stationStepTimer > 0) {
    stationStepTimer--;
    return;
  }

  switch (stationStep) {
    case SERVICE_DOCK_INTERIOR:
      if (dockCinemaPhase == 2) {
        stationFlowActive = false;
        stationStep = SERVICE_IDLE;
        dockCinemaPhase = 0;
        launchScene();
        return;
      }
      dockCinemaPhase = 0;
      uiMode = UI_STATUS;
      statusLine = "DOCKED";
      stationStep = SERVICE_MARKET;
      stationStepTimer = 18;
      break;

    case SERVICE_MARKET:
      sellAllCargoHere();
      refuelAtStation();
      uiMode = UI_MARKET;
      statusLine = "MARKET";
      stationStep = SERVICE_EQUIP;
      stationStepTimer = 22;
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
      stationStepTimer = 22;
      break;

    case SERVICE_STATUS:
      updateMissionsOnDock();
      decayLegalStatusInDock();
      uiMode = (pendingJumpSystem != 255) ? UI_MAP : UI_STATUS;
      statusLine = (pendingJumpSystem != 255) ? "ROUTE" : "STATUS";
      stationStep = SERVICE_BAR;
      stationStepTimer = 18;
      break;

    case SERVICE_BAR:
      processBarMissionOnDock();
      uiMode = UI_BAR;
      statusLine = "BAR";
      stationStep = SERVICE_LAUNCH;
      stationStepTimer = 18;
      break;

    case SERVICE_LAUNCH:
      dockCinemaPhase = 2;
      stationStep = SERVICE_DOCK_INTERIOR;
      stationStepTimer = 18;
      uiMode = UI_DOCK_INTERIOR;
      statusLine = "CLEARANCE";
      break;

    default:
      stationFlowActive = false;
      stationStep = SERVICE_IDLE;
      dockCinemaPhase = 0;
      uiMode = UI_STATUS;
      break;
  }
}

// -----------------------------------------------------
// PROJECTION / DRAW
// -----------------------------------------------------
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
  float camShakeX = 0.0f;
  float camShakeY = 0.0f;
  if (hitShakeTimer > 0) {
    camShakeX += ((millis() >> 2) & 1 ? 1.5f : -1.5f);
    camShakeY += ((millis() >> 1) & 1 ? -1.0f : 1.0f);
  } else if (dockPhase != DOCK_NONE || janus.mode == MODE_LAUNCH || currentGoal == GOAL_ESCAPE) {
    float shakeMul = heavyPlanetView ? 0.18f : 0.40f;
    camShakeX = sinf(millis() * 0.00075f) * shakeMul;
    camShakeY = cosf(millis() * 0.00052f) * shakeMul;
  }
  sx = (int)(SCREEN_W * 0.5f + x3 * scale + camShakeX);
  sy = (int)(SCREEN_H * 0.42f + y3 * scale + camShakeY);

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
  uint16_t c = colorDim(UI_AMBER_BRIGHT, heavyPlanetView ? 0.58f : 0.78f);

  M5.Display.drawFastHLine(cx - 10, cy, 6, c);
  M5.Display.drawFastHLine(cx + 5, cy, 6, c);
  M5.Display.drawFastVLine(cx, cy - 8, 5, c);
  M5.Display.drawFastVLine(cx, cy + 4, 5, c);

  M5.Display.drawPixel(cx, cy, TFT_WHITE);
  M5.Display.drawPixel(cx, cy + 1, colorDim(TFT_WHITE, 0.35f));
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

    if (px > cx - rx + 1 && px < cx + rx - 1 && py > cy - ry + 1 && py < cy + ry - 1) {
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

    if (px > cx - rx + 1 && px < cx + rx - 1 && py > cy - ry + 1 && py < cy + ry - 1) {
      M5.Display.drawPixel(px, py, TFT_GREEN);
    }
  }

  M5.Display.drawPixel(cx, cy, TFT_GREEN);
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
  M5.Display.setTextSize(1);

  if (millis() - lastHudCompose > 160) {
    hudCacheLeft1 = String("G") + String(pilot.galaxyIndex + 1) + " " + String(galaxy[pilot.currentSystem].name);

    String modeName = "DCK";
    if (janus.mode == MODE_LAUNCH) modeName = "LCH";
    else if (janus.mode == MODE_PATROL) modeName = "CRS";
    else if (janus.mode == MODE_COMBAT) modeName = "CMB";
    else if (janus.mode == MODE_RETURN) modeName = "RTN";

    hudCacheLeft2 = String(getLegalName(pilot.legalStatus)) + " " + modeName;
    hudCacheLeft3 = String(getRankName(pilot.kills));
    hudCacheLeft4 = String("K:") + String(pilot.kills);

    hudCacheRight1 = "CR";
    hudCacheRight2 = String(pilot.credits);
    hudCacheRight3 = "FU";
    hudCacheRight4 = String(pilot.fuel);

    lastHudCompose = millis();
  }

  auto drawTextSimple = [](int x, int y, const String& s, uint16_t color) {
    M5.Display.setTextColor(color, TFT_BLACK);
    M5.Display.setCursor(x, y);
    M5.Display.print(s);
  };

  auto drawRightSimple = [&](int y, const String& s, uint16_t color) {
    int16_t w = M5.Display.textWidth(s.c_str());
    int x = SCREEN_W - 2 - w;
    if (x < 0) x = 0;
    drawTextSimple(x, y, s, color);
  };

  uint16_t hudCol = colorDim(UI_AMBER_BRIGHT, survivalFocusMode ? 1.00f : (heavyPlanetView ? 0.70f : 0.90f));
  uint16_t hudSoft = colorDim(UI_SOFT, survivalFocusMode ? 0.88f : (heavyPlanetView ? 0.55f : 0.74f));
  uint16_t statusCol = pilot.missileIncoming ? TFT_RED : (survivalFocusMode ? colorDim(UI_AMBER_BRIGHT, 1.00f) : colorDim(UI_AMBER_BRIGHT, 0.90f));

  drawTextSimple(2, 2,  hudCacheLeft1, hudCol);
  drawTextSimple(2, 10, hudCacheLeft2, hudSoft);
  drawTextSimple(2, 18, hudCacheLeft3, hudSoft);
  drawTextSimple(2, 26, hudCacheLeft4, hudSoft);

  drawRightSimple(2,  hudCacheRight1, hudSoft);
  drawRightSimple(10, hudCacheRight2, hudCol);
  drawRightSimple(18, hudCacheRight3, hudSoft);
  drawRightSimple(26, hudCacheRight4, hudCol);

  drawClassicEliteBottomHud();

  M5.Display.setTextColor(statusCol, TFT_BLACK);
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
    if (pendingJumpSystem != 255) M5.Display.setCursor(2, 100), M5.Display.printf("TGT  %s", galaxy[pendingJumpSystem].name);
    else M5.Display.setCursor(2, 100), M5.Display.printf("LAW  %s", getLegalName(pilot.legalStatus));
    M5.Display.setCursor(2, 108); M5.Display.printf("RANK %s", getRankName(pilot.kills));
    M5.Display.setCursor(2, 116); M5.Display.printf("SAFE %.1f M:%s/%s MIC %.2f", learnState.systemDeathPenalty[pilot.currentSystem], blindEyeState.online ? "E" : "-", beaconState.online ? "B" : "-", localMicRms);
  }
}


void drawBarScreen() {
  M5.Display.fillScreen(TFT_BLACK);
  for (int y = 0; y < SCREEN_H; ++y) {
    uint16_t c = mix565(TFT_BLACK, 0x0841, y / 180.0f);
    M5.Display.drawFastHLine(0, y, SCREEN_W, c);
  }

  M5.Display.setTextColor(UI_AMBER_BRIGHT, TFT_BLACK);
  M5.Display.setCursor(2, 2); M5.Display.print("ORBITAL BAR");

  fillDitherRect(4, 18, 36, 46, colorDim(UI_SOFT, 0.20f), colorDim(UI_DIM, 0.08f), 0);
  fillDitherRect(46, 18, 36, 46, colorDim(UI_SOFT, 0.17f), colorDim(UI_DIM, 0.07f), 0);
  fillDitherRect(88, 18, 36, 46, colorDim(UI_SOFT, 0.14f), colorDim(UI_DIM, 0.06f), 0);

  M5.Display.drawRoundRect(4,18,36,46,3,colorDim(UI_AMBER_BRIGHT,0.65f));
  M5.Display.drawRoundRect(46,18,36,46,3,colorDim(UI_SOFT,0.55f));
  M5.Display.drawRoundRect(88,18,36,46,3,colorDim(UI_SOFT,0.55f));

  M5.Display.drawCircle(22, 33, 8, TFT_WHITE);
  M5.Display.drawCircle(64, 33, 8, TFT_WHITE);
  M5.Display.drawCircle(106,33, 8, TFT_WHITE);
  M5.Display.drawFastHLine(15, 45, 14, UI_AMBER_BRIGHT);
  M5.Display.drawFastHLine(57, 45, 14, UI_SOFT);
  M5.Display.drawFastHLine(99, 45, 14, UI_SOFT);

  M5.Display.setCursor(8, 56); M5.Display.print("FIXER");
  M5.Display.setCursor(50, 56); M5.Display.print("MINER");
  M5.Display.setCursor(92, 56); M5.Display.print("SCOUT");
  M5.Display.setCursor(2, 68); M5.Display.printf("ROLE %s", getCareerName(pilot.careerTrack));
  M5.Display.setCursor(2, 78); M5.Display.printf("FIT %s %s", getShipBuildName(), getShipDoctrineName());

  if (!currentBarMission.active) {
    M5.Display.setCursor(2, 94); M5.Display.printf("NEXT %s", getCareerObjective());
    M5.Display.setCursor(2, 106); M5.Display.print("NO CONTRACT");
    return;
  }

  M5.Display.setCursor(2, 88); M5.Display.printf("NEXT %s", getCareerObjective());
  M5.Display.setCursor(2, 96); M5.Display.print(currentBarMission.title);
  M5.Display.setCursor(2, 104); M5.Display.printf("TARGET %s", galaxy[currentBarMission.targetSystem].name);
  if (currentBarMission.type == 3) {
    M5.Display.setCursor(2, 114); M5.Display.printf("M%u CAP%u PAY%d", currentBarMission.mechCount, currentShipMechCapacity(), currentBarMission.reward);
  } else if (currentBarMission.type == 4) {
    M5.Display.setCursor(2, 114); M5.Display.printf("%s W%u P%d", getBiomeName(currentBarMission.biome), currentBarMission.surfaceWaves, currentBarMission.reward);
  } else {
    M5.Display.setCursor(2, 114); M5.Display.printf("PAY %d", currentBarMission.reward);
  }
}
void drawMissionScreen() {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(UI_AMBER_BRIGHT, TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(2, 2);
  M5.Display.print("MISSION");

  if (surfaceOp.active) {
    M5.Display.setCursor(2, 16); M5.Display.printf("GROUND %s", getSurfaceObjectiveName(surfaceOp.objective));
    M5.Display.setCursor(2, 28); M5.Display.printf("BIOME %s", getBiomeName(surfaceOp.biome));
    M5.Display.setCursor(2, 40); M5.Display.printf("WAVES %u", surfaceOp.wavesRemaining);
    M5.Display.setCursor(2, 52); M5.Display.printf("MECH ARM %u", surfaceOp.mechArmor);
    M5.Display.setCursor(2, 64); M5.Display.printf("HEAT %u", surfaceOp.mechHeat);
    M5.Display.setCursor(2, 76); M5.Display.printf("PAY %u SALV %u", surfaceOp.payout, surfaceOp.salvageRoll);
    M5.Display.setCursor(2, 88); M5.Display.printf("PARTS %u/%u/%u/%u", pilot.mechAlloys, pilot.mechServos, pilot.mechCores, pilot.mechWeaponParts);
    M5.Display.setCursor(2, 100); M5.Display.printf("FRAME %s", getShipFrameName(pilot.shipFrame));
    M5.Display.setCursor(2, 112); M5.Display.printf("MECH LVL %u WPN %u", pilot.mechLevel, pilot.mechWeaponTier);
    return;
  }

  if (pilot.missionStage == MISSION_CONSTRICTOR) {
    M5.Display.setCursor(2, 16); M5.Display.print("HUNT CONSTRICTOR");
    M5.Display.setCursor(2, 28); M5.Display.print("FIND & DESTROY");
    M5.Display.setCursor(2, 40); M5.Display.printf("STATE:%d", pilot.missionProgress);
  } else if (pilot.missionStage == MISSION_THARGOID_DOCS) {
    M5.Display.setCursor(2, 16); M5.Display.print("THARGOID DOCS");
    M5.Display.setCursor(2, 28); M5.Display.print("EXPECT AMBUSH");
  } else if (pilot.mechOwned) {
    M5.Display.setCursor(2, 16); M5.Display.printf("CAREER %s", getCareerName(pilot.careerTrack));
    M5.Display.setCursor(2, 28); M5.Display.printf("NEXT %s", getCareerObjective());
    M5.Display.setCursor(2, 40); M5.Display.printf("SHIP %s %ut", getShipBuildName(), getShipTonnage());
    M5.Display.setCursor(2, 52); M5.Display.printf("CAP %u DRILL T%u", currentShipMechCapacity(), pilot.drillTier);
    M5.Display.setCursor(2, 64); M5.Display.printf("DOC %s OPS %u", getShipDoctrineName(), pilot.surfaceOpsCompleted);
    M5.Display.setCursor(2, 76); M5.Display.printf("MECH LVL %u ARM %u", pilot.mechLevel, pilot.mechArmorMax);
    M5.Display.setCursor(2, 88); M5.Display.printf("PARTS %u/%u/%u/%u", pilot.mechAlloys, pilot.mechServos, pilot.mechCores, pilot.mechWeaponParts);
  } else {
    M5.Display.setCursor(2, 16); M5.Display.printf("CAREER %s", getCareerName(pilot.careerTrack));
    M5.Display.setCursor(2, 28); M5.Display.printf("NEXT %s", getCareerObjective());
    M5.Display.setCursor(2, 40); M5.Display.printf("SHIP %s %ut", getShipBuildName(), getShipTonnage());
    M5.Display.setCursor(2, 52); M5.Display.printf("DOC %s DRILL T%u", getShipDoctrineName(), pilot.drillTier);
    M5.Display.setCursor(2, 64); M5.Display.printf("HAUL %u OPS %u", pilot.mechHaulsCompleted, pilot.surfaceOpsCompleted);
    M5.Display.setCursor(2, 76); M5.Display.print("BAR GATES THE MECH LOOP");
    M5.Display.setCursor(2, 88); M5.Display.print("TRADER->HAULER->MERC");
  }
}

void drawWitchSpaceScreen() {
  heavyPlanetView = false;
  survivalFocusMode = false;
  M5.Display.fillScreen(TFT_BLACK);
  drawFakeModernBackdrop();
  M5.Display.fillRect(0,0,SCREEN_W,SCREEN_H,colorDim(TFT_BLACK,0.82f));
  uint8_t warpSeed = (uint8_t)((millis() >> 4) & 0xFF);
  for (int i = 0; i < STAR_COUNT; ++i) {
    int x = procHash8(i, warpSeed, 5) % SCREEN_W;
    int y = procHash8(i, warpSeed, 17) % SCREEN_H;
    int len = 3 + (procHash8(i, warpSeed, 29) & 7);
    uint16_t c = ((i + warpSeed) & 3) == 0 ? TFT_WHITE : TFT_MAGENTA;
    M5.Display.drawLine(x, y, x, y + len, c);
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
  M5.Display.setCursor(2, 104); M5.Display.printf("BLD %s %s", getShipBuildName(), getShipDoctrineName());
  M5.Display.setCursor(2, 114); M5.Display.printf("%ut %s B%d", getShipTonnage(), getFactionName(systemFaction[pilot.currentSystem]), systemThargoidBlight[pilot.currentSystem]);
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
  M5.Display.setCursor(2, 96);  M5.Display.printf("SECTOR %s", getLocalSectorStateName());
  M5.Display.setCursor(78, 96); M5.Display.printf("%s", getFactionName(systemFaction[pilot.currentSystem]));
  M5.Display.setCursor(2, 106); M5.Display.printf("BIO %s %s", getBiomeName(systemBiome[pilot.currentSystem]), getPhenomenonName(systemPhenomenon[pilot.currentSystem]));
  M5.Display.setCursor(2, 116); M5.Display.printf("F%d R%d N%d B%d", systemTradeFlux[pilot.currentSystem], systemRaidHeat[pilot.currentSystem], systemNeed[pilot.currentSystem], systemThargoidBlight[pilot.currentSystem]);
  M5.Display.setCursor(72, 118); M5.Display.printf("%d cr", pilot.credits);
}

void drawEquipScreen() {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(UI_AMBER_BRIGHT, TFT_BLACK);
  M5.Display.setTextSize(1);

  M5.Display.setCursor(2, 2);  M5.Display.print("EQUIPMENT");
  M5.Display.setCursor(2, 16); M5.Display.printf("SCOOP  %s", hasEquipment(EQ_FUEL_SCOOP) ? "FIT" : "NONE");
  M5.Display.setCursor(2, 28); M5.Display.printf("GALDRV %s", hasEquipment(EQ_GAL_HYPERDRIVE) ? "FIT" : "NONE");
  M5.Display.setCursor(2, 40); M5.Display.printf("CARGO  %s", hasEquipment(EQ_CARGO_EXPANSION) ? "FIT" : "STD");
  M5.Display.setCursor(2, 52); M5.Display.printf("BUILD  %s", getShipBuildName());
  M5.Display.setCursor(2, 64); M5.Display.printf("DOC    %s", getShipDoctrineName());
  M5.Display.setCursor(2, 76); M5.Display.printf("DRILL  T%u", pilot.drillTier);
  M5.Display.setCursor(2, 88); M5.Display.printf("LASER  %s", hasEquipment(EQ_MILITARY_LASER) ? "MIL" : "PULSE");
  M5.Display.setCursor(2, 100); M5.Display.printf("DOCK   %s", hasEquipment(EQ_DOCK_COMPUTER) ? "FIT" : "NONE");
  M5.Display.setCursor(2, 118); M5.Display.printf("%d cr", pilot.credits);
}

void drawDockedScreen() {
  heavyPlanetView = false;
  survivalFocusMode = false;
  M5.Display.fillScreen(TFT_BLACK);
  drawFakeModernBackdrop();
  drawCinematicStars();
  stationSpin += 0.010f;
  stationPos = {sinf(stationSpin) * 3.0f, cosf(stationSpin * 0.7f) * 2.0f, 132.0f};
  planetPos = {18.0f, -10.0f, 260.0f};
  drawDominantPlanet();
  drawSilhouetteStation();
  drawCrosshair();
  drawEliteHUD();
}


void drawFakeModernBackdrop() {
  uint8_t skySeed = (pilot.currentSystem * 17) ^ (pilot.galaxyIndex * 29);
  int drift = (int)(millis() >> 6);
  uint16_t backTone = 0x1062;
  uint16_t ribbonBaseA = 0x39C7;
  uint16_t ribbonBaseB = 0x7C7F;
  if (systemPhenomenon[pilot.currentSystem] == PHENOM_PULSAR) {
    backTone = 0x084C;
    ribbonBaseA = 0x1C9F;
    ribbonBaseB = 0x7DFF;
  } else if (systemPhenomenon[pilot.currentSystem] == PHENOM_QUASAR) {
    backTone = 0x180C;
    ribbonBaseA = 0xA0B7;
    ribbonBaseB = 0xF81F;
  }

  // Unified scene-first background: one cheap gradient + coded haze ribbons.
  for (int y = 0; y < SCREEN_H; ++y) {
    float t = y / (float)SCREEN_H;
    float haze = procCell01((y >> 2) + drift, skySeed, skySeed + 5) * (survivalFocusMode ? 0.02f : (heavyPlanetView ? 0.04f : 0.08f));
    uint16_t c = mix565(TFT_BLACK, backTone, t * 0.24f + haze);
    M5.Display.drawFastHLine(0, y, SCREEN_W, c);
  }

  // kkrieger-style sizecoded nebula: a few moving bands with deterministic noise.
  int bandCount = survivalFocusMode ? 3 : 6;
  for (int i = 0; i < bandCount; ++i) {
    int yy = 10 + i * 15 + ((drift + i * 7) & 3);
    int off = (int)(sinf((millis() * 0.00045f) + i * 0.9f) * 8.0f);
    int lenA = 46 + (procHash8(i, skySeed, 3) & 15);
    int lenB = 28 + (procHash8(i, skySeed, 9) & 15);
    uint16_t ribbonA = colorDim(mix565(ribbonBaseA, ribbonBaseB, procCell01(i, skySeed, 1)), 0.10f + procCell01(i, drift, 2) * 0.08f);
    uint16_t ribbonB = colorDim(mix565(colorDim(ribbonBaseA, 0.65f), colorDim(ribbonBaseB, 0.78f), procCell01(i + 9, skySeed, 4)), 0.06f + procCell01(i + 5, drift, 6) * 0.06f);
    M5.Display.drawFastHLine(8 + off, yy, lenA, ribbonA);
    M5.Display.drawFastHLine(54 - off / 2, yy + 2, lenB, ribbonB);
  }
}

void drawCinematicStars() {
  uint8_t starSeed = (pilot.currentSystem * 11) ^ (pilot.galaxyIndex * 23);
  for (int i = 0; i < STAR_COUNT; ++i) {
    int sx, sy; float d;
    if (!projectPoint(stars[i], sx, sy, d)) continue;
    float twinkle = 0.72f + procCell01(i, (int)d, starSeed) * 0.28f;
    uint16_t base = ((i + starSeed) & 3) == 0 ? UI_SOFT : TFT_WHITE;
    uint16_t c = colorDim(base, clampf((1.0f - fabsf(d) / 300.0f) * twinkle, 0.15f, 0.82f));
    M5.Display.drawPixel(sx, sy, c);
    if (!heavyPlanetView && !survivalFocusMode && ((i + frameParity) & 7) == 0) {
      M5.Display.drawPixel(sx + 1, sy, colorDim(c, 0.4f));
    }
  }
}

void drawParallaxDust() {
  if (heavyPlanetView || survivalFocusMode) return;
  for (int i = 0; i < 10; ++i) {
    int x = (i * 23 + (int)(millis() / (8 + i))) % SCREEN_W;
    int y = (i * 11 + (int)(millis() / (18 + i))) % 96;
    int len = 1 + (int)(shipSpeed * 8.0f);
    M5.Display.drawFastVLine(x, y, len, colorDim(UI_SOFT, 0.22f));
  }
}

void drawSilhouetteShip(const SpaceShip &s, uint16_t core, uint16_t glow) {
  int sx, sy; float d;
  if (!projectPoint(s.pos, sx, sy, d)) return;

  bool heavyTransport = (s.type == SHIP_TRANSPORT || s.type == SHIP_PYTHON || s.type == SHIP_ANACONDA || s.type == SHIP_CONSTRICTOR);
  bool frigateClass = (s.type == SHIP_PYTHON || s.type == SHIP_ANACONDA || s.type == SHIP_CONSTRICTOR);
  bool fastHunter = (s.type == SHIP_MAMBA || s.type == SHIP_ASP || s.type == SHIP_SIDEWINDER || s.type == SHIP_ADDER || s.type == SHIP_VIPER);
  bool mediumHull = (s.type == SHIP_COBRA || s.type == SHIP_TRANSPORT || s.type == SHIP_ASP || s.type == SHIP_CONSTRICTOR);
  float classBoost = frigateClass ? 1.45f : (heavyTransport ? 1.22f : (mediumHull ? 1.10f : 1.0f));
  int scale = clampi((int)((165.0f / d) * classBoost), 4, frigateClass ? 26 : 20);
  int hullW = scale * 2 + (heavyTransport ? 8 : 0) + (frigateClass ? 6 : 0);
  int hullH = scale + 3 + ((s.role == ROLE_THARGOID || s.role == ROLE_BOSS) ? 2 : 0) + (heavyTransport ? 2 : 0);
  int phase = (millis()/110 + s.type * 7) & 3;
  int panelSeed = (s.type * 11) ^ (s.role * 17);
  int spineH = clampi(hullH / 2 + 1, 2, 16);
  int noseLen = scale + (frigateClass ? scale / 2 : 0);

  // soft glow
  if (!heavyPlanetView && !survivalFocusMode) {
    for (int i = 0; i < 2; ++i) {
      M5.Display.drawRoundRect(sx - hullW/2 - i, sy - hullH/2 - i, hullW + i*2, hullH + i*2, 2, colorDim(glow, 0.35f - i*0.10f));
    }
  }

  // main body
  fillDitherRect(sx - hullW/2, sy - hullH/2, hullW, hullH, core, colorDim(core, 0.38f), phase);
  M5.Display.drawRoundRect(sx - hullW/2, sy - hullH/2, hullW, hullH, 2, colorDim(TFT_WHITE, 0.75f));

  // pseudo texture / panels
  int panelStep = heavyPlanetView ? 4 : 3;
  for (int y = sy - hullH/2 + 2; y < sy + hullH/2 - 1; y += panelStep) {
    float panelLight = 0.10f + procCell01(y, panelSeed, phase) * 0.10f;
    M5.Display.drawFastHLine(sx - hullW/2 + 2, y, hullW - 4, colorDim(TFT_WHITE, panelLight));
  }
  for (int x = sx - hullW/2 + 3; x < sx + hullW/2 - 2; x += 4 + ((panelSeed + x) & 1)) {
    M5.Display.drawFastVLine(x, sy - hullH/2 + 2, hullH - 4, colorDim(TFT_BLACK, 0.35f));
  }
  if (hullW > 10) {
    int canopyX = sx + hullW / 4;
    M5.Display.drawFastVLine(canopyX, sy - 1, 3, colorDim(TFT_WHITE, 0.55f));
  }

  M5.Display.drawFastVLine(sx, sy - spineH, spineH * 2 + 1, colorDim(TFT_BLACK, 0.24f));
  M5.Display.drawFastHLine(sx - hullW/3, sy, hullW / 2, colorDim(TFT_WHITE, 0.12f));

  if (heavyTransport) {
    int podW = clampi(scale + (frigateClass ? 3 : 0), 4, 20);
    int podH = clampi(scale / 2 + 2 + (frigateClass ? 1 : 0), 3, 10);
    fillDitherRect(sx - hullW/2 + 1, sy - hullH/2 - podH, podW, podH, colorDim(core, 0.75f), colorDim(core, 0.30f), phase);
    fillDitherRect(sx - hullW/2 + 1, sy + hullH/2, podW, podH, colorDim(core, 0.75f), colorDim(core, 0.30f), phase + 1);
    M5.Display.drawRect(sx - hullW/2 + 1, sy - hullH/2 - podH, podW, podH, colorDim(TFT_WHITE, 0.45f));
    M5.Display.drawRect(sx - hullW/2 + 1, sy + hullH/2, podW, podH, colorDim(TFT_WHITE, 0.45f));
    if (frigateClass) {
      int deckW = hullW / 2;
      int deckX = sx - deckW / 2;
      int deckY = sy - hullH / 2 - podH - 2;
      fillDitherRect(deckX, deckY, deckW, podH + 1, colorDim(core, 0.62f), colorDim(core, 0.26f), phase + 2);
      M5.Display.drawRect(deckX, deckY, deckW, podH + 1, colorDim(TFT_WHITE, 0.38f));
      M5.Display.drawFastHLine(deckX + 2, deckY + podH / 2, deckW - 4, colorDim(TFT_WHITE, 0.16f));
    }
  } else if (fastHunter) {
    M5.Display.drawTriangle(sx - scale, sy, sx - hullW/2 - scale, sy - scale/2, sx - hullW/2 - scale, sy + scale/2, colorDim(glow, 0.40f));
    M5.Display.drawFastHLine(sx - hullW/2 - scale/2, sy, scale/2 + 2, colorDim(TFT_WHITE, 0.55f));
  } else {
    int fin = scale / 2 + 2;
    M5.Display.drawLine(sx - hullW/2, sy - hullH/2, sx - hullW/2 - fin, sy - 1, colorDim(glow, 0.26f));
    M5.Display.drawLine(sx - hullW/2, sy + hullH/2, sx - hullW/2 - fin, sy + 1, colorDim(glow, 0.26f));
  }

  // nose cone
  M5.Display.fillTriangle(sx + hullW/2, sy,
                          sx + hullW/2 + noseLen, sy - scale/2 - 1,
                          sx + hullW/2 + noseLen, sy + scale/2 + 1,
                          colorDim(glow, 0.75f));
  M5.Display.drawTriangle(sx + hullW/2, sy,
                          sx + hullW/2 + noseLen, sy - scale/2 - 1,
                          sx + hullW/2 + noseLen, sy + scale/2 + 1,
                          TFT_WHITE);

  // wings / biomech fins
  if (s.role == ROLE_THARGOID || s.role == ROLE_THARGON) {
    M5.Display.drawTriangle(sx - 2, sy - hullH/2, sx - hullW/2 - scale/2, sy, sx - 2, sy + hullH/2, colorDim(glow, 0.55f));
    M5.Display.drawTriangle(sx + 2, sy - hullH/2, sx + hullW/2 + scale/3, sy, sx + 2, sy + hullH/2, colorDim(core, 0.45f));
    if (scale > 6) {
      M5.Display.drawCircle(sx, sy, scale / 2 + 1, colorDim(TFT_WHITE, 0.18f));
      M5.Display.drawLine(sx - hullW / 3, sy - hullH / 2, sx + hullW / 3, sy + hullH / 2, colorDim(TFT_BLACK, 0.24f));
    }
  } else {
    M5.Display.drawFastHLine(sx - hullW/2, sy - hullH/2 - 1, hullW/2, colorDim(glow, 0.25f));
    M5.Display.drawFastHLine(sx - hullW/2, sy + hullH/2 + 1, hullW/2, colorDim(glow, 0.25f));
    if (frigateClass) {
      int wingSpan = hullH + 4;
      M5.Display.drawLine(sx - hullW/4, sy - hullH/2 - 1, sx + hullW/6, sy - wingSpan, colorDim(glow, 0.22f));
      M5.Display.drawLine(sx - hullW/4, sy + hullH/2 + 1, sx + hullW/6, sy + wingSpan, colorDim(glow, 0.22f));
      M5.Display.drawFastVLine(sx - hullW/2 + 2, sy - hullH/2, hullH, colorDim(TFT_WHITE, 0.10f));
    }
  }

  if (s.role == ROLE_BOSS) {
    M5.Display.drawFastVLine(sx, sy - hullH/2 - scale/2, scale + 2, colorDim(TFT_WHITE, 0.60f));
    if (scale > 7) {
      M5.Display.drawRoundRect(sx - hullW/2 - 2, sy - hullH/2 - 2, hullW + 4, hullH + 4, 2, colorDim(glow, 0.20f));
    }
  }

  // engines
  int engineH = frigateClass ? 7 : 5;
  M5.Display.drawFastVLine(sx - hullW/2 - 1, sy - engineH / 2, engineH, glow);
  M5.Display.drawPixel(sx - hullW/2 - 2, sy, colorDim(glow, 0.85f));
  if (!heavyPlanetView) {
    M5.Display.drawPixel(sx - hullW/2 - 3, sy, colorDim(glow, 0.55f));
  }
  if (heavyTransport) {
    M5.Display.drawFastVLine(sx - hullW/2 - 4, sy - 3, 7, colorDim(glow, 0.60f));
    if (frigateClass) {
      M5.Display.drawFastVLine(sx - hullW/2 - 7, sy - 4, 9, colorDim(glow, 0.38f));
    }
  }
}

void drawSilhouetteStation() {
  int sx, sy; float d;
  if (!projectPoint(stationPos, sx, sy, d)) return;

  int px, py; float pd;
  bool planetVisible = projectPoint(planetPos, px, py, pd);
  float shadowFactor = 1.0f;
  if (planetVisible && d < pd) {
    float distToPlanet = sqrtf((sx - px)*(sx - px) + (sy - py)*(sy - py));
    if (distToPlanet < PLANET_SCREEN_RADIUS + 10) shadowFactor = 0.88f;
  }

  float presenceBoost = (dockPhase != DOCK_NONE) ? 1.22f : ((janus.mode == MODE_LAUNCH) ? 1.16f : 1.0f);
  int scale = clampi((int)((215.0f / d) * presenceBoost), 9, 38);
  int w = scale * 2 + 2;
  int h = scale * 2 + 2;
  int sideSpan = scale / 2 + 4;
  int armDepth = scale / 3 + 4;

  fillDitherRect(sx - w/2, sy - h/2, w, h,
                 colorDim(UI_SOFT, 0.40f * shadowFactor),
                 colorDim(UI_DIM, 0.18f * shadowFactor),
                 0);
  M5.Display.drawRect(sx - w/2, sy - h/2, w, h, colorDim(TFT_WHITE, 0.6f * shadowFactor));
  fillDitherRect(sx - w/2 - sideSpan, sy - h/3, sideSpan, h * 2 / 3,
                 colorDim(UI_SOFT, 0.28f * shadowFactor),
                 colorDim(UI_DIM, 0.12f * shadowFactor), 1);
  fillDitherRect(sx + w/2, sy - h/3, sideSpan, h * 2 / 3,
                 colorDim(UI_SOFT, 0.28f * shadowFactor),
                 colorDim(UI_DIM, 0.12f * shadowFactor), 2);
  M5.Display.drawRect(sx - w/2 - sideSpan, sy - h/3, sideSpan, h * 2 / 3, colorDim(TFT_WHITE, 0.36f * shadowFactor));
  M5.Display.drawRect(sx + w/2, sy - h/3, sideSpan, h * 2 / 3, colorDim(TFT_WHITE, 0.36f * shadowFactor));

  int slotW = clampi(scale / 2 + 6, 7, 18);
  int slotH = clampi(scale + 8, 12, 28);
  int slotX = sx + w/2 - slotW - 2;
  int slotY = sy - slotH / 2;
  M5.Display.fillRect(slotX, slotY, slotW, slotH, TFT_BLACK);
  M5.Display.drawRect(slotX, slotY, slotW, slotH, colorDim(UI_AMBER_BRIGHT, 0.8f * shadowFactor));
  M5.Display.drawFastHLine(slotX + 1, sy, slotW - 2, colorDim(UI_SOFT, 0.20f * shadowFactor));
  M5.Display.drawFastVLine(slotX + slotW / 2, slotY + 2, slotH - 4, colorDim(TFT_BLACK, 0.35f));

  for (int i = -2; i <= 2; ++i) {
    M5.Display.drawFastHLine(sx - w/2 + 3, sy + i * 4, w - 8, colorDim(UI_SOFT, 0.20f * shadowFactor));
  }
  for (int x = sx - w/2 + 7; x < sx + w/2 - 7; x += heavyPlanetView ? 8 : 6) {
    M5.Display.drawFastVLine(x, sy - h/2 + 4, h - 8, colorDim(TFT_BLACK, 0.18f * shadowFactor));
  }

  M5.Display.drawLine(sx - w/2 - sideSpan, sy - h/3, sx - w/2 - sideSpan - armDepth, sy - h/2, colorDim(UI_SOFT, 0.22f * shadowFactor));
  M5.Display.drawLine(sx - w/2 - sideSpan, sy + h/3, sx - w/2 - sideSpan - armDepth, sy + h/2, colorDim(UI_SOFT, 0.22f * shadowFactor));
  M5.Display.drawLine(sx + w/2 + sideSpan, sy - h/3, sx + w/2 + sideSpan + armDepth, sy - h/2, colorDim(UI_SOFT, 0.22f * shadowFactor));
  M5.Display.drawLine(sx + w/2 + sideSpan, sy + h/3, sx + w/2 + sideSpan + armDepth, sy + h/2, colorDim(UI_SOFT, 0.22f * shadowFactor));

  if (d < 260.0f) {
    M5.Display.drawRoundRect(sx - w/2 - 2, sy - h/2 - 2, w + 4, h + 4, 2, colorDim(UI_AMBER_BRIGHT, 0.18f * shadowFactor));
  }
}


void drawDominantPlanet() {
  if (!pilot.docked && janus.mode == MODE_LAUNCH && launchTimer > 18) {
    return;
  }

  int sx, sy; float depth;
  if (!projectPoint(planetPos, sx, sy, depth)) return;

  int r = heavyPlanetView ? 44 : PLANET_SCREEN_RADIUS;
  uint8_t worldSeed = (pilot.currentSystem * 13) ^ (pilot.galaxyIndex * 29);
  uint16_t planetA = mix565(0x149D, 0x7DFF, 0.20f);
  uint16_t planetB = 0x0410;
  uint16_t terrainA = mix565(0x07E0, 0x7DFF, 0.35f);
  uint16_t ringColor = 0xBDF7;
  if (systemBiome[pilot.currentSystem] == BIOME_ICE) {
    planetA = 0xB6DF;
    planetB = 0x39CF;
    terrainA = 0xFFFF;
    ringColor = 0xC73F;
  } else if (systemBiome[pilot.currentSystem] == BIOME_JUNGLE) {
    planetA = 0x25C6;
    planetB = 0x0140;
    terrainA = 0x7FE0;
    ringColor = 0x9EF3;
  } else if (systemBiome[pilot.currentSystem] == BIOME_VOLCANIC) {
    planetA = 0xD2A0;
    planetB = 0x2000;
    terrainA = 0xFD20;
    ringColor = 0xFC60;
  }

  fillDitherCircle(sx, sy, r, planetA, planetB, worldSeed & 3);
  for (int i = 0; i < (heavyPlanetView ? 2 : 3); ++i) {
    M5.Display.drawCircle(sx - i / 2, sy, r + i, colorDim(planetA, 0.22f - i * 0.05f));
  }

  int beltStep = heavyPlanetView ? 4 : 2;
  int rr = r * r;
  for (int dy = -r + 4; dy <= r - 4; dy += beltStep) {
    int span = (int)sqrtf((float)(rr - dy * dy));
    float band = procCell01(dy + r, worldSeed, pilot.currentSystem);
    if (span < 8) continue;
    int start = sx - span / 2 + (procHash8(dy, r, worldSeed) % clampi(span, 1, 96));
    int len = clampi((int)(span * (0.18f + band * 0.30f)), 2, span);
    if (start + len > sx + span) len = (sx + span) - start;
    if (len > 0) {
      uint16_t terrain = mix565(terrainA, planetA, band * 0.35f);
      M5.Display.drawFastHLine(start, sy + dy, len, colorDim(terrain, heavyPlanetView ? 0.10f : 0.16f));
    }
  }

  int terminatorR = r * 2 / 3;
  int terminatorX = sx + r / 3;
  int terminatorStep = heavyPlanetView ? 4 : 2;
  for (int dy = -terminatorR; dy <= terminatorR; dy += terminatorStep) {
    int maxDx = (int)sqrtf((float)(terminatorR * terminatorR - dy * dy));
    int lineX = terminatorX;
    int lineLen = maxDx + 1;
    if (lineX + lineLen > sx + r) lineLen = (sx + r) - lineX + 1;
    if (lineLen > 0) {
      M5.Display.drawFastHLine(lineX, sy + dy, lineLen, colorDim(0x0010, 0.55f));
    }
  }

  if (!heavyPlanetView || (frameParity & 1) == 0) {
    M5.Display.drawPixel(sx - r / 3, sy - r / 4, colorDim(TFT_WHITE, 0.75f));
    M5.Display.drawPixel(sx - r / 3 - 1, sy - r / 4, colorDim(0x7DFF, 0.45f));
  }

  int ringY = sy + 5;
  int ringOuterW = r + 10;
  int ringOuterH = heavyPlanetView ? 5 : 7;
  M5.Display.drawEllipse(sx, ringY, ringOuterW, ringOuterH, colorDim(ringColor, 0.26f));
  M5.Display.drawEllipse(sx, ringY, ringOuterW - 3, clampi(ringOuterH - 2, 1, 16), colorDim(UI_SOFT, 0.12f));
}

void drawProceduralSun() {
  int sx, sy; float depth;
  if (!projectPoint(sunPos, sx, sy, depth)) return;

  int r = clampi((int)(96.0f / depth), 3, heavyPlanetView ? 10 : 16);
  int haloLayers = survivalFocusMode ? 2 : 3;
  uint16_t halo = 0xFB60;
  uint16_t coreA = 0xFFE0;
  uint16_t coreB = 0xFD20;
  if (systemPhenomenon[pilot.currentSystem] == PHENOM_PULSAR) {
    halo = 0x7DFF;
    coreA = 0xFFFF;
    coreB = 0x6DFF;
  } else if (systemPhenomenon[pilot.currentSystem] == PHENOM_QUASAR) {
    halo = 0xF81F;
    coreA = 0xFFFF;
    coreB = 0xFB56;
  }
  for (int i = haloLayers; i >= 1; --i) {
    M5.Display.drawCircle(sx, sy, r + i * 2, colorDim(halo, 0.24f - i * 0.04f));
  }
  fillDitherCircle(sx, sy, r, coreA, coreB, (uint8_t)((millis() >> 6) & 3));

  if (!heavyPlanetView && !survivalFocusMode) {
    M5.Display.drawFastHLine(sx - r - 4, sy, r + 2, colorDim(coreA, 0.18f));
    M5.Display.drawPixel(sx + r + 2, sy - 1, colorDim(coreA, 0.35f));
    if (systemPhenomenon[pilot.currentSystem] == PHENOM_PULSAR) {
      M5.Display.drawFastVLine(sx, sy - r - 8, r * 2 + 16, colorDim(coreA, 0.28f));
    } else if (systemPhenomenon[pilot.currentSystem] == PHENOM_QUASAR) {
      M5.Display.drawFastVLine(sx, sy - r - 10, r * 2 + 20, colorDim(halo, 0.30f));
      M5.Display.drawFastHLine(sx - r - 8, sy, r * 2 + 16, colorDim(halo, 0.18f));
    }
  }
}

void drawCinematicExplosions() {
  for (int i = 0; i < 24; ++i) {
    if (!debris[i].alive) continue;
    int sx, sy; float d;
    if (projectPoint(debris[i].pos, sx, sy, d)) {
      uint16_t c = (i & 1) ? UI_AMBER_BRIGHT : TFT_WHITE;
      M5.Display.drawPixel(sx, sy, c);
      if ((i & 3) == 0) M5.Display.drawPixel(sx + 1, sy, colorDim(c, 0.5f));
    }
  }
}


void drawDamageOverlay() {
  if (screenFlashTimer <= 0) return;
  for (int y = 0; y < SCREEN_H; y += 3) {
    uint16_t c = (screenFlashTimer & 1) ? colorDim(TFT_RED, 0.35f) : colorDim(UI_AMBER_BRIGHT, 0.20f);
    M5.Display.drawFastHLine(0, y, SCREEN_W, c);
  }
}

void triggerHitFX(uint8_t strength = 3) {
  screenFlashTimer = 2 + strength;
  hitShakeTimer = 3 + strength;
}

void drawCombatOverdriveFX() {
  int cx = SCREEN_W / 2;
  int cy = SCREEN_H / 2 - 2;

  if (laserBeamTimer > 0) {
    int tx, ty; float d;
    if (projectPoint(lastLaserImpactPos, tx, ty, d)) {
      uint16_t beamA = hasEquipment(EQ_MILITARY_LASER) ? TFT_RED : UI_AMBER_BRIGHT;
      M5.Display.drawLine(cx, cy, tx, ty, beamA);
      M5.Display.drawLine(cx, cy + 1, tx, ty + 1, colorDim(beamA, 0.55f));
      M5.Display.drawPixel(tx, ty, TFT_WHITE);
      M5.Display.drawCircle(tx, ty, 2, colorDim(beamA, 0.45f));
    } else if (laserFiring) {
      M5.Display.drawFastHLine(cx - 2, cy, 22, colorDim(UI_AMBER_BRIGHT, 0.70f));
    }
  }

  if (missileTrailTimer > 0) {
    int tx, ty; float d;
    if (projectPoint(lastMissileTargetPos, tx, ty, d)) {
      M5.Display.drawLine(cx - 4, cy + 4, tx, ty, TFT_GREEN);
      M5.Display.drawLine(cx - 3, cy + 5, tx, ty + 1, colorDim(TFT_GREEN, 0.45f));
      M5.Display.drawCircle(tx, ty, 3, colorDim(TFT_WHITE, 0.60f));
    }
  }

  if (screenFlashTimer > 0 && !heavyPlanetView) drawDamageOverlay();
}

void drawDockingTunnelFX(bool outgoing) {
  int t = outgoing ? launchTimer : tunnelTimer;
  int p = clampi(outgoing ? (50 - t) : t, 0, 50);
  int w = 20 + p * 2;
  int h = 12 + p;
  int cx = SCREEN_W / 2;
  int cy = SCREEN_H / 2 - 6;

  for (int i = 0; i < 4; ++i) {
    M5.Display.drawRoundRect(cx - w/2 - i*3, cy - h/2 - i*2, w + i*6, h + i*4, 3, colorDim(UI_AMBER_BRIGHT, 0.45f - i*0.08f));
  }
  fillDitherRect(cx - w/3, cy - h/3, w * 2 / 3, h * 2 / 3, colorDim(UI_DIM, outgoing ? 0.20f : 0.14f), TFT_BLACK, 0);
}

void drawEnemyDamageFX(const SpaceShip &e) {
  int sx, sy; float d;
  if (!projectPoint(e.pos, sx, sy, d)) return;
  float hpFrac = e.hp / shipStats[e.type].hp;
  if (hpFrac < 0.55f) {
    M5.Display.drawPixel(sx - 2, sy - 2, UI_AMBER_BRIGHT);
    M5.Display.drawPixel(sx + 2, sy + 1, UI_AMBER_BRIGHT);
  }
  if (hpFrac < 0.25f) {
    M5.Display.drawPixel(sx - 3, sy, TFT_WHITE);
    M5.Display.drawPixel(sx + 3, sy - 1, TFT_RED);
  }
}

void drawMiningBeamFX() {
  if (!laserFiring) return;
  for (int i = 0; i < 12; ++i) {
    if (!asteroids[i].alive) continue;
    if (fabsf(asteroids[i].pos.x) < 10.0f && fabsf(asteroids[i].pos.y) < 10.0f && asteroids[i].pos.z < 135.0f) {
      int sx, sy; float d;
      if (!projectPoint(asteroids[i].pos, sx, sy, d)) return;
      int cx = SCREEN_W / 2;
      int cy = SCREEN_H / 2 - 2;
      uint16_t beam = (miningBeamPhase & 1) ? UI_AMBER_BRIGHT : TFT_WHITE;
      M5.Display.drawLine(cx, cy, sx, sy, beam);
      M5.Display.drawLine(cx, cy+1, sx, sy+1, colorDim(beam, 0.55f));
      return;
    }
  }
}

void drawFlightScreen() {
  M5.Display.fillScreen(TFT_BLACK);

  int psx, psy; float pd;
  heavyPlanetView = projectPoint(planetPos, psx, psy, pd) &&
                    (psx > 8 && psx < SCREEN_W - 8 && psy > 8 && psy < SCREEN_H - 8 && pd < 520.0f);
  survivalFocusMode = pilot.missileIncoming ||
                      janus.mode == MODE_COMBAT ||
                      dockPhase != DOCK_NONE ||
                      currentGoal == GOAL_ESCAPE ||
                      shipShield < 42.0f ||
                      shipEnergy < 38.0f ||
                      pilot.fuel < 18;

  // unified order: background -> celestial -> station -> world objects -> effects -> cockpit -> HUD
  drawFakeModernBackdrop();
  drawCinematicStars();
  drawProceduralSun();
  drawDominantPlanet();
  drawSilhouetteStation();
  drawAsteroids();

  for (int i = 0; i < MAX_TRAFFIC; ++i) {
    if (!traffic[i].alive) continue;
    drawSilhouetteShip(traffic[i], colorDim(UI_SOFT, 0.55f), colorDim(UI_SOFT, 0.25f));
  }

  for (int i = 0; i < MAX_ENEMIES; ++i) {
    if (!enemies[i].alive) continue;
    uint16_t core = colorDim(TFT_RED, 0.60f);
    uint16_t glow = colorDim(UI_AMBER_BRIGHT, 0.28f);
    if (enemies[i].role == ROLE_POLICE) { core = colorDim(TFT_YELLOW, 0.60f); glow = colorDim(TFT_WHITE, 0.20f); }
    else if (enemies[i].role == ROLE_THARGOID || enemies[i].role == ROLE_THARGON) { core = colorDim(TFT_MAGENTA, 0.65f); glow = colorDim(0x7DFF, 0.26f); }
    else if (enemies[i].role == ROLE_BOSS) { core = colorDim(TFT_CYAN, 0.65f); glow = colorDim(TFT_WHITE, 0.22f); }
    drawSilhouetteShip(enemies[i], core, glow);
  }

  for (int i = 0; i < 10; ++i) {
    if (!canisters[i].alive) continue;
    int sx, sy; float d;
    if (projectPoint(canisters[i].pos, sx, sy, d)) {
      M5.Display.fillRect(sx - 2, sy - 2, 4, 4, UI_AMBER_BRIGHT);
      M5.Display.drawRect(sx - 3, sy - 3, 6, 6, colorDim(TFT_WHITE, 0.6f));
    }
  }

  if (!heavyPlanetView) drawParallaxDust();
  drawCinematicExplosions();
  drawMiningBeamFX();
  drawCombatOverdriveFX();
  drawCockpitView();
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
  M5.Display.startWrite();
  if (uiMode == UI_WITCHSPACE) { drawWitchSpaceScreen(); M5.Display.endWrite(); return; }
  if (uiMode == UI_MAP)        { drawGalaxyMapScreen(); M5.Display.endWrite(); return; }
  if (uiMode == UI_MISSION)    { drawMissionScreen(); M5.Display.endWrite(); return; }
  if (uiMode == UI_STATUS)     { drawStatusScreen(); M5.Display.endWrite(); return; }
  if (uiMode == UI_MARKET)     { drawMarketScreen(); M5.Display.endWrite(); return; }
  if (uiMode == UI_EQUIP)      { drawEquipScreen(); M5.Display.endWrite(); return; }
  if (uiMode == UI_LOCAL)      { drawLocalMapScreen(); M5.Display.endWrite(); return; }
  if (uiMode == UI_BAR)        { drawBarScreen(); M5.Display.endWrite(); return; }
  if (uiMode == UI_DOCK_INTERIOR) { drawDockInteriorScreen(); M5.Display.endWrite(); return; }

  if (pilot.docked) drawDockedScreen();
  else drawFlightScreen();
  M5.Display.endWrite();
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
      return (pilot.kills * 0.05f) + (galaxy[pilot.currentSystem].danger * 0.35f) - ((pilot.shipFrame >= FRAME_FREIGHTER && pilot.careerTrack == CAREER_MECH_HAULER && pilot.surfaceOpsCompleted == 0) ? 0.55f : 0.0f);
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
  if (target == pilot.currentSystem) return;
  int capacity = cargoLimit() - pilot.cargoCount;
  if (capacity <= 0) return;

  struct TradeOption {
    uint8_t item;
    int profit;
  };

  TradeOption opts[MAX_MARKET_ITEMS];
  for (int i = 0; i < MAX_MARKET_ITEMS; ++i) {
    int buyHere = getCommodityPrice(i, galaxy[pilot.currentSystem]);
    int sellThere = getCommodityPrice(i, galaxy[target]);
    opts[i].item = i;
    opts[i].profit = sellThere - buyHere + getCommodityDoctrineBias(i);
  }

  for (int i = 0; i < MAX_MARKET_ITEMS; ++i) {
    for (int j = i + 1; j < MAX_MARKET_ITEMS; ++j) {
      if (opts[j].profit > opts[i].profit) {
        TradeOption t = opts[i];
        opts[i] = opts[j];
        opts[j] = t;
      }
    }
  }

  for (int i = 0; i < MAX_MARKET_ITEMS && capacity > 0; ++i) {
    if (opts[i].profit <= 0) continue;
    int roll = 2 + (int)random(0, 4);
    int amount = (capacity < roll) ? capacity : roll;
    for (int n = 0; n < amount && pilot.cargoCount < cargoLimit(); ++n) {
      if (!buyCommodity(opts[i].item)) break;
    }
    capacity = cargoLimit() - pilot.cargoCount;
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
  int enemyCount = 0;
  int alloyCount = 0;
  for (int i = 0; i < MAX_ENEMIES; ++i) if (enemies[i].alive) enemyCount++;
  for (int i = 0; i < pilot.cargoCount; ++i) if (pilot.cargoType[i] == COM_ALLOYS) alloyCount++;

  if (shipShield < 25.0f || shipEnergy < 20.0f) {
    pilot.energyMode = 3;
    currentGoal = GOAL_ESCAPE;
    return;
  }
  if ((pilot.missionStage != MISSION_NONE && pilot.missionProgress == 0) ||
      (currentBarMission.active && currentBarMission.targetSystem != pilot.currentSystem)) {
    currentGoal = GOAL_MISSION;
    return;
  }
  if (enemyCount > 0 && shipShield > 40.0f && dockPhase == DOCK_NONE) {
    pilot.energyMode = 2;
    currentGoal = GOAL_FIGHT;
    return;
  }
  if (pilot.fuel < 20 && hasEquipment(EQ_FUEL_SCOOP) && !pilot.docked) {
    pilot.energyMode = 3;
    currentGoal = GOAL_REFUEL;
    return;
  }
  if (alloyCount >= 3 || pilot.cargoCount >= cargoLimit() * 0.8f || pilot.fuel < 30) {
    pilot.energyMode = 1;
    currentGoal = GOAL_DOCK;
    return;
  }
  if (pilot.docked) {
    if (pilot.credits > 500 && !hasAllCoreEquipment()) currentGoal = GOAL_UPGRADE;
    else currentGoal = GOAL_TRADE;
    return;
  }
  pilot.energyMode = 0;
  currentGoal = GOAL_TRADE;
}

uint8_t pickBestTradeItem(uint8_t target) {
  StarSystem &from = galaxy[pilot.currentSystem];
  StarSystem &to   = galaxy[target];

  int bestScore = -9999;
  uint8_t bestItem = 0;

  for (uint8_t i = 0; i < MAX_MARKET_ITEMS; ++i) {
    int buyP = getCommodityPrice(i, from);
    int sellP = getCommodityPrice(i, to);
    int score = sellP - buyP - fuelCostTo(target) + getCommodityDoctrineBias(i);
    if (score > bestScore) {
      bestScore = score;
      bestItem = i;
    }
  }
  return bestItem;
}

uint8_t pickTradeTarget() {
  if (currentBarMission.active) {
    if (currentBarMission.type == 0 || currentBarMission.type == 2 || currentBarMission.type == 3 || currentBarMission.type == 4) {
      int cost = fuelCostTo(currentBarMission.targetSystem);
      if (currentBarMission.targetSystem != pilot.currentSystem && cost <= pilot.fuel) {
        return currentBarMission.targetSystem;
      }
    }
  }

  int bestScore = -9999;
  uint8_t bestTarget = pilot.currentSystem;
  uint8_t doctrine = getShipDoctrine();

  for (uint8_t i = 0; i < MAX_SYSTEMS; ++i) {
    if (i == pilot.currentSystem) continue;
    int fuelCost = fuelCostTo(i);
    if (fuelCost > pilot.fuel) continue;

    int score = bestCommodityMarginHere(i) - fuelCost * 2 - galaxy[i].danger;
    score += systemTradeFlux[i] * 5;
    score += systemNeed[i] * 2;
    score -= systemRaidHeat[i] * 4;
    score -= systemThargoidBlight[i] * 7;
    score -= (int)(learnState.systemDeathPenalty[i] * 40.0f);
    if (doctrine == DOCTRINE_MINING) score += getLiveMarketOffset(i, COM_ALLOYS) * 3 + getLiveMarketOffset(i, COM_MACHINERY);
    if (doctrine == DOCTRINE_LOGISTICS) score += systemNeed[i] * 3 - galaxy[i].danger;
    if (doctrine == DOCTRINE_ASSAULT || doctrine == DOCTRINE_COMMAND) score += systemTradeFlux[i] * 2 + systemFactionPower[i];

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
    if (!enemies[i].alive || enemies[i].pos.z <= 8.0f) continue;
    float threat = calculateThreat(enemies[i]);
    if (enemies[i].role == ROLE_THARGOID || enemies[i].role == ROLE_BOSS) threat += 25.0f;
    if (threat > maxThreat) {
      maxThreat = threat;
      targetIndex = i;
    }
  }

  if (laserHeat >= 90) statusLine = "LASER HOT";
  else if (isDamaged(DMG_LASER)) statusLine = "LASER DMG";

  if (targetIndex < 0) {
    janus.mode = MODE_PATROL;
    patrolTimer = 140;
    statusLine = "SEARCH";
    return;
  }

  SpaceShip &e = enemies[targetIndex];

  if (pilot.missileIncoming) {
    if (hasEquipment(EQ_ECM)) {
      pilot.missileIncoming = false;
      statusLine = "ECM";
      playToneSafe(520, 50);
    } else {
      shipShield -= 10.0f;
      shipEnergy -= 4.0f;
      pilot.missileIncoming = false;
      statusLine = "MISSILE HIT";
      triggerHitFX(4);
      playHitSound();
    }
  }

  float timeToTarget = e.pos.z / (shipSpeed * 40.0f + 10.0f);
  float leadX = e.pos.x + e.vel.x * timeToTarget;
  float leadY = e.pos.y + e.vel.y * timeToTarget;

  float targetYaw = atan2f(leadX, e.pos.z);
  float targetPitch = -atan2f(leadY, e.pos.z);

  shipYaw += clampf(targetYaw * 0.045f + sensorBiasYaw * 0.15f, -0.06f, 0.06f);
  shipPitch += clampf(targetPitch * 0.040f, -0.05f, 0.05f);
  shipRoll = clampf(-targetYaw * 0.9f, -0.6f, 0.6f);
  shipSpeed = clampf(shipSpeed + 0.003f, 0.12f, 0.42f);

  bool inCross = fabsf(leadX) < 12 && fabsf(leadY) < 12 && e.pos.z < 150;

  if (pilot.missiles > 0 &&
      (e.type == SHIP_CONSTRICTOR || e.type == SHIP_PYTHON || e.type == SHIP_ANACONDA || e.role == ROLE_BOSS || e.role == ROLE_THARGOID) &&
      e.pos.z < 180 && random(0, 100) < 35) {
    e.hp -= 50.0f;
    pilot.missiles--;
    lastMissileTargetPos = e.pos;
    missileTrailTimer = 8;
    statusLine = "MISSILE AWAY";
    playToneSafe(320, 90);
  }

  if (inCross && laserHeat < 90 && !isDamaged(DMG_LASER)) {
    laserFiring = true;
    laserHeat += 8.0f;
    float laserDamage = hasEquipment(EQ_MILITARY_LASER) ? 15.0f : 9.0f;
    if (usingDualMiningRig()) laserDamage *= 0.58f;
    if (e.type == SHIP_TRANSPORT || e.type == SHIP_PYTHON || e.type == SHIP_ANACONDA || e.type == SHIP_CONSTRICTOR) laserDamage -= 2.0f;
    e.hp -= laserDamage;
    lastLaserImpactPos = e.pos;
    laserBeamTimer = 4;
    statusLine = "FIRE";
    triggerHitFX(1);
    playLaserSoundProc();
    trainJanus(f, 0.05f);
  }

  if (shipShield < 10.0f || shipEnergy < 15.0f) {
    janus.mode = MODE_RETURN;
    returnTimer = 180;
    statusLine = "RETREAT";
    trainJanus(f, -0.18f);
  } else {
    trainJanus(f, 0.02f);
  }

  janus.decisions++;
}

void updateDockApproach() {
  if (dockPhase == DOCK_NONE) {
    beginDockRun();
    return;
  }

  float dx = -stationPos.x;
  float dy = -stationPos.y;

  if (dockPhase == DOCK_APPROACH) {
    float range = stationPos.z;
    float yawGain = (range > 260.0f) ? 0.00022f : 0.00060f;
    float pitchGain = (range > 260.0f) ? 0.00020f : 0.00055f;
    float closure = (range > 520.0f) ? 5.0f : ((range > 320.0f) ? 2.8f : ((range > 180.0f) ? 1.6f : 0.85f));

    shipYaw += clampf(dx * yawGain, -0.018f, 0.018f);
    shipPitch += clampf(-dy * pitchGain, -0.018f, 0.018f);
    shipRoll = clampf(shipRoll * 0.94f - dx * 0.00020f, -0.22f, 0.22f);
    shipSpeed = clampf(shipSpeed + 0.0012f, 0.11f, 0.20f);
    stationPos.x *= (range > 240.0f) ? 0.996f : 0.992f;
    stationPos.y *= (range > 240.0f) ? 0.996f : 0.992f;
    stationPos.z -= closure;

    int meters = clampi((int)stationPos.z, 0, 999);
    if ((frameParity & 1) == 0) {
      statusLine = String("APP ") + String(meters) + "M";
    }

    if (fabsf(stationPos.x) < 18.0f && fabsf(stationPos.y) < 15.0f && stationPos.z < 160.0f) {
      dockPhase = DOCK_ALIGN;
      statusLine = "DOCK ALIGN";
    }
    return;
  }

  if (dockPhase == DOCK_ALIGN) {
    float alignFactor = hasEquipment(EQ_DOCK_COMPUTER) ? 0.80f : 0.92f;
    shipYaw *= alignFactor;
    shipPitch *= alignFactor;
    shipRoll *= 0.92f;
    stationPos.x *= hasEquipment(EQ_DOCK_COMPUTER) ? 0.86f : 0.92f;
    stationPos.y *= hasEquipment(EQ_DOCK_COMPUTER) ? 0.86f : 0.92f;
    stationPos.z -= hasEquipment(EQ_DOCK_COMPUTER) ? 1.9f : 1.1f;

    float slotPhase = fabsf(sinf(stationSpin));
    float alignmentError = fabsf(stationPos.x) + fabsf(stationPos.y) + fabsf(shipYaw) * 90.0f + fabsf(shipPitch) * 90.0f;
    float entryThreshold = hasEquipment(EQ_DOCK_COMPUTER) ? 12.0f : 8.0f;
    bool spinWindowOk = hasEquipment(EQ_DOCK_COMPUTER) ? true : (slotPhase < 0.28f);

    if (alignmentError < entryThreshold && stationPos.z < 74.0f && spinWindowOk) {
      dockPhase = DOCK_TUNNEL;
      tunnelTimer = 0;
      statusLine = "DOCK ENTRY";
      return;
    }

    if (stationPos.z < 62.0f && (!spinWindowOk || alignmentError > entryThreshold * 1.8f)) {
      dockPhase = DOCK_APPROACH;
      stationPos.z = 240.0f;
      stationPos.x *= 1.3f;
      stationPos.y *= 1.3f;
      shipShield -= hasEquipment(EQ_DOCK_COMPUTER) ? 0.5f : 2.0f;
      statusLine = "ALIGN FAIL";
      return;
    }
    return;
  }

  if (dockPhase == DOCK_TUNNEL) {
    tunnelTimer++;
    shipYaw *= 0.86f;
    shipPitch *= 0.86f;
    shipRoll *= 0.88f;
    shipSpeed = clampf(shipSpeed * 0.985f, 0.06f, 0.16f);
    if (tunnelTimer > 50) {
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
      dockCinemaPhase = 1;
      uiMode = UI_DOCK_INTERIOR;
      statusLine = "DOCK OK";
      playDockSoundProc();
    }
  }
}


void janusDockedDecision() {
  float f[12];
  buildFeatures(f);

  decayLegalStatusInDock();
  updateMissionsOnDock();
  if (!currentBarMission.active) generateBarMission();

  StarSystem &sys = galaxy[pilot.currentSystem];
  bool soldSomething = false;
  for (int i = pilot.cargoCount - 1; i >= 0; --i) {
    uint8_t item = pilot.cargoType[i];
    int sellPrice = getCommodityPrice(item, sys);
    bool forcedLiquidation = (pilot.fuel < 18 || pilot.cargoCount >= cargoLimit() - 2);
    if (sellPrice >= pilot.cargoBuyPrice[i] || forcedLiquidation) {
      pilot.credits += sellPrice;
      pilot.totalProfit += (sellPrice - pilot.cargoBuyPrice[i]);
      registerMarketActivity(item, true);
      for (int j = i; j < pilot.cargoCount - 1; ++j) {
        pilot.cargoType[j] = pilot.cargoType[j + 1];
        pilot.cargoBuyPrice[j] = pilot.cargoBuyPrice[j + 1];
      }
      pilot.cargoCount--;
      soldSomething = true;
    }
  }

  refuelAtStation();
  autoBuyEquipment();
  janusRestockMissiles();
  processBarMissionOnDock();

  if (pilot.damagedFlags != DMG_NONE && pilot.credits >= 100) {
    pilot.credits -= 100;
    pilot.damagedFlags = DMG_NONE;
    statusLine = "REPAIRED";
  }

  pendingJumpSystem = 255;

  if (hasEquipment(EQ_GAL_HYPERDRIVE) && pilot.kills > 25 && pilot.credits > 1200 && pilot.galaxyIndex < 7) {
    galacticJump();
    trainJanus(f, 0.08f);
    saveBestAgentIfImproved();
    janus.decisions++;
    return;
  }

  uint8_t target = pickTradeTarget();
  if (target != pilot.currentSystem && fuelCostTo(target) <= pilot.fuel) {
    if (pilot.credits >= 90 || pilot.cargoCount > 0 || canAffordAnyCommodityFor(target) || currentBarMission.active) {
      pendingJumpSystem = target;
      janusPlanTrade(target);
    }
  }

  if (pilot.cargoCount == 0 && pendingJumpSystem != 255 && pilot.credits >= 40) {
    janusPlanTrade(pendingJumpSystem);
  }

  shipEnergy = clampf(shipEnergy + 4.0f, 0.0f, 100.0f);
  shipShield = clampf(shipShield + 4.0f, 0.0f, 100.0f);

  statusLine = soldSomething ? "TRADE TURN" : ((pendingJumpSystem != 255) ? "NEW ROUTE" : "REDOCK LOOP");
  mapBrowseTimer = (pendingJumpSystem != 255) ? 26 : 0;
  launchScene();
  trainJanus(f, 0.14f);
  saveBestAgentIfImproved();
  janus.decisions++;
}

void janusSpaceDecision() {
  if (dockPhase != DOCK_NONE) {
    updateDockApproach();
    return;
  }

  if (janus.mode == MODE_LAUNCH) {
    shipSpeed = clampf(shipSpeed + 0.004f, 0.10f, 0.20f);
    stationPos.z += 1.6f;
    stationPos.x *= 0.97f;
    stationPos.y *= 0.97f;
    shipYaw *= 0.96f;
    shipPitch *= 0.96f;
    shipRoll *= 0.96f;
    statusLine = "LAUNCH";
    if (--launchTimer <= 0 || stationPos.z > 420.0f) {
      janus.mode = MODE_PATROL;
      patrolTimer = 380 + random(0, 220);
      statusLine = "CRUISE";
    }
    return;
  }

  if (janus.mode == MODE_PATROL) {
    if (launchLockout > 0) launchLockout--;
    if (pilot.cargoCount >= cargoLimit() - 1) {
      janus.mode = MODE_RETURN;
      returnTimer = 180;
      statusLine = "SELL ORE";
      return;
    }
    shipYaw += sensorBiasYaw * 0.025f + frandRange(-0.003f, 0.003f);
    shipPitch += frandRange(-0.0015f, 0.0015f);
    shipRoll = clampf(shipRoll * 0.97f + frandRange(-0.006f, 0.006f), -0.22f, 0.22f);
    shipSpeed = clampf(shipSpeed + frandRange(-0.001f, 0.002f), 0.10f, 0.21f);

    if (hasEquipment(EQ_FUEL_SCOOP) && pilot.fuel < 62) {
      float targetYaw = atan2f(sunPos.x, sunPos.z);
      float targetPitch = -atan2f(sunPos.y, sunPos.z);
      shipYaw += clampf(targetYaw * 0.04f, -0.024f, 0.024f);
      shipPitch += clampf(targetPitch * 0.04f, -0.024f, 0.024f);
      shipSpeed = clampf(shipSpeed, 0.10f, 0.18f);
      statusLine = "SUN SEEK";
    } else if (random(0, 100) < (8 + (int)(remoteExplorationBias * 20.0f))) {
      planetPos.z -= 2.6f;
      mapBrowseTimer = 10;
      statusLine = "PLANET RUN";
    }

    stationPos.z += 0.5f;
    if (stationPos.z > 420.0f) stationPos.z = 420.0f;

    bool miningLane = false;
    bool miningEnabled = (pilot.drillTier > 0 || hasEquipment(EQ_MILITARY_LASER));
    if (miningEnabled && pilot.cargoCount < cargoLimit() - 1) {
      for (int i = 0; i < 12; ++i) {
        if (!asteroids[i].alive) continue;
        if (fabsf(asteroids[i].pos.x) < 14.0f && fabsf(asteroids[i].pos.y) < 12.0f && asteroids[i].pos.z < 150.0f) {
          miningLane = true;
          break;
        }
      }
      if (!miningLane && (systemTradeFlux[pilot.currentSystem] > 4 || systemNeed[pilot.currentSystem] > 4) && random(0, 100) < 28) {
        spawnAsteroidField();
        mapBrowseTimer = 6;
      }
      if (miningLane) {
        shipSpeed = clampf(shipSpeed, 0.08f, 0.16f);
        laserFiring = true;
        statusLine = "DRILL LANE";
      }
    }

    int contactChance = 2 + galaxy[pilot.currentSystem].danger + systemRaidHeat[pilot.currentSystem] / 2 + systemThargoidBlight[pilot.currentSystem] / 2;
    if (shipShield > 45.0f && shipEnergy > 45.0f && random(0, 100) < contactChance) {
      if (systemFaction[pilot.currentSystem] == FACTION_THARGOID || systemThargoidBlight[pilot.currentSystem] > 5) spawnThargoidEncounter();
      else spawnSingleEnemy();
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

    // Only allow intersystem jump after a real patrol phase away from station.
    if (pendingJumpSystem != 255 && launchLockout <= 0 && stationPos.z > 395.0f &&
        patrolTimer < (320 + (int)(remoteExplorationBias * 120.0f)) && pilot.fuel >= fuelCostTo(pendingJumpSystem) &&
        shipShield > 30.0f && shipEnergy > 30.0f && !miningLane) {
      beginVisibleJump(pendingJumpSystem);
      pendingJumpSystem = 255;
      statusLine = "JUMP";
      return;
    }

    bool lowResources = (shipShield < 35.0f || shipEnergy < 35.0f || pilot.fuel < 10);
    bool bored = (patrolTimer <= 0 && random(0, 100) < (22 + (int)(remoteExplorationBias * 30.0f)));
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
  uint8_t sys = pilot.currentSystem;
  int roll = random(0, 100) - systemRaidHeat[sys] + systemTradeFlux[sys] / 2 - systemThargoidBlight[sys];

  if (roll < 12) {
    spawnSingleEnemy();
    systemRaidHeat[sys] = clampi((int)systemRaidHeat[sys] + 1, 0, 9);
    systemFactionPower[sys] = clampi((int)systemFactionPower[sys] - 1, 0, 9);
    statusLine = "AMBUSH";
  } else if (roll < 18 && pilot.legalStatus != LEGAL_CLEAN) {
    spawnPoliceWing();
    systemRaidHeat[sys] = clampi((int)systemRaidHeat[sys] + 1, 0, 9);
  } else if (roll < 30) {
    systemTradeFlux[sys] = clampi((int)systemTradeFlux[sys] + 2, 0, 9);
    systemFactionPower[sys] = clampi((int)systemFactionPower[sys] + 1, 0, 9);
    seedRouteConductivity(sys, (sys + 1 + random(0, MAX_SYSTEMS - 1)) % MAX_SYSTEMS, 1);
    statusLine = "CARAVAN";
  } else if (roll < 40) {
    systemNeed[sys] = clampi((int)systemNeed[sys] + 2, 0, 9);
    statusLine = "AID CALL";
  } else if (roll < 50) {
    spawnAsteroidField();
    systemTradeFlux[sys] = clampi((int)systemTradeFlux[sys] + 1, 0, 9);
    statusLine = "MINING RUSH";
  } else if (roll < 58 && (pilot.missionStage == MISSION_THARGOID_DOCS || systemThargoidBlight[sys] > 4 || systemFaction[sys] == FACTION_THARGOID)) {
    spawnThargoidEncounter();
    systemRaidHeat[sys] = clampi((int)systemRaidHeat[sys] + 2, 0, 9);
    systemThargoidBlight[sys] = clampi((int)systemThargoidBlight[sys] + 1, 0, 9);
    systemFaction[sys] = (systemThargoidBlight[sys] > 7) ? FACTION_THARGOID : systemFaction[sys];
    statusLine = "INCURSION";
  } else if (roll < 66 && systemNeed[sys] > 5) {
    systemFactionPower[sys] = clampi((int)systemFactionPower[sys] - 1, 0, 9);
    systemTradeFlux[sys] = clampi((int)systemTradeFlux[sys] - 1, 0, 9);
    statusLine = "UNREST";
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
    if (stars[i].z < 10) {
      stars[i].x = frandRange(-90, 90);
      stars[i].y = frandRange(-90, 90);
      stars[i].z = 200;
    }
  }
}

void updatePlanetSun() {
  static float orbitA = 0.0f;

  if (!pilot.docked && janus.mode == MODE_LAUNCH) {
    stationPos.z -= 5.6f;
    stationPos.x *= 0.974f;
    stationPos.y *= 0.974f;

    planetPos.z = stationPos.z + 150.0f;
    planetPos.x = stationPos.x + 12.0f;
    planetPos.y = stationPos.y - 8.0f;

    sunPos.z = 520.0f + cosf(millis() * 0.00045f) * 16.0f;
    sunPos.x = -78.0f + cosf(millis() * 0.0006f) * 10.0f;
    sunPos.y = -30.0f + sinf(millis() * 0.0005f) * 6.0f;
    return;
  }

  float stellarRate = 0.004f + shipSpeed * 0.004f;
  if (systemPhenomenon[pilot.currentSystem] == PHENOM_PULSAR) stellarRate *= 1.25f;
  else if (systemPhenomenon[pilot.currentSystem] == PHENOM_QUASAR) stellarRate *= 1.40f;
  orbitA += stellarRate;

  float biomeDrift = (systemBiome[pilot.currentSystem] == BIOME_VOLCANIC) ? 52.0f : ((systemBiome[pilot.currentSystem] == BIOME_ICE) ? 38.0f : 42.0f);
  planetPos.x = sinf(orbitA * 0.3f) * biomeDrift + shipYaw * 2.0f;
  planetPos.y = cosf(orbitA * 0.2f) * (systemBiome[pilot.currentSystem] == BIOME_JUNGLE ? 22.0f : 18.0f) + shipPitch * 2.0f;
  planetPos.z = 360.0f + sinf(orbitA * 0.5f) * (systemBiome[pilot.currentSystem] == BIOME_ICE ? 28.0f : 35.0f);

  sunPos.x = 95.0f + cosf(orbitA * 0.15f) * 30.0f;
  sunPos.y = -45.0f + sinf(orbitA * 0.17f) * 16.0f;
  sunPos.z = 430.0f + cosf(orbitA * 0.22f) * 24.0f;
  if (systemPhenomenon[pilot.currentSystem] == PHENOM_PULSAR) {
    sunPos.y += sinf(orbitA * 2.2f) * 8.0f;
  } else if (systemPhenomenon[pilot.currentSystem] == PHENOM_QUASAR) {
    sunPos.x += cosf(orbitA * 1.8f) * 10.0f;
    sunPos.z += sinf(orbitA * 1.5f) * 18.0f;
  }

  if (!pilot.docked) {
    stationPos.x += clampf(-stationPos.x * 0.002f, -0.4f, 0.4f);
    stationPos.y += clampf(-stationPos.y * 0.002f, -0.2f, 0.2f);
  }
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
      playExplosionSoundProc();

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
  updateRemoteMeshInfluence();
  updateLocalEchoMic();
  updateSensorInfluence();
  if (millis() - lastVisualPhaseTick > 220) {
    lastVisualPhaseTick = millis();
    frameParity ^= 1;
  }
  if (screenFlashTimer > 0) screenFlashTimer--;
  if (hitShakeTimer > 0) hitShakeTimer--;
  if (laserBeamTimer > 0) laserBeamTimer--;
  if (missileTrailTimer > 0) missileTrailTimer--;
  if (mapBrowseTimer > 0) mapBrowseTimer--;
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

  updateLivingGalaxy();

  if (pilot.docked) {
    if (surfaceOp.active) {
      uiMode = UI_MISSION;
      if (millis() - lastAgentTick > 160) {
        lastAgentTick = millis();
        updateSurfaceOperation();
      }
      return;
    }

    if (!jumpInProgress && uiMode == UI_MAP && mapBrowseTimer <= 0) uiMode = UI_MARKET;
    updateStationFlow();

    if (!stationFlowActive) {
      if (dockTimer > 0) {
        dockTimer--;
        if (dockTimer == 0 && millis() - lastAgentTick > 150) {
          lastAgentTick = millis();
          if (!stationFlowActive) beginStationFlow();
        }
      } else if (millis() - lastAgentTick > 500) {
        lastAgentTick = millis();
        if (!stationFlowActive) janusDockedDecision();
      }
    }
  } else {
    if (mapBrowseTimer > 0 && !jumpInProgress && !inWitchSpace && janus.mode != MODE_COMBAT) uiMode = UI_MAP;
    else if (!jumpInProgress && !inWitchSpace && uiMode == UI_MAP && mapBrowseTimer <= 0) uiMode = UI_FLIGHT;
    if (janus.mode != MODE_LAUNCH) updateGoal();

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
          if (launchLockout > 0) {
            janusSpaceDecision();
          } else {
            janus.mode = MODE_RETURN;
            updateDockApproach();
          }
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
    updateFuelScoop();
    updateCollisionAvoidance();
    updateIntersystemEvents();
    updateShipState();
    mineAsteroidsIfHit();
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
  localMicReady = initLocalEchoMic();

  Serial.begin(115200);
  delay(100);

  ensureMeshWiFi();
  LittleFS.begin(true);
  clearKnownStaleFiles();
  randomSeed((uint32_t)esp_random());

  loadState();
  loadLearnState();
  loadBestAgentIfAvailable();
  generateGalaxy(pilot.galaxyIndex);
  bool worldLoaded = loadWorldState();
  if (!worldLoaded && !currentBarMission.active) generateBarMission();

  M5.Display.setRotation(0);
  M5.Display.setColorDepth(16);
  M5.Display.setTextSize(1);
  applyBrightness();

  resetStars();
  resetEnemies();
  resetDebris();
  resetCanisters();
  initTraffic();

  if (!worldLoaded) {
    stationPos = {0.0f, 0.0f, 250.0f};
    planetPos  = {0.0f, 0.0f, 320.0f};
    sunPos     = {80.0f, -40.0f, 420.0f};

    shipEnergy = 100.0f;
    shipShield = 100.0f;
    laserHeat  = 0.0f;
    shipSpeed  = 0.12f;

    pilot.docked = false;
    launchScene();
    statusLine = "JANUS LIVE";
  } else {
    shipEnergy = clampf(shipEnergy, 0.0f, 100.0f);
    shipShield = clampf(shipShield, 0.0f, 100.0f);
    laserHeat  = clampf(laserHeat, 0.0f, 100.0f);
    shipSpeed  = clampf(shipSpeed, 0.0f, 0.42f);
  }

  survivalStartMs = millis();
  renderTick();
}

void loop() {
  M5.update();
  handleButton();

  unsigned long now = millis();

  if (paused) {
    if (now - lastRenderTick >= PAUSED_RENDER_INTERVAL_MS) {
      lastRenderTick = now;
      renderTick();
    }
    delay(4);
    return;
  }

  uint8_t logicCatchup = 0;
  while (now - lastLogicTick >= LOGIC_INTERVAL_MS && logicCatchup < 3) {
    lastLogicTick += LOGIC_INTERVAL_MS;
    logicTick();
    logicCatchup++;
    now = millis();
  }

  if (now - lastRenderTick >= RENDER_INTERVAL_MS) {
    lastRenderTick = now;
    renderTick();
  }

  if (now - lastSaveTick >= 5000) {
    lastSaveTick = now;
    saveState();
    saveLearnState();
  }

  delay(0);
}
