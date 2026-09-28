#pragma once
// Game_Tank.ino
// Drop this file INSIDE the Jakeboy_Arcade folder.
// It self-registers with the JakeBoy launcher; no launcher edits are required.
//
// TANK
// First-person 2.5D tank campaign for ESP32 / ST7789 240x320 portrait.
//
// Controls:
//   LEFT 3-position switch  = left tread forward / neutral / reverse
//   RIGHT 3-position switch = right tread forward / neutral / reverse
//   LEFT button             = machine gun (bursts)
//   RIGHT button            = cannon
//
// Integration contract:
//   - Uses the shared GameInput button state/edges from GameAPI.h.
//   - Reads the latching tread switches directly from Hardware.h because their
//     positions are continuous gameplay controls rather than one-shot menu edges.
//   - Uses the launcher-owned display object, already initialized in setup().
//   - Uses millis()-driven updates only; there are no delay() calls.
//   - All game state is isolated inside namespace TankGame.

#include "GameAPI.h"
#include "Hardware.h"
#include <Adafruit_ST7789.h>
#include <math.h>

namespace TankGame {

// ============================================================
// DISPLAY / BASIC CONSTANTS
// ============================================================

Adafruit_ST7789 &screen = ::display;

constexpr int SCREEN_W = 240;
constexpr int SCREEN_H = 320;
constexpr int VIEW_TOP = 24;
constexpr int VIEW_BOTTOM = 278;
constexpr int VIEW_H = VIEW_BOTTOM - VIEW_TOP;
constexpr int HORIZON_Y = 132;
constexpr int CENTER_X = SCREEN_W / 2;

constexpr float PI_F = 3.14159265358979323846f;
constexpr float TWO_PI_F = PI_F * 2.0f;
constexpr float DEG2RAD = PI_F / 180.0f;
constexpr float FOV = 70.0f * DEG2RAD;
constexpr float FOCAL = 170.0f;
constexpr float NEAR_CLIP = 0.8f;

constexpr uint32_t FRAME_MS = 20;  // up to 50 FPS render target
constexpr uint32_t SIM_MS = 33;    // ~30 Hz simulation
constexpr float SIM_DT = 0.033f;

// Camera is mounted at roughly tank/turret eye height, not high above the map.
constexpr float CAMERA_HEIGHT = 2.2f;

// ============================================================
// TUNING
// ============================================================

constexpr int STARTING_LIVES = 10;
constexpr int PLAYER_MAX_HEALTH = 100;
constexpr float PLAYER_RADIUS = 1.5f;
constexpr float PLAYER_MAX_FORWARD = 8.0f;
constexpr float PLAYER_MAX_REVERSE = 4.5f;
constexpr float TREAD_ACCEL = 6.0f;
constexpr float TREAD_BRAKE = 9.0f;
constexpr float TREAD_SEPARATION = 4.5f;

constexpr float CANNON_RELOAD = 1.60f;
constexpr int CANNON_DAMAGE = 50;
constexpr float CANNON_RANGE = 220.0f;
constexpr float CANNON_SPLASH_RADIUS = 4.0f;

constexpr int MG_BURST_SHOTS = 4;
constexpr float MG_SHOT_INTERVAL = 0.09f;
constexpr float MG_BURST_PAUSE = 0.35f;
constexpr float MG_RANGE = 150.0f;
constexpr float MG_SPREAD = 2.5f * DEG2RAD;
constexpr int MG_TANK_DAMAGE = 2;

constexpr float RESPAWN_INVULN = 3.0f;

constexpr int MAX_TROOPS = 18;
constexpr int MAX_TANKS = 10;
constexpr int MAX_SHELLS = 10;
constexpr int MAX_OBSTACLES = 48;
constexpr int MAX_EFFECTS = 18;
constexpr int MAX_MISSILES = 4;

constexpr int LEG_HEALTH = 150;
constexpr float BOSS_BASE_SPEED = 4.0f;
constexpr float BOSS_TWO_LEG_SPEED = 3.4f;
constexpr float BOSS_ONE_LEG_SPEED = 2.7f;
constexpr float MISSILE_WARNING = 3.5f;
constexpr float MISSILE_RADIUS = 6.0f;

constexpr uint16_t COL_SKY = 0x6D3D;
constexpr uint16_t COL_GROUND = 0x6A64;
constexpr uint16_t COL_SAND = 0xD5C7;
constexpr uint16_t COL_EARTH = 0x7B64;
constexpr uint16_t COL_ROAD = 0x528A;
constexpr uint16_t COL_STONE = 0x8C71;
constexpr uint16_t COL_DARK = 0x2945;
constexpr uint16_t COL_BOSS = 0x632C;
constexpr uint16_t COL_TANK = 0x4B44;
constexpr uint16_t COL_TROOP = 0xFFFF;
constexpr uint16_t COL_WARNING = ST77XX_RED;
constexpr uint16_t COL_MARKER = ST77XX_RED;
constexpr uint16_t COL_HUD = ST77XX_WHITE;

// ============================================================
// MATH
// ============================================================

struct Vec2 {
  float x;
  float y;
};

static Vec2 add(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
static Vec2 sub(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
static Vec2 mul(Vec2 a, float s) { return {a.x * s, a.y * s}; }
static float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
static float len2(Vec2 a) { return dot(a, a); }
static float length(Vec2 a) { return sqrtf(len2(a)); }

static Vec2 normalize(Vec2 a) {
  float l = length(a);
  return l > 0.0001f ? mul(a, 1.0f / l) : Vec2{0, 0};
}

static float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

static float approach(float current, float target, float amount) {
  if (current < target) return fminf(current + amount, target);
  if (current > target) return fmaxf(current - amount, target);
  return current;
}

static float wrapAngle(float a) {
  while (a < 0) a += TWO_PI_F;
  while (a >= TWO_PI_F) a -= TWO_PI_F;
  return a;
}

static Vec2 forwardFromHeading(float h) {
  return {sinf(h), cosf(h)};
}

static Vec2 rightFromHeading(float h) {
  return {cosf(h), -sinf(h)};
}

static float distancePointSegment(Vec2 p, Vec2 a, Vec2 b) {
  Vec2 ab = sub(b, a);
  float denom = len2(ab);
  float t = denom > 0.0001f ? dot(sub(p, a), ab) / denom : 0.0f;
  t = clampf(t, 0.0f, 1.0f);
  return length(sub(p, add(a, mul(ab, t))));
}

static bool circlesOverlap(Vec2 a, float ar, Vec2 b, float br) {
  float rr = ar + br;
  return len2(sub(a, b)) < rr * rr;
}

// ============================================================
// DETERMINISTIC RNG
// ============================================================

uint32_t rngState = 0x9E3779B9u;

static uint32_t tankRandom() {
  uint32_t x = rngState;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  rngState = x;
  return x;
}

static float random01() {
  return (tankRandom() & 0x00FFFFFFu) / 16777215.0f;
}

static float randomRange(float a, float b) {
  return a + (b - a) * random01();
}

// ============================================================
// TYPES
// ============================================================

enum class TreadCommand : int8_t {
  Reverse = -1,
  Neutral = 0,
  Forward = 1
};

enum class GameMode : uint8_t {
  Intro,
  Playing,
  PlayerDestroyed,
  GameOver,
  Victory
};

enum class CampaignZone : uint8_t {
  Beach,
  Trenches,
  Town,
  City,
  Pursuit
};

enum class TankKind : uint8_t {
  Holder,
  Pursuer,
  Maneuverer,
  Heavy,
  Light
};

enum class TankAIState : uint8_t {
  Idle,
  Approach,
  Maneuver,
  Aim,
  Recover
};

enum class BossPhase : uint8_t {
  Inactive,
  Pursuit,
  Collapsing,
  ReleasedTanks,
  CoreOpening,
  CoreExposed,
  Destroyed
};

enum class ObstacleType : uint8_t {
  Circle,
  Segment
};

enum class EffectType : uint8_t {
  Spark,
  Explosion,
  Muzzle,
  Dust
};

struct TreadState {
  TreadCommand command;
  float speed;
};

struct Player {
  Vec2 position;
  Vec2 lastSafePosition;
  float heading;
  TreadState left;
  TreadState right;
  int health;
  int lives;
  float cannonCooldown;
  float invuln;
  float destroyedTimer;
  float mgShotTimer;
  float mgPauseTimer;
  int mgShotsRemaining;
  bool mgBurstActive;
};

struct Troop {
  bool active;
  Vec2 position;
  Vec2 velocity;
  float phase;
};

struct EnemyTank {
  bool active;
  TankKind kind;
  TankAIState state;
  Vec2 position;
  Vec2 home;
  float heading;
  float speed;
  float fireCooldown;
  float stateTimer;
  int health;
  bool bossReleased;
};

struct EnemyShell {
  bool active;
  Vec2 position;
  Vec2 velocity;
  float life;
  int damage;
};

struct Obstacle {
  bool active;
  ObstacleType type;
  Vec2 a;
  Vec2 b;
  float radius;
  bool solid;
  bool blocksShots;
  uint16_t color;
  float height;
};

struct Effect {
  bool active;
  EffectType type;
  Vec2 position;
  float age;
  float life;
  float size;
};

struct FortressLeg {
  int health;
  bool destroyed;
  float gaitPhase;
};

struct Missile {
  bool active;
  Vec2 target;
  float countdown;
};

struct Boss {
  BossPhase phase;
  Vec2 position;
  FortressLeg legs[3];
  float northSpeed;
  float gaitTime;
  float missileTimer;
  float collapseTimer;
  int releasedRemaining;
  int releasedSpawned;
  float releasedSpawnTimer;
  float coreOpenTimer;
  Vec2 corePosition;
};

struct Projection {
  bool visible;
  int x;
  int groundY;
  float depth;
  float scale;
};

struct ShotHit {
  bool hit;
  float distance;
  uint8_t kind; // 1 obstacle, 2 troop, 3 tank, 4 leg, 5 core
  int index;
  Vec2 point;
};

struct GameState {
  GameMode mode;
  CampaignZone zone;
  Player player;
  Boss boss;
  Troop troops[MAX_TROOPS];
  EnemyTank tanks[MAX_TANKS];
  EnemyShell shells[MAX_SHELLS];
  Obstacle obstacles[MAX_OBSTACLES];
  Effect effects[MAX_EFFECTS];
  Missile missiles[MAX_MISSILES];
  uint32_t lastUpdateMs;
  uint32_t accumulatorMs;
  uint32_t lastFrameMs;
  uint32_t introStartMs;
  uint32_t modeEnteredMs;
  float markerRotation;
  float damageFlash;
  float shakeTimer;
  float shakeMagnitude;
  bool restartArmed;
  int zoneSerial;
};

GameState g;

// ============================================================
// FORWARD DECLARATIONS
// ============================================================

void startNewRun();
void loadZone(CampaignZone zone);
void renderFrame();
void updateSimulation(float dt, const GameInput& input);
void fireCannon();
void beginMGBurst();
void fireMGShot();
void damagePlayer(int amount);
void destroyPlayer();
void respawnPlayer();
void beginBossEncounter();
void beginCollapse();
void destroyCore();

// ============================================================
// AUDIO (NONBLOCKING)
// ============================================================

struct ToneChannel {
  int pin;
  uint32_t untilMs;
  uint16_t freq;
};

constexpr int TANK_BUZZER_LEFT_PIN = 21;
constexpr int TANK_BUZZER_RIGHT_PIN = 22;
constexpr int TANK_LEFT_UP_PIN = 33;
constexpr int TANK_LEFT_DOWN_PIN = 32;
constexpr int TANK_RIGHT_UP_PIN = 26;
constexpr int TANK_RIGHT_DOWN_PIN = 25;

ToneChannel toneL = {TANK_BUZZER_LEFT_PIN, 0, 0};
ToneChannel toneR = {TANK_BUZZER_RIGHT_PIN, 0, 0};

static void startTone(ToneChannel& c, uint16_t freq, uint16_t durationMs) {
  c.freq = freq;
  c.untilMs = millis() + durationMs;
  ledcWriteTone(c.pin, freq);
}

static void updateAudio() {
  uint32_t now = millis();
  if (toneL.freq && (int32_t)(now - toneL.untilMs) >= 0) {
    ledcWriteTone(toneL.pin, 0);
    toneL.freq = 0;
  }
  if (toneR.freq && (int32_t)(now - toneR.untilMs) >= 0) {
    ledcWriteTone(toneR.pin, 0);
    toneR.freq = 0;
  }
}

static void soundMG() { startTone(toneL, 1000, 24); }
static void soundCannon() {
  startTone(toneL, 130, 100);
  startTone(toneR, 95, 120);
}
static void soundHit() { startTone(toneR, 220, 80); }
static void soundWarn() { startTone(toneR, 720, 70); }
static void soundExplosion() {
  startTone(toneL, 90, 150);
  startTone(toneR, 125, 170);
}

// ============================================================
// POOL HELPERS
// ============================================================

static void clearPools() {
  for (int i = 0; i < MAX_TROOPS; ++i) g.troops[i].active = false;
  for (int i = 0; i < MAX_TANKS; ++i) g.tanks[i].active = false;
  for (int i = 0; i < MAX_SHELLS; ++i) g.shells[i].active = false;
  for (int i = 0; i < MAX_OBSTACLES; ++i) g.obstacles[i].active = false;
  for (int i = 0; i < MAX_EFFECTS; ++i) g.effects[i].active = false;
  for (int i = 0; i < MAX_MISSILES; ++i) g.missiles[i].active = false;
}

static int spawnObstacleCircle(Vec2 p, float radius, float height, uint16_t color, bool solid=true, bool blocks=true) {
  for (int i = 0; i < MAX_OBSTACLES; ++i) {
    if (!g.obstacles[i].active) {
      g.obstacles[i] = {true, ObstacleType::Circle, p, p, radius, solid, blocks, color, height};
      return i;
    }
  }
  return -1;
}

static int spawnObstacleSegment(Vec2 a, Vec2 b, float height, uint16_t color, bool solid=true, bool blocks=true) {
  for (int i = 0; i < MAX_OBSTACLES; ++i) {
    if (!g.obstacles[i].active) {
      g.obstacles[i] = {true, ObstacleType::Segment, a, b, 0.0f, solid, blocks, color, height};
      return i;
    }
  }
  return -1;
}

static int spawnTroop(Vec2 p) {
  for (int i = 0; i < MAX_TROOPS; ++i) {
    if (!g.troops[i].active) {
      g.troops[i].active = true;
      g.troops[i].position = p;
      g.troops[i].velocity = {0, 0};
      g.troops[i].phase = randomRange(0, TWO_PI_F);
      return i;
    }
  }
  return -1;
}

static int tankHealthForKind(TankKind k) {
  switch (k) {
    case TankKind::Heavy: return 150;
    case TankKind::Light: return 60;
    default: return 100;
  }
}

static int spawnTank(Vec2 p, TankKind kind, bool bossReleased=false) {
  for (int i = 0; i < MAX_TANKS; ++i) {
    if (!g.tanks[i].active) {
      EnemyTank &t = g.tanks[i];
      t.active = true;
      t.kind = kind;
      t.state = TankAIState::Idle;
      t.position = p;
      t.home = p;
      t.heading = PI_F;
      t.speed = 0;
      t.fireCooldown = randomRange(1.0f, 2.0f);
      t.stateTimer = 0;
      t.health = tankHealthForKind(kind);
      t.bossReleased = bossReleased;
      return i;
    }
  }
  return -1;
}

static int spawnEffect(EffectType type, Vec2 p, float life, float size) {
  int oldest = -1;
  float oldestAge = -1;
  for (int i = 0; i < MAX_EFFECTS; ++i) {
    if (!g.effects[i].active) {
      g.effects[i] = {true, type, p, 0, life, size};
      return i;
    }
    if (g.effects[i].age > oldestAge) {
      oldestAge = g.effects[i].age;
      oldest = i;
    }
  }
  if (oldest >= 0) {
    g.effects[oldest] = {true, type, p, 0, life, size};
  }
  return oldest;
}

static int spawnEnemyShell(Vec2 p, Vec2 velocity, int damage) {
  for (int i = 0; i < MAX_SHELLS; ++i) {
    if (!g.shells[i].active) {
      g.shells[i] = {true, p, velocity, 5.0f, damage};
      return i;
    }
  }
  return -1;
}

// ============================================================
// COLLISION / RAY HELPERS
// ============================================================

static bool circleHitsObstacle(Vec2 p, float radius, const Obstacle& o) {
  if (!o.active || !o.solid) return false;
  if (o.type == ObstacleType::Circle) return circlesOverlap(p, radius, o.a, o.radius);
  return distancePointSegment(p, o.a, o.b) < radius;
}

static bool canOccupy(Vec2 p, float radius, bool includeTanks=true) {
  for (int i = 0; i < MAX_OBSTACLES; ++i) {
    if (circleHitsObstacle(p, radius, g.obstacles[i])) return false;
  }
  if (includeTanks) {
    for (int i = 0; i < MAX_TANKS; ++i) {
      if (g.tanks[i].active && circlesOverlap(p, radius, g.tanks[i].position, 1.55f)) return false;
    }
  }
  if (g.boss.phase == BossPhase::Pursuit) {
    static const Vec2 legOffsets[3] = {{-9,-1},{9,-1},{0,11}};
    for (int i = 0; i < 3; ++i) {
      if (g.boss.legs[i].destroyed) continue;
      Vec2 lp = add(g.boss.position, legOffsets[i]);
      if (circlesOverlap(p, radius, lp, 4.7f)) return false;
    }
  }
  return true;
}

static Vec2 moveWithCollision(Vec2 current, Vec2 delta, float radius) {
  Vec2 desired = add(current, delta);
  if (canOccupy(desired, radius)) return desired;

  Vec2 xOnly = {desired.x, current.y};
  if (canOccupy(xOnly, radius)) return xOnly;

  Vec2 yOnly = {current.x, desired.y};
  if (canOccupy(yOnly, radius)) return yOnly;

  return current;
}

static float rayCircle(Vec2 origin, Vec2 dir, Vec2 center, float radius) {
  Vec2 m = sub(origin, center);
  float b = dot(m, dir);
  float c = dot(m, m) - radius * radius;
  if (c > 0.0f && b > 0.0f) return -1;
  float disc = b*b - c;
  if (disc < 0) return -1;
  float t = -b - sqrtf(disc);
  if (t < 0) t = 0;
  return t;
}

static float cross2(Vec2 a, Vec2 b) { return a.x*b.y - a.y*b.x; }

static float raySegment(Vec2 origin, Vec2 dir, Vec2 a, Vec2 b) {
  Vec2 v1 = sub(origin, a);
  Vec2 v2 = sub(b, a);
  float den = cross2(dir, v2);
  if (fabsf(den) < 0.0001f) return -1;
  float t = cross2(v2, v1) / den;
  float u = cross2(dir, v1) / den;
  if (t >= 0 && u >= 0 && u <= 1) return t;
  return -1;
}

static bool lineBlocked(Vec2 a, Vec2 b) {
  Vec2 d = sub(b, a);
  float total = length(d);
  if (total < 0.01f) return false;
  d = mul(d, 1.0f / total);
  for (int i = 0; i < MAX_OBSTACLES; ++i) {
    Obstacle &o = g.obstacles[i];
    if (!o.active || !o.blocksShots) continue;
    float t = (o.type == ObstacleType::Circle)
      ? rayCircle(a, d, o.a, o.radius)
      : raySegment(a, d, o.a, o.b);
    if (t >= 0 && t < total) return true;
  }
  return false;
}

// ============================================================
// INPUT
// ============================================================

static TreadCommand readTread(int upPin, int downPin) {
  bool up = digitalRead(upPin) == LOW;
  bool down = digitalRead(downPin) == LOW;
  if (up && down) return TreadCommand::Neutral;

  // The physical switches are mounted electrically opposite the logical
  // tank direction: DOWN contact is the player's forward position and UP
  // contact is reverse. Keep that correction local to Tank.
  if (down) return TreadCommand::Forward;
  if (up) return TreadCommand::Reverse;
  return TreadCommand::Neutral;
}

static float commandSpeed(TreadCommand c) {
  if (c == TreadCommand::Forward) return PLAYER_MAX_FORWARD;
  if (c == TreadCommand::Reverse) return -PLAYER_MAX_REVERSE;
  return 0;
}

static void updateTread(TreadState& tread, TreadCommand command, float dt) {
  tread.command = command;
  float target = commandSpeed(command);
  float accel = TREAD_ACCEL;

  if ((tread.speed > 0 && target <= 0) || (tread.speed < 0 && target >= 0)) {
    accel = TREAD_BRAKE;
  }

  tread.speed = approach(tread.speed, target, accel * dt);
}

// ============================================================
// CAMPAIGN / LEVELS
// ============================================================

static void scatterTroops(int count, float yMin, float yMax, float xMin, float xMax) {
  for (int i = 0; i < count; ++i) {
    spawnTroop({randomRange(xMin, xMax), randomRange(yMin, yMax)});
  }
}

static void loadBeach() {
  spawnObstacleSegment({-22,28},{-8,28},2.5f,COL_DARK);
  spawnObstacleSegment({8,28},{22,28},2.5f,COL_DARK);
  spawnObstacleCircle({-12,48},2.0f,2.0f,COL_STONE);
  spawnObstacleCircle({13,58},2.5f,2.2f,COL_STONE);
  spawnObstacleSegment({-25,76},{-7,76},2.7f,COL_DARK);
  spawnObstacleSegment({6,82},{25,82},2.7f,COL_DARK);
  scatterTroops(8, 30, 105, -20, 20);
  spawnTank({10,96}, TankKind::Holder);
}

static void loadTrenches() {
  spawnObstacleSegment({-26,24},{-8,24},3.0f,COL_EARTH);
  spawnObstacleSegment({5,35},{26,35},3.0f,COL_EARTH);
  spawnObstacleSegment({-26,68},{-2,68},3.0f,COL_EARTH);
  spawnObstacleSegment({7,78},{26,78},3.0f,COL_EARTH);
  spawnObstacleSegment({-26,112},{-7,112},3.0f,COL_EARTH);
  spawnObstacleSegment({5,120},{26,120},3.0f,COL_EARTH);
  scatterTroops(9, 24, 128, -21, 21);
  spawnTank({-12,61}, TankKind::Pursuer);
  spawnTank({12,104}, TankKind::Maneuverer);
}

static void loadTown() {
  spawnObstacleSegment({-26,25},{-11,25},8.0f,COL_STONE);
  spawnObstacleSegment({11,25},{26,25},8.0f,COL_STONE);
  spawnObstacleSegment({-26,55},{-5,55},9.0f,COL_STONE);
  spawnObstacleSegment({8,70},{26,70},10.0f,COL_STONE);
  spawnObstacleSegment({-26,100},{-10,100},8.0f,COL_STONE);
  spawnObstacleSegment({9,111},{26,111},11.0f,COL_STONE);
  spawnObstacleCircle({-7,79},3.0f,2.0f,COL_DARK);
  spawnObstacleCircle({8,88},3.0f,2.0f,COL_DARK);
  scatterTroops(7, 28, 120, -20, 20);
  spawnTank({-14,45}, TankKind::Holder);
  spawnTank({13,91}, TankKind::Light);
  spawnTank({0,124}, TankKind::Maneuverer);
}

static void loadCity() {
  spawnObstacleSegment({-27,20},{-8,20},14.0f,COL_STONE);
  spawnObstacleSegment({9,20},{27,20},13.0f,COL_STONE);
  spawnObstacleSegment({-27,51},{-3,51},16.0f,COL_DARK);
  spawnObstacleSegment({7,67},{27,67},18.0f,COL_STONE);
  spawnObstacleSegment({-27,94},{-11,94},18.0f,COL_STONE);
  spawnObstacleSegment({4,104},{27,104},15.0f,COL_DARK);
  spawnObstacleCircle({-5,74},4.0f,3.0f,COL_DARK);
  spawnObstacleCircle({9,119},3.0f,3.0f,COL_DARK);
  scatterTroops(6, 25, 125, -20, 20);
  spawnTank({12,43}, TankKind::Heavy);
  spawnTank({-12,88}, TankKind::Maneuverer);
  spawnTank({4,126}, TankKind::Pursuer);
}

static void loadPursuit() {
  // Sparse, regenerating open terrain. The boss encounter is the level.
  spawnObstacleCircle({-20,30},2.0f,1.5f,COL_STONE,false,false);
  spawnObstacleCircle({18,60},2.5f,1.8f,COL_STONE,false,false);
  spawnObstacleCircle({-16,95},2.0f,1.5f,COL_STONE,false,false);
  beginBossEncounter();
}

void loadZone(CampaignZone zone) {
  for (int i = 0; i < MAX_TROOPS; ++i) g.troops[i].active = false;
  for (int i = 0; i < MAX_TANKS; ++i) g.tanks[i].active = false;
  for (int i = 0; i < MAX_SHELLS; ++i) g.shells[i].active = false;
  for (int i = 0; i < MAX_OBSTACLES; ++i) g.obstacles[i].active = false;
  for (int i = 0; i < MAX_MISSILES; ++i) g.missiles[i].active = false;

  g.zone = zone;
  g.zoneSerial++;

  if (zone == CampaignZone::Beach) loadBeach();
  else if (zone == CampaignZone::Trenches) loadTrenches();
  else if (zone == CampaignZone::Town) loadTown();
  else if (zone == CampaignZone::City) loadCity();
  else loadPursuit();
}

static void transitionZone(CampaignZone next) {
  // Preserve campaign survival state, but move the player to the south entrance.
  g.player.position = {0, 7};
  g.player.lastSafePosition = g.player.position;
  if (fabsf(g.player.heading) > PI_F * 0.75f && fabsf(g.player.heading) < PI_F * 1.25f) {
    g.player.heading = 0;
  }
  loadZone(next);
}

static void updateCampaign() {
  if (g.zone == CampaignZone::Pursuit) return;
  if (g.player.position.y < 142.0f) return;

  if (g.zone == CampaignZone::Beach) transitionZone(CampaignZone::Trenches);
  else if (g.zone == CampaignZone::Trenches) transitionZone(CampaignZone::Town);
  else if (g.zone == CampaignZone::Town) transitionZone(CampaignZone::City);
  else if (g.zone == CampaignZone::City) transitionZone(CampaignZone::Pursuit);
}

// ============================================================
// PLAYER
// ============================================================

static void updatePlayerMovement(float dt) {
  Player &p = g.player;
  updateTread(p.left, readTread(TANK_LEFT_UP_PIN, TANK_LEFT_DOWN_PIN), dt);
  updateTread(p.right, readTread(TANK_RIGHT_UP_PIN, TANK_RIGHT_DOWN_PIN), dt);

  float linear = (p.left.speed + p.right.speed) * 0.5f;
  float angular = (p.right.speed - p.left.speed) / TREAD_SEPARATION;

  p.heading = wrapAngle(p.heading + angular * dt);

  Vec2 delta = mul(forwardFromHeading(p.heading), linear * dt);
  Vec2 before = p.position;
  p.position = moveWithCollision(p.position, delta, PLAYER_RADIUS);

  // Troops never block the player.
  for (int i = 0; i < MAX_TROOPS; ++i) {
    if (g.troops[i].active && circlesOverlap(p.position, PLAYER_RADIUS, g.troops[i].position, 0.4f)) {
      g.troops[i].active = false;
      spawnEffect(EffectType::Dust, g.troops[i].position, 0.35f, 1.0f);
    }
  }

  if (length(sub(p.position, before)) > 0.001f && canOccupy(p.position, PLAYER_RADIUS)) {
    bool safe = true;
    if (g.boss.phase == BossPhase::Pursuit && length(sub(p.position, g.boss.position)) < 22) safe = false;
    for (int i = 0; i < MAX_MISSILES; ++i) {
      if (g.missiles[i].active && length(sub(p.position, g.missiles[i].target)) < MISSILE_RADIUS + 2) safe = false;
    }
    if (safe) p.lastSafePosition = p.position;
  }
}

void damagePlayer(int amount) {
  if (g.mode != GameMode::Playing || g.player.invuln > 0) return;
  g.player.health -= amount;
  g.damageFlash = 0.22f;
  g.shakeTimer = 0.2f;
  g.shakeMagnitude = 2.2f;
  soundHit();
  if (g.player.health <= 0) destroyPlayer();
}

void destroyPlayer() {
  if (g.mode != GameMode::Playing) return;
  g.mode = GameMode::PlayerDestroyed;
  g.modeEnteredMs = millis();
  g.player.destroyedTimer = 1.2f;
  g.player.left.speed = 0;
  g.player.right.speed = 0;
  g.player.mgBurstActive = false;
  spawnEffect(EffectType::Explosion, g.player.position, 0.9f, 5.0f);
  soundExplosion();
}

void respawnPlayer() {
  g.player.lives--;
  if (g.player.lives <= 0) {
    g.mode = GameMode::GameOver;
    g.modeEnteredMs = millis();
    g.restartArmed = false;
    return;
  }

  Vec2 candidates[7] = {
    g.player.lastSafePosition,
    add(g.player.lastSafePosition,{5,0}),
    add(g.player.lastSafePosition,{-5,0}),
    add(g.player.lastSafePosition,{0,-6}),
    add(g.player.lastSafePosition,{5,-6}),
    add(g.player.lastSafePosition,{-5,-6}),
    {0,7}
  };

  Vec2 chosen = {0,7};
  for (int i=0;i<7;i++) {
    if (canOccupy(candidates[i], PLAYER_RADIUS)) {
      chosen = candidates[i];
      break;
    }
  }

  g.player.position = chosen;
  g.player.health = PLAYER_MAX_HEALTH;
  g.player.invuln = RESPAWN_INVULN;
  g.player.cannonCooldown = 0;
  g.player.mgBurstActive = false;
  g.player.mgShotsRemaining = 0;
  g.mode = GameMode::Playing;
  g.modeEnteredMs = millis();
}

// ============================================================
// SHOT TRACE / WEAPONS
// ============================================================

static ShotHit traceShot(Vec2 origin, Vec2 dir, float maxDistance, bool cannon) {
  ShotHit h = {false, maxDistance, 0, -1, add(origin, mul(dir, maxDistance))};

  for (int i=0;i<MAX_OBSTACLES;i++) {
    Obstacle &o = g.obstacles[i];
    if (!o.active || !o.blocksShots) continue;
    float t = o.type == ObstacleType::Circle
      ? rayCircle(origin, dir, o.a, o.radius)
      : raySegment(origin, dir, o.a, o.b);
    if (t >= 0 && t < h.distance) {
      h = {true,t,1,i,add(origin,mul(dir,t))};
    }
  }

  for (int i=0;i<MAX_TROOPS;i++) {
    if (!g.troops[i].active) continue;
    float t = rayCircle(origin,dir,g.troops[i].position,0.55f);
    if (t >= 0 && t < h.distance) h = {true,t,2,i,add(origin,mul(dir,t))};
  }

  for (int i=0;i<MAX_TANKS;i++) {
    if (!g.tanks[i].active) continue;
    float t = rayCircle(origin,dir,g.tanks[i].position,1.7f);
    if (t >= 0 && t < h.distance) h = {true,t,3,i,add(origin,mul(dir,t))};
  }

  if (g.boss.phase == BossPhase::Pursuit) {
    static const Vec2 legOffsets[3] = {{-9,-1},{9,-1},{0,11}};
    for (int i=0;i<3;i++) {
      if (g.boss.legs[i].destroyed) continue;
      Vec2 p = add(g.boss.position, legOffsets[i]);
      float t = rayCircle(origin,dir,p,2.6f); // vulnerable lower joint
      if (t >= 0 && t < h.distance) h = {true,t,4,i,add(origin,mul(dir,t))};
    }
  }

  if (g.boss.phase == BossPhase::CoreExposed) {
    float t = rayCircle(origin,dir,g.boss.corePosition,1.8f);
    if (t >= 0 && t < h.distance) h = {true,t,5,0,add(origin,mul(dir,t))};
  }

  return h;
}

static void destroyTank(int i) {
  if (i < 0 || i >= MAX_TANKS || !g.tanks[i].active) return;
  bool wasBoss = g.tanks[i].bossReleased;
  Vec2 p = g.tanks[i].position;
  g.tanks[i].active = false;
  spawnEffect(EffectType::Explosion,p,0.8f,4.0f);
  soundExplosion();

  if (wasBoss && g.boss.phase == BossPhase::ReleasedTanks) {
    if (g.boss.releasedRemaining > 0) g.boss.releasedRemaining--;
    if (g.boss.releasedRemaining == 0 && g.boss.releasedSpawned >= 5) {
      g.boss.phase = BossPhase::CoreOpening;
      g.boss.coreOpenTimer = 0;
      startTone(toneR, 410, 150);
    }
  }
}

static void damageTank(int i, int damage) {
  if (i < 0 || i >= MAX_TANKS || !g.tanks[i].active) return;
  g.tanks[i].health -= damage;
  spawnEffect(EffectType::Spark,g.tanks[i].position,0.18f,1.0f);
  if (g.tanks[i].health <= 0) destroyTank(i);
}

static void damageBossLeg(int i, int damage) {
  if (i < 0 || i >= 3 || g.boss.legs[i].destroyed) return;
  g.boss.legs[i].health -= damage;
  if (g.boss.legs[i].health <= 0) {
    g.boss.legs[i].destroyed = true;
    static const Vec2 legOffsets[3] = {{-9,-1},{9,-1},{0,11}};
    spawnEffect(EffectType::Explosion,add(g.boss.position,legOffsets[i]),1.1f,6.0f);
    soundExplosion();

    int alive = 0;
    for (int j=0;j<3;j++) if (!g.boss.legs[j].destroyed) alive++;
    if (alive == 2) g.boss.northSpeed = BOSS_TWO_LEG_SPEED;
    else if (alive == 1) g.boss.northSpeed = BOSS_ONE_LEG_SPEED;
    else beginCollapse();
  }
}

static void applyCannonSplash(Vec2 p) {
  for (int i=0;i<MAX_TROOPS;i++) {
    if (g.troops[i].active && length(sub(g.troops[i].position,p)) <= CANNON_SPLASH_RADIUS) {
      g.troops[i].active = false;
    }
  }
  for (int i=0;i<MAX_TANKS;i++) {
    if (g.tanks[i].active && length(sub(g.tanks[i].position,p)) <= CANNON_SPLASH_RADIUS) {
      damageTank(i, 20);
    }
  }
}

void fireCannon() {
  if (g.player.cannonCooldown > 0 || g.mode != GameMode::Playing) return;
  g.player.cannonCooldown = CANNON_RELOAD;
  Vec2 dir = forwardFromHeading(g.player.heading);
  ShotHit h = traceShot(g.player.position,dir,CANNON_RANGE,true);

  soundCannon();
  g.shakeTimer = 0.12f;
  g.shakeMagnitude = 1.2f;
  spawnEffect(EffectType::Muzzle,add(g.player.position,mul(dir,2.0f)),0.12f,1.0f);

  if (h.hit) {
    spawnEffect(EffectType::Explosion,h.point,0.35f,2.2f);
    if (h.kind == 2) g.troops[h.index].active = false;
    else if (h.kind == 3) damageTank(h.index,CANNON_DAMAGE);
    else if (h.kind == 4) damageBossLeg(h.index,CANNON_DAMAGE);
    else if (h.kind == 5) destroyCore();
    applyCannonSplash(h.point);
  }
}

void beginMGBurst() {
  if (g.player.mgBurstActive || g.player.mgPauseTimer > 0) return;
  g.player.mgBurstActive = true;
  g.player.mgShotsRemaining = MG_BURST_SHOTS;
  g.player.mgShotTimer = 0;
}

void fireMGShot() {
  float angle = g.player.heading + randomRange(-MG_SPREAD,MG_SPREAD);
  Vec2 dir = forwardFromHeading(angle);
  ShotHit h = traceShot(g.player.position,dir,MG_RANGE,false);
  soundMG();
  if (!h.hit) return;
  spawnEffect(EffectType::Spark,h.point,0.10f,0.5f);
  if (h.kind == 2) g.troops[h.index].active = false;
  else if (h.kind == 3) damageTank(h.index,MG_TANK_DAMAGE);
  // Boss legs/core intentionally ignore MG.
}

static void updateWeapons(float dt, const GameInput& input) {
  Player &p = g.player;
  p.cannonCooldown = fmaxf(0,p.cannonCooldown-dt);
  p.mgPauseTimer = fmaxf(0,p.mgPauseTimer-dt);

  if (input.rightPressed) fireCannon();
  if (input.leftPressed) beginMGBurst();

  if (!p.mgBurstActive && input.leftButton && p.mgPauseTimer <= 0) {
    beginMGBurst();
  }

  if (p.mgBurstActive) {
    p.mgShotTimer -= dt;
    if (p.mgShotTimer <= 0 && p.mgShotsRemaining > 0) {
      fireMGShot();
      p.mgShotsRemaining--;
      p.mgShotTimer += MG_SHOT_INTERVAL;
    }
    if (p.mgShotsRemaining <= 0) {
      p.mgBurstActive = false;
      p.mgPauseTimer = MG_BURST_PAUSE;
    }
  }
}

// ============================================================
// TROOPS
// ============================================================

static void updateTroops(float dt) {
  for (int i=0;i<MAX_TROOPS;i++) {
    Troop &t = g.troops[i];
    if (!t.active) continue;

    Vec2 away = sub(t.position,g.player.position);
    float d = length(away);
    t.phase += dt;

    if (d < 18.0f) {
      Vec2 right = rightFromHeading(g.player.heading);
      float side = dot(away,right) >= 0 ? 1.0f : -1.0f;
      t.velocity = mul(right,side * 3.2f);
    } else {
      t.velocity = {sinf(t.phase*0.7f)*0.4f, 0.8f};
    }

    Vec2 desired = add(t.position,mul(t.velocity,dt));
    bool blocked=false;
    for(int j=0;j<MAX_OBSTACLES;j++){
      if(circleHitsObstacle(desired,0.35f,g.obstacles[j])) { blocked=true; break; }
    }
    if(!blocked) t.position=desired;
  }
}

// ============================================================
// ENEMY TANK AI
// ============================================================

static float tankMoveSpeed(TankKind k) {
  if (k == TankKind::Light) return 4.5f;
  if (k == TankKind::Heavy) return 2.2f;
  return 3.0f;
}

static float tankAimTime(TankKind k) {
  if (k == TankKind::Heavy) return 1.15f;
  if (k == TankKind::Light) return 0.60f;
  return 0.85f;
}

static int tankDamage(TankKind k) {
  if (k == TankKind::Heavy) return 42;
  if (k == TankKind::Light) return 20;
  return 28;
}

static void fireEnemyShell(EnemyTank& t) {
  Vec2 toPlayer = sub(g.player.position,t.position);
  Vec2 dir = normalize(toPlayer);
  float speed = t.kind == TankKind::Heavy ? 12.0f : 15.0f;
  spawnEnemyShell(add(t.position,mul(dir,2.0f)),mul(dir,speed),tankDamage(t.kind));
  startTone(toneR, 600, 55);
}

static void updateEnemyTank(EnemyTank& t, float dt) {
  Vec2 toPlayer = sub(g.player.position,t.position);
  float d = length(toPlayer);
  t.fireCooldown = fmaxf(0,t.fireCooldown-dt);
  t.stateTimer -= dt;

  float desiredHeading = atan2f(toPlayer.x,toPlayer.y);
  float diff = desiredHeading - t.heading;
  while(diff > PI_F) diff -= TWO_PI_F;
  while(diff < -PI_F) diff += TWO_PI_F;
  t.heading = wrapAngle(t.heading + clampf(diff,-1.4f*dt,1.4f*dt));

  bool los = !lineBlocked(t.position,g.player.position);

  if (t.state == TankAIState::Idle) {
    if (d < (t.bossReleased ? 90.0f : 42.0f)) t.state = TankAIState::Approach;
  }

  if (t.state == TankAIState::Approach) {
    bool wantsMove = d > 20.0f;
    if (t.kind == TankKind::Holder) wantsMove = d > 30.0f;
    if (wantsMove) {
      float speed = tankMoveSpeed(t.kind);
      Vec2 step = mul(forwardFromHeading(t.heading),speed*dt);
      Vec2 next = add(t.position,step);
      bool blocked=false;
      for(int i=0;i<MAX_OBSTACLES;i++) if(circleHitsObstacle(next,1.55f,g.obstacles[i])) {blocked=true;break;}
      if(!blocked && !circlesOverlap(next,1.55f,g.player.position,PLAYER_RADIUS)) t.position=next;
    }
    if (los && d < 55.0f && t.fireCooldown <= 0) {
      t.state = TankAIState::Aim;
      t.stateTimer = tankAimTime(t.kind);
      soundWarn();
    }
  } else if (t.state == TankAIState::Aim) {
    if (!los) {
      t.state = TankAIState::Approach;
    } else if (t.stateTimer <= 0) {
      fireEnemyShell(t);
      t.fireCooldown = t.kind == TankKind::Light ? 2.2f : (t.kind == TankKind::Heavy ? 4.2f : 3.0f);
      t.state = TankAIState::Recover;
      t.stateTimer = 0.5f;
    }
  } else if (t.state == TankAIState::Recover) {
    if (t.stateTimer <= 0) t.state = TankAIState::Approach;
  }

  if (!t.bossReleased && length(sub(t.position,t.home)) > 70.0f) {
    t.state = TankAIState::Idle;
  }
}

static void updateEnemyTanks(float dt) {
  for (int i=0;i<MAX_TANKS;i++) if(g.tanks[i].active) updateEnemyTank(g.tanks[i],dt);
}

static void updateEnemyShells(float dt) {
  for (int i=0;i<MAX_SHELLS;i++) {
    EnemyShell &s = g.shells[i];
    if (!s.active) continue;
    Vec2 old = s.position;
    s.position = add(s.position,mul(s.velocity,dt));
    s.life -= dt;

    bool impact = false;
    for (int j=0;j<MAX_OBSTACLES;j++) {
      if (circleHitsObstacle(s.position,0.25f,g.obstacles[j])) {impact=true;break;}
    }

    if (!impact && distancePointSegment(g.player.position,old,s.position) < PLAYER_RADIUS + 0.25f) {
      impact = true;
      damagePlayer(s.damage);
    }

    if (impact || s.life <= 0) {
      spawnEffect(EffectType::Explosion,s.position,0.22f,1.3f);
      s.active=false;
    }
  }
}

// ============================================================
// BOSS
// ============================================================

void beginBossEncounter() {
  Boss &b = g.boss;
  b.phase = BossPhase::Pursuit;
  b.position = {0,78};
  b.northSpeed = BOSS_BASE_SPEED;
  b.gaitTime = 0;
  b.missileTimer = 5.0f;
  b.collapseTimer = 0;
  b.releasedRemaining = 0;
  b.releasedSpawned = 0;
  b.releasedSpawnTimer = 0;
  b.coreOpenTimer = 0;
  b.corePosition = {0,0};
  for (int i=0;i<3;i++) {
    b.legs[i].health = LEG_HEALTH;
    b.legs[i].destroyed = false;
    b.legs[i].gaitPhase = i * (TWO_PI_F/3.0f);
  }
}

static Vec2 bossLegPos(int i) {
  static const Vec2 offsets[3] = {{-9,-1},{9,-1},{0,11}};
  Vec2 p = add(g.boss.position,offsets[i]);
  // tiny procedural step sway, not full physical gait
  p.x += sinf(g.boss.gaitTime*1.2f + g.boss.legs[i].gaitPhase) * 1.2f;
  return p;
}

static void scheduleMissile() {
  for (int i=0;i<MAX_MISSILES;i++) {
    if (!g.missiles[i].active) {
      Vec2 v = mul(forwardFromHeading(g.player.heading),(g.player.left.speed+g.player.right.speed)*0.15f);
      Vec2 target = add(g.player.position,mul(v,1.0f));
      target.x += randomRange(-3.5f,3.5f);
      target.y += randomRange(-2.0f,4.0f);
      g.missiles[i] = {true,target,MISSILE_WARNING};
      soundWarn();
      return;
    }
  }
}

static void updateMissiles(float dt) {
  for(int i=0;i<MAX_MISSILES;i++){
    Missile &m=g.missiles[i];
    if(!m.active) continue;
    m.countdown -= dt;
    if(m.countdown<=0){
      if(length(sub(g.player.position,m.target))<=MISSILE_RADIUS) damagePlayer(55);
      spawnEffect(EffectType::Explosion,m.target,0.7f,6.0f);
      soundExplosion();
      m.active=false;
    }
  }
}

void beginCollapse() {
  g.boss.phase = BossPhase::Collapsing;
  g.boss.collapseTimer = 0;
  g.boss.northSpeed = 0;
  for(int i=0;i<MAX_MISSILES;i++) g.missiles[i].active=false;
  soundExplosion();
  g.shakeTimer=1.0f;
  g.shakeMagnitude=3.0f;
}

static void finalizeCollapse() {
  g.boss.phase = BossPhase::ReleasedTanks;
  g.boss.releasedRemaining = 5;
  g.boss.releasedSpawned = 0;
  g.boss.releasedSpawnTimer = 0.8f;
  // Wreck collision leaves broad routes around both sides.
  spawnObstacleSegment({-14,g.boss.position.y-3},{14,g.boss.position.y+8},7.0f,COL_BOSS,true,true);
}

static void updateBossPursuit(float dt) {
  Boss &b=g.boss;
  b.gaitTime += dt;
  b.position.y += b.northSpeed*dt;
  b.missileTimer -= dt;

  if(b.missileTimer<=0){
    scheduleMissile();
    b.missileTimer=randomRange(5.0f,7.5f);
  }

  // Stomp hazard when a gait phase is near its "landing" point.
  for(int i=0;i<3;i++){
    if(b.legs[i].destroyed) continue;
    float phase=fmodf(b.gaitTime*1.2f+b.legs[i].gaitPhase,TWO_PI_F);
    if(phase>5.95f && phase<6.05f){
      Vec2 p=bossLegPos(i);
      if(length(sub(g.player.position,p))<5.5f) damagePlayer(65);
      spawnEffect(EffectType::Dust,p,0.45f,4.0f);
    }
  }

  // Floating origin keeps pursuit coordinates bounded.
  if(g.player.position.y>100.0f){
    float shift=60.0f;
    g.player.position.y-=shift;
    g.player.lastSafePosition.y-=shift;
    b.position.y-=shift;
    for(int i=0;i<MAX_TANKS;i++) if(g.tanks[i].active){g.tanks[i].position.y-=shift;g.tanks[i].home.y-=shift;}
    for(int i=0;i<MAX_TROOPS;i++) if(g.troops[i].active) g.troops[i].position.y-=shift;
    for(int i=0;i<MAX_OBSTACLES;i++) if(g.obstacles[i].active){g.obstacles[i].a.y-=shift;g.obstacles[i].b.y-=shift;}
    for(int i=0;i<MAX_MISSILES;i++) if(g.missiles[i].active) g.missiles[i].target.y-=shift;
  }
}

static void updateBossReleased(float dt) {
  Boss &b=g.boss;
  if(b.releasedSpawned<5){
    b.releasedSpawnTimer-=dt;
    if(b.releasedSpawnTimer<=0){
      static const TankKind kinds[5]={TankKind::Light,TankKind::Light,TankKind::Holder,TankKind::Maneuverer,TankKind::Heavy};
      float side=(b.releasedSpawned%2==0)?-1.0f:1.0f;
      Vec2 p={b.position.x+side*(7.0f+2.0f*b.releasedSpawned),b.position.y-10.0f-b.releasedSpawned*2.0f};
      spawnTank(p,kinds[b.releasedSpawned],true);
      b.releasedSpawned++;
      b.releasedSpawnTimer=1.0f;
    }
  }
}

static void updateBoss(float dt) {
  if(g.boss.phase==BossPhase::Pursuit){
    updateBossPursuit(dt);
    updateMissiles(dt);
  } else if(g.boss.phase==BossPhase::Collapsing){
    g.boss.collapseTimer+=dt;
    if(g.boss.collapseTimer>4.3f) finalizeCollapse();
  } else if(g.boss.phase==BossPhase::ReleasedTanks){
    updateBossReleased(dt);
  } else if(g.boss.phase==BossPhase::CoreOpening){
    g.boss.coreOpenTimer+=dt;
    if(g.boss.coreOpenTimer>=2.0f){
      g.boss.phase=BossPhase::CoreExposed;
      g.boss.corePosition={g.boss.position.x+4.0f,g.boss.position.y-5.0f};
      startTone(toneR,520,160);
    }
  }
}

void destroyCore() {
  if(g.boss.phase!=BossPhase::CoreExposed) return;
  g.boss.phase=BossPhase::Destroyed;
  g.mode=GameMode::Victory;
  g.modeEnteredMs=millis();
  g.restartArmed=false;
  spawnEffect(EffectType::Explosion,g.boss.corePosition,1.5f,9.0f);
  soundExplosion();
}

// ============================================================
// EFFECTS
// ============================================================

static void updateEffects(float dt) {
  for(int i=0;i<MAX_EFFECTS;i++){
    if(!g.effects[i].active) continue;
    g.effects[i].age+=dt;
    if(g.effects[i].age>=g.effects[i].life) g.effects[i].active=false;
  }
  g.damageFlash=fmaxf(0,g.damageFlash-dt);
  g.shakeTimer=fmaxf(0,g.shakeTimer-dt);
  if(g.player.invuln>0) g.player.invuln=fmaxf(0,g.player.invuln-dt);
}

// ============================================================
// PROJECTION / RENDERING
// ============================================================

static int frameShakeY = 0;

static Projection project(Vec2 world) {
  Vec2 rel=sub(world,g.player.position);
  Vec2 f=forwardFromHeading(g.player.heading);
  Vec2 r=rightFromHeading(g.player.heading);
  float cx=dot(rel,r);
  float depth=dot(rel,f);
  if(depth<=NEAR_CLIP) return {false,0,0,depth,0};

  float sx=CENTER_X+(cx/depth)*FOCAL;
  float scale=FOCAL/depth;
  if(sx<-80 || sx>SCREEN_W+80) return {false,(int)sx,0,depth,scale};

  // Perspective ground point using an actual tank-height camera.
  // The old 42.0f value effectively placed the camera dozens of metres up.
  int gy=HORIZON_Y+(int)(CAMERA_HEIGHT*scale)+frameShakeY;
  return {true,(int)sx,gy,depth,scale};
}

static void zonePalette(uint16_t &sky,uint16_t &ground) {
  sky=COL_SKY; ground=COL_GROUND;
  if(g.zone==CampaignZone::Beach) ground=COL_SAND;
  else if(g.zone==CampaignZone::Trenches) ground=COL_EARTH;
  else if(g.zone==CampaignZone::Town) ground=COL_ROAD;
  else if(g.zone==CampaignZone::City) {sky=0x528A;ground=0x4208;}
  else {sky=0x73AE;ground=0x630C;}
}

static void renderBackground() {
  uint16_t sky,ground;
  zonePalette(sky,ground);
  screen.fillRect(0,VIEW_TOP,SCREEN_W,HORIZON_Y-VIEW_TOP,sky);
  screen.fillRect(0,HORIZON_Y,SCREEN_W,VIEW_BOTTOM-HORIZON_Y,ground);

  // Cheap perspective lane / ground bands for motion cues.
  for(int y=HORIZON_Y+24;y<VIEW_BOTTOM;y+=32){
    screen.drawFastHLine(0,y,SCREEN_W,0x5AEB);
  }
}

static void renderSegment(const Obstacle& o) {
  Projection a=project(o.a);
  Projection b=project(o.b);
  if(!a.visible && !b.visible) return;
  if(a.depth<=NEAR_CLIP || b.depth<=NEAR_CLIP) return;

  int topA=a.groundY-(int)(o.height*a.scale);
  int topB=b.groundY-(int)(o.height*b.scale);
  int16_t xs[4]={(int16_t)a.x,(int16_t)b.x,(int16_t)b.x,(int16_t)a.x};
  int16_t ys[4]={(int16_t)a.groundY,(int16_t)b.groundY,(int16_t)topB,(int16_t)topA};
  screen.fillTriangle(xs[0],ys[0],xs[1],ys[1],xs[2],ys[2],o.color);
  screen.fillTriangle(xs[0],ys[0],xs[2],ys[2],xs[3],ys[3],o.color);
  screen.drawLine(xs[3],ys[3],xs[2],ys[2],ST77XX_WHITE);
}

static void renderCircleObstacle(const Obstacle& o) {
  Projection p=project(o.a);
  if(!p.visible) return;
  int w=(int)clampf(o.radius*2.0f*p.scale,2,80);
  int h=(int)clampf(o.height*p.scale,2,100);
  screen.fillRect(p.x-w/2,p.groundY-h,w,h,o.color);
}

static void renderTroop(const Troop& t) {
  Projection p=project(t.position);
  if(!p.visible || p.depth>110) return;
  int h=(int)clampf(1.8f*p.scale,3,42);
  int w=(h/4 > 2 ? h/4 : 2);
  int y=p.groundY-h;
  screen.fillCircle(p.x,y+(w/2 > 1 ? w/2 : 1),(w/2 > 1 ? w/2 : 1),COL_TROOP);
  screen.drawFastVLine(p.x,y+w,h-w,COL_TROOP);
  if(h>9){
    screen.drawLine(p.x,y+h/2,p.x-w,y+h*3/4,COL_TROOP);
    screen.drawLine(p.x,y+h/2,p.x+w,y+h*3/4,COL_TROOP);
  }
}

static void drawBrokenTriangle(int cx,int cy,int radius,float rotation) {
  float a0=rotation;
  int x[3],y[3];
  for(int i=0;i<3;i++){
    float a=a0+i*TWO_PI_F/3.0f-PI_F/2;
    x[i]=cx+(int)(cosf(a)*radius);
    y[i]=cy+(int)(sinf(a)*radius);
  }
  for(int i=0;i<3;i++){
    int j=(i+1)%3;
    float sx=x[i]*0.82f+x[j]*0.18f;
    float sy=y[i]*0.82f+y[j]*0.18f;
    float ex=x[i]*0.18f+x[j]*0.82f;
    float ey=y[i]*0.18f+y[j]*0.82f;
    screen.drawLine((int)sx,(int)sy,(int)ex,(int)ey,COL_MARKER);
  }
}

static void renderEnemyTank(const EnemyTank& t) {
  Projection p=project(t.position);
  if(!p.visible || p.depth>190) return;
  int w=(int)clampf(3.2f*p.scale,4,90);
  int h=(int)clampf(2.2f*p.scale,3,60);
  int y=p.groundY-h;
  uint16_t c=t.kind==TankKind::Heavy?0x7A49:COL_TANK;
  screen.fillRect(p.x-w/2,y+h/3,w,h*2/3,c);
  screen.fillRect(p.x-w/4,y,w/2,h/2,c);
  screen.drawFastHLine(p.x,y+h/4,(w/2 > 2 ? w/2 : 2),ST77XX_WHITE);

  int markerR=(int)clampf(9.0f + 80.0f/p.depth,8,18);
  drawBrokenTriangle(p.x,y-7,markerR,g.markerRotation);
}

static void renderShell(const EnemyShell& s) {
  Projection p=project(s.position);
  if(!p.visible) return;
  int r=(int)clampf(12.0f/p.depth,1,4);
  screen.fillCircle(p.x,p.groundY-r*3,r,ST77XX_YELLOW);
}

static void renderMissiles() {
  for(int i=0;i<MAX_MISSILES;i++){
    if(!g.missiles[i].active) continue;
    Projection p=project(g.missiles[i].target);
    if(!p.visible) continue;
    int rx=(int)clampf(MISSILE_RADIUS*p.scale,3,75);
    int ry=(rx/3 > 2 ? rx/3 : 2);
    bool blink=((int)(g.missiles[i].countdown*6)&1)==0;
    uint16_t c=blink?ST77XX_RED:ST77XX_YELLOW;
    // Adafruit_GFX has drawCircle on all supported versions; use
    // three compressed horizontal rings to suggest a ground ellipse.
    screen.drawFastHLine(p.x-rx,p.groundY,rx*2+1,c);
    screen.drawFastHLine(p.x-rx*3/4,p.groundY-ry,rx*3/2+1,c);
    screen.drawFastHLine(p.x-rx*3/4,p.groundY+ry,rx*3/2+1,c);
  }
}

static void renderBoss() {
  Boss &b=g.boss;
  if(b.phase==BossPhase::Inactive || b.phase==BossPhase::Destroyed) return;

  if(b.phase==BossPhase::Pursuit || b.phase==BossPhase::Collapsing){
    for(int i=0;i<3;i++){
      Vec2 lp=bossLegPos(i);
      Projection p=project(lp);
      if(!p.visible) continue;
      int w=(int)clampf(7.0f*p.scale,5,120);
      int h=(int)clampf(24.0f*p.scale,14,260);
      int lean=(b.phase==BossPhase::Collapsing)?(int)(g.boss.collapseTimer*10.0f):0;
      screen.fillRect(p.x-w/2+lean,p.groundY-h,w,h,COL_BOSS);
      screen.drawRect(p.x-w/2+lean,p.groundY-h,w,h,0xB596);
      if(!b.legs[i].destroyed){
        int markerR=(int)clampf(10.0f+140.0f/p.depth,9,21);
        drawBrokenTriangle(p.x+lean,p.groundY-(int)(4.0f*p.scale),markerR,g.markerRotation);
      }
    }

    Projection body=project(add(b.position,{0,7}));
    if(body.visible){
      int w=(int)clampf(30.0f*body.scale,20,220);
      int h=(int)clampf(12.0f*body.scale,12,170);
      int lean=(b.phase==BossPhase::Collapsing)?(int)(g.boss.collapseTimer*10.0f):0;
      screen.fillRect(body.x-w/2+lean,body.groundY-h-(int)(18*body.scale),w,h,COL_BOSS);
    }
  } else {
    // Fallen fortress / wreck.
    Projection p=project(b.position);
    if(p.visible){
      int w=(int)clampf(34.0f*p.scale,30,220);
      int h=(int)clampf(9.0f*p.scale,10,80);
      screen.fillRect(p.x-w/2,p.groundY-h,w,h,COL_BOSS);
    }
    if(b.phase==BossPhase::CoreOpening || b.phase==BossPhase::CoreExposed){
      Projection c=project(b.corePosition);
      if(c.visible && b.phase==BossPhase::CoreExposed){
        int r=(int)clampf(1.8f*c.scale,3,26);
        screen.fillCircle(c.x,c.groundY-r*2,r,ST77XX_YELLOW);
        drawBrokenTriangle(c.x,c.groundY-r*2,r+8,g.markerRotation);
      }
    }
  }
}

static void renderEffects() {
  for(int i=0;i<MAX_EFFECTS;i++){
    Effect &e=g.effects[i];
    if(!e.active) continue;
    Projection p=project(e.position);
    if(!p.visible) continue;
    float f=1.0f-e.age/e.life;
    int r=(int)clampf(e.size*p.scale*(0.4f+0.8f*(1.0f-f)),1,45);
    uint16_t c=ST77XX_YELLOW;
    if(e.type==EffectType::Dust)c=0xA514;
    if(e.type==EffectType::Spark)c=ST77XX_WHITE;
    if(e.type==EffectType::Explosion)c=(f>0.5f?ST77XX_YELLOW:ST77XX_RED);
    screen.fillCircle(p.x,p.groundY-r,r,c);
  }
}

static const char* zoneName() {
  switch(g.zone){
    case CampaignZone::Beach:return "BEACH";
    case CampaignZone::Trenches:return "TRENCH";
    case CampaignZone::Town:return "TOWN";
    case CampaignZone::City:return "CITY";
    default:return "PURSUIT";
  }
}

static const char* compassName(float h) {
  int idx=(int)floorf((wrapAngle(h)+PI_F/8.0f)/(PI_F/4.0f))&7;
  static const char* names[8]={"N","NE","E","SE","S","SW","W","NW"};
  return names[idx];
}

static void renderHUD() {
  // Top strip
  screen.fillRect(0,0,SCREEN_W,VIEW_TOP,ST77XX_BLACK);
  screen.setTextSize(1);
  screen.setTextColor(COL_HUD);
  screen.setCursor(4,5);
  screen.print("L:");
  screen.print(g.player.lives);
  screen.setCursor(42,5);
  screen.print(zoneName());
  screen.setCursor(111,5);
  screen.print(compassName(g.player.heading));

  // HP bar
  screen.drawRect(158,5,76,8,ST77XX_WHITE);
  int hpw=(int)(72.0f*clampf(g.player.health/(float)PLAYER_MAX_HEALTH,0,1));
  screen.fillRect(160,7,hpw,4,g.player.health<30?ST77XX_RED:ST77XX_GREEN);

  // Bottom strip
  screen.fillRect(0,VIEW_BOTTOM,SCREEN_W,SCREEN_H-VIEW_BOTTOM,ST77XX_BLACK);
  screen.setCursor(5,286);
  screen.print("CANNON ");
  float ready=1.0f-clampf(g.player.cannonCooldown/CANNON_RELOAD,0,1);
  screen.drawRect(48,286,70,8,ST77XX_WHITE);
  screen.fillRect(50,288,(int)(66*ready),4,ready>=0.999f?ST77XX_GREEN:ST77XX_YELLOW);

  if(g.boss.phase==BossPhase::ReleasedTanks){
    screen.setCursor(135,286);
    screen.print("DEF:");
    screen.print(g.boss.releasedRemaining);
  }

  // Crosshair
  screen.drawFastHLine(CENTER_X-5,160,11,ST77XX_WHITE);
  screen.drawFastVLine(CENTER_X,155,11,ST77XX_WHITE);

  // Missile danger contextual warning
  for(int i=0;i<MAX_MISSILES;i++){
    if(!g.missiles[i].active) continue;
    float d=length(sub(g.player.position,g.missiles[i].target));
    if(d<=MISSILE_RADIUS+2){
      screen.setTextSize(2);
      screen.setTextColor(ST77XX_RED);
      screen.setCursor(73,38);
      screen.print("MISSILE");
      screen.setTextSize(1);
      screen.setTextColor(ST77XX_WHITE);
      screen.setCursor(104,57);
      screen.print((int)ceilf(g.missiles[i].countdown));

      Vec2 safe=normalize(sub(g.player.position,g.missiles[i].target));
      Vec2 f=forwardFromHeading(g.player.heading);
      Vec2 r=rightFromHeading(g.player.heading);
      float sx=dot(safe,r), sy=dot(safe,f);
      int ax=CENTER_X+(int)(sx*22);
      int ay=82-(int)(sy*16);
      screen.drawLine(CENTER_X,82,ax,ay,ST77XX_YELLOW);
      break;
    }
  }

  if(g.player.invuln>0 && ((int)(g.player.invuln*6)&1)==0){
    screen.drawRect(1,VIEW_TOP+1,SCREEN_W-2,VIEW_H-2,ST77XX_CYAN);
  }

  if(g.damageFlash>0){
    screen.drawRect(0,VIEW_TOP,SCREEN_W,VIEW_H,ST77XX_RED);
    screen.drawRect(2,VIEW_TOP+2,SCREEN_W-4,VIEW_H-4,ST77XX_RED);
  }
}

static void renderIntro() {
  screen.fillScreen(ST77XX_BLACK);
  screen.setTextColor(ST77XX_WHITE);
  screen.setTextSize(3);
  screen.setCursor(58,76);
  screen.print("TANK");
  screen.setTextSize(2);
  screen.setTextColor(ST77XX_RED);
  screen.setCursor(29,132);
  screen.print("INVADE THE");
  screen.setCursor(77,158);
  screen.print("NORTH");
  screen.setTextSize(1);
  screen.setTextColor(ST77XX_WHITE);
  screen.setCursor(22,235);
  screen.print("TREAD SWITCHES = DRIVE");
  screen.setCursor(22,250);
  screen.print("LEFT BTN = MG  RIGHT = CANNON");
}

static void renderEndScreen(bool victory) {
  screen.fillScreen(ST77XX_BLACK);
  screen.setTextSize(2);
  screen.setTextColor(victory?ST77XX_GREEN:ST77XX_RED);
  screen.setCursor(victory?20:36,110);
  screen.print(victory?"FORTRESS DESTROYED":"MISSION FAILED");
  screen.setTextColor(ST77XX_WHITE);
  screen.setCursor(victory?37:38,142);
  screen.print(victory?"MISSION COMPLETE":"TANKS LOST");
  screen.setTextSize(1);
  screen.setCursor(52,215);
  screen.print("RELEASE BUTTONS");
  screen.setCursor(54,232);
  screen.print("THEN PRESS TO RESTART");
}

void renderFrame() {
  // One camera-shake sample for the entire frame. This prevents different
  // objects from jittering independently during the same refresh.
  frameShakeY = (g.shakeTimer > 0)
    ? (int)randomRange(-g.shakeMagnitude, g.shakeMagnitude)
    : 0;

  if(g.mode==GameMode::Intro){ renderIntro(); return; }
  if(g.mode==GameMode::GameOver){ renderEndScreen(false); return; }
  if(g.mode==GameMode::Victory){ renderEndScreen(true); return; }

  renderBackground();

  // Painter-style approximate back-to-front passes.
  // Obstacles are mostly arranged so this is sufficient for the ESP32 target.
  for(int pass=0;pass<2;pass++){
    for(int i=0;i<MAX_OBSTACLES;i++){
      Obstacle &o=g.obstacles[i];
      if(!o.active) continue;
      Projection p=project(o.a);
      if(!p.visible) continue;
      if((pass==0 && p.depth<45)||(pass==1 && p.depth>=45)) continue;
      if(o.type==ObstacleType::Segment) renderSegment(o); else renderCircleObstacle(o);
    }
  }

  renderBoss();
  for(int i=0;i<MAX_TROOPS;i++) if(g.troops[i].active) renderTroop(g.troops[i]);
  for(int i=0;i<MAX_TANKS;i++) if(g.tanks[i].active) renderEnemyTank(g.tanks[i]);
  for(int i=0;i<MAX_SHELLS;i++) if(g.shells[i].active) renderShell(g.shells[i]);
  renderMissiles();
  renderEffects();
  renderHUD();
}

// ============================================================
// MAIN SIMULATION
// ============================================================

void updateSimulation(float dt, const GameInput& input) {
  g.markerRotation=wrapAngle(g.markerRotation+0.8f*dt);

  if(g.mode==GameMode::Intro){
    if(millis()-g.introStartMs>=2500 || input.leftPressed || input.rightPressed){
      g.mode=GameMode::Playing;
      g.modeEnteredMs=millis();
    }
    return;
  }

  if(g.mode==GameMode::PlayerDestroyed){
    g.player.destroyedTimer-=dt;
    updateEffects(dt);
    if(g.player.destroyedTimer<=0) respawnPlayer();
    return;
  }

  if(g.mode==GameMode::GameOver || g.mode==GameMode::Victory){
    if(!input.leftButton && !input.rightButton) g.restartArmed=true;
    if(g.restartArmed && (input.leftPressed || input.rightPressed)) startNewRun();
    return;
  }

  updatePlayerMovement(dt);
  updateCampaign();
  updateTroops(dt);
  updateEnemyTanks(dt);
  updateEnemyShells(dt);
  updateBoss(dt);
  updateWeapons(dt,input);
  updateEffects(dt);
}

// ============================================================
// LIFECYCLE
// ============================================================

void startNewRun() {
  clearPools();
  rngState = 0xC0FFEEu ^ millis();

  g.mode=GameMode::Intro;
  g.zone=CampaignZone::Beach;
  g.zoneSerial=0;
  g.markerRotation=0;
  g.damageFlash=0;
  g.shakeTimer=0;
  g.shakeMagnitude=0;
  g.restartArmed=false;

  g.player.position={0,7};
  g.player.lastSafePosition=g.player.position;
  g.player.heading=0;
  g.player.left={TreadCommand::Neutral,0};
  g.player.right={TreadCommand::Neutral,0};
  g.player.health=PLAYER_MAX_HEALTH;
  g.player.lives=STARTING_LIVES;
  g.player.cannonCooldown=0;
  g.player.invuln=0;
  g.player.destroyedTimer=0;
  g.player.mgShotTimer=0;
  g.player.mgPauseTimer=0;
  g.player.mgShotsRemaining=0;
  g.player.mgBurstActive=false;

  g.boss.phase=BossPhase::Inactive;

  loadZone(CampaignZone::Beach);
  g.introStartMs=millis();
  g.modeEnteredMs=g.introStartMs;
  g.lastUpdateMs=g.introStartMs;
  g.lastFrameMs=0;
  g.accumulatorMs=0;
}

void enter() {
  // The same display instance drew the menu. Clear it before the Tank intro.
  screen.setRotation(0);
  // Push pixels faster so whole-frame redraws spend less time visibly sweeping
  // across the ST7789. 40 MHz is a common stable rate for ESP32 + ST7789 SPI.
  screen.setSPISpeed(40000000);
  screen.setTextWrap(false);
  Serial.print("Tank display: ");
  Serial.print(screen.width());
  Serial.print('x');
  Serial.println(screen.height());
  startNewRun();
  renderFrame();
}

void update(const GameInput& input) {
  uint32_t now=millis();
  uint32_t elapsed=now-g.lastUpdateMs;
  g.lastUpdateMs=now;
  if(elapsed>100) elapsed=100;
  g.accumulatorMs+=elapsed;

  // Button edge state is meaningful for the current launcher tick. To avoid
  // double-consuming an edge if multiple fixed steps run, only the first step
  // receives the edge; held states remain available on every step.
  bool first=true;
  while(g.accumulatorMs>=SIM_MS){
    if(first){
      updateSimulation(SIM_DT,input);
      first=false;
    } else {
      GameInput held=input;
      held.leftPressed=false;
      held.rightPressed=false;
      updateSimulation(SIM_DT,held);
    }
    g.accumulatorMs-=SIM_MS;
  }

  updateAudio();

  if(now-g.lastFrameMs>=FRAME_MS){
    g.lastFrameMs=now;
    renderFrame();
  }
}

} // namespace TankGame
