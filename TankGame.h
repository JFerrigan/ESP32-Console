#pragma once

#include <Arduino.h>
#include <math.h>
#include <string.h>
#include "GameAPI.h"
#include "GameRenderMemory.h"
#include "Hardware.h"
#include "MusicPlayer.h"

// Jakeboy Tank
// Complete self-contained game implementation for the current launcher API.
// The launcher owns display/SPI/buttons/menu. Tank owns only its namespaced state.

namespace tank {

// -----------------------------------------------------------------------------
// Render configuration
// -----------------------------------------------------------------------------

constexpr int RW = 120;
constexpr int RH = 160;
constexpr int WORLD_H = 128;
constexpr int HORIZON_Y = 54;
constexpr float FOCAL = 79.6f;
constexpr float EYE_Z = 1.25f;
constexpr float NEAR_D = 0.20f;
constexpr float PI_F = 3.14159265358979323846f;
constexpr float TWO_PI_F = 6.28318530717958647692f;

constexpr uint32_t FIXED_US = 16667;      // ~60 Hz simulation
constexpr uint32_t FRAME_US = 40000;      // 25 Hz presentation
constexpr uint8_t MAX_CATCHUP = 4;

// Indexed world/cockpit frame + world-only depth buffer.
// This is intentionally much smaller than a 240x320 RGB565 framebuffer.
static_assert(RW * RH == GameRenderMemory::FRAME_BYTES, "shared frame size mismatch");
static_assert(RW * WORLD_H == GameRenderMemory::DEPTH_BYTES, "shared depth size mismatch");
static uint8_t *const frame = GameRenderMemory::bytes;
static uint8_t *const depthBuf = GameRenderMemory::bytes + GameRenderMemory::FRAME_BYTES;
static uint16_t physicalRow[RW * 2];

// Palette indexes
//  0 black           1 white           2 sky light       3 sky dark
//  4 sand            5 dirt            6 grass           7 road
//  8 concrete        9 dark metal     10 armor          11 red
// 12 amber          13 smoke          14 bright green   15 orange
// 16 rubble         17 water          18 trench         19 enemy green
// 20 enemy dark grn 21 flash          22 cockpit light  23 cockpit dark
// 24 core           25 blue steel     26 gray           27 brown
// 28 dark red       29 dark amber     30 shadow         31 cyan
static const uint16_t PALETTE[32] = {
  0x0000, 0xFFFF, 0x9D9F, 0x42D5,
  0xD5E8, 0x8B85, 0x5BE5, 0x630C,
  0x9CF3, 0x3186, 0x7BEF, 0xF800,
  0xFD20, 0x6B4D, 0x07E0, 0xFC00,
  0x738E, 0x041F, 0x31A4, 0x4E69,
  0x2D25, 0xFFE0, 0xB5B6, 0x2945,
  0xF81F, 0x4A69, 0x7BEF, 0x79E0,
  0x6000, 0x8200, 0x18E3, 0x07FF
};

// -----------------------------------------------------------------------------
// Core types
// -----------------------------------------------------------------------------

struct Vec2 { float x, y; };
struct Vec3 { float x, y, z; };
struct CamV { float r, h, d; };
struct ScreenV { float x, y, invD; };

struct Pose {
  float x;
  float y;
  float heading;
};

enum class Mode : uint8_t {
  BRIEFING,
  PLAYING,
  DYING,
  RESPAWN_GATE,
  GAME_OVER,
  VICTORY_SEQUENCE,
  VICTORY_WAIT
};

enum class Tread : int8_t {
  Reverse = -1,
  Stop = 0,
  Forward = 1
};

enum class EnemyClass : uint8_t { Light, Medium, Heavy };
enum class EnemyRole : uint8_t { Holder, Pursuer, Maneuverer };
enum class BossPhase : uint8_t {
  Dormant,
  Walking,
  Collapsing,
  FallenDeploying,
  CoreOpening,
  CoreExposed,
  Defeated
};

enum class EffectType : uint8_t { None, Flash, Explosion, TankExplosion, Dust, Spark };

struct StaticBox {
  float x, y, w, d, h;
  uint8_t color;
  bool solid;
};

struct EnemyDef {
  float x, y;
  EnemyClass cls;
  EnemyRole role;
};

struct Enemy {
  float x, y;
  float heading;
  float turretHeading;
  int16_t health;
  uint8_t defIndex;
  bool alive;
  bool active;
  bool windup;
  uint32_t windupUntil;
  uint32_t reloadUntil;
  uint32_t observedUntil;
};

struct SoldierDef { float x, y; };
struct Soldier {
  float x, y;
  float homeX, homeY;
  float heading;
  bool alive;
  bool dying;
  uint8_t anim;
  uint32_t nextDecision;
  uint32_t deathAt;
};

struct Projectile {
  bool active;
  float x, y;
  float px, py;
  float vx, vy;
  uint8_t damage;
  uint32_t expires;
};

struct Effect {
  EffectType type;
  float x, y;
  uint32_t born;
  uint16_t life;
};

struct WarningZone {
  bool active;
  bool impacted;
  float x, y, radius;
  uint32_t start;
  uint32_t impactAt;
  uint8_t damage;
};

struct Boss {
  BossPhase phase;
  float x, y;
  float bodyZ;
  float collapseSide;
  uint32_t phaseStart;
  int16_t legHealth[3];
  bool legAlive[3];
  uint8_t defendersReleased;
  uint8_t defendersDestroyed;
  uint32_t nextRelease;
  uint32_t nextMissile;
  uint32_t nextFoot;
  float coreX, coreY;
};

struct BossDefender {
  Enemy e;
  EnemyClass cls;
  bool released;
  bool countedDead;
  bool deploying;
};

struct Player {
  Pose pose;
  int16_t health;
  uint8_t lives;
  uint32_t invulnerableUntil;
  uint32_t cannonReadyAt;
  uint32_t cannonFlashUntil;
  uint32_t cannonRecoilUntil;
  uint8_t mgRounds;
  uint32_t mgNextRound;
  uint32_t mgReadyAt;
  float safeX, safeY, safeHeading;
  uint32_t nextSafeSave;
};

struct GameState {
  Mode mode;
  Player player;
  Boss boss;
  uint32_t simNow;
  uint32_t modeStart;
  uint32_t briefingUntil;
  uint32_t accumulatorUs;
  uint32_t lastOuterUs;
  uint32_t lastFrameUs;
  bool controlsArmed;
  bool terminalReleased;
  bool musicWasEnabled;
  uint8_t area;
  char message[24];
  uint32_t messageUntil;
  uint16_t score;
  bool restartAtBeach;
  bool waterDeath;
};

static GameState g;

// -----------------------------------------------------------------------------
// Authored campaign data
// -----------------------------------------------------------------------------

// Solid authored structures. The path is intentionally open enough that ordinary
// combat can be bypassed while the geometry still creates cover and landmarks.
static const StaticBox WORLD_BOXES[] = {
  // Beach / seawall
  {-24,  58, 18,  5, 3.0f, 8, true}, { 23,  58, 20,  5, 3.0f, 8, true},
  {-17,  88, 11, 10, 4.0f,16, true}, { 17,  94, 12, 10, 4.5f,16, true},
  // Defensive line
  {-25, 135, 15, 10, 5.0f, 8, true}, { 24, 142, 16, 12, 5.0f, 8, true},
  {-18, 175, 10, 18, 4.0f,16, true}, { 21, 182, 13, 14, 4.0f,16, true},
  // Trenches / dugouts
  {-30, 235, 10, 18, 3.5f,18, true}, { 28, 250, 12, 18, 3.5f,18, true},
  {-20, 292, 12, 14, 3.5f,16, true}, { 22, 318, 12, 16, 3.5f,16, true},
  // Town
  {-29, 375, 18, 22, 8.0f,26, true}, { -5, 382, 14, 18, 7.0f, 8, true},
  { 24, 390, 20, 20, 8.0f,16, true}, { 30, 430, 16, 22,10.0f,26, true},
  {-27, 445, 18, 19, 9.0f,16, true}, { -3, 458, 14, 18, 6.0f, 8, true},
  // City
  {-40, 520, 22, 30,14.0f,26, true}, { -9, 510, 17, 24,12.0f, 8, true},
  { 25, 525, 24, 30,15.0f,16, true}, { 43, 565, 18, 26,13.0f,26, true},
  {-39, 578, 21, 29,14.0f,16, true}, { -7, 590, 16, 22,11.0f, 8, true},
  { 26, 605, 22, 28,15.0f,26, true}, {  0, 632, 12, 16,19.0f, 8, true},
  // Reveal / final battlefield ruins
  {-55, 690, 20, 16, 6.0f,16, true}, { 51, 706, 24, 18, 7.0f,16, true},
  {-62, 770, 18, 14, 5.0f,16, true}, { 60, 810, 22, 16, 5.0f,16, true},
  {-55, 875, 20, 15, 5.0f,16, true}, { 56, 915, 22, 15, 5.0f,16, true}
};
constexpr uint8_t WORLD_BOX_COUNT = sizeof(WORLD_BOXES) / sizeof(WORLD_BOXES[0]);

static const EnemyDef ENEMY_DEFS[] = {
  {-15,  78, EnemyClass::Light,  EnemyRole::Holder},
  { 18, 120, EnemyClass::Medium, EnemyRole::Holder},
  {-20, 162, EnemyClass::Medium, EnemyRole::Maneuverer},
  { 19, 208, EnemyClass::Light,  EnemyRole::Pursuer},
  {-22, 254, EnemyClass::Medium, EnemyRole::Holder},
  { 24, 302, EnemyClass::Medium, EnemyRole::Maneuverer},
  { -8, 335, EnemyClass::Heavy,  EnemyRole::Holder},
  {-26, 385, EnemyClass::Light,  EnemyRole::Pursuer},
  { 17, 410, EnemyClass::Medium, EnemyRole::Holder},
  {-18, 455, EnemyClass::Medium, EnemyRole::Maneuverer},
  { 28, 478, EnemyClass::Heavy,  EnemyRole::Holder},
  {-32, 530, EnemyClass::Medium, EnemyRole::Holder},
  { 12, 548, EnemyClass::Heavy,  EnemyRole::Maneuverer},
  { 35, 585, EnemyClass::Light,  EnemyRole::Pursuer},
  {-18, 616, EnemyClass::Medium, EnemyRole::Holder},
  { 25, 644, EnemyClass::Heavy,  EnemyRole::Holder},
  {-38, 700, EnemyClass::Medium, EnemyRole::Holder},
  { 40, 734, EnemyClass::Heavy,  EnemyRole::Holder},
  {-42, 805, EnemyClass::Medium, EnemyRole::Pursuer}
};
constexpr uint8_t ENEMY_COUNT = sizeof(ENEMY_DEFS) / sizeof(ENEMY_DEFS[0]);
static Enemy enemies[ENEMY_COUNT];

static const SoldierDef SOLDIER_DEFS[] = {
  {-12,  42}, { 10, 48}, {-17, 105}, {15, 111}, {-25, 155}, {23, 165},
  {-18, 225}, {18, 230}, {-28, 275}, {26, 286}, {-11, 323}, {13, 332},
  {-26, 365}, {25, 372}, {-14, 418}, {16, 425}, {-30, 470}, {31, 481},
  {-36, 515}, {34, 535}, {-20, 562}, {20, 575}, {-38, 610}, {38, 628}
};
constexpr uint8_t SOLDIER_COUNT = sizeof(SOLDIER_DEFS) / sizeof(SOLDIER_DEFS[0]);
static Soldier soldiers[SOLDIER_COUNT];

constexpr uint8_t MAX_PROJECTILES = 24;
constexpr uint8_t MAX_EFFECTS = 40;
static Projectile projectiles[MAX_PROJECTILES];
static Effect effects[MAX_EFFECTS];
static WarningZone missileZones[3];
static WarningZone footZone;
static BossDefender bossDefenders[6];

// -----------------------------------------------------------------------------
// Utility helpers
// -----------------------------------------------------------------------------

static inline float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

static inline int clampi(int v, int lo, int hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

static inline float sqrf(float v) { return v * v; }
static inline float dist2(float ax, float ay, float bx, float by) {
  return sqrf(ax - bx) + sqrf(ay - by);
}
static inline float wrapAngle(float a) {
  while (a < -PI_F) a += TWO_PI_F;
  while (a >  PI_F) a -= TWO_PI_F;
  return a;
}
static inline float angleTo(float ax, float ay, float bx, float by) {
  // Heading zero points north; positive rotates clockwise.
  return atan2f(bx - ax, by - ay);
}
static inline float approachAngle(float current, float target, float maxStep) {
  float d = wrapAngle(target - current);
  if (d > maxStep) d = maxStep;
  if (d < -maxStep) d = -maxStep;
  return wrapAngle(current + d);
}
static inline uint32_t elapsedUs(uint32_t now, uint32_t then) { return now - then; }
static inline uint32_t elapsedMs(uint32_t now, uint32_t then) { return now - then; }

static uint32_t hash2i(int x, int y) {
  uint32_t h = (uint32_t)x * 0x8da6b343u ^ (uint32_t)y * 0xd8163841u;
  h ^= h >> 13; h *= 0x85ebca6bu; h ^= h >> 16;
  return h;
}

static int classHealth(EnemyClass c) {
  if (c == EnemyClass::Light) return 50;
  if (c == EnemyClass::Medium) return 100;
  return 150;
}
static float classSpeed(EnemyClass c) {
  if (c == EnemyClass::Light) return 3.5f;
  if (c == EnemyClass::Medium) return 2.4f;
  return 1.6f;
}
static uint32_t classReloadMs(EnemyClass c) {
  if (c == EnemyClass::Light) return 3000;
  if (c == EnemyClass::Medium) return 3500;
  return 4200;
}
static uint8_t classDamage(EnemyClass c) {
  if (c == EnemyClass::Light) return 20;
  if (c == EnemyClass::Medium) return 25;
  return 30;
}

static Tread readTread(int upPin, int downPin) {
  const bool up = digitalRead(upPin) == LOW;
  const bool down = digitalRead(downPin) == LOW;
  if (up && !down) return Tread::Forward;
  if (!up && down) return Tread::Reverse;
  return Tread::Stop; // center and impossible double-contact both stop safely
}

static bool switchesCentered() {
  return digitalRead(LEFT_UP_PIN) != LOW && digitalRead(LEFT_DOWN_PIN) != LOW &&
         digitalRead(RIGHT_UP_PIN) != LOW && digitalRead(RIGHT_DOWN_PIN) != LOW;
}

static bool bossFreeRoam(float y) {
  return y >= 690.0f || g.boss.phase != BossPhase::Dormant;
}

constexpr float BOUNDARY_TANK_LENGTH = 3.0f;
constexpr float BOUNDARY_PUSH_OUT = 10.0f * BOUNDARY_TANK_LENGTH;

static float corridorHalfWidth(float y) {
  if (bossFreeRoam(y)) return 160.0f;
  if (y < 110) return 27.5f + BOUNDARY_PUSH_OUT;   // beach: hedgehog barricades
  if (y < 210) return 30.0f + BOUNDARY_PUSH_OUT;   // defense line
  if (y < 350) return 32.0f + BOUNDARY_PUSH_OUT;   // trenches: tighter and more claustrophobic
  if (y < 490) return 36.0f + BOUNDARY_PUSH_OUT;   // town
  if (y < 650) return 43.0f + BOUNDARY_PUSH_OUT;   // city
  if (y < 690) return 52.0f + BOUNDARY_PUSH_OUT;   // fortress approach
  return 160.0f;
}

static bool deepWaterAt(float x, float y) {
  (void)x;
  return y < 2.8f;
}

static uint8_t areaForY(float y) {
  if (y < 110) return 0;
  if (y < 210) return 1;
  if (y < 350) return 2;
  if (y < 490) return 3;
  if (y < 650) return 4;
  if (y < 710) return 5;
  return 6;
}

static const char* areaName(uint8_t a) {
  switch (a) {
    case 0: return "BEACH";
    case 1: return "DEFENSE LINE";
    case 2: return "TRENCHES";
    case 3: return "TOWN";
    case 4: return "CITY";
    case 5: return "FORTRESS AHEAD";
    default: return "FINAL FIELD";
  }
}

static void setMessage(const char* s, uint32_t ms) {
  strncpy(g.message, s, sizeof(g.message) - 1);
  g.message[sizeof(g.message) - 1] = 0;
  g.messageUntil = g.simNow + ms;
}

// -----------------------------------------------------------------------------
// Audio — nonblocking, game-owned while Tank is active
// -----------------------------------------------------------------------------

struct ToneState {
  uint8_t pin;
  uint16_t freq;
  uint32_t until;
  bool active;
};
static ToneState tone1{BUZZER_1_PIN, 0, 0, false};
static ToneState tone2{BUZZER_2_PIN, 0, 0, false};

static void startTone(ToneState &t, uint16_t hz, uint16_t ms) {
  if (!Music::gameEffectsAllowed()) { t.active = false; return; }
  t.freq = hz;
  t.until = g.simNow + ms;
  t.active = true;
  ledcWriteTone(t.pin, hz);
}
static void stopTone(ToneState &t) {
  if (t.active && Music::gameEffectsAllowed()) ledcWriteTone(t.pin, 0);
  t.active = false;
}
static void updateAudio() {
  if (tone1.active && (int32_t)(g.simNow - tone1.until) >= 0) stopTone(tone1);
  if (tone2.active && (int32_t)(g.simNow - tone2.until) >= 0) stopTone(tone2);
}
static void sfxCannon() { startTone(tone1, 115, 85); }
static void sfxMG() { startTone(tone2, 720, 35); }
static void sfxHit() { startTone(tone2, 260, 65); }
static void sfxWarning() { startTone(tone2, 980, 80); }
static void sfxExplosion() { startTone(tone1, 82, 150); }
static void stopGameAudio() { stopTone(tone1); stopTone(tone2); }

// -----------------------------------------------------------------------------
// Collision and tracing
// -----------------------------------------------------------------------------

static bool circleIntersectsBox(float cx, float cy, float r, const StaticBox &b) {
  float qx = clampf(cx, b.x - b.w * 0.5f, b.x + b.w * 0.5f);
  float qy = clampf(cy, b.y - b.d * 0.5f, b.y + b.d * 0.5f);
  return dist2(cx, cy, qx, qy) < r * r;
}

// The fallen fortress is a breached shell rather than one monolithic collision
// box.  The center lane is the deployment tunnel used by the six boss tanks.
// Rendering, player/enemy collision, projectile blocking, and line of sight all
// use these same three pieces so there is no invisible wall across the breach.
constexpr uint8_t FALLEN_FORTRESS_PIECES = 3;
constexpr float FALLEN_WRECK_W = 28.0f;
constexpr float FALLEN_WRECK_D = 17.0f;
constexpr float FALLEN_WRECK_H = 5.0f;
constexpr float FALLEN_BREACH_D = 7.0f;
constexpr float FALLEN_RAIL_D = (FALLEN_WRECK_D - FALLEN_BREACH_D) * 0.5f;
constexpr float FALLEN_REAR_W = 4.5f;

static StaticBox fallenFortressPiece(uint8_t index) {
  const float side = g.boss.collapseSide;
  const float wreckX = g.boss.x + side * 7.0f;
  const float railY = FALLEN_BREACH_D * 0.5f + FALLEN_RAIL_D * 0.5f;

  if (index == 0) {
    return {wreckX, g.boss.y - railY, FALLEN_WRECK_W, FALLEN_RAIL_D, FALLEN_WRECK_H, 9, true};
  }
  if (index == 1) {
    return {wreckX, g.boss.y + railY, FALLEN_WRECK_W, FALLEN_RAIL_D, FALLEN_WRECK_H, 9, true};
  }

  // Close the inner end of the wreck while leaving the outer collapsed side open.
  const float innerEdgeX = wreckX - side * (FALLEN_WRECK_W * 0.5f);
  const float rearX = innerEdgeX + side * (FALLEN_REAR_W * 0.5f);
  return {rearX, g.boss.y, FALLEN_REAR_W, FALLEN_BREACH_D, FALLEN_WRECK_H, 9, true};
}

static bool fallenFortressActive() {
  return g.boss.phase >= BossPhase::FallenDeploying && g.boss.phase != BossPhase::Defeated;
}

static bool playerPositionBlocked(float x, float y, float radius) {
  if (!bossFreeRoam(y)) {
    float hw = corridorHalfWidth(y) - radius;
    if (x < -hw || x > hw) return true;
  }
  if (y < 1.0f || y > 1000.0f) return true;
  for (uint8_t i = 0; i < WORLD_BOX_COUNT; ++i) {
    if (WORLD_BOXES[i].solid && circleIntersectsBox(x, y, radius, WORLD_BOXES[i])) return true;
  }
  // Fallen fortress collision exactly matches the visible breached shell.
  if (fallenFortressActive()) {
    for (uint8_t i = 0; i < FALLEN_FORTRESS_PIECES; ++i) {
      StaticBox piece = fallenFortressPiece(i);
      if (circleIntersectsBox(x, y, radius, piece)) return true;
    }
  }
  return false;
}

static void movePlayer(float dx, float dy) {
  constexpr float R = 0.85f;
  float nx = g.player.pose.x + dx;
  if (!playerPositionBlocked(nx, g.player.pose.y, R)) g.player.pose.x = nx;
  float ny = g.player.pose.y + dy;
  if (!playerPositionBlocked(g.player.pose.x, ny, R)) g.player.pose.y = ny;
}

static bool segmentBox2D(float x0, float y0, float x1, float y1,
                         const StaticBox &b, float &tHit) {
  float tmin = 0.0f, tmax = 1.0f;
  float dx = x1 - x0, dy = y1 - y0;
  const float mins[2] = {b.x - b.w * 0.5f, b.y - b.d * 0.5f};
  const float maxs[2] = {b.x + b.w * 0.5f, b.y + b.d * 0.5f};
  const float p[2] = {x0, y0};
  const float d[2] = {dx, dy};
  for (int a = 0; a < 2; ++a) {
    if (fabsf(d[a]) < 0.0001f) {
      if (p[a] < mins[a] || p[a] > maxs[a]) return false;
    } else {
      float ood = 1.0f / d[a];
      float t1 = (mins[a] - p[a]) * ood;
      float t2 = (maxs[a] - p[a]) * ood;
      if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
      if (t1 > tmin) tmin = t1;
      if (t2 < tmax) tmax = t2;
      if (tmin > tmax) return false;
    }
  }
  tHit = tmin;
  return true;
}

static bool lineBlocked(float x0, float y0, float x1, float y1, float *outT = nullptr) {
  float best = 2.0f;
  for (uint8_t i = 0; i < WORLD_BOX_COUNT; ++i) {
    float t;
    if (WORLD_BOXES[i].solid && segmentBox2D(x0, y0, x1, y1, WORLD_BOXES[i], t)) {
      if (t < best) best = t;
    }
  }
  if (fallenFortressActive()) {
    for (uint8_t i = 0; i < FALLEN_FORTRESS_PIECES; ++i) {
      StaticBox piece = fallenFortressPiece(i);
      float t;
      if (segmentBox2D(x0, y0, x1, y1, piece, t) && t < best) best = t;
    }
  }
  if (best <= 1.0f) {
    if (outT) *outT = best;
    return true;
  }
  return false;
}

static bool rayCircle(float ox, float oy, float dx, float dy,
                      float cx, float cy, float radius, float maxRange, float &outT) {
  float rx = cx - ox;
  float ry = cy - oy;
  float proj = rx * dx + ry * dy;
  if (proj < 0 || proj > maxRange) return false;
  float perp2 = rx * rx + ry * ry - proj * proj;
  float r2 = radius * radius;
  if (perp2 > r2) return false;
  float thc = sqrtf(r2 - perp2);
  float t = proj - thc;
  if (t < 0) t = proj + thc;
  if (t < 0 || t > maxRange) return false;
  outT = t;
  return true;
}

static bool segmentCircleHit(float x0, float y0, float x1, float y1,
                             float cx, float cy, float radius, float &tHit) {
  // Robust swept point-vs-circle test. Enemy shells move a noticeable distance
  // per simulation tick, so checking only a sample point can tunnel through the
  // player. This also catches a shell that starts a tick already inside the hull.
  const float sx = x0 - cx;
  const float sy = y0 - cy;
  const float r2 = radius * radius;
  if (sx * sx + sy * sy <= r2) {
    tHit = 0.0f;
    return true;
  }

  const float dx = x1 - x0;
  const float dy = y1 - y0;
  const float a = dx * dx + dy * dy;
  if (a < 0.0000001f) return false;

  const float b = 2.0f * (sx * dx + sy * dy);
  const float c = sx * sx + sy * sy - r2;
  const float disc = b * b - 4.0f * a * c;
  if (disc < 0.0f) return false;

  const float root = sqrtf(disc);
  const float inv2a = 0.5f / a;
  const float t0 = (-b - root) * inv2a;
  const float t1 = (-b + root) * inv2a;
  if (t0 >= 0.0f && t0 <= 1.0f) { tHit = t0; return true; }
  if (t1 >= 0.0f && t1 <= 1.0f) { tHit = t1; return true; }
  return false;
}

// -----------------------------------------------------------------------------
// Effects / damage
// -----------------------------------------------------------------------------

static void spawnEffect(EffectType type, float x, float y, uint16_t life) {
  int slot = -1;
  uint32_t oldest = 0xFFFFFFFFu;
  for (uint8_t i = 0; i < MAX_EFFECTS; ++i) {
    if (effects[i].type == EffectType::None) { slot = i; break; }
    if (effects[i].born < oldest) { oldest = effects[i].born; slot = i; }
  }
  effects[slot] = {type, x, y, g.simNow, life};
}

static void killSoldier(Soldier &s) {
  if (!s.alive && !s.dying) return;
  s.alive = false;
  s.dying = true;
  s.anim = 0;
  s.deathAt = g.simNow;
  g.score += 20;
}

static void startWaterDeath() {
  if (g.mode != Mode::PLAYING || !g.controlsArmed) return;
  if (g.player.lives > 0) --g.player.lives;
  g.player.health = 0;
  g.mode = Mode::DYING;
  g.modeStart = g.simNow;
  g.terminalReleased = false;
  g.controlsArmed = false;
  stopGameAudio();
  g.restartAtBeach = true;
  g.waterDeath = true;
  setMessage("SUNK", 1800);
}

static void damagePlayer(uint8_t damage) {
  if (g.mode != Mode::PLAYING || !g.controlsArmed) return;
  if ((int32_t)(g.simNow - g.player.invulnerableUntil) < 0) return;
  g.player.health -= damage;
  sfxHit();
  setMessage("ARMOR HIT", 650);
  if (g.player.health <= 0) {
    g.player.health = 0;
    g.mode = Mode::DYING;
    g.modeStart = g.simNow;
    g.terminalReleased = false; // used only if this death becomes GAME OVER
    if (g.player.lives > 0) --g.player.lives;
    g.controlsArmed = false;
    stopGameAudio();
    sfxExplosion();
    spawnEffect(EffectType::Explosion, g.player.pose.x, g.player.pose.y, 1000);
    setMessage("TANK DESTROYED", 1700);
  }
}

static void killEnemy(Enemy &e) {
  if (!e.alive) return;
  e.alive = false;
  e.active = false;
  e.windup = false;
  g.score += 100;
  // Enemy tanks get a longer, rising retro fireball/mushroom-cloud death.
  spawnEffect(EffectType::TankExplosion, e.x, e.y, 1600);
  sfxExplosion();
}

static void damageEnemy(Enemy &e, int dmg) {
  if (!e.alive) return;
  e.health -= dmg;
  if (e.health <= 0) killEnemy(e);
  else spawnEffect(EffectType::Spark, e.x, e.y, 220);
}

static void damageBossDefender(uint8_t idx, int dmg) {
  if (idx >= 6) return;
  BossDefender &bd = bossDefenders[idx];
  if (!bd.released || !bd.e.alive) return;
  damageEnemy(bd.e, dmg);
  if (!bd.e.alive && !bd.countedDead) {
    bd.countedDead = true;
    ++g.boss.defendersDestroyed;
  }
}

// -----------------------------------------------------------------------------
// Player weapons
// -----------------------------------------------------------------------------

enum HitKind : uint8_t { HIT_NONE, HIT_WORLD, HIT_ENEMY, HIT_SOLDIER, HIT_BOSS_LEG, HIT_BOSS_DEFENDER, HIT_CORE };
struct ShotHit {
  HitKind kind;
  float t;
  int index;
  float x, y;
};

static ShotHit tracePlayerShot(float maxRange, bool machineGun) {
  ShotHit hit{HIT_NONE, maxRange, -1, 0, 0};
  float s = sinf(g.player.pose.heading);
  float c = cosf(g.player.pose.heading);
  float ex = g.player.pose.x + s * maxRange;
  float ey = g.player.pose.y + c * maxRange;
  float wallT;
  if (lineBlocked(g.player.pose.x, g.player.pose.y, ex, ey, &wallT)) {
    hit.kind = HIT_WORLD;
    hit.t = wallT * maxRange;
  }

  for (uint8_t i = 0; i < ENEMY_COUNT; ++i) {
    Enemy &e = enemies[i];
    if (!e.alive || !e.active) continue;
    float t;
    if (rayCircle(g.player.pose.x, g.player.pose.y, s, c, e.x, e.y, 1.2f, hit.t, t)) {
      hit = {HIT_ENEMY, t, i, g.player.pose.x + s*t, g.player.pose.y + c*t};
    }
  }

  for (uint8_t i = 0; i < SOLDIER_COUNT; ++i) {
    Soldier &p = soldiers[i];
    if (!p.alive) continue;
    float t;
    if (rayCircle(g.player.pose.x, g.player.pose.y, s, c, p.x, p.y, 0.38f, hit.t, t)) {
      hit = {HIT_SOLDIER, t, i, g.player.pose.x + s*t, g.player.pose.y + c*t};
    }
  }

  for (uint8_t i = 0; i < 6; ++i) {
    BossDefender &bd = bossDefenders[i];
    if (!bd.released || !bd.e.alive) continue;
    float t;
    if (rayCircle(g.player.pose.x, g.player.pose.y, s, c, bd.e.x, bd.e.y, 1.2f, hit.t, t)) {
      hit = {HIT_BOSS_DEFENDER, t, i, g.player.pose.x + s*t, g.player.pose.y + c*t};
    }
  }

  if (g.boss.phase == BossPhase::Walking) {
    const float lx[3] = {-7.0f, 0.0f, 7.0f};
    const float ly[3] = {-3.0f, 4.0f, -3.0f};
    for (uint8_t i = 0; i < 3; ++i) {
      if (!g.boss.legAlive[i]) continue;
      float t;
      float bx = g.boss.x + lx[i];
      float by = g.boss.y + ly[i];
      if (rayCircle(g.player.pose.x, g.player.pose.y, s, c, bx, by, 2.15f, hit.t, t)) {
        hit = {HIT_BOSS_LEG, t, i, g.player.pose.x + s*t, g.player.pose.y + c*t};
      }
    }
  }

  if (g.boss.phase == BossPhase::CoreExposed) {
    float t;
    if (rayCircle(g.player.pose.x, g.player.pose.y, s, c,
                  g.boss.coreX, g.boss.coreY, 1.7f, hit.t, t)) {
      hit = {HIT_CORE, t, 0, g.player.pose.x + s*t, g.player.pose.y + c*t};
    }
  }

  if (hit.kind == HIT_NONE || hit.kind == HIT_WORLD) {
    hit.x = g.player.pose.x + s * hit.t;
    hit.y = g.player.pose.y + c * hit.t;
  }
  (void)machineGun;
  return hit;
}

static void resolveCannonHit(const ShotHit &hit) {
  switch (hit.kind) {
    case HIT_ENEMY:
      damageEnemy(enemies[hit.index], 50);
      break;
    case HIT_SOLDIER:
      killSoldier(soldiers[hit.index]);
      spawnEffect(EffectType::Explosion, hit.x, hit.y, 350);
      break;
    case HIT_BOSS_DEFENDER:
      damageBossDefender((uint8_t)hit.index, 50);
      break;
    case HIT_BOSS_LEG:
      if (g.boss.legAlive[hit.index]) {
        g.boss.legHealth[hit.index] -= 50;
        spawnEffect(EffectType::Explosion, hit.x, hit.y, 500);
        if (g.boss.legHealth[hit.index] <= 0) {
          g.boss.legAlive[hit.index] = false;
          setMessage("LEG DESTROYED", 1200);
          sfxExplosion();
        }
      }
      break;
    case HIT_CORE:
      g.boss.phase = BossPhase::Defeated;
      g.boss.phaseStart = g.simNow;
      g.mode = Mode::VICTORY_SEQUENCE;
      g.modeStart = g.simNow;
      spawnEffect(EffectType::Explosion, g.boss.coreX, g.boss.coreY, 1600);
      stopGameAudio();
      sfxExplosion();
      setMessage("FORTRESS DESTROYED", 2200);
      break;
    default:
      spawnEffect(EffectType::Explosion, hit.x, hit.y, 320);
      break;
  }

  // Splash affects ordinary nearby enemies/soldiers, but never boss objectives.
  for (uint8_t i = 0; i < ENEMY_COUNT; ++i) {
    if (hit.kind == HIT_ENEMY && i == (uint8_t)hit.index) continue;
    Enemy &e = enemies[i];
    if (!e.alive || !e.active) continue;
    float d2 = dist2(e.x, e.y, hit.x, hit.y);
    if (d2 < 3.5f * 3.5f && !lineBlocked(hit.x, hit.y, e.x, e.y)) {
      int dmg = (int)(30.0f * (1.0f - sqrtf(d2) / 3.5f));
      if (dmg > 0) damageEnemy(e, dmg);
    }
  }
  for (uint8_t i = 0; i < SOLDIER_COUNT; ++i) {
    Soldier &s = soldiers[i];
    if (!s.alive) continue;
    if (dist2(s.x, s.y, hit.x, hit.y) < 3.5f*3.5f && !lineBlocked(hit.x, hit.y, s.x, s.y)) {
      killSoldier(s);
    }
  }
}

static void fireCannon() {
  if (!g.controlsArmed || (int32_t)(g.simNow - g.player.cannonReadyAt) < 0) return;
  g.player.cannonReadyAt = g.simNow + 1250;
  g.player.cannonFlashUntil = g.simNow + 70;
  g.player.cannonRecoilUntil = g.simNow + 180;
  ShotHit hit = tracePlayerShot(110.0f, false);
  resolveCannonHit(hit);
  sfxCannon();
}

static void fireMgRound() {
  ShotHit hit = tracePlayerShot(60.0f, true);
  if (hit.kind == HIT_SOLDIER) {
    killSoldier(soldiers[hit.index]);
    spawnEffect(EffectType::Spark, hit.x, hit.y, 180);
  } else if (hit.kind == HIT_ENEMY) {
    Enemy &e = enemies[hit.index];
    EnemyClass cls = ENEMY_DEFS[e.defIndex].cls;
    if (cls == EnemyClass::Light) damageEnemy(e, 1);
    else spawnEffect(EffectType::Spark, hit.x, hit.y, 120);
  } else if (hit.kind == HIT_BOSS_DEFENDER) {
    BossDefender &bd = bossDefenders[hit.index];
    if (bd.cls == EnemyClass::Light) damageBossDefender(hit.index, 1);
    else spawnEffect(EffectType::Spark, hit.x, hit.y, 120);
  } else {
    spawnEffect(EffectType::Spark, hit.x, hit.y, 110);
  }
  sfxMG();
}

static void updateWeapons(const GameInput &input) {
  if (!g.controlsArmed || g.mode != Mode::PLAYING) return;

  // Physical RIGHT button = cannon. Physical LEFT button = machine gun.
  if (input.rightButton) fireCannon();

  if (g.player.mgRounds == 0) {
    if ((input.leftPressed || input.leftButton) &&
        (int32_t)(g.simNow - g.player.mgReadyAt) >= 0) {
      g.player.mgRounds = 3;
      g.player.mgNextRound = g.simNow;
    }
  }
  if (g.player.mgRounds > 0 && (int32_t)(g.simNow - g.player.mgNextRound) >= 0) {
    fireMgRound();
    --g.player.mgRounds;
    if (g.player.mgRounds > 0) g.player.mgNextRound += 80;
    else g.player.mgReadyAt = g.simNow + 220;
  }
}

// -----------------------------------------------------------------------------
// Enemy projectile / AI
// -----------------------------------------------------------------------------

static bool spawnProjectile(float x, float y, float heading, uint8_t dmg) {
  for (uint8_t i = 0; i < MAX_PROJECTILES; ++i) {
    if (!projectiles[i].active) {
      float s = sinf(heading), c = cosf(heading);
      projectiles[i] = {true, x, y, x, y, s * 16.0f, c * 16.0f, dmg, g.simNow + 6000};
      return true;
    }
  }
  return false;
}

static uint8_t currentWindups() {
  uint8_t count = 0;
  for (uint8_t i = 0; i < ENEMY_COUNT; ++i) if (enemies[i].alive && enemies[i].windup) ++count;
  for (uint8_t i = 0; i < 6; ++i) if (bossDefenders[i].released && bossDefenders[i].e.alive && bossDefenders[i].e.windup) ++count;
  return count;
}

static void updateEnemyOne(Enemy &e, EnemyClass cls, EnemyRole role, float dt) {
  if (!e.alive) return;
  float d2p = dist2(e.x, e.y, g.player.pose.x, g.player.pose.y);
  e.active = d2p < 115.0f * 115.0f;
  if (!e.active) return;

  bool protectedPlayer = !g.controlsArmed || (int32_t)(g.simNow - g.player.invulnerableUntil) < 0 || g.mode != Mode::PLAYING;
  float desired = angleTo(e.x, e.y, g.player.pose.x, g.player.pose.y);
  e.turretHeading = approachAngle(e.turretHeading, desired, 1.6f * dt);

  bool clear = !lineBlocked(e.x, e.y, g.player.pose.x, g.player.pose.y);
  float dist = sqrtf(d2p);
  bool visibleThreat = clear && dist < 48.0f;
  if (visibleThreat) e.observedUntil = g.simNow + 2500;

  if (e.windup) {
    if ((int32_t)(g.simNow - e.windupUntil) >= 0) {
      e.windup = false;
      if (!protectedPlayer && clear) {
        float commitHeading = e.turretHeading;
        if (spawnProjectile(e.x, e.y, commitHeading, classDamage(cls))) {
          startTone(tone2, cls == EnemyClass::Heavy ? 150 : 190, 90);
          e.reloadUntil = g.simNow + classReloadMs(cls);
        } else {
          e.reloadUntil = g.simNow + 300;
        }
      } else {
        e.reloadUntil = g.simNow + 500;
      }
    }
    return;
  }

  if (!protectedPlayer && clear && dist < 43.0f && dist > 3.0f &&
      (int32_t)(g.simNow - e.reloadUntil) >= 0 && currentWindups() < 2) {
    e.windup = true;
    e.windupUntil = g.simNow + (cls == EnemyClass::Heavy ? 1200 : 850);
    sfxWarning();
    return;
  }

  // Bounded local movement. Holders mostly stay put; pursuers/maneuverers move
  // within a modest region rather than chasing across the campaign.
  if (role != EnemyRole::Holder && dist > 15.0f && dist < 45.0f) {
    float moveHeading = desired;
    if (role == EnemyRole::Maneuverer) moveHeading += 0.55f * sinf((float)g.simNow * 0.0012f + e.defIndex);
    e.heading = approachAngle(e.heading, moveHeading, 1.2f * dt);
    float spd = classSpeed(cls) * 0.55f;
    float nx = e.x + sinf(e.heading) * spd * dt;
    float ny = e.y + cosf(e.heading) * spd * dt;
    if (!playerPositionBlocked(nx, ny, 1.0f)) { e.x = nx; e.y = ny; }
  }
}

static void updateBossDefenderDeployment(BossDefender &bd, uint8_t index, float dt) {
  if (!bd.deploying || !bd.e.alive) return;

  // Three slightly separated lanes keep successive defenders visually distinct.
  // The target lies beyond the open outer face of the collapsed fortress.
  const int lane = (int)(index % 3) - 1;
  const float targetX = g.boss.x + g.boss.collapseSide * 25.0f;
  const float targetY = g.boss.y + lane * 1.25f;
  const float desired = angleTo(bd.e.x, bd.e.y, targetX, targetY);
  bd.e.heading = approachAngle(bd.e.heading, desired, 2.8f * dt);
  bd.e.turretHeading = bd.e.heading;
  bd.e.active = true;
  bd.e.windup = false;

  constexpr float DEPLOY_SPEED = 4.8f;
  float nx = bd.e.x + sinf(bd.e.heading) * DEPLOY_SPEED * dt;
  float ny = bd.e.y + cosf(bd.e.heading) * DEPLOY_SPEED * dt;
  if (!playerPositionBlocked(nx, ny, 1.0f)) {
    bd.e.x = nx;
    bd.e.y = ny;
  }

  // Once the tank is several units beyond the wreck mouth, hand it to normal AI.
  float outward = (bd.e.x - g.boss.x) * g.boss.collapseSide;
  if (outward >= 24.0f || dist2(bd.e.x, bd.e.y, targetX, targetY) < 1.5f * 1.5f) {
    bd.deploying = false;
    bd.e.reloadUntil = g.simNow + 700;
  }
}

static void updateEnemies(float dt) {
  for (uint8_t i = 0; i < ENEMY_COUNT; ++i) {
    updateEnemyOne(enemies[i], ENEMY_DEFS[i].cls, ENEMY_DEFS[i].role, dt);
  }
  for (uint8_t i = 0; i < 6; ++i) {
    if (!bossDefenders[i].released || !bossDefenders[i].e.alive) continue;
    if (bossDefenders[i].deploying) {
      updateBossDefenderDeployment(bossDefenders[i], i, dt);
    } else {
      updateEnemyOne(bossDefenders[i].e, bossDefenders[i].cls, EnemyRole::Pursuer, dt);
    }
    if (!bossDefenders[i].e.alive && !bossDefenders[i].countedDead) {
      bossDefenders[i].countedDead = true;
      ++g.boss.defendersDestroyed;
    }
  }
}

static void updateProjectiles(float dt) {
  // Player pose is near the tank/camera center, while the incoming shell has
  // visible width. Use a slightly expanded hull so visible edge hits count.
  constexpr float PLAYER_SHELL_HIT_RADIUS = 1.15f;

  for (uint8_t i = 0; i < MAX_PROJECTILES; ++i) {
    Projectile &p = projectiles[i];
    if (!p.active) continue;
    if ((int32_t)(g.simNow - p.expires) >= 0) { p.active = false; continue; }

    p.px = p.x;
    p.py = p.y;
    const float nx = p.x + p.vx * dt;
    const float ny = p.y + p.vy * dt;

    // Resolve the earliest contact on the swept segment. The old world-first
    // branch could consume a shell at a wall even when the player contact was
    // slightly earlier on the same simulation step.
    float tPlayer = 2.0f;
    float tWorld = 2.0f;
    const bool playerHit = segmentCircleHit(
        p.x, p.y, nx, ny,
        g.player.pose.x, g.player.pose.y, PLAYER_SHELL_HIT_RADIUS, tPlayer);
    const bool worldHit = lineBlocked(p.x, p.y, nx, ny, &tWorld);

    if (playerHit && (!worldHit || tPlayer <= tWorld)) {
      p.x += (nx - p.x) * tPlayer;
      p.y += (ny - p.y) * tPlayer;
      damagePlayer(p.damage);
      spawnEffect(EffectType::Explosion, p.x, p.y, 300);
      p.active = false;
      continue;
    }

    if (worldHit) {
      p.x += (nx - p.x) * tWorld;
      p.y += (ny - p.y) * tWorld;
      spawnEffect(EffectType::Explosion, p.x, p.y, 300);
      p.active = false;
      continue;
    }

    p.x = nx;
    p.y = ny;
  }
}

// -----------------------------------------------------------------------------
// Infantry
// -----------------------------------------------------------------------------

static void updateSoldiers(float dt) {
  for (uint8_t i = 0; i < SOLDIER_COUNT; ++i) {
    Soldier &s = soldiers[i];
    if (!s.alive) continue;
    float d2p = dist2(s.x, s.y, g.player.pose.x, g.player.pose.y);
    if (d2p > 100.0f * 100.0f) continue;
    if ((int32_t)(g.simNow - s.nextDecision) >= 0) {
      s.nextDecision = g.simNow + 200 + (i * 37u) % 160u;
      if (d2p < 11.0f * 11.0f) {
        float away = angleTo(g.player.pose.x, g.player.pose.y, s.x, s.y);
        s.heading = away + ((i & 1) ? 0.35f : -0.35f);
      } else {
        s.heading += ((i & 1) ? 0.20f : -0.17f);
      }
    }
    if (d2p < 18.0f * 18.0f) {
      float spd = 2.2f;
      float nx = s.x + sinf(s.heading) * spd * dt;
      float ny = s.y + cosf(s.heading) * spd * dt;
      if (!playerPositionBlocked(nx, ny, 0.25f)) { s.x = nx; s.y = ny; }
      s.anim++;
    }
  }
}

// -----------------------------------------------------------------------------
// Boss attacks and phase machine
// -----------------------------------------------------------------------------

static uint8_t livingBossLegs() {
  return (g.boss.legAlive[0] ? 1 : 0) + (g.boss.legAlive[1] ? 1 : 0) + (g.boss.legAlive[2] ? 1 : 0);
}

static void beginBossCollapse() {
  g.boss.phase = BossPhase::Collapsing;
  g.boss.phaseStart = g.simNow;
  g.boss.collapseSide = g.player.pose.x <= g.boss.x ? 1.0f : -1.0f;
  for (uint8_t i = 0; i < 3; ++i) missileZones[i].active = false;
  footZone.active = false;
  setMessage("FORTRESS FALLING", 2500);
  sfxExplosion();
}

static void commitMissileVolley() {
  // Two fixed zones; deliberately offset so the pair does not erase every escape.
  const float h = g.player.pose.heading;
  const float fx = sinf(h), fy = cosf(h);
  const float rx = cosf(h), ry = -sinf(h);
  float tx[2] = { g.player.pose.x, g.player.pose.x + rx * 8.0f + fx * 4.0f };
  float ty[2] = { g.player.pose.y, g.player.pose.y + ry * 8.0f + fy * 4.0f };
  for (uint8_t i = 0; i < 2; ++i) {
    missileZones[i] = {true, false, tx[i], ty[i], 4.0f, g.simNow, g.simNow + 3000, 40};
  }
  sfxWarning();
}

static void commitFootfall() {
  footZone = {true, false, g.player.pose.x, g.player.pose.y, 3.6f,
              g.simNow, g.simNow + 1600, 60};
  sfxWarning();
}

static void updateWarningZone(WarningZone &z) {
  if (!z.active) return;
  if (!z.impacted && (int32_t)(g.simNow - z.impactAt) >= 0) {
    z.impacted = true;
    spawnEffect(EffectType::Explosion, z.x, z.y, 650);
    if (dist2(z.x, z.y, g.player.pose.x, g.player.pose.y) < z.radius * z.radius) {
      float d = sqrtf(dist2(z.x, z.y, g.player.pose.x, g.player.pose.y));
      float frac = 1.0f - clampf(d / z.radius, 0.0f, 1.0f);
      uint8_t dmg = (uint8_t)(z.damage * (0.5f + 0.5f * frac));
      damagePlayer(dmg);
    }
    sfxExplosion();
  }
  if (z.impacted && elapsedMs(g.simNow, z.impactAt) > 500) z.active = false;
}

static void spawnBossDefender(uint8_t index) {
  if (index >= 6 || bossDefenders[index].released) return;
  static const EnemyClass classes[6] = {
    EnemyClass::Light, EnemyClass::Medium, EnemyClass::Medium,
    EnemyClass::Medium, EnemyClass::Light, EnemyClass::Heavy
  };

  const int lane = (int)(index % 3) - 1;
  BossDefender &bd = bossDefenders[index];
  bd.cls = classes[index];
  bd.released = true;
  bd.countedDead = false;
  bd.deploying = true;

  // Spawn inside the central tunnel, safely clear of both side rails.  Tanks first
  // drive through the visible breach before switching to normal pursuit behavior.
  bd.e.x = g.boss.x + g.boss.collapseSide * 8.0f;
  bd.e.y = g.boss.y + lane * 1.8f;
  float exitX = g.boss.x + g.boss.collapseSide * 25.0f;
  float exitY = g.boss.y + lane * 1.25f;
  bd.e.heading = angleTo(bd.e.x, bd.e.y, exitX, exitY);
  bd.e.turretHeading = bd.e.heading;
  bd.e.health = classHealth(bd.cls);
  bd.e.defIndex = index;
  bd.e.alive = true;
  bd.e.active = true;
  bd.e.windup = false;
  bd.e.windupUntil = 0;
  bd.e.reloadUntil = g.simNow + 1500;
  bd.e.observedUntil = 0;

  // Defensive fallback: if authored geometry changes later, never leave a newly
  // released tank embedded in a solid piece.  Walk it toward the breach mouth.
  for (uint8_t tries = 0; tries < 12 && playerPositionBlocked(bd.e.x, bd.e.y, 1.0f); ++tries) {
    bd.e.x += g.boss.collapseSide * 1.0f;
  }
}

static void updateBoss(float dt) {
  if (g.boss.phase == BossPhase::Dormant) {
    if (g.player.pose.y > 650.0f) {
      g.boss.phase = BossPhase::Walking;
      g.boss.phaseStart = g.simNow;
      setMessage("FORTRESS CONTACT", 1800);
      g.boss.nextMissile = g.simNow + 4500;
      g.boss.nextFoot = g.simNow + 3000;
    }
    return;
  }

  if (g.boss.phase == BossPhase::Walking) {
    uint8_t legs = livingBossLegs();
    if (legs == 0) { beginBossCollapse(); return; }
    float speed = legs == 3 ? 2.0f : (legs == 2 ? 1.6f : 1.2f);
    if (g.boss.y < 920.0f) g.boss.y += speed * dt;

    if ((int32_t)(g.simNow - g.boss.nextMissile) >= 0) {
      bool anyZone = false;
      for (uint8_t i = 0; i < 3; ++i) if (missileZones[i].active) anyZone = true;
      if (!anyZone && !footZone.active) {
        commitMissileVolley();
        g.boss.nextMissile = g.simNow + 5500;
      }
    }
    if ((int32_t)(g.simNow - g.boss.nextFoot) >= 0 && !footZone.active) {
      bool anyZone = false;
      for (uint8_t i = 0; i < 3; ++i) if (missileZones[i].active) anyZone = true;
      if (!anyZone) {
        commitFootfall();
        g.boss.nextFoot = g.simNow + 4500;
      }
    }
  } else if (g.boss.phase == BossPhase::Collapsing) {
    uint32_t age = elapsedMs(g.simNow, g.boss.phaseStart);
    float t = clampf(age / 5000.0f, 0.0f, 1.0f);
    g.boss.bodyZ = 11.0f - 8.0f * t;
    g.boss.x += g.boss.collapseSide * 0.012f; // slow visible sideways fall drift
    if (age >= 5000) {
      g.boss.phase = BossPhase::FallenDeploying;
      g.boss.phaseStart = g.simNow;
      g.boss.nextRelease = g.simNow + 900;
      setMessage("DESTROY 6 DEFENDERS", 2200);
    }
  } else if (g.boss.phase == BossPhase::FallenDeploying) {
    if (g.boss.defendersReleased < 6 && (int32_t)(g.simNow - g.boss.nextRelease) >= 0) {
      spawnBossDefender(g.boss.defendersReleased);
      ++g.boss.defendersReleased;
      g.boss.nextRelease = g.simNow + 2000;
    }
    if (g.boss.defendersDestroyed >= 6) {
      g.boss.phase = BossPhase::CoreOpening;
      g.boss.phaseStart = g.simNow;
      setMessage("CORE OPENING", 1500);
      startTone(tone1, 330, 180);
    }
  } else if (g.boss.phase == BossPhase::CoreOpening) {
    if (elapsedMs(g.simNow, g.boss.phaseStart) >= 1500) {
      g.boss.phase = BossPhase::CoreExposed;
      g.boss.coreX = g.boss.x + g.boss.collapseSide * 23.0f;
      g.boss.coreY = g.boss.y;
      setMessage("FIRE ON CORE", 1600);
    }
  }

  for (uint8_t i = 0; i < 3; ++i) updateWarningZone(missileZones[i]);
  updateWarningZone(footZone);
}

// -----------------------------------------------------------------------------
// Player movement / lifecycle
// -----------------------------------------------------------------------------

static void updateDrive(float dt) {
  if (!g.controlsArmed || g.mode != Mode::PLAYING) return;
  int L = (int)readTread(LEFT_UP_PIN, LEFT_DOWN_PIN);
  int R = (int)readTread(RIGHT_UP_PIN, RIGHT_DOWN_PIN);
  float linear = 7.0f * (float)(L + R) * 0.5f;
  float angular = 1.65f * (float)(L - R) * 0.5f;
  float mid = g.player.pose.heading + angular * dt * 0.5f;
  movePlayer(sinf(mid) * linear * dt, cosf(mid) * linear * dt);
  g.player.pose.heading = wrapAngle(g.player.pose.heading + angular * dt);
}

static void updateSafeBreadcrumb() {
  if (!g.controlsArmed || g.mode != Mode::PLAYING) return;
  if ((int32_t)(g.simNow - g.player.nextSafeSave) < 0) return;
  g.player.nextSafeSave = g.simNow + 1000;
  bool danger = false;
  for (uint8_t i = 0; i < 3; ++i) {
    if (missileZones[i].active && dist2(missileZones[i].x, missileZones[i].y,
        g.player.pose.x, g.player.pose.y) < 8.0f * 8.0f) danger = true;
  }
  if (footZone.active && dist2(footZone.x, footZone.y, g.player.pose.x, g.player.pose.y) < 8.0f*8.0f) danger = true;
  if (!danger) {
    g.player.safeX = g.player.pose.x;
    g.player.safeY = g.player.pose.y;
    g.player.safeHeading = g.player.pose.heading;
  }
}

static void respawnPlayer() {
  float x = g.player.pose.x;
  float y = g.player.pose.y;
  float heading = g.player.pose.heading;

  if (g.restartAtBeach) {
    x = 0.0f;
    y = 18.0f;
    heading = 0.0f;
  } else {
    // A normal death does NOT reset campaign progress or send the player back to
    // the beach. Respawn at the death position/heading so the fight continues
    // exactly where it left off. If dynamic boss geometry changed during the
    // death/blackout and made that exact point invalid, fall back to the most
    // recent safe breadcrumb rather than placing the tank inside solid geometry.
    if (playerPositionBlocked(x, y, 0.85f)) {
      x = g.player.safeX;
      y = g.player.safeY;
      heading = g.player.safeHeading;
    }

    // Final defensive fallback for an unusual geometry transition.
    if (playerPositionBlocked(x, y, 0.85f)) {
      x = 0.0f;
      y = clampf(g.player.pose.y - 4.0f, 8.0f, 930.0f);
      while (playerPositionBlocked(x, y, 0.85f) && y > 8.0f) y -= 4.0f;
    }
  }

  g.player.pose = {x, y, heading};
  g.player.safeX = x;
  g.player.safeY = y;
  g.player.safeHeading = heading;
  g.player.nextSafeSave = g.simNow + 1000;
  g.player.health = 100;
  g.player.cannonReadyAt = g.simNow;
  g.player.mgRounds = 0;
  g.player.mgReadyAt = g.simNow;
  g.controlsArmed = false;
  g.player.invulnerableUntil = 0;
  g.mode = Mode::RESPAWN_GATE;
  g.modeStart = g.simNow;
  g.restartAtBeach = false;
  g.waterDeath = false;
  setMessage("RESPAWN", 1200);
}

static void updateLifecycle(const GameInput &input) {
  if (g.mode == Mode::BRIEFING) {
    if (!g.controlsArmed && switchesCentered() && !input.leftButton && !input.rightButton &&
        (int32_t)(g.simNow - g.briefingUntil) >= 0) {
      g.controlsArmed = true;
      g.mode = Mode::PLAYING;
      g.player.invulnerableUntil = g.simNow + 1200;
      setMessage("INVADE THE NORTH", 1100);
    }
  } else if (g.mode == Mode::DYING) {
    // 0-1700 ms: destruction animation.
    // 1700-2350 ms: guaranteed full-black hold (650 ms).
    // Then automatically respawn in place. Only an actual game over waits for
    // a fire-button press and resets the whole run back to the beach.
    if (elapsedMs(g.simNow, g.modeStart) >= 2350) {
      if (g.player.lives == 0) {
        g.mode = Mode::GAME_OVER;
        g.modeStart = g.simNow;
        g.terminalReleased = false;
        setMessage("GAME OVER", 60000);
      } else {
        respawnPlayer();
      }
    }
  } else if (g.mode == Mode::RESPAWN_GATE) {
    if (switchesCentered() && !input.leftButton && !input.rightButton) {
      g.controlsArmed = true;
      g.mode = Mode::PLAYING;
      g.player.invulnerableUntil = g.simNow + 3000;
      // No text callout here: the flashing cockpit/tank effect below is the
      // feedback that the freshly respawned player is temporarily protected.
    }
  } else if (g.mode == Mode::GAME_OVER || g.mode == Mode::VICTORY_WAIT) {
    if (!input.leftButton && !input.rightButton) g.terminalReleased = true;
    if (g.terminalReleased && (input.leftPressed || input.rightPressed)) {
      // Full new run; exit remains available through launcher both-button chord.
      // Forward declaration below.
    }
  } else if (g.mode == Mode::VICTORY_SEQUENCE) {
    if (elapsedMs(g.simNow, g.modeStart) > 3400) {
      g.mode = Mode::VICTORY_WAIT;
      g.terminalReleased = false;
      setMessage("MISSION COMPLETE", 60000);
    }
  }
}

// -----------------------------------------------------------------------------
// Renderer: camera, indexed framebuffer, depth-tested triangles/sprites
// -----------------------------------------------------------------------------

static inline void put(int x, int y, uint8_t c) {
  if ((unsigned)x < RW && (unsigned)y < RH) frame[y * RW + x] = c;
}

static inline uint8_t depthCode(float d) {
  // 8-bit linear camera-depth buffer. At this 120x128 render resolution,
  // sub-meter depth precision is sufficient and saves 15,360 bytes of DRAM
  // versus the previous uint16_t buffer. Larger values are nearer.
  constexpr float FAR_D = 125.0f;
  if (d <= NEAR_D) return 255;
  if (d >= FAR_D) return 1;
  float t = (FAR_D - d) / (FAR_D - NEAR_D);
  int v = 1 + (int)(t * 254.0f + 0.5f);
  if (v < 1) v = 1;
  if (v > 255) v = 255;
  return (uint8_t)v;
}

static inline void putDepth(int x, int y, float d, uint8_t c) {
  if ((unsigned)x >= RW || (unsigned)y >= WORLD_H || d <= NEAR_D) return;
  uint8_t z = depthCode(d);
  uint8_t &dst = depthBuf[y * RW + x];
  if (z >= dst) { dst = z; frame[y * RW + x] = c; }
}

static inline CamV worldToCam(float x, float y, float z) {
  float sh = sinf(g.player.pose.heading);
  float ch = cosf(g.player.pose.heading);
  float dx = x - g.player.pose.x;
  float dy = y - g.player.pose.y;
  CamV v;
  v.d = dx * sh + dy * ch;
  v.r = dx * ch - dy * sh;
  v.h = z - EYE_Z;
  return v;
}

static bool projectCam(const CamV &v, ScreenV &s) {
  if (v.d < NEAR_D) return false;
  s.x = 60.0f + FOCAL * v.r / v.d;
  s.y = (float)HORIZON_Y - FOCAL * v.h / v.d;
  s.invD = 1.0f / v.d;
  return true;
}

static uint8_t clipNear(const CamV in[3], CamV out[4]) {
  CamV tmp[5];
  uint8_t n = 0;
  for (uint8_t i = 0; i < 3; ++i) {
    const CamV &a = in[i];
    const CamV &b = in[(i + 1) % 3];
    bool ina = a.d >= NEAR_D;
    bool inb = b.d >= NEAR_D;
    if (ina) tmp[n++] = a;
    if (ina != inb) {
      float t = (NEAR_D - a.d) / (b.d - a.d);
      CamV q;
      q.d = NEAR_D;
      q.r = a.r + (b.r - a.r) * t;
      q.h = a.h + (b.h - a.h) * t;
      tmp[n++] = q;
    }
  }
  for (uint8_t i = 0; i < n; ++i) out[i] = tmp[i];
  return n;
}

static float edgef(float ax, float ay, float bx, float by, float px, float py) {
  return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

static void rasterTri(const ScreenV &a, const ScreenV &b, const ScreenV &c, uint8_t color) {
  float area = edgef(a.x, a.y, b.x, b.y, c.x, c.y);
  if (fabsf(area) < 0.001f) return;
  int minX = clampi((int)floorf(fminf(a.x, fminf(b.x, c.x))), 0, RW - 1);
  int maxX = clampi((int)ceilf (fmaxf(a.x, fmaxf(b.x, c.x))), 0, RW - 1);
  int minY = clampi((int)floorf(fminf(a.y, fminf(b.y, c.y))), 0, WORLD_H - 1);
  int maxY = clampi((int)ceilf (fmaxf(a.y, fmaxf(b.y, c.y))), 0, WORLD_H - 1);
  float invArea = 1.0f / area;
  for (int y = minY; y <= maxY; ++y) {
    for (int x = minX; x <= maxX; ++x) {
      float px = x + 0.5f, py = y + 0.5f;
      float w0 = edgef(b.x, b.y, c.x, c.y, px, py);
      float w1 = edgef(c.x, c.y, a.x, a.y, px, py);
      float w2 = edgef(a.x, a.y, b.x, b.y, px, py);
      if ((area > 0 && w0 >= 0 && w1 >= 0 && w2 >= 0) ||
          (area < 0 && w0 <= 0 && w1 <= 0 && w2 <= 0)) {
        w0 *= invArea; w1 *= invArea; w2 *= invArea;
        float invD = a.invD * w0 + b.invD * w1 + c.invD * w2;
        if (invD <= 0) continue;
        float d = 1.0f / invD;
        uint8_t shade = color;
        if (((x + y) & 7) == 0 && color != 11 && color != 21 && color < 28) {
          // Very light authored-looking surface texture without extra RAM assets.
          shade = color;
        }
        putDepth(x, y, d, shade);
      }
    }
  }
}

static void drawWorldTri(Vec3 a, Vec3 b, Vec3 c, uint8_t color) {
  CamV cv[3] = {worldToCam(a.x,a.y,a.z), worldToCam(b.x,b.y,b.z), worldToCam(c.x,c.y,c.z)};
  CamV clipped[4];
  uint8_t n = clipNear(cv, clipped);
  if (n < 3) return;
  ScreenV p0;
  if (!projectCam(clipped[0], p0)) return;
  for (uint8_t i = 1; i + 1 < n; ++i) {
    ScreenV p1, p2;
    if (projectCam(clipped[i], p1) && projectCam(clipped[i+1], p2)) rasterTri(p0, p1, p2, color);
  }
}

static void drawBoxYaw(float cx, float cy, float z0, float w, float d, float h, float yaw, uint8_t color) {
  float sx = w * 0.5f, sy = d * 0.5f;
  float cs = cosf(yaw), sn = sinf(yaw);
  Vec3 v[8];
  const float lx[4] = {-sx, sx, sx, -sx};
  const float ly[4] = {-sy, -sy, sy, sy};
  for (uint8_t i = 0; i < 4; ++i) {
    float wx = cx + lx[i] * cs + ly[i] * sn;
    float wy = cy - lx[i] * sn + ly[i] * cs;
    v[i] = {wx, wy, z0};
    v[i+4] = {wx, wy, z0 + h};
  }
  const uint8_t tris[12][3] = {
    {0,1,5},{0,5,4}, {1,2,6},{1,6,5}, {2,3,7},{2,7,6},
    {3,0,4},{3,4,7}, {4,5,6},{4,6,7}, {0,2,1},{0,3,2}
  };
  for (uint8_t i = 0; i < 12; ++i) {
    uint8_t faceColor = color;
    if (i >= 8 && i < 10 && color != 11) faceColor = color == 8 ? 26 : color;
    drawWorldTri(v[tris[i][0]], v[tris[i][1]], v[tris[i][2]], faceColor);
  }
}

static void drawBox(const StaticBox &b) {
  drawBoxYaw(b.x, b.y, 0, b.w, b.d, b.h, 0, b.color);
  // Window/battle-damage accents on taller facades.
  if (b.h > 6.0f) {
    float z = b.h * 0.55f;
    drawBoxYaw(b.x, b.y - b.d*0.505f, z, b.w*0.55f, 0.08f, b.h*0.18f, 0, 9);
  }
}

static uint8_t groundColorAt(float x, float y) {
  uint8_t area = areaForY(y);
  uint8_t base = 5;
  if (area == 0) base = 4;
  else if (area == 1) base = 5;
  else if (area == 2) base = 6;
  else if (area == 3) base = 5;
  else if (area == 4) base = 16;
  else if (area >= 5) base = 5;

  // Roads / avenues
  if ((area == 3 && fabsf(x) < 7.0f) || (area == 4 && fabsf(x) < 9.0f) ||
      (area >= 5 && fabsf(x) < 8.0f)) base = 7;

  // Visual trenches. Crossings leave gaps, matching tank-traversable authored bridges.
  if (area == 2) {
    bool crossing = fmodf(y, 55.0f) > 23.0f && fmodf(y,55.0f) < 34.0f;
    if (!crossing && (fabsf(x - 18.0f) < 2.2f || fabsf(x + 20.0f) < 2.2f)) base = 18;
  }

  // Beach surf behind the player.
  if (y < 8.0f) base = 17;

  uint32_t h = hash2i((int)floorf(x * 0.7f), (int)floorf(y * 0.7f));
  if ((h & 31u) == 0u && base != 17 && base != 18 && base != 7) {
    if (base == 4) return 5;
    if (base == 5) return 27;
    if (base == 6) return 5;
    if (base == 16) return 26;
  }
  return base;
}

static void clearAndDrawGround() {
  // Sky
  for (int y = 0; y < HORIZON_Y; ++y) {
    uint8_t c = y < 25 ? 3 : 2;
    for (int x = 0; x < RW; ++x) frame[y*RW+x] = c;
  }
  memset(depthBuf, 0, RW * WORLD_H);

  float sh = sinf(g.player.pose.heading);
  float ch = cosf(g.player.pose.heading);
  for (int sy = HORIZON_Y; sy < WORLD_H; ++sy) {
    float denom = (float)(sy - HORIZON_Y) + 0.5f;
    float d = EYE_Z * FOCAL / denom;
    if (d > 120.0f) d = 120.0f;
    float lateralScale = d / FOCAL;
    for (int sx = 0; sx < RW; ++sx) {
      float r = ((float)sx + 0.5f - 60.0f) * lateralScale;
      float wx = g.player.pose.x + sh*d + ch*r;
      float wy = g.player.pose.y + ch*d - sh*r;
      frame[sy*RW+sx] = groundColorAt(wx, wy);
      depthBuf[sy*RW+sx] = depthCode(d);
    }
  }

  // Cockpit band base; detailed deck is drawn later.
  for (int y = WORLD_H; y < RH; ++y) {
    for (int x = 0; x < RW; ++x) frame[y*RW+x] = 23;
  }
}

static void drawDepthRectSprite(float wx, float wy, float wz,
                                float worldW, float worldH,
                                const uint8_t *pixels, int sw, int sh) {
  CamV c = worldToCam(wx, wy, wz + worldH * 0.5f);
  ScreenV p;
  if (!projectCam(c, p) || c.d > 125.0f) return;
  float pw = FOCAL * worldW / c.d;
  float ph = FOCAL * worldH / c.d;
  if (pw < 1 || ph < 1) return;
  int x0 = (int)(p.x - pw*0.5f), y0 = (int)(p.y - ph*0.5f);
  int x1 = (int)(p.x + pw*0.5f), y1 = (int)(p.y + ph*0.5f);
  for (int y = y0; y <= y1; ++y) {
    if ((unsigned)y >= WORLD_H) continue;
    int v = clampi((int)(((float)(y-y0) / fmaxf(1.0f,(float)(y1-y0+1))) * sh), 0, sh-1);
    for (int x = x0; x <= x1; ++x) {
      if ((unsigned)x >= RW) continue;
      int u = clampi((int)(((float)(x-x0) / fmaxf(1.0f,(float)(x1-x0+1))) * sw), 0, sw-1);
      uint8_t col = pixels[v*sw+u];
      if (col) putDepth(x, y, c.d, col);
    }
  }
}

// Finished low-resolution authored-style sprites encoded directly as palette indexes.
static const uint8_t TANK_LIGHT_SPRITE[11*8] = {
  0,0,0,0,21,21,0,0,0,0,0,
  0,0,0,0,19,19,19,0,0,0,0,
  0,9,19,19,20,14,20,19,19,9,0,
  9,9,19,14,14,14,14,14,19,9,9,
  9,9,19,14,20,20,20,14,19,9,9,
  9,9,19,14,14,19,14,14,19,9,9,
  0,9,19,19,19,19,19,19,19,9,0,
  0,0,9,0,9,0,9,0,9,0,0
};
static const uint8_t TANK_MED_SPRITE[11*8] = {
  0,0,0,0,21,21,21,0,0,0,0,
  0,0,0,19,19,20,19,19,0,0,0,
  0,9,19,20,14,14,14,20,19,9,0,
  9,9,19,14,14,20,14,14,19,9,9,
  9,9,19,14,20,20,20,14,19,9,9,
  9,9,19,14,14,14,14,14,19,9,9,
  9,9,19,19,19,19,19,19,19,9,9,
  0,9,9,0,9,9,9,0,9,9,0
};
static const uint8_t TANK_HEAVY_SPRITE[11*8] = {
  0,0,0,21,21,21,21,21,0,0,0,
  0,0,19,19,20,20,20,19,19,0,0,
  0,9,19,20,14,14,14,20,19,9,0,
  9,9,19,14,14,20,14,14,19,9,9,
  9,9,19,14,20,20,20,14,19,9,9,
  9,9,20,14,14,14,14,14,20,9,9,
  9,9,19,19,19,19,19,19,19,9,9,
  9,0,9,0,9,0,9,0,9,0,9
};
static const uint8_t SOLDIER_SPRITE[5*9] = {
  0,0,19,0,0,
  0,19,19,19,0,
  0,0,20,0,0,
  0,20,20,20,0,
  19,20,20,20,19,
  0,20,20,20,0,
  0,20,0,20,0,
  20,0,0,0,20,
  20,0,0,0,20
};
static const uint8_t SOLDIER_FALLEN_SPRITE[9*5] = {
  0,0,19,19,19,0,0,0,0,
  0,19,20,20,20,19,0,0,0,
  20,20,20,20,20,20,19,0,0,
  0,19,20,20,20,19,0,20,20,
  0,0,0,20,20,0,0,0,20
};
static const uint8_t BLOOD_POOL_SPRITE[9*5] = {
  0,0,0,28,11,28,0,0,0,
  0,28,11,11,11,11,28,0,0,
  28,11,11,28,11,11,11,28,0,
  0,28,11,11,11,11,28,0,0,
  0,0,0,28,11,28,0,0,0
};
static const uint8_t SHELL_SPRITE[3*3] = {
  0,15,0,
  15,21,15,
  0,15,0
};

static const uint8_t* tankSpriteFor(EnemyClass cls) {
  if (cls == EnemyClass::Light) return TANK_LIGHT_SPRITE;
  if (cls == EnemyClass::Medium) return TANK_MED_SPRITE;
  return TANK_HEAVY_SPRITE;
}

static void drawEnemySprite(const Enemy &e, EnemyClass cls) {
  if (!e.alive || !e.active) return;
  float w = cls == EnemyClass::Light ? 2.4f : (cls == EnemyClass::Medium ? 2.9f : 3.4f);
  float h = cls == EnemyClass::Light ? 1.8f : (cls == EnemyClass::Medium ? 2.0f : 2.25f);
  drawDepthRectSprite(e.x, e.y, 0, w, h, tankSpriteFor(cls), 11, 8);

  // Windup lamp communicates the attack before the projectile appears.
  if (e.windup) {
    CamV c = worldToCam(e.x, e.y, h + 0.3f);
    ScreenV p;
    if (projectCam(c,p)) putDepth((int)p.x,(int)p.y,c.d,12);
  }
}

static void drawSoldier(const Soldier &s) {
  if (s.alive) {
    drawDepthRectSprite(s.x, s.y, 0, 0.75f, 1.85f, SOLDIER_SPRITE, 5, 9);
    return;
  }
  if (!s.dying) return;

  uint32_t age = elapsedMs(g.simNow, s.deathAt);
  float fall = clampf(age / 260.0f, 0.0f, 1.0f);
  float bleed = clampf((age - 90.0f) / 700.0f, 0.0f, 1.0f);
  float dx = sinf(s.heading);
  float dy = cosf(s.heading);
  float bodyX = s.x + dx * (0.16f * fall);
  float bodyY = s.y + dy * (0.16f * fall);

  if (bleed > 0.02f) {
    drawDepthRectSprite(bodyX + dx * 0.12f, bodyY + dy * 0.12f, 0.01f,
                        0.70f + 0.95f * bleed, 0.22f + 0.22f * bleed,
                        BLOOD_POOL_SPRITE, 9, 5);
  }

  if (fall < 0.45f) {
    drawDepthRectSprite(bodyX, bodyY, 0.0f,
                        0.75f + 0.25f * fall,
                        1.85f - 0.95f * fall,
                        SOLDIER_SPRITE, 5, 9);
  } else {
    drawDepthRectSprite(bodyX + dx * 0.06f, bodyY + dy * 0.06f, 0.0f,
                        1.35f + 0.30f * fall, 0.56f,
                        SOLDIER_FALLEN_SPRITE, 9, 5);
  }
}

static void drawProjectile(const Projectile &p) {
  if (!p.active) return;
  drawDepthRectSprite(p.x, p.y, 0.8f, 0.45f, 0.45f, SHELL_SPRITE, 3, 3);
}

static void drawEffectBlob(float wx, float wy, float wz, float worldRadius,
                           uint8_t outerCol, uint8_t innerCol, uint32_t phase) {
  CamV c = worldToCam(wx, wy, wz);
  ScreenV p;
  if (!projectCam(c,p) || c.d > 120.0f) return;

  int rad = clampi((int)(FOCAL * worldRadius / c.d), 1, 9);
  int r2 = rad * rad;
  int inner = rad > 2 ? rad - 2 : 1;
  int inner2 = inner * inner;
  for (int yy = -rad; yy <= rad; ++yy) {
    for (int xx = -rad; xx <= rad; ++xx) {
      int d2 = xx*xx + yy*yy;
      if (d2 > r2) continue;

      // Chunky checker/dither keeps the cloud in the same authored 8-bit style
      // as the tanks instead of reading like a smooth modern particle effect.
      uint32_t h = (uint32_t)((xx + rad) * 17 + (yy + rad) * 31) + phase;
      if (d2 <= inner2) {
        if ((h & 3u) != 0u) putDepth((int)p.x + xx, (int)p.y + yy, c.d - 0.02f, innerCol);
      } else if ((h & 1u) == 0u) {
        putDepth((int)p.x + xx, (int)p.y + yy, c.d - 0.02f, outerCol);
      }
    }
  }
}

static void drawTankExplosion(const Effect &e, uint32_t age) {
  const float t = clampf(age / (float)e.life, 0.0f, 1.0f);
  const uint32_t phase = age / 45u;

  // Initial ground-level blast blooms quickly around the destroyed hull.
  if (age < 360u) {
    float b = age / 360.0f;
    drawEffectBlob(e.x, e.y, 0.75f + b * 0.25f,
                   0.65f + b * 1.45f, 15, 21, phase);
    if (age < 180u) {
      drawEffectBlob(e.x, e.y, 0.95f, 0.42f + b * 0.65f, 12, 1, phase + 7u);
    }
  }

  // The stem climbs vertically from the wreck. Separate lumpy fire cells make
  // the shape read as a tiny arcade mushroom cloud rather than one tall circle.
  float topZ = 1.15f + 5.0f * t;
  int stemBlobs = 3 + (int)(t * 3.0f);
  for (int i = 0; i < stemBlobs; ++i) {
    float u = (i + 1) / (float)(stemBlobs + 1);
    float wobble = sinf((float)i * 2.3f + age * 0.006f) * (0.10f + 0.14f * u);
    float z = 0.65f + (topZ - 0.65f) * u;
    float r = 0.28f + 0.18f * u;
    uint8_t outer = age < 1050u ? 12 : 29;
    uint8_t inner = age < 1250u ? 15 : 12;
    drawEffectBlob(e.x + wobble, e.y, z, r, outer, inner, phase + i * 5u);
  }

  // Broad cap: five overlapping low-resolution fire lobes rise above the tank.
  // It starts compact, then spreads as it climbs and darkens near the end.
  float spread = 0.45f + 0.95f * t;
  float capR = 0.62f + 0.55f * t;
  uint8_t capOuter = age < 950u ? 15 : (age < 1350u ? 12 : 29);
  uint8_t capInner = age < 1150u ? 21 : 15;
  drawEffectBlob(e.x, e.y, topZ, capR * 1.15f, capOuter, capInner, phase + 11u);
  drawEffectBlob(e.x - spread, e.y, topZ - 0.08f, capR * 0.82f, capOuter, capInner, phase + 17u);
  drawEffectBlob(e.x + spread, e.y, topZ - 0.05f, capR * 0.88f, capOuter, capInner, phase + 23u);
  drawEffectBlob(e.x - spread * 0.35f, e.y - spread * 0.45f, topZ + 0.28f,
                 capR * 0.72f, capOuter, capInner, phase + 29u);
  drawEffectBlob(e.x + spread * 0.30f, e.y + spread * 0.38f, topZ + 0.18f,
                 capR * 0.68f, capOuter, capInner, phase + 37u);
}

static void drawEffect(const Effect &e) {
  if (e.type == EffectType::None) return;
  uint32_t age = elapsedMs(g.simNow, e.born);
  if (age >= e.life) return;

  if (e.type == EffectType::TankExplosion) {
    drawTankExplosion(e, age);
    return;
  }

  CamV c = worldToCam(e.x, e.y, 0.8f);
  ScreenV p;
  if (!projectCam(c,p) || c.d > 120) return;
  float t = 1.0f - age / (float)e.life;
  int rad = clampi((int)(FOCAL * (0.25f + 1.8f*(1.0f-t)) / c.d), 1, 8);
  uint8_t col = e.type == EffectType::Spark ? 21 : (e.type == EffectType::Dust ? 13 : 15);
  for (int yy=-rad; yy<=rad; ++yy) for (int xx=-rad; xx<=rad; ++xx) {
    if (xx*xx+yy*yy <= rad*rad && (((xx+yy+age/40)&1)==0)) putDepth((int)p.x+xx,(int)p.y+yy,c.d,col);
  }
}

static void drawWorldLine(Vec3 a, Vec3 b, uint8_t color) {
  CamV ca = worldToCam(a.x,a.y,a.z), cb = worldToCam(b.x,b.y,b.z);
  if (ca.d < NEAR_D && cb.d < NEAR_D) return;
  if (ca.d < NEAR_D || cb.d < NEAR_D) {
    float t = (NEAR_D - ca.d) / (cb.d - ca.d);
    CamV q{ca.r + (cb.r-ca.r)*t, ca.h+(cb.h-ca.h)*t, NEAR_D};
    if (ca.d < NEAR_D) ca=q; else cb=q;
  }
  ScreenV pa,pb;
  if(!projectCam(ca,pa)||!projectCam(cb,pb)) return;
  int x0=(int)pa.x,y0=(int)pa.y,x1=(int)pb.x,y1=(int)pb.y;
  int dx=abs(x1-x0), sx=x0<x1?1:-1;
  int dy=-abs(y1-y0), sy=y0<y1?1:-1;
  int err=dx+dy, steps=max(dx,-dy), n=0;
  while(true){
    float u=steps? n/(float)steps:0;
    float d=ca.d+(cb.d-ca.d)*u;
    putDepth(x0,y0,d,color);
    if(x0==x1&&y0==y1) break;
    int e2=2*err; if(e2>=dy){err+=dy;x0+=sx;} if(e2<=dx){err+=dx;y0+=sy;} ++n;
  }
}

static void drawWarning(const WarningZone &z, uint8_t color) {
  if (!z.active) return;
  const int N=18;
  Vec3 prev{z.x+z.radius,z.y,0.03f};
  for(int i=1;i<=N;++i){
    float a=TWO_PI_F*i/N;
    Vec3 cur{z.x+cosf(a)*z.radius,z.y+sinf(a)*z.radius,0.03f};
    drawWorldLine(prev,cur,color); prev=cur;
  }
}

static void drawTargetMarker(float wx, float wy, float wz, float sizeWorld) {
  CamV c = worldToCam(wx,wy,wz);
  ScreenV p;
  if (!projectCam(c,p) || c.d > 115.0f) return;

  // Keep the lock marker from hiding tiny distant sprites. At long range it is
  // only three corner pixels; nearby targets retain the small broken triangle.
  int r=clampi((int)(FOCAL*sizeWorld/c.d),2,5);
  float phase=g.simNow*0.0005f;
  float ax[3],ay[3];
  for(int i=0;i<3;++i){float a=phase-PI_F*0.5f+i*TWO_PI_F/3.0f; ax[i]=p.x+cosf(a)*r; ay[i]=p.y+sinf(a)*r;}

  if (c.d > 42.0f) {
    for (int i=0;i<3;++i) putDepth((int)ax[i],(int)ay[i],c.d-0.02f,11);
    return;
  }

  for(int i=0;i<3;++i){
    int j=(i+1)%3;
    float x0=ax[i]+0.30f*(ax[j]-ax[i]), y0=ay[i]+0.30f*(ay[j]-ay[i]);
    float x1=ax[i]+0.70f*(ax[j]-ax[i]), y1=ay[i]+0.70f*(ay[j]-ay[i]);
    int steps=clampi((int)fmaxf(fabsf(x1-x0),fabsf(y1-y0)),1,20);
    for(int k=0;k<=steps;++k){float t=k/(float)steps; putDepth((int)(x0+(x1-x0)*t),(int)(y0+(y1-y0)*t),c.d-0.02f,11);}
  }
}

static void drawBoss() {
  if (g.boss.phase == BossPhase::Dormant || g.boss.phase == BossPhase::Defeated) return;
  if (g.boss.phase == BossPhase::Walking || g.boss.phase == BossPhase::Collapsing) {
    float bodyZ = g.boss.phase == BossPhase::Walking ? 11.0f : g.boss.bodyZ;
    drawBoxYaw(g.boss.x,g.boss.y,bodyZ,18,26,11,0,25);
    const float lx[3]={-7,0,7}; const float ly[3]={-3,4,-3};
    for(uint8_t i=0;i<3;++i){
      if(g.boss.legAlive[i]){
        drawBoxYaw(g.boss.x+lx[i],g.boss.y+ly[i],0,3.5f,3.5f,11,0,10);
        drawBoxYaw(g.boss.x+lx[i],g.boss.y+ly[i],0.7f,4.2f,4.2f,1.4f,0,11);
        drawTargetMarker(g.boss.x+lx[i],g.boss.y+ly[i],2.6f,1.4f);
      } else {
        drawBoxYaw(g.boss.x+lx[i],g.boss.y+ly[i],0,4.0f,5.0f,2.0f,0,28);
      }
    }
  } else {
    // Draw the exact same breached shell used by collision/line-of-sight.
    // The center tunnel remains visibly and physically open for deploying tanks.
    for(uint8_t i=0;i<FALLEN_FORTRESS_PIECES;++i){
      StaticBox piece=fallenFortressPiece(i);
      drawBoxYaw(piece.x,piece.y,0,piece.w,piece.d,piece.h,0,piece.color);
    }

    // Upper wreckage sits entirely above the solid side rails, adding silhouette
    // without creating any invisible horizontal collision footprint.
    float wreckX=g.boss.x+g.boss.collapseSide*7.0f;
    float railY=FALLEN_BREACH_D*0.5f+FALLEN_RAIL_D*0.5f;
    drawBoxYaw(wreckX-g.boss.collapseSide*5.0f,g.boss.y-railY,3.5f,12,4.0f,4,0,25);
    drawBoxYaw(wreckX+g.boss.collapseSide*5.5f,g.boss.y+railY,3.0f,7,3.6f,2.6f,0,10);

    // A low torn threshold makes the deployment mouth obvious but is intentionally
    // driveable; at this height it reads as debris/floor rather than a wall.
    float mouthX=g.boss.x+g.boss.collapseSide*19.5f;
    drawBoxYaw(mouthX,g.boss.y,0.02f,3.0f,FALLEN_BREACH_D-0.6f,0.10f,0,28);

    if(g.boss.phase==BossPhase::CoreOpening || g.boss.phase==BossPhase::CoreExposed){
      float t=g.boss.phase==BossPhase::CoreOpening?clampf(elapsedMs(g.simNow,g.boss.phaseStart)/1500.0f,0,1):1;
      float cx=g.boss.x+g.boss.collapseSide*(20.0f+3.0f*t);
      drawBoxYaw(cx,g.boss.y,0.5f,2.8f,2.8f,2.0f,0,24);
      if(g.boss.phase==BossPhase::CoreExposed) drawTargetMarker(g.boss.coreX,g.boss.coreY,2.2f,1.5f);
    }
  }
}

static Vec3 cross3(Vec3 a, Vec3 b) {
  return {a.y*b.z - a.z*b.y,
          a.z*b.x - a.x*b.z,
          a.x*b.y - a.y*b.x};
}

static Vec3 normalize3(Vec3 v) {
  float m = sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
  if (m < 0.0001f) return {0.0f, 0.0f, 0.0f};
  return {v.x/m, v.y/m, v.z/m};
}

static void drawSteelBeam3D(Vec3 a, Vec3 b, float thickness) {
  // Build an actual rectangular steel prism between arbitrary 3D endpoints.
  // This lets the hedgehog members lean upward/downward instead of being
  // horizontal boxes or screen-space lines.
  Vec3 axis = normalize3({b.x-a.x, b.y-a.y, b.z-a.z});
  Vec3 helper = fabsf(axis.z) < 0.88f ? Vec3{0.0f,0.0f,1.0f} : Vec3{0.0f,1.0f,0.0f};
  Vec3 u = normalize3(cross3(axis, helper));
  Vec3 v = normalize3(cross3(axis, u));
  float h = thickness * 0.5f;
  u = {u.x*h, u.y*h, u.z*h};
  v = {v.x*h, v.y*h, v.z*h};

  Vec3 q[8] = {
    {a.x-u.x-v.x, a.y-u.y-v.y, a.z-u.z-v.z},
    {a.x+u.x-v.x, a.y+u.y-v.y, a.z+u.z-v.z},
    {a.x+u.x+v.x, a.y+u.y+v.y, a.z+u.z+v.z},
    {a.x-u.x+v.x, a.y-u.y+v.y, a.z-u.z+v.z},
    {b.x-u.x-v.x, b.y-u.y-v.y, b.z-u.z-v.z},
    {b.x+u.x-v.x, b.y+u.y-v.y, b.z+u.z-v.z},
    {b.x+u.x+v.x, b.y+u.y+v.y, b.z+u.z+v.z},
    {b.x-u.x+v.x, b.y-u.y+v.y, b.z-u.z+v.z}
  };
  const uint8_t tris[12][3] = {
    {0,1,5},{0,5,4}, {1,2,6},{1,6,5},
    {2,3,7},{2,7,6}, {3,0,4},{3,4,7},
    {4,5,6},{4,6,7}, {0,2,1},{0,3,2}
  };
  for (uint8_t i=0; i<12; ++i) {
    uint8_t col = (i & 3u) == 0u ? 26 : ((i & 3u) == 1u ? 9 : 8);
    drawWorldTri(q[tris[i][0]], q[tris[i][1]], q[tris[i][2]], col);
  }
}

static void drawCzechHedgehog(float x, float y) {
  // Three massive steel beams cross through the same center like the real
  // Czech hedgehogs in the reference image. Each member has one ground-contact
  // end and one high end, giving the obstacle three feet and three raised arms.
  // The raised ends stand above an enemy tank silhouette.
  const float lowZ = 0.08f;
  const float highZ = 3.15f;
  const float beam = 0.48f;

  // Left/right member: low on one side, high on the other.
  drawSteelBeam3D({x-3.00f, y, lowZ},
                  {x+3.00f, y, highZ}, beam);

  // Front/back member slopes the opposite way.
  drawSteelBeam3D({x, y-3.00f, highZ},
                  {x, y+3.00f, lowZ}, beam);

  // Third member runs diagonally through the first two, creating the strong
  // six-arm "metal jack" silhouette visible in the supplied photograph.
  drawSteelBeam3D({x-2.25f, y-2.25f, lowZ},
                  {x+2.25f, y+2.25f, highZ}, beam);

  // Chunky welded center plates make the intersection read as one heavy object.
  drawBoxYaw(x, y, 1.25f, 1.10f, 1.10f, 0.72f, PI_F*0.125f, 26);
  drawBoxYaw(x, y, 1.42f, 0.72f, 0.72f, 0.38f, PI_F*0.375f, 9);
}

static void drawBarbedFenceSegment(float wallX, float y, float side) {
  constexpr float HALF_LEN = 3.15f;
  drawBoxYaw(wallX, y - HALF_LEN, 0.0f, 0.24f, 0.24f, 1.75f, 0.0f, 16);
  drawBoxYaw(wallX, y + HALF_LEN, 0.0f, 0.24f, 0.24f, 1.75f, 0.0f, 16);
  for (int row = 0; row < 3; ++row) {
    float z = 0.56f + row * 0.38f;
    drawWorldLine({wallX, y - HALF_LEN, z}, {wallX, y + HALF_LEN, z}, 1);
    for (int k = 0; k < 6; ++k) {
      float yy = y - 2.6f + k * 1.05f;
      drawWorldLine({wallX - 0.18f * side, yy, z + 0.10f},
                    {wallX + 0.18f * side, yy + 0.26f, z - 0.10f}, 1);
    }
  }
  // Low entanglement at the base makes the barrier feel continuous and flush
  // with the invisible blocking wall.
  drawBoxYaw(wallX + side * 0.18f, y, 0.02f, 0.22f, 6.10f, 0.14f, 0.0f, 13);
}

static void drawWreckedTankBarrier(float x, float y, float side) {
  // Recognizable burnt-out tank hull parked lengthwise along the boundary.
  const float yaw = side > 0.0f ? 0.05f : -0.05f;
  drawBoxYaw(x, y, 0.0f, 3.8f, 4.0f, 0.72f, yaw, 9);           // tracks / lower hull
  drawBoxYaw(x, y - 0.12f, 0.58f, 3.15f, 3.45f, 0.82f, yaw, 26); // upper hull
  drawBoxYaw(x + side * 0.15f, y - 0.28f, 1.28f, 1.65f, 1.55f, 0.56f,
             yaw + side * 0.10f, 10);                           // crooked turret
  drawBoxYaw(x - side * 0.95f, y - 0.46f, 1.48f, 2.35f, 0.24f, 0.20f,
             yaw + side * 0.10f, 9);                            // broken gun barrel

  // Dark track gaps / battle damage accents.
  drawBoxYaw(x - side * 1.45f, y, 0.10f, 0.32f, 3.35f, 0.34f, yaw, 30);
  drawBoxYaw(x + side * 1.45f, y, 0.10f, 0.32f, 3.35f, 0.34f, yaw, 30);
  drawBoxYaw(x + side * 0.62f, y + 0.72f, 0.88f, 0.48f, 0.50f, 0.24f, yaw, 28);
}

static void drawWreckedCarBarrier(float x, float y, float side, uint32_t variant) {
  // Low, readable 1940s-style wrecked car silhouette. Cars are aligned with
  // the wall so neighboring wrecks visually join into a continuous barricade.
  float skew = (variant & 1u) ? 0.035f : -0.035f;
  if (side < 0.0f) skew = -skew;

  // Wheels first so the body visibly sits on them.
  const float wheelX = 1.22f;
  const float wheelY = 1.22f;
  drawBoxYaw(x - wheelX, y - wheelY, 0.05f, 0.34f, 0.62f, 0.54f, skew, 30);
  drawBoxYaw(x + wheelX, y - wheelY, 0.05f, 0.34f, 0.62f, 0.54f, skew, 30);
  drawBoxYaw(x - wheelX, y + wheelY, 0.05f, 0.34f, 0.62f, 0.54f, skew, 30);
  drawBoxYaw(x + wheelX, y + wheelY, 0.05f, 0.34f, 0.62f, 0.54f, skew, 30);

  // Main body: long and low, with distinct hood/trunk masses.
  drawBoxYaw(x, y, 0.34f, 2.85f, 4.05f, 0.62f, skew, 16);
  drawBoxYaw(x, y - 1.36f, 0.75f, 2.62f, 1.18f, 0.32f, skew, 26); // hood
  drawBoxYaw(x, y + 1.42f, 0.72f, 2.52f, 0.92f, 0.30f, skew, 26); // trunk

  // Cabin and roof. Offset/tilt a little to sell the wrecked condition.
  float crush = (variant & 2u) ? 0.12f : -0.10f;
  drawBoxYaw(x + side * crush, y + 0.05f, 0.86f, 2.22f, 1.78f, 0.72f,
             skew + side * 0.035f, 26);
  drawBoxYaw(x + side * crush, y + 0.03f, 1.47f, 1.82f, 1.32f, 0.20f,
             skew + side * 0.035f, 9);

  // Dark windshield and side-window band make it read immediately as a car.
  drawBoxYaw(x + side * 0.02f, y - 0.66f, 1.14f, 1.70f, 0.10f, 0.44f,
             skew, 30);
  drawBoxYaw(x + side * 0.02f, y + 0.55f, 1.14f, 1.70f, 0.10f, 0.40f,
             skew, 30);

  // Missing panel / rust-dark damage patch.
  float damageY = (variant & 4u) ? 0.88f : -0.92f;
  drawBoxYaw(x + side * 1.20f, y + damageY, 0.58f, 0.18f, 0.62f, 0.30f,
             skew, 28);
}

static void drawBoundaryRange(uint8_t area, float areaStart, float areaEnd,
                              float step, float visibleStart, float visibleEnd) {
  if (visibleEnd < areaStart || visibleStart > areaEnd) return;

  float first = fmaxf(visibleStart, areaStart);
  int firstIndex = (int)floorf((first - areaStart) / step);
  if (firstIndex < 0) firstIndex = 0;
  float y = areaStart + firstIndex * step;
  if (y < first - 0.001f) y += step;

  for (; y <= areaEnd + 0.001f && y <= visibleEnd; y += step) {
    if (bossFreeRoam(y)) continue;
    const float hw = corridorHalfWidth(y);
    const int worldIndex = (int)floorf((y - areaStart) / step + 0.5f);

    for (int s = 0; s < 2; ++s) {
      const float side = s == 0 ? -1.0f : 1.0f;

      if (area == 0) {
        // Each hedgehog extends ~3 units inward/outward. Center it so its inner
        // arm ends exactly at the invisible collision boundary.
        drawCzechHedgehog(side * (hw + 3.0f), y);
      } else if (area == 1 || area == 2) {
        drawBarbedFenceSegment(side * hw, y, side);
      } else {
        // Town/city barrier: fixed alternating wrecks. Their inner body edge is
        // placed on the same invisible wall used by collision.
        const bool tankHull = ((worldIndex + s) % 3) == 0;
        if (tankHull) {
          drawWreckedTankBarrier(side * (hw + 1.90f), y, side);
        } else {
          uint32_t variant = hash2i(worldIndex, 700 + s * 37 + area * 101);
          drawWreckedCarBarrier(side * (hw + 1.43f), y, side, variant);
        }
      }
    }
  }
}

static void drawThemedBoundaryProps() {
  // IMPORTANT: placements are anchored to fixed world coordinates, never to
  // the player's current position. Only the visible range changes as the player
  // moves, so props cannot slide, pop to a new phase, or shift under motion.
  const float visibleStart = fmaxf(0.0f, g.player.pose.y - 58.0f);
  const float visibleEnd = g.player.pose.y + 138.0f;

  // Slight overlap between repeated pieces keeps each boundary visually solid.
  drawBoundaryRange(0,   0.0f, 109.99f, 4.05f, visibleStart, visibleEnd); // hedgehogs
  drawBoundaryRange(1, 110.0f, 209.99f, 6.00f, visibleStart, visibleEnd); // defense wire
  drawBoundaryRange(2, 210.0f, 349.99f, 6.00f, visibleStart, visibleEnd); // trench wire
  drawBoundaryRange(3, 350.0f, 489.99f, 3.82f, visibleStart, visibleEnd); // town wrecks
  drawBoundaryRange(4, 490.0f, 649.99f, 3.82f, visibleStart, visibleEnd); // city wrecks
  drawBoundaryRange(5, 650.0f, 689.99f, 3.82f, visibleStart, visibleEnd); // approach
}

static void drawStaticWorld() {
  for(uint8_t i=0;i<WORLD_BOX_COUNT;++i){
    float d2=dist2(WORLD_BOXES[i].x,WORLD_BOXES[i].y,g.player.pose.x,g.player.pose.y);
    if(d2<130.0f*130.0f) drawBox(WORLD_BOXES[i]);
  }
  drawThemedBoundaryProps();
}

static void drawActors() {
  for(uint8_t i=0;i<ENEMY_COUNT;++i){
    drawEnemySprite(enemies[i], ENEMY_DEFS[i].cls);
    if(enemies[i].alive && enemies[i].active && !lineBlocked(g.player.pose.x,g.player.pose.y,enemies[i].x,enemies[i].y))
      drawTargetMarker(enemies[i].x,enemies[i].y,2.5f,1.2f);
  }
  for(uint8_t i=0;i<6;++i){
    if(bossDefenders[i].released){
      drawEnemySprite(bossDefenders[i].e,bossDefenders[i].cls);
      if(bossDefenders[i].e.alive && !lineBlocked(g.player.pose.x,g.player.pose.y,bossDefenders[i].e.x,bossDefenders[i].e.y))
        drawTargetMarker(bossDefenders[i].e.x,bossDefenders[i].e.y,2.5f,1.2f);
    }
  }
  for(uint8_t i=0;i<SOLDIER_COUNT;++i) drawSoldier(soldiers[i]);
  for(uint8_t i=0;i<MAX_PROJECTILES;++i) drawProjectile(projectiles[i]);
  for(uint8_t i=0;i<MAX_EFFECTS;++i) drawEffect(effects[i]);
}

static void fillRectLogical(int x,int y,int w,int h,uint8_t c){
  int x0=clampi(x,0,RW),y0=clampi(y,0,RH),x1=clampi(x+w,0,RW),y1=clampi(y+h,0,RH);
  for(int yy=y0;yy<y1;++yy) for(int xx=x0;xx<x1;++xx) frame[yy*RW+xx]=c;
}
static void lineLogical(int x0,int y0,int x1,int y1,uint8_t c){
  int dx=abs(x1-x0),sx=x0<x1?1:-1,dy=-abs(y1-y0),sy=y0<y1?1:-1,err=dx+dy;
  while(true){put(x0,y0,c); if(x0==x1&&y0==y1)break; int e2=2*err;if(e2>=dy){err+=dy;x0+=sx;}if(e2<=dx){err+=dx;y0+=sy;}}
}

// 3x5 font for compact HUD/messages.
static const uint8_t FONT_3X5[37][5] = {
  {0,0,0,0,0}, // space
  {7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},
  {7,4,7,1,7},{7,4,7,5,7},{7,1,1,1,1},{7,5,7,5,7},{7,5,7,1,7},
  {2,5,7,5,5},{6,5,6,5,6},{3,4,4,4,3},{6,5,5,5,6},{7,4,6,4,7},{7,4,6,4,4},
  {3,4,5,5,3},{5,5,7,5,5},{7,2,2,2,7},{1,1,1,5,2},{5,5,6,5,5},{4,4,4,4,7},
  {5,7,7,5,5},{5,7,7,7,5},{2,5,5,5,2},{6,5,6,4,4},{2,5,5,3,1},{6,5,6,5,5},
  {3,4,2,1,6},{7,2,2,2,2},{5,5,5,5,7},{5,5,5,5,2},{5,5,7,7,5},{5,5,2,5,5},
  {5,5,2,2,2},{7,1,2,4,7}
};

static int fontIndex(char c){
  if (c == ' ') return 0;
  if (c >= '0' && c <= '9') return 1 + (c - '0');
  if (c >= 'A' && c <= 'Z') return 11 + (c - 'A');
  return 0;
}
static void text3x5(int x,int y,const char* s,uint8_t col,int scale=1){
  while(*s){int fi=fontIndex(*s++);for(int yy=0;yy<5;++yy){uint8_t bits=FONT_3X5[fi][yy];for(int xx=0;xx<3;++xx)if(bits&(1<<(2-xx)))fillRectLogical(x+xx*scale,y+yy*scale,scale,scale,col);}x+=4*scale;}
}

static bool playerInDangerZone() {
  for(uint8_t i=0;i<3;++i) if(missileZones[i].active && !missileZones[i].impacted && dist2(missileZones[i].x,missileZones[i].y,g.player.pose.x,g.player.pose.y)<missileZones[i].radius*missileZones[i].radius) return true;
  if(footZone.active&&!footZone.impacted&&dist2(footZone.x,footZone.y,g.player.pose.x,g.player.pose.y)<footZone.radius*footZone.radius)return true;
  return false;
}

// Full-screen first-person destruction sequence. The player's tank is the
// cockpit, so the death animation happens through the view rather than drawing
// a detached third-person tank sprite.
static void drawDeathAnimation() {
  if (g.mode != Mode::DYING) return;
  const uint32_t age = elapsedMs(g.simNow, g.modeStart);
  const int cx = 60;
  const int cy = 102;

  // Once the destruction animation is finished, hold a completely black frame.
  // updateLifecycle() keeps this black for 650 ms before auto-respawning.
  if (age >= 1700) {
    fillRectLogical(0, 0, RW, RH, 0);
    return;
  }

  if (g.waterDeath) {
    int waterTop = RH - clampi((int)(age / 10), 0, RH + 6);
    if (waterTop < 0) waterTop = 0;

    for (int y = waterTop; y < RH; ++y) {
      for (int x = 0; x < RW; ++x) {
        uint8_t col = (((x >> 2) + (y >> 1) + (int)(age / 45)) & 1) ? 17 : 31;
        frame[y * RW + x] = col;
      }
    }

    int foamY = clampi(waterTop, 0, RH - 1);
    for (int x = 0; x < RW; ++x) {
      int wave = foamY + (((x / 5) + (int)(age / 60)) & 1);
      if (wave >= 0 && wave < RH) frame[wave * RW + x] = ((x + (int)(age/25)) & 2) ? 1 : 21;
    }

    // Cockpit sinks below the rising waterline and darkens out.
    int sink = clampi((int)(age / 18), 0, 44);
    fillRectLogical(0, clampi(126 + sink, 0, RH), RW, RH, 0);

    for (int i = 0; i < 16; ++i) {
      int bx = 10 + ((i * 37 + (int)(age * 0.7f)) % 100);
      int by = RH - 8 - ((int)(age / 14) + i * 9) % 78;
      if (by > waterTop + 3 && by < RH - 4) {
        put(bx, by, 1);
        if ((i & 1) == 0 && by > 1) put(bx, by - 1, 31);
      }
    }

    if (age >= 920) {
      fillRectLogical(47, 76, 26, 9, 30);
      text3x5(50, 78, "SUNK", 21, 1);
    }
    return;
  }

  // Immediate white-hot impact flash.
  if (age < 110) {
    uint8_t col = ((age / 35) & 1u) ? 21 : 1;
    fillRectLogical(0, 0, RW, RH, col);
    return;
  }

  // Expanding blast centered just above the mantlet. A dithered outer ring
  // keeps the effect bright without needing alpha blending or another buffer.
  if (age < 720) {
    float t = (age - 110) / 610.0f;
    int outer = 8 + (int)(t * 72.0f);
    int inner = (int)(outer * 0.55f);
    int inner2 = (int)(outer * 0.25f);
    int outer2 = outer * outer;
    int innerSq = inner * inner;
    int inner2Sq = inner2 * inner2;
    int y0 = clampi(cy - outer, 0, RH - 1);
    int y1 = clampi(cy + outer, 0, RH - 1);
    int x0 = clampi(cx - outer, 0, RW - 1);
    int x1 = clampi(cx + outer, 0, RW - 1);
    for (int y = y0; y <= y1; ++y) {
      int dy = y - cy;
      for (int x = x0; x <= x1; ++x) {
        int dx = x - cx;
        int d2 = dx*dx + dy*dy;
        if (d2 > outer2) continue;
        if (d2 <= inner2Sq) {
          frame[y*RW+x] = ((x + y + (int)(age/18)) & 1) ? 1 : 21;
        } else if (d2 <= innerSq) {
          if (((x + y + (int)(age/25)) & 1) == 0) frame[y*RW+x] = 15;
        } else if (((x*3 + y + (int)(age/31)) & 3) == 0) {
          frame[y*RW+x] = 12;
        }
      }
    }

    // Deterministic radial sparks so there is no RNG state added to gameplay.
    for (int i = 0; i < 14; ++i) {
      float a = (TWO_PI_F * i / 14.0f) + age * 0.0017f;
      int len = 8 + (int)(t * (18 + (i % 5) * 4));
      int sx = cx + (int)(cosf(a) * (outer * 0.30f));
      int sy = cy + (int)(sinf(a) * (outer * 0.30f));
      int ex = cx + (int)(cosf(a) * len);
      int ey = cy + (int)(sinf(a) * len);
      lineLogical(sx, sy, ex, ey, (i & 1) ? 21 : 15);
    }
  }

  // Smoke/failed electronics creep over the view after the blast.
  if (age >= 520) {
    uint32_t smokeAge = age - 520;
    int coverage = clampi((int)(smokeAge / 10), 0, 105);
    for (int y = 0; y < RH; y += 2) {
      for (int x = 0; x < RW; x += 2) {
        uint32_t h = hash2i(x + (int)(age/80), y * 3);
        if ((int)(h % 120u) < coverage) put(x, y, (h & 4u) ? 30 : 0);
      }
    }
  }

  // Final dead-tank blackout before the respawn relocation occurs.
  if (age >= 1180) {
    int rows = clampi((int)((age - 1180) / 4), 0, RH);
    fillRectLogical(0, RH - rows, RW, rows, 0);
  }

  if (age >= 900) {
    fillRectLogical(39, 75, 43, 9, 30);
    text3x5(43, 77, "DESTROYED", 21, 1);
  }
}

// During spawn protection, pulse a bright shield-like flash over the visible
// tank/cockpit. This makes invulnerability readable without another HUD message.
static void drawSpawnInvulnerabilityFlash() {
  if (g.mode != Mode::PLAYING) return;
  if ((int32_t)(g.simNow - g.player.invulnerableUntil) >= 0) return;

  const uint32_t remaining = g.player.invulnerableUntil - g.simNow;
  const bool bright = ((g.simNow / 95u) & 1u) == 0;
  if (!bright) return;

  uint8_t c = ((g.simNow / 190u) & 1u) ? 31 : 1;
  int recoil = ((int32_t)(g.simNow - g.player.cannonRecoilUntil) < 0) ? 4 : 0;
  int baseY = 133 + recoil;

  // Cockpit edge and mantlet outline.
  lineLogical(0, 127, 119, 127, c);
  lineLogical(4, 133, 115, 133, c);
  lineLogical(48, baseY - 5, 72, baseY - 5, c);
  lineLogical(48, baseY + 4, 72, baseY + 4, c);
  lineLogical(48, baseY - 5, 48, baseY + 4, c);
  lineLogical(72, baseY - 5, 72, baseY + 4, c);

  // Flash along the cannon barrel itself.
  for (int y = 83 + recoil; y < baseY - 5; y += 3) {
    put(58, y, c);
    put(62, y, c);
  }

  // Sparse energy shimmer across the armored deck. It fades in density during
  // the last second so the player can feel the protection ending.
  int stride = remaining < 1000 ? 10 : 6;
  for (int y = 136; y < RH; y += 4) {
    for (int x = 6 + ((y/4)&1)*2; x < 114; x += stride) {
      if (((x + y + (int)(g.simNow/95u)) & 3) == 0) put(x, y, c);
    }
  }
}

static void drawCockpit() {
  // Angled armored deck integrated into lower world.
  for(int y=118;y<RH;++y){
    int inset=(y<132)?(132-y):0;
    for(int x=0;x<RW;++x){
      if(y>=WORLD_H || x<28-inset || x>91+inset) frame[y*RW+x]=23;
    }
  }
  lineLogical(0,128,119,128,10);
  lineLogical(0,129,119,129,9);
  fillRectLogical(3,132,114,25,23);
  fillRectLogical(5,134,110,2,10);
  put(8,138,22); put(111,138,22); put(8,153,22); put(111,153,22);

  // Cannon and mantlet, visibly projecting into world.
  int recoil=((int32_t)(g.simNow-g.player.cannonRecoilUntil)<0)?4:0;
  int baseY=133+recoil;
  for(int y=82+recoil;y<baseY;++y){
    float t=(y-(82+recoil))/(float)(baseY-(82+recoil));
    int half=1+(int)(t*7.0f);
    fillRectLogical(60-half,y,half*2+1,1,y<108?9:10);
  }
  fillRectLogical(49,baseY-4,23,8,9);
  fillRectLogical(53,baseY-2,15,5,10);
  if((int32_t)(g.simNow-g.player.cannonFlashUntil)<0){fillRectLogical(57,78,7,4,21);fillRectLogical(59,75,3,4,15);}

  // Compact open-center crosshair at the true firing ray / horizon. The old
  // 11x11 mark could completely cover a tank once it shrank to only a few pixels.
  lineLogical(57,HORIZON_Y,58,HORIZON_Y,1); lineLogical(62,HORIZON_Y,63,HORIZON_Y,1);
  lineLogical(60,HORIZON_Y-3,60,HORIZON_Y-2,1); lineLogical(60,HORIZON_Y+2,60,HORIZON_Y+3,1);

  // Armor: five segments, partial by health.
  text3x5(5,139,"ARM",22,1);
  int hp=clampi(g.player.health,0,100);
  for(int i=0;i<5;++i){int seg=clampi(hp-i*20,0,20);uint8_t col=seg>0?14:28;fillRectLogical(5+i*8,146,6,5,col);if(seg>0&&seg<20)fillRectLogical(5+i*8+(seg*6/20),146,6-(seg*6/20),5,28);}

  // Cannon ready lamp.
  bool ready=(int32_t)(g.simNow-g.player.cannonReadyAt)>=0;
  fillRectLogical(57,143,7,7,9); fillRectLogical(59,145,3,3,ready?11:28);

  // Danger lamp.
  bool danger=playerInDangerZone();
  fillRectLogical(68,143,7,7,9); fillRectLogical(70,145,3,3,danger?12:29);

  // Compass.
  static const char* dirs[8]={"N","NE","E","SE","S","SW","W","NW"};
  int dir=(int)floorf((wrapAngle(g.player.pose.heading)+PI_F/8.0f)/ (PI_F/4.0f));
  dir%=8;if(dir<0)dir+=8;
  text3x5(84,139,"HDG",22,1); text3x5(89,146,dirs[dir],1,1);

  // Lives, three tank pips.
  for(int i=0;i<3;++i){int x=94+i*7,y=153;uint8_t c=i<g.player.lives?14:30;fillRectLogical(x,y,5,2,c);put(x+2,y-1,c);}

  // Defender count during final battle.
  if(g.boss.phase==BossPhase::FallenDeploying){
    char buf[8]; snprintf(buf,sizeof(buf),"D %d",6-g.boss.defendersDestroyed); text3x5(46,153,buf,12,1);
  }

  // Temporary message / terminal status.
  if((int32_t)(g.simNow-g.messageUntil)<0 || g.mode==Mode::GAME_OVER || g.mode==Mode::VICTORY_WAIT){
    int len=strlen(g.message); int x=clampi(60-len*2,1,118-len*4); fillRectLogical(x-2,120,len*4+3,7,30); text3x5(x,121,g.message,1,1);
  }
  if(!g.controlsArmed && (g.mode==Mode::BRIEFING||g.mode==Mode::RESPAWN_GATE)){
    text3x5(36,112,"CENTER TREADS",21,1);
  }
}

// Game Over is the only death-related state that waits for a click.  Keep it
// visually separate from gameplay so the player never sees the death location
// again after the blackout.
static void drawGameOverOverlay() {
  if (g.mode != Mode::GAME_OVER) return;
  fillRectLogical(0, 0, RW, RH, 0);
  text3x5(44, 66, "GAME OVER", 21, 1);
  text3x5(22, 82, "PRESS FIRE TO RESTART", 1, 1);
}

static void presentFrame() {
  display.startWrite();
  display.setAddrWindow(0, 0, 240, 320);
  for(int ly=0;ly<RH;++ly){
    const uint8_t* src=&frame[ly*RW];
    for(int x=0;x<RW;++x){uint16_t c=PALETTE[src[x]&31];physicalRow[x*2]=c;physicalRow[x*2+1]=c;}
    display.writePixels(physicalRow, RW*2);
    display.writePixels(physicalRow, RW*2);
  }
  display.endWrite();
}

static void render() {
  clearAndDrawGround();
  drawStaticWorld();
  drawBoss();
  for(uint8_t i=0;i<3;++i) drawWarning(missileZones[i],12);
  drawWarning(footZone,11);
  drawActors();
  drawCockpit();
  drawSpawnInvulnerabilityFlash();
  drawDeathAnimation();
  drawGameOverOverlay();
  presentFrame();
}

// -----------------------------------------------------------------------------
// Run reset and simulation
// -----------------------------------------------------------------------------

static void startNewRun() {
  memset(&g, 0, sizeof(g));
  memset(projectiles, 0, sizeof(projectiles));
  memset(effects, 0, sizeof(effects));
  memset(missileZones, 0, sizeof(missileZones));
  memset(&footZone, 0, sizeof(footZone));
  memset(bossDefenders, 0, sizeof(bossDefenders));

  g.simNow = millis();
  g.mode = Mode::BRIEFING;
  g.modeStart = g.simNow;
  g.briefingUntil = g.simNow + 700;
  g.controlsArmed = false;
  g.player.pose = {0.0f, 18.0f, 0.0f};
  g.player.health = 100;
  g.player.lives = 3;
  g.player.invulnerableUntil = 0;
  g.player.cannonReadyAt = g.simNow;
  g.player.mgReadyAt = g.simNow;
  g.player.safeX = 0; g.player.safeY = 18; g.player.safeHeading = 0;
  g.player.nextSafeSave = g.simNow + 1000;
  g.area = 0;
  g.restartAtBeach = false;
  g.waterDeath = false;
  g.lastOuterUs = micros();
  g.lastFrameUs = g.lastOuterUs - FRAME_US;
  setMessage("INVADE THE NORTH", 1200);

  for(uint8_t i=0;i<ENEMY_COUNT;++i){
    enemies[i].x=ENEMY_DEFS[i].x; enemies[i].y=ENEMY_DEFS[i].y;
    enemies[i].heading=PI_F; enemies[i].turretHeading=PI_F;
    enemies[i].health=classHealth(ENEMY_DEFS[i].cls); enemies[i].defIndex=i;
    enemies[i].alive=true; enemies[i].active=false; enemies[i].windup=false;
    enemies[i].windupUntil=0; enemies[i].reloadUntil=g.simNow+1200+(i%4)*350; enemies[i].observedUntil=0;
  }
  for(uint8_t i=0;i<SOLDIER_COUNT;++i){
    soldiers[i].x=SOLDIER_DEFS[i].x; soldiers[i].y=SOLDIER_DEFS[i].y;
    soldiers[i].homeX=soldiers[i].x; soldiers[i].homeY=soldiers[i].y;
    soldiers[i].heading=(i&1)?1.2f:-1.1f; soldiers[i].alive=true; soldiers[i].dying=false;
    soldiers[i].anim=0; soldiers[i].nextDecision=g.simNow+200+i*23; soldiers[i].deathAt=0;
  }

  g.boss.phase = BossPhase::Dormant;
  g.boss.x = 0; g.boss.y = 760; g.boss.bodyZ = 11;
  g.boss.phaseStart = g.simNow; g.boss.collapseSide = 1;
  for(uint8_t i=0;i<3;++i){g.boss.legHealth[i]=200;g.boss.legAlive[i]=true;}
  g.boss.defendersReleased=0;g.boss.defendersDestroyed=0;
  g.boss.nextRelease=0;g.boss.nextMissile=0;g.boss.nextFoot=0;
  g.boss.coreX=0;g.boss.coreY=0;
}

static void updateEffects() {
  for(uint8_t i=0;i<MAX_EFFECTS;++i){
    if(effects[i].type!=EffectType::None && elapsedMs(g.simNow,effects[i].born)>=effects[i].life) effects[i].type=EffectType::None;
  }
}

static void simulateTick(const GameInput &input) {
  g.simNow += 17; // deterministic approximate ms progression for gameplay timers

  updateLifecycle(input);

  if (g.mode == Mode::GAME_OVER) {
    // The two physical shooting buttons are the only restart controls.
    // First observe one clean frame with BOTH buttons released so a fire button
    // held during the fatal hit cannot instantly restart the run. After that,
    // either physical fire button by itself restarts. Use the live button state
    // rather than leftPressed/rightPressed edge flags, which can be lost across
    // the DYING -> GAME_OVER mode transition.
    if (!g.terminalReleased) {
      if (!input.leftButton && !input.rightButton) g.terminalReleased = true;
    } else if (input.leftButton || input.rightButton) {
      startNewRun();
    }
    updateAudio();
    updateEffects();
    return;
  }

  if (g.mode == Mode::VICTORY_WAIT) {
    if (!input.leftButton && !input.rightButton) g.terminalReleased = true;
    if (g.terminalReleased && (input.leftPressed || input.rightPressed)) startNewRun();
    updateAudio();
    updateEffects();
    return;
  }

  if (g.mode == Mode::PLAYING) {
    updateDrive(1.0f / 60.0f);
    if (deepWaterAt(g.player.pose.x, g.player.pose.y)) startWaterDeath();
    updateWeapons(input);
    updateSoldiers(1.0f / 60.0f);
    updateEnemies(1.0f / 60.0f);
    updateProjectiles(1.0f / 60.0f);
    updateBoss(1.0f / 60.0f);
    updateSafeBreadcrumb();

    uint8_t newArea = areaForY(g.player.pose.y);
    if (newArea != g.area) {
      g.area = newArea;
      setMessage(areaName(newArea), 1200);
    }
  } else if (g.mode == Mode::VICTORY_SEQUENCE) {
    updateEffects();
    updateLifecycle(input);
  } else if (g.mode == Mode::DYING || g.mode == Mode::RESPAWN_GATE || g.mode == Mode::BRIEFING) {
    // Keep ordinary/boss persistence intact, but dangerous projectiles do not target a protected tank.
    updateBoss(1.0f / 60.0f);
  }

  updateEffects();
  updateAudio();
}

// -----------------------------------------------------------------------------
// Launcher entry points
// -----------------------------------------------------------------------------

void enter() {
  display.setRotation(0);
  display.fillScreen(ST77XX_BLACK);

  startNewRun();
  g.accumulatorUs = 0;
  render();
}

void update(const GameInput &input) {
  uint32_t nowUs = micros();
  uint32_t deltaUs = elapsedUs(nowUs, g.lastOuterUs);
  g.lastOuterUs = nowUs;
  if (deltaUs > 120000u) deltaUs = 120000u;
  g.accumulatorUs += deltaUs;

  uint8_t steps = 0;
  while (g.accumulatorUs >= FIXED_US && steps < MAX_CATCHUP) {
    simulateTick(input);
    g.accumulatorUs -= FIXED_US;
    ++steps;
  }
  if (steps == MAX_CATCHUP && g.accumulatorUs >= FIXED_US) g.accumulatorUs = 0;

  if (elapsedUs(nowUs, g.lastFrameUs) >= FRAME_US) {
    g.lastFrameUs = nowUs;
    render();
  }
}

} // namespace tank
