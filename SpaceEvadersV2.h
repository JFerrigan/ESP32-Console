#pragma once

#include <Arduino.h>
#include <Adafruit_ST7789.h>
#include <math.h>
#include <stdio.h>
#include "GameAPI.h"
#include "GameRenderMemory.h"
#include "Hardware.h"

namespace SpaceEvadersV2 {

// ============================================================
// CONFIGURATION / PALETTE
// ============================================================

constexpr int SCREEN_W = 240;
constexpr int SCREEN_H = 320;
constexpr float SIM_DT = 1.0f / 120.0f;
constexpr uint32_t SIM_SCALE = 120;
constexpr uint64_t SIM_TICK_UNITS = 1000000ULL;
constexpr uint32_t MAX_ELAPSED_US = 33334;
constexpr uint8_t MAX_CATCHUP_TICKS = 4;
constexpr uint32_t RENDER_INTERVAL_US = 16667;

constexpr float SHIP_SPEED = 170.0f;
constexpr float BOLT_SPEED = 250.0f;
constexpr float BOLT_HALF_X = 2.5f;
constexpr float BOLT_HALF_Y = 1.0f;
constexpr float SHIP_HALF_X = 6.0f;
constexpr float SHIP_HALF_Y = 7.0f;
constexpr float LEFT_SHIP_X = 23.0f;
constexpr float RIGHT_SHIP_X = 216.0f;
// Ship sprite occupies centerY +/- 10 pixels. Let it reach the physical
// screen edges so horizontal shots can hit every bunker row, including the
// two extreme rows at the top and bottom of the staggered base layout.
constexpr float SHIP_MIN_Y = 10.0f;
constexpr float SHIP_MAX_Y = 309.0f;
constexpr float START_Y = 159.5f;
constexpr float LEFT_MUZZLE_X = 36.0f;
constexpr float RIGHT_MUZZLE_X = 203.0f;
constexpr float TIME_EPS = 0.000001f;

constexpr uint32_t BUTTON_RELEASE_MS = 15;
constexpr uint32_t FIGHT_HOLD_MS = 650;
constexpr uint32_t DEATH_BURST_MS = 180;
constexpr uint32_t FATALITY_HOLD_MS = 700;
constexpr uint32_t MATCH_READY_LOCK_MS = 450;
constexpr uint32_t MUZZLE_MS = 45;
constexpr uint32_t IMPACT_MS = 70;

constexpr uint8_t MAX_PROJECTILES = 4;
constexpr uint8_t MAX_BUNKERS = 6;
constexpr uint8_t MAX_EFFECTS = 12;
constexpr uint8_t MAX_EVENTS = 16;
constexpr uint8_t MAX_CANDIDATES = 24;
constexpr uint8_t MAX_DIRTY = 64;
constexpr uint16_t SCRATCH_PIXELS = 2048; // 4096 bytes RGB565
constexpr uint8_t REBUILD_ROWS = 8;       // 240*8 = 1920 pixels
constexpr uint8_t REBUILD_STRIPS_PER_UPDATE = 4;

constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((uint16_t)(r & 0xF8) << 8) |
                    ((uint16_t)(g & 0xFC) << 3) |
                    ((uint16_t)b >> 3));
}

constexpr uint16_t C_BG          = rgb565(0x04, 0x07, 0x0D);
constexpr uint16_t C_PANEL       = rgb565(0x08, 0x10, 0x1B);
constexpr uint16_t C_STRUCTURE   = rgb565(0x17, 0x23, 0x32);
constexpr uint16_t C_MUTED_TEXT  = rgb565(0x79, 0x88, 0x99);
constexpr uint16_t C_WHITE       = rgb565(0xF4, 0xFF, 0xFF);
constexpr uint16_t C_LEFT        = rgb565(0x35, 0xD9, 0xF4);
constexpr uint16_t C_LEFT_SHADOW = rgb565(0x14, 0x5C, 0x93);
constexpr uint16_t C_LEFT_BUNKER = rgb565(0x24, 0x78, 0x87);
constexpr uint16_t C_RIGHT       = rgb565(0xFF, 0x95, 0x52);
constexpr uint16_t C_RIGHT_ALT   = rgb565(0xCF, 0x44, 0x7F);
constexpr uint16_t C_RIGHT_BUNKER= rgb565(0x85, 0x49, 0x63);
constexpr uint16_t C_GOLD        = rgb565(0xFF, 0xD5, 0x6B);
constexpr uint16_t C_FATAL       = rgb565(0xFF, 0x52, 0x69);
constexpr uint16_t C_STAR_DIM    = rgb565(0x21, 0x2D, 0x3D);
constexpr uint16_t C_STAR_BRIGHT = rgb565(0x3A, 0x4B, 0x61);

static_assert(SCRATCH_PIXELS * sizeof(uint16_t) == 4096, "scratch buffer must remain 4 KiB");
static_assert(MAX_CANDIDATES >= 24, "collision candidate budget unexpectedly reduced");

// ============================================================
// TYPES
// ============================================================

enum class PlayerId : uint8_t { Left = 0, Right = 1 };
enum class Phase : uint8_t { FightSplash, Playing, DeathBurst, Fatality, MatchOver };
enum class RoundResult : uint8_t { None, LeftWin, RightWin, Draw };
enum class EffectType : uint8_t { Impact, Intercept, Explosion };
enum class EventType : uint8_t { Fire, BunkerImpact, ProjectileIntercept, ShipDestroyed, PlayerReady };
enum class CandidateKind : uint8_t { Bunker, Intercept, ShipHit, Exit, ShipBoundary };

struct RectI {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
};

struct Vec2 {
  float x;
  float y;
};

struct CenterBox {
  float cx;
  float cy;
  float hx;
  float hy;
};

struct Player {
  float centerY;
  float previousCenterY;
  uint8_t wins;
  bool alive;
  int8_t movingSign;
  bool muzzleActive;
  uint32_t muzzleStartedMs;
};

struct Projectile {
  bool active;
  PlayerId owner;
  uint16_t generation;
  float centerX;
  float centerY;
};

struct Bunker {
  PlayerId owner;
  int16_t x;
  int16_t y;
  uint8_t occupiedRows[12];
};

struct ButtonGate {
  bool armed;
  bool trackingRelease;
  bool prevHeld;
  uint32_t releaseStartedMs;
};

struct InputState {
  int8_t moveSign[2];
  uint8_t pendingFire[2];
  ButtonGate gate[2];
};

struct MatchState {
  Phase phase;
  RoundResult result;
  uint16_t attemptNumber;
  bool fightReady[2];
  bool rematchReady[2];
  bool presentationReady;
  bool overlayVisible;
  bool overlayRemoving;
  uint32_t phaseStartedMs;
  uint32_t presentationStartedMs;
  uint32_t phaseGeneration;
};

struct VisualEffect {
  bool active;
  EffectType type;
  PlayerId owner;
  float x;
  float y;
  uint32_t startMs;
  uint16_t durationMs;
  uint8_t seed;
};

struct GameEvent {
  EventType type;
  PlayerId player;
  float x;
  float y;
};

struct CollisionCandidate {
  CandidateKind kind;
  float t;
  int8_t shotA;
  int8_t shotB;
  int8_t bunker;
  int8_t row;
  int8_t col;
  int8_t player;
};

struct ShipRenderSnapshot {
  bool visible;
  RectI bounds;
  uint16_t appearance;
};

struct ProjectileRenderSnapshot {
  bool visible;
  RectI bounds;
};

struct RenderState {
  RectI dirty[MAX_DIRTY];
  uint8_t dirtyCount;
  RectI scratchRect;

  bool rebuilding;
  int16_t rebuildY;
  uint32_t rebuildRevision;

  ShipRenderSnapshot ship[2];
  ProjectileRenderSnapshot shot[MAX_PROJECTILES];
  uint8_t shownWins[2];
  bool shownReady[2];
  uint32_t lastRenderUs;

  uint32_t dirtyOverflowCount;
  uint32_t transferredPixels;
};

struct Diagnostics {
  uint32_t droppedTickDebt;
  uint32_t collisionGuardTrips;
  uint32_t eventOverflow;
};

struct GameState {
  Player players[2];
  Projectile shots[MAX_PROJECTILES];
  Bunker bunkers[MAX_BUNKERS];
  InputState input;
  MatchState match;
  VisualEffect effects[MAX_EFFECTS];
  GameEvent events[MAX_EVENTS];
  uint8_t eventCount;
  RenderState render;
  Diagnostics diag;

  uint64_t simAccumulator;
  uint32_t previousSimUs;
  bool initialized;
};

static GameState g;
static_assert(SCRATCH_PIXELS * sizeof(uint16_t) <= GameRenderMemory::CAPACITY,
              "pixel buffer exceeds shared memory");
static uint16_t *const scratchPixels = reinterpret_cast<uint16_t *>(GameRenderMemory::bytes);

// ============================================================
// CONSTANT ASSETS
// ============================================================

struct StarPoint { uint8_t x; uint16_t y; bool bright; };

// Twelve points plus their X mirrors = 24 deterministic stars.
static const StarPoint STAR_PAIRS[12] = {
  { 21,  23, false }, { 34, 111, false }, { 46, 204, true  },
  { 59, 288, false }, { 72,  72, false }, { 84, 167, false },
  { 95, 251, true  }, {103,  35, false }, {109, 129, false },
  {113, 222, false }, {116, 306, false }, { 66, 314, true  }
};

// 5x7 column font: A-Z, 0-9, space, !, -
static const uint8_t FONT5X7[39][5] = {
  {0x7E,0x11,0x11,0x11,0x7E}, // A
  {0x7F,0x49,0x49,0x49,0x36}, // B
  {0x3E,0x41,0x41,0x41,0x22}, // C
  {0x7F,0x41,0x41,0x22,0x1C}, // D
  {0x7F,0x49,0x49,0x49,0x41}, // E
  {0x7F,0x09,0x09,0x09,0x01}, // F
  {0x3E,0x41,0x49,0x49,0x7A}, // G
  {0x7F,0x08,0x08,0x08,0x7F}, // H
  {0x00,0x41,0x7F,0x41,0x00}, // I
  {0x20,0x40,0x41,0x3F,0x01}, // J
  {0x7F,0x08,0x14,0x22,0x41}, // K
  {0x7F,0x40,0x40,0x40,0x40}, // L
  {0x7F,0x02,0x0C,0x02,0x7F}, // M
  {0x7F,0x04,0x08,0x10,0x7F}, // N
  {0x3E,0x41,0x41,0x41,0x3E}, // O
  {0x7F,0x09,0x09,0x09,0x06}, // P
  {0x3E,0x41,0x51,0x21,0x5E}, // Q
  {0x7F,0x09,0x19,0x29,0x46}, // R
  {0x46,0x49,0x49,0x49,0x31}, // S
  {0x01,0x01,0x7F,0x01,0x01}, // T
  {0x3F,0x40,0x40,0x40,0x3F}, // U
  {0x1F,0x20,0x40,0x20,0x1F}, // V
  {0x3F,0x40,0x38,0x40,0x3F}, // W
  {0x63,0x14,0x08,0x14,0x63}, // X
  {0x07,0x08,0x70,0x08,0x07}, // Y
  {0x61,0x51,0x49,0x45,0x43}, // Z
  {0x3E,0x51,0x49,0x45,0x3E}, // 0
  {0x00,0x42,0x7F,0x40,0x00}, // 1
  {0x42,0x61,0x51,0x49,0x46}, // 2
  {0x21,0x41,0x45,0x4B,0x31}, // 3
  {0x18,0x14,0x12,0x7F,0x10}, // 4
  {0x27,0x45,0x45,0x45,0x39}, // 5
  {0x3C,0x4A,0x49,0x49,0x30}, // 6
  {0x01,0x71,0x09,0x05,0x03}, // 7
  {0x36,0x49,0x49,0x49,0x36}, // 8
  {0x06,0x49,0x49,0x29,0x1E}, // 9
  {0x00,0x00,0x00,0x00,0x00}, // space
  {0x00,0x00,0x5F,0x00,0x00}, // !
  {0x08,0x08,0x08,0x08,0x08}  // -
};

static const int8_t EXPLOSION_DIRS[8][2] = {
  { 1, 0}, {-1, 0}, { 0, 1}, { 0,-1},
  { 1, 1}, {-1, 1}, { 1,-1}, {-1,-1}
};

// ============================================================
// OPTIONAL AUDIO ADAPTER (STRICT DROP-IN CORE = SILENT)
// ============================================================

namespace Audio {
  inline void fire(PlayerId) {}
  inline void bunkerImpact(PlayerId) {}
  inline void intercept() {}
  inline void fight() {}
  inline void death(PlayerId) {}
  inline void ready(PlayerId) {}
  inline void matchVictory(PlayerId) {}
  inline void cancel() {}
}

// ============================================================
// BASIC HELPERS
// ============================================================

inline uint8_t playerIndex(PlayerId p) { return p == PlayerId::Left ? 0 : 1; }
inline PlayerId opposite(PlayerId p) { return p == PlayerId::Left ? PlayerId::Right : PlayerId::Left; }
inline uint32_t elapsedMs(uint32_t now, uint32_t then) { return now - then; }
inline uint32_t elapsedUs(uint32_t now, uint32_t then) { return now - then; }
inline int16_t iround(float v) { return (int16_t)(v + 0.5f); }
inline float fmin2(float a, float b) { return a < b ? a : b; }
inline float fmax2(float a, float b) { return a > b ? a : b; }
inline int16_t imin16(int16_t a, int16_t b) { return a < b ? a : b; }
inline int16_t imax16(int16_t a, int16_t b) { return a > b ? a : b; }

RectI clipRect(RectI r) {
  int16_t x1 = imax16(0, r.x);
  int16_t y1 = imax16(0, r.y);
  int16_t x2 = imin16(SCREEN_W, (int16_t)(r.x + r.w));
  int16_t y2 = imin16(SCREEN_H, (int16_t)(r.y + r.h));
  if (x2 <= x1 || y2 <= y1) return {0,0,0,0};
  return {x1,y1,(int16_t)(x2-x1),(int16_t)(y2-y1)};
}

bool rectEmpty(const RectI &r) { return r.w <= 0 || r.h <= 0; }

bool rectsOverlapOrTouch(const RectI &a, const RectI &b) {
  return !(a.x + a.w < b.x || b.x + b.w < a.x ||
           a.y + a.h < b.y || b.y + b.h < a.y);
}

RectI unionRect(const RectI &a, const RectI &b) {
  int16_t x1 = imin16(a.x, b.x);
  int16_t y1 = imin16(a.y, b.y);
  int16_t x2 = imax16((int16_t)(a.x+a.w), (int16_t)(b.x+b.w));
  int16_t y2 = imax16((int16_t)(a.y+a.h), (int16_t)(b.y+b.h));
  return {x1,y1,(int16_t)(x2-x1),(int16_t)(y2-y1)};
}

uint32_t rectArea(const RectI &r) {
  if (rectEmpty(r)) return 0;
  return (uint32_t)r.w * (uint32_t)r.h;
}

bool rectEqual(const RectI &a, const RectI &b) {
  return a.x==b.x && a.y==b.y && a.w==b.w && a.h==b.h;
}

bool rectIntersects(const RectI &a, const RectI &b) {
  return !(a.x + a.w <= b.x || b.x + b.w <= a.x ||
           a.y + a.h <= b.y || b.y + b.h <= a.y);
}

// ============================================================
// INPUT
// ============================================================

int8_t readMoveSign(int upPin, int downPin) {
  const bool up = digitalRead(upPin) == LOW;
  const bool down = digitalRead(downPin) == LOW;
  if (up && !down) return -1;
  if (!up && down) return +1;
  return 0; // center or invalid/both-active = neutral
}

void resetButtonGate(ButtonGate &gate) {
  gate.armed = false;
  gate.trackingRelease = false;
  gate.prevHeld = true; // require a fresh stable release after every phase boundary
  gate.releaseStartedMs = 0;
}

void resetInputGates() {
  resetButtonGate(g.input.gate[0]);
  resetButtonGate(g.input.gate[1]);
  g.input.pendingFire[0] = 0;
  g.input.pendingFire[1] = 0;
}

bool pollButtonGate(ButtonGate &gate, bool held, bool rawPressed, uint32_t nowMs) {
  bool accepted = false;

  if (!gate.armed) {
    if (held) {
      gate.trackingRelease = false;
    } else {
      if (!gate.trackingRelease) {
        gate.trackingRelease = true;
        gate.releaseStartedMs = nowMs;
      } else if (elapsedMs(nowMs, gate.releaseStartedMs) >= BUTTON_RELEASE_MS) {
        gate.armed = true;
        gate.trackingRelease = false;
      }
    }
  }

  const bool rising = rawPressed || (held && !gate.prevHeld);
  if (gate.armed && rising) {
    accepted = true;
    gate.armed = false;
    gate.trackingRelease = false;
  }

  gate.prevHeld = held;
  return accepted;
}

uint8_t activeShotCount(PlayerId p) {
  const uint8_t start = p == PlayerId::Left ? 0 : 2;
  uint8_t count = 0;
  for (uint8_t i = start; i < start + 2; ++i) if (g.shots[i].active) ++count;
  return count;
}

void queueFireAttempt(PlayerId p) {
  const uint8_t idx = playerIndex(p);
  const uint8_t used = activeShotCount(p) + g.input.pendingFire[idx];
  if (used < 2 && g.input.pendingFire[idx] < 2) {
    ++g.input.pendingFire[idx];
  }
}

void markPanelDirty(PlayerId p);
void registerFightReady(PlayerId p, uint32_t nowMs);
void registerRematchReady(PlayerId p, uint32_t nowMs);

void pollInput(const GameInput &input, uint32_t nowMs) {
  // Both players intentionally share the same raw Y sign table from the handoff.
  g.input.moveSign[0] = readMoveSign(LEFT_UP_PIN, LEFT_DOWN_PIN);
  g.input.moveSign[1] = readMoveSign(RIGHT_UP_PIN, RIGHT_DOWN_PIN);

  const bool leftAccepted = pollButtonGate(g.input.gate[0], input.leftButton, input.leftPressed, nowMs);
  const bool rightAccepted = pollButtonGate(g.input.gate[1], input.rightButton, input.rightPressed, nowMs);

  if (g.match.phase == Phase::Playing) {
    if (leftAccepted) queueFireAttempt(PlayerId::Left);
    if (rightAccepted) queueFireAttempt(PlayerId::Right);
  } else if (g.match.phase == Phase::FightSplash && g.match.presentationReady) {
    if (leftAccepted) registerFightReady(PlayerId::Left, nowMs);
    if (rightAccepted) registerFightReady(PlayerId::Right, nowMs);
  } else if (g.match.phase == Phase::MatchOver && g.match.presentationReady &&
             elapsedMs(nowMs, g.match.presentationStartedMs) >= MATCH_READY_LOCK_MS) {
    if (leftAccepted) registerRematchReady(PlayerId::Left, nowMs);
    if (rightAccepted) registerRematchReady(PlayerId::Right, nowMs);
  }
}

// ============================================================
// BUNKERS / PROJECTILES
// ============================================================

void resetBunker(Bunker &b) {
  for (uint8_t r = 0; r < 12; ++r) {
    b.occupiedRows[r] = (r == 0 || r == 11) ? 0x06 : 0x0F;
  }
}

bool isCellOccupied(const Bunker &b, uint8_t row, uint8_t col) {
  if (row >= 12 || col >= 4) return false;
  return (b.occupiedRows[row] & (1u << col)) != 0;
}

RectI bunkerCellRect(const Bunker &b, uint8_t row, uint8_t col) {
  return {(int16_t)(b.x + col * 4), (int16_t)(b.y + row * 4), 4, 4};
}

CenterBox bunkerCellBox(const Bunker &b, uint8_t row, uint8_t col) {
  return {(float)b.x + col * 4.0f + 2.0f,
          (float)b.y + row * 4.0f + 2.0f,
          2.0f, 2.0f};
}

void markDirty(RectI r);
void markBunkerNeighborhoodDirty(uint8_t bunker, uint8_t row, uint8_t col) {
  RectI r = bunkerCellRect(g.bunkers[bunker], row, col);
  r.x -= 2; r.y -= 2; r.w += 4; r.h += 4;
  markDirty(r);
}

bool clearCell(uint8_t bunker, uint8_t row, uint8_t col) {
  if (bunker >= MAX_BUNKERS || row >= 12 || col >= 4) return false;
  Bunker &b = g.bunkers[bunker];
  const uint8_t bit = (uint8_t)(1u << col);
  if ((b.occupiedRows[row] & bit) == 0) return false;
  b.occupiedRows[row] &= (uint8_t)~bit;
  markBunkerNeighborhoodDirty(bunker, row, col);
  return true;
}

bool allBunkersCleared(PlayerId owner) {
  for (uint8_t b = 0; b < MAX_BUNKERS; ++b) {
    if (g.bunkers[b].owner != owner) continue;
    for (uint8_t r = 0; r < 12; ++r) {
      if (g.bunkers[b].occupiedRows[r] != 0) return false;
    }
  }
  return true;
}

Vec2 projectileVelocity(PlayerId owner) {
  return { owner == PlayerId::Left ? BOLT_SPEED : -BOLT_SPEED, 0.0f };
}

CenterBox projectileBox(const Projectile &p) {
  return {p.centerX, p.centerY, BOLT_HALF_X, BOLT_HALF_Y};
}

CenterBox shipBox(PlayerId p) {
  const uint8_t i = playerIndex(p);
  return {p == PlayerId::Left ? LEFT_SHIP_X : RIGHT_SHIP_X,
          g.players[i].centerY,
          SHIP_HALF_X, SHIP_HALF_Y};
}

void deactivateProjectile(uint8_t slot) {
  if (slot < MAX_PROJECTILES) g.shots[slot].active = false;
}

int8_t freeShotSlot(PlayerId p) {
  const uint8_t start = p == PlayerId::Left ? 0 : 2;
  for (uint8_t i = start; i < start + 2; ++i) if (!g.shots[i].active) return (int8_t)i;
  return -1;
}

void emitEvent(EventType type, PlayerId p, float x, float y) {
  if (g.eventCount >= MAX_EVENTS) {
    ++g.diag.eventOverflow;
    return;
  }
  g.events[g.eventCount++] = {type, p, x, y};
}

bool tryFire(PlayerId p) {
  const int8_t slot = freeShotSlot(p);
  if (slot < 0) return false;
  Projectile &s = g.shots[(uint8_t)slot];
  s.active = true;
  s.owner = p;
  ++s.generation;
  s.centerX = p == PlayerId::Left ? LEFT_MUZZLE_X : RIGHT_MUZZLE_X;
  s.centerY = g.players[playerIndex(p)].centerY;
  emitEvent(EventType::Fire, p, s.centerX, s.centerY);
  return true;
}

void consumeFireAttempts() {
  for (uint8_t p = 0; p < 2; ++p) {
    uint8_t count = g.input.pendingFire[p];
    g.input.pendingFire[p] = 0;
    PlayerId id = p == 0 ? PlayerId::Left : PlayerId::Right;
    while (count--) {
      // A rejected attempt is intentionally discarded immediately.
      tryFire(id);
    }
  }
}

// ============================================================
// SWEPT COLLISION
// ============================================================

bool sweptAabb(const CenterBox &a, Vec2 va,
               const CenterBox &b, Vec2 vb,
               float maxTime, float &outT) {
  const float aminX = a.cx - a.hx, amaxX = a.cx + a.hx;
  const float aminY = a.cy - a.hy, amaxY = a.cy + a.hy;
  const float bminX = b.cx - b.hx, bmaxX = b.cx + b.hx;
  const float bminY = b.cy - b.hy, bmaxY = b.cy + b.hy;
  const float rvx = va.x - vb.x;
  const float rvy = va.y - vb.y;

  float entryX = -1.0e30f, exitX = 1.0e30f;
  float entryY = -1.0e30f, exitY = 1.0e30f;

  if (fabsf(rvx) < TIME_EPS) {
    if (amaxX < bminX || aminX > bmaxX) return false;
  } else {
    const float t1 = (bminX - amaxX) / rvx;
    const float t2 = (bmaxX - aminX) / rvx;
    entryX = fmin2(t1, t2);
    exitX = fmax2(t1, t2);
  }

  if (fabsf(rvy) < TIME_EPS) {
    if (amaxY < bminY || aminY > bmaxY) return false;
  } else {
    const float t1 = (bminY - amaxY) / rvy;
    const float t2 = (bmaxY - aminY) / rvy;
    entryY = fmin2(t1, t2);
    exitY = fmax2(t1, t2);
  }

  float entry = fmax2(entryX, entryY);
  const float exitT = fmin2(exitX, exitY);
  if (exitT < -TIME_EPS || entry > exitT + TIME_EPS || entry > maxTime + TIME_EPS) return false;
  if (entry < 0.0f) entry = 0.0f;
  if (entry > maxTime + TIME_EPS) return false;
  outT = entry;
  return true;
}

bool betterBunkerTie(const Projectile &shot, uint8_t b, uint8_t r, uint8_t c,
                     uint8_t bestB, uint8_t bestR, uint8_t bestC) {
  const CenterBox a = bunkerCellBox(g.bunkers[b], r, c);
  const CenterBox z = bunkerCellBox(g.bunkers[bestB], bestR, bestC);
  const float da = fabsf(a.cy - shot.centerY);
  const float dz = fabsf(z.cy - shot.centerY);
  if (da < dz - TIME_EPS) return true;
  if (da > dz + TIME_EPS) return false;
  if (a.cy < z.cy - TIME_EPS) return true;
  if (a.cy > z.cy + TIME_EPS) return false;
  if (shot.owner == PlayerId::Left) return a.cx < z.cx;
  return a.cx > z.cx;
}

bool findFirstBunkerContact(uint8_t shotSlot, float maxTime, CollisionCandidate &out) {
  const Projectile &s = g.shots[shotSlot];
  const CenterBox sb = projectileBox(s);
  const Vec2 sv = projectileVelocity(s.owner);
  bool found = false;
  float bestT = maxTime + 1.0f;
  uint8_t bestB = 0, bestR = 0, bestC = 0;

  for (uint8_t b = 0; b < MAX_BUNKERS; ++b) {
    for (uint8_t r = 0; r < 12; ++r) {
      const uint8_t bits = g.bunkers[b].occupiedRows[r];
      if (!bits) continue;
      for (uint8_t c = 0; c < 4; ++c) {
        if ((bits & (1u << c)) == 0) continue;
        float t;
        if (!sweptAabb(sb, sv, bunkerCellBox(g.bunkers[b], r, c), {0,0}, maxTime, t)) continue;
        if (!found || t < bestT - TIME_EPS ||
            (fabsf(t - bestT) <= TIME_EPS && betterBunkerTie(s, b, r, c, bestB, bestR, bestC))) {
          found = true;
          bestT = t;
          bestB = b; bestR = r; bestC = c;
        }
      }
    }
  }

  if (!found) return false;
  out = {CandidateKind::Bunker, bestT, (int8_t)shotSlot, -1,
         (int8_t)bestB, (int8_t)bestR, (int8_t)bestC, -1};
  return true;
}

void addCandidate(CollisionCandidate *list, uint8_t &count, const CollisionCandidate &c) {
  if (count < MAX_CANDIDATES) list[count++] = c;
}

uint8_t gatherCandidates(float remaining, const float shipVy[2], CollisionCandidate *list) {
  uint8_t count = 0;

  for (uint8_t i = 0; i < MAX_PROJECTILES; ++i) {
    if (!g.shots[i].active) continue;
    const Projectile &s = g.shots[i];
    const Vec2 sv = projectileVelocity(s.owner);

    CollisionCandidate bunkerCandidate;
    if (findFirstBunkerContact(i, remaining, bunkerCandidate)) addCandidate(list, count, bunkerCandidate);

    const PlayerId target = opposite(s.owner);
    float shipT;
    const CenterBox targetBox = shipBox(target);
    if (sweptAabb(projectileBox(s), sv, targetBox,
                  {0.0f, shipVy[playerIndex(target)]}, remaining, shipT)) {
      addCandidate(list, count, {CandidateKind::ShipHit, shipT, (int8_t)i, -1, -1,-1,-1,
                                  (int8_t)playerIndex(target)});
    }

    float exitT = 1.0e30f;
    if (s.owner == PlayerId::Left) {
      const float threshold = SCREEN_W + BOLT_HALF_X;
      exitT = (threshold - s.centerX) / sv.x;
    } else {
      const float threshold = -BOLT_HALF_X;
      exitT = (threshold - s.centerX) / sv.x;
    }
    if (exitT < 0.0f) exitT = 0.0f;
    if (exitT <= remaining + TIME_EPS) {
      addCandidate(list, count, {CandidateKind::Exit, exitT, (int8_t)i, -1,-1,-1,-1,-1});
    }
  }

  // Opposing-shot pairs only. Same-owner projectiles never interact.
  for (uint8_t a = 0; a < MAX_PROJECTILES; ++a) {
    if (!g.shots[a].active) continue;
    for (uint8_t b = a + 1; b < MAX_PROJECTILES; ++b) {
      if (!g.shots[b].active || g.shots[a].owner == g.shots[b].owner) continue;
      float t;
      if (sweptAabb(projectileBox(g.shots[a]), projectileVelocity(g.shots[a].owner),
                    projectileBox(g.shots[b]), projectileVelocity(g.shots[b].owner),
                    remaining, t)) {
        addCandidate(list, count, {CandidateKind::Intercept, t, (int8_t)a, (int8_t)b,-1,-1,-1,-1});
      }
    }
  }

  for (uint8_t p = 0; p < 2; ++p) {
    const float vy = shipVy[p];
    if (fabsf(vy) < TIME_EPS) continue;
    const float y = g.players[p].centerY;
    float t = 1.0e30f;
    if (vy < 0.0f) t = (SHIP_MIN_Y - y) / vy;
    else t = (SHIP_MAX_Y - y) / vy;
    if (t < 0.0f) t = 0.0f;
    if (t <= remaining + TIME_EPS) {
      addCandidate(list, count, {CandidateKind::ShipBoundary, t,-1,-1,-1,-1,-1,(int8_t)p});
    }
  }

  return count;
}

bool findEarliestBatch(const CollisionCandidate *all, uint8_t allCount,
                       CollisionCandidate *batch, uint8_t &batchCount, float &earliest) {
  if (!allCount) return false;
  earliest = all[0].t;
  for (uint8_t i = 1; i < allCount; ++i) if (all[i].t < earliest) earliest = all[i].t;
  batchCount = 0;
  for (uint8_t i = 0; i < allCount; ++i) {
    if (fabsf(all[i].t - earliest) <= TIME_EPS && batchCount < MAX_CANDIDATES) {
      batch[batchCount++] = all[i];
    }
  }
  return batchCount > 0;
}

void advanceBodies(float seconds, const float shipVy[2]) {
  if (seconds <= 0.0f) return;
  for (uint8_t i = 0; i < MAX_PROJECTILES; ++i) {
    if (!g.shots[i].active) continue;
    g.shots[i].centerX += projectileVelocity(g.shots[i].owner).x * seconds;
  }
  for (uint8_t p = 0; p < 2; ++p) g.players[p].centerY += shipVy[p] * seconds;
}

void resolveContactBatch(const CollisionCandidate *batch, uint8_t count,
                         float shipVy[2], uint8_t &hitMask) {
  bool consume[MAX_PROJECTILES] = {false,false,false,false};

  struct CellRef { uint8_t b,r,c; };
  CellRef cells[MAX_PROJECTILES];
  uint8_t cellCount = 0;

  // Priority 1: bunker contacts.
  for (uint8_t i = 0; i < count; ++i) {
    const CollisionCandidate &c = batch[i];
    if (c.kind != CandidateKind::Bunker || c.shotA < 0) continue;
    const uint8_t s = (uint8_t)c.shotA;
    if (!g.shots[s].active) continue;
    consume[s] = true;
    bool duplicate = false;
    for (uint8_t k = 0; k < cellCount; ++k) {
      if (cells[k].b == c.bunker && cells[k].r == c.row && cells[k].c == c.col) { duplicate = true; break; }
    }
    if (!duplicate && cellCount < MAX_PROJECTILES) {
      cells[cellCount++] = {(uint8_t)c.bunker,(uint8_t)c.row,(uint8_t)c.col};
    }
  }
  for (uint8_t k = 0; k < cellCount; ++k) {
    const RectI cr = bunkerCellRect(g.bunkers[cells[k].b], cells[k].r, cells[k].c);
    if (clearCell(cells[k].b, cells[k].r, cells[k].c)) {
      const Bunker &b = g.bunkers[cells[k].b];
      emitEvent(EventType::BunkerImpact, b.owner, cr.x + 2.0f, cr.y + 2.0f);
    }
  }
  for (uint8_t s = 0; s < MAX_PROJECTILES; ++s) if (consume[s]) deactivateProjectile(s);

  // Priority 2: opposing projectile contact graph.
  for (uint8_t s = 0; s < MAX_PROJECTILES; ++s) consume[s] = false;
  for (uint8_t i = 0; i < count; ++i) {
    const CollisionCandidate &c = batch[i];
    if (c.kind != CandidateKind::Intercept || c.shotA < 0 || c.shotB < 0) continue;
    const uint8_t a = (uint8_t)c.shotA, b = (uint8_t)c.shotB;
    if (g.shots[a].active && g.shots[b].active) {
      consume[a] = true; consume[b] = true;
    }
  }
  bool anyIntercept = false;
  float ix = 0, iy = 0; uint8_t in = 0;
  for (uint8_t s = 0; s < MAX_PROJECTILES; ++s) {
    if (consume[s] && g.shots[s].active) {
      ix += g.shots[s].centerX; iy += g.shots[s].centerY; ++in;
      deactivateProjectile(s); anyIntercept = true;
    }
  }
  if (anyIntercept && in) emitEvent(EventType::ProjectileIntercept, PlayerId::Left, ix/in, iy/in);

  // Priority 3: ship contacts. Ships remain targets until the tick completes.
  for (uint8_t i = 0; i < count; ++i) {
    const CollisionCandidate &c = batch[i];
    if (c.kind != CandidateKind::ShipHit || c.shotA < 0 || c.player < 0) continue;
    const uint8_t s = (uint8_t)c.shotA;
    if (!g.shots[s].active) continue;
    hitMask |= (uint8_t)(1u << (uint8_t)c.player);
    deactivateProjectile(s);
  }

  // Priority 4: exits.
  for (uint8_t i = 0; i < count; ++i) {
    const CollisionCandidate &c = batch[i];
    if (c.kind == CandidateKind::Exit && c.shotA >= 0 && g.shots[(uint8_t)c.shotA].active) {
      deactivateProjectile((uint8_t)c.shotA);
    }
  }

  // Priority 5: ship boundaries.
  for (uint8_t i = 0; i < count; ++i) {
    const CollisionCandidate &c = batch[i];
    if (c.kind != CandidateKind::ShipBoundary || c.player < 0) continue;
    const uint8_t p = (uint8_t)c.player;
    if (shipVy[p] < 0.0f) g.players[p].centerY = SHIP_MIN_Y;
    else if (shipVy[p] > 0.0f) g.players[p].centerY = SHIP_MAX_Y;
    shipVy[p] = 0.0f;
  }
}

RoundResult classifyTickResult(uint8_t hitMask) {
  if ((hitMask & 0x03) == 0x03) return RoundResult::Draw;
  if (hitMask & 0x01) return RoundResult::RightWin;
  if (hitMask & 0x02) return RoundResult::LeftWin;
  return RoundResult::None;
}

void commitRoundResult(RoundResult result, uint32_t nowMs);
void commitBaseVictory(RoundResult result, uint32_t nowMs);

void stepSimulation(float dt, uint32_t nowMs) {
  g.players[0].previousCenterY = g.players[0].centerY;
  g.players[1].previousCenterY = g.players[1].centerY;
  g.players[0].movingSign = g.input.moveSign[0];
  g.players[1].movingSign = g.input.moveSign[1];

  consumeFireAttempts();

  float shipVy[2] = {
    g.input.moveSign[0] * SHIP_SPEED,
    g.input.moveSign[1] * SHIP_SPEED
  };

  uint8_t hitMask = 0;
  float remaining = dt;
  uint8_t guard = 0;

  while (remaining > TIME_EPS && guard++ < 12) {
    CollisionCandidate all[MAX_CANDIDATES];
    CollisionCandidate batch[MAX_CANDIDATES];
    const uint8_t allCount = gatherCandidates(remaining, shipVy, all);
    uint8_t batchCount = 0;
    float earliest = 0.0f;

    if (!findEarliestBatch(all, allCount, batch, batchCount, earliest)) {
      advanceBodies(remaining, shipVy);
      remaining = 0.0f;
      break;
    }

    if (earliest > remaining) earliest = remaining;
    advanceBodies(earliest, shipVy);
    remaining -= earliest;
    resolveContactBatch(batch, batchCount, shipVy, hitMask);

    // Every zero-time event must consume a projectile or clamp a velocity.
    // The guard remains as a hard safety net against malformed overlap states.
  }

  if (guard >= 12 && remaining > TIME_EPS) {
    ++g.diag.collisionGuardTrips;
  } else if (remaining > TIME_EPS) {
    advanceBodies(remaining, shipVy);
  }

  // Bunker destruction persists across ship deaths. Clearing every bunker cell
  // owned by the opponent ends the whole match immediately.
  const bool leftBasesGone = allBunkersCleared(PlayerId::Left);
  const bool rightBasesGone = allBunkersCleared(PlayerId::Right);
  if (leftBasesGone || rightBasesGone) {
    const RoundResult baseResult =
      (leftBasesGone && rightBasesGone) ? RoundResult::Draw :
      leftBasesGone ? RoundResult::RightWin : RoundResult::LeftWin;
    commitBaseVictory(baseResult, nowMs);
    return;
  }

  const RoundResult result = classifyTickResult(hitMask);
  if (result != RoundResult::None) commitRoundResult(result, nowMs);
}

// ============================================================
// EFFECTS / EVENTS
// ============================================================

RectI effectBounds(const VisualEffect &e) {
  const int16_t x = iround(e.x), y = iround(e.y);
  if (e.type == EffectType::Explosion) return {(int16_t)(x-14),(int16_t)(y-14),29,29};
  if (e.type == EffectType::Intercept) return {(int16_t)(x-6),(int16_t)(y-6),13,13};
  return {(int16_t)(x-5),(int16_t)(y-5),11,11};
}

void markDirty(RectI r);

int8_t allocateEffect(EffectType type) {
  (void)type;
  for (uint8_t i = 0; i < MAX_EFFECTS; ++i) if (!g.effects[i].active) return (int8_t)i;

  // Replace the oldest non-explosion effect. Explosion presentation is protected.
  int8_t best = -1;
  uint32_t oldestAge = 0;
  const uint32_t now = millis();
  for (uint8_t i = 0; i < MAX_EFFECTS; ++i) {
    if (g.effects[i].type == EffectType::Explosion) continue;
    const uint32_t age = elapsedMs(now, g.effects[i].startMs);
    if (best < 0 || age > oldestAge) { best = (int8_t)i; oldestAge = age; }
  }
  if (best >= 0) markDirty(effectBounds(g.effects[(uint8_t)best]));
  return best;
}

void spawnEffect(EffectType type, PlayerId owner, float x, float y,
                 uint32_t nowMs, uint16_t duration, uint8_t seed = 0) {
  const int8_t slot = allocateEffect(type);
  if (slot < 0) return;
  VisualEffect &e = g.effects[(uint8_t)slot];
  e.active = true; e.type = type; e.owner = owner; e.x = x; e.y = y;
  e.startMs = nowMs; e.durationMs = duration; e.seed = seed;
  markDirty(effectBounds(e));
}

void clearEffects() {
  for (uint8_t i = 0; i < MAX_EFFECTS; ++i) {
    if (g.effects[i].active) markDirty(effectBounds(g.effects[i]));
    g.effects[i].active = false;
  }
}

void updateEffects(uint32_t nowMs) {
  for (uint8_t i = 0; i < MAX_EFFECTS; ++i) {
    VisualEffect &e = g.effects[i];
    if (!e.active) continue;
    if (elapsedMs(nowMs, e.startMs) >= e.durationMs) {
      markDirty(effectBounds(e));
      e.active = false;
    }
  }

  for (uint8_t p = 0; p < 2; ++p) {
    Player &pl = g.players[p];
    if (pl.muzzleActive && elapsedMs(nowMs, pl.muzzleStartedMs) >= MUZZLE_MS) {
      pl.muzzleActive = false;
      // Snapshot comparison will dirty the ship/muzzle union.
    }
  }
}

void consumeEvents(uint32_t nowMs) {
  for (uint8_t i = 0; i < g.eventCount; ++i) {
    const GameEvent &ev = g.events[i];
    switch (ev.type) {
      case EventType::Fire: {
        Player &p = g.players[playerIndex(ev.player)];
        p.muzzleActive = true;
        p.muzzleStartedMs = nowMs;
        Audio::fire(ev.player);
        break;
      }
      case EventType::BunkerImpact:
        spawnEffect(EffectType::Impact, ev.player, ev.x, ev.y, nowMs, IMPACT_MS, i);
        Audio::bunkerImpact(ev.player);
        break;
      case EventType::ProjectileIntercept:
        spawnEffect(EffectType::Intercept, PlayerId::Left, ev.x, ev.y, nowMs, IMPACT_MS, i);
        Audio::intercept();
        break;
      case EventType::ShipDestroyed:
        spawnEffect(EffectType::Explosion, ev.player, ev.x, ev.y, nowMs, DEATH_BURST_MS, i);
        Audio::death(ev.player);
        break;
      case EventType::PlayerReady:
        Audio::ready(ev.player);
        break;
    }
  }
  g.eventCount = 0;
}

// ============================================================
// DIRTY RECT MANAGEMENT
// ============================================================

void markDirty(RectI r) {
  r = clipRect(r);
  if (rectEmpty(r)) return;

  // Opportunistically merge into an existing rectangle when the union is cheap.
  for (uint8_t i = 0; i < g.render.dirtyCount; ++i) {
    RectI &d = g.render.dirty[i];
    if (!rectsOverlapOrTouch(d, r)) continue;
    RectI u = unionRect(d, r);
    const uint32_t combined = rectArea(d) + rectArea(r);
    if (rectArea(u) * 4 <= combined * 5) {
      d = u;
      return;
    }
  }

  if (g.render.dirtyCount < MAX_DIRTY) {
    g.render.dirty[g.render.dirtyCount++] = r;
    return;
  }

  // Overflow: preserve coverage by merging with the entry having minimum added area.
  ++g.render.dirtyOverflowCount;
  uint8_t best = 0;
  uint32_t bestCost = 0xFFFFFFFFu;
  for (uint8_t i = 0; i < g.render.dirtyCount; ++i) {
    RectI u = unionRect(g.render.dirty[i], r);
    uint32_t cost = rectArea(u) - rectArea(g.render.dirty[i]);
    if (cost < bestCost) { bestCost = cost; best = i; }
  }
  g.render.dirty[best] = unionRect(g.render.dirty[best], r);
}

void mergeDirtyRects() {
  bool changed = true;
  while (changed) {
    changed = false;
    for (uint8_t i = 0; i < g.render.dirtyCount && !changed; ++i) {
      for (uint8_t j = i + 1; j < g.render.dirtyCount; ++j) {
        RectI a = g.render.dirty[i], b = g.render.dirty[j];
        if (!rectsOverlapOrTouch(a,b)) continue;
        RectI u = unionRect(a,b);
        const uint32_t combined = rectArea(a) + rectArea(b);
        if (rectArea(u) * 4 > combined * 5) continue;
        g.render.dirty[i] = u;
        for (uint8_t k = j + 1; k < g.render.dirtyCount; ++k) g.render.dirty[k-1] = g.render.dirty[k];
        --g.render.dirtyCount;
        changed = true;
        break;
      }
    }
  }
}

// ============================================================
// BUFFER-LOCAL RASTERIZER
// ============================================================

void putPixelPhysical(int16_t x, int16_t y, uint16_t color) {
  const RectI &s = g.render.scratchRect;
  if (x < s.x || y < s.y || x >= s.x + s.w || y >= s.y + s.h) return;
  const uint16_t lx = (uint16_t)(x - s.x);
  const uint16_t ly = (uint16_t)(y - s.y);
  scratchPixels[(uint32_t)ly * s.w + lx] = color;
}

void fillRectPhysical(RectI r, uint16_t color) {
  RectI s = g.render.scratchRect;
  int16_t x1 = imax16(r.x, s.x);
  int16_t y1 = imax16(r.y, s.y);
  int16_t x2 = imin16((int16_t)(r.x+r.w), (int16_t)(s.x+s.w));
  int16_t y2 = imin16((int16_t)(r.y+r.h), (int16_t)(s.y+s.h));
  if (x2 <= x1 || y2 <= y1) return;
  for (int16_t y = y1; y < y2; ++y) {
    uint16_t *row = &scratchPixels[(uint32_t)(y-s.y) * s.w + (x1-s.x)];
    for (int16_t x = x1; x < x2; ++x) *row++ = color;
  }
}

void drawHLinePhysical(int16_t x, int16_t y, int16_t w, uint16_t color) {
  fillRectPhysical({x,y,w,1}, color);
}

void drawVLinePhysical(int16_t x, int16_t y, int16_t h, uint16_t color) {
  fillRectPhysical({x,y,1,h}, color);
}

RectI mapPlayerRect(PlayerId p, int16_t u, int16_t v, int16_t w, int16_t h) {
  if (p == PlayerId::Left) {
    return {(int16_t)(240 - (v + h)), u, h, w};
  }
  return {v, (int16_t)(320 - (u + w)), h, w};
}

void fillPlayerRect(PlayerId p, int16_t u, int16_t v, int16_t w, int16_t h, uint16_t color) {
  fillRectPhysical(mapPlayerRect(p,u,v,w,h), color);
}

int8_t glyphIndex(char c) {
  if (c >= 'A' && c <= 'Z') return (int8_t)(c - 'A');
  if (c >= '0' && c <= '9') return (int8_t)(26 + c - '0');
  if (c == ' ') return 36;
  if (c == '!') return 37;
  if (c == '-') return 38;
  return 36;
}

void drawGlyph(PlayerId p, char c, int16_t u, int16_t v, uint8_t scale, uint16_t color) {
  const uint8_t *cols = FONT5X7[(uint8_t)glyphIndex(c)];
  for (uint8_t x = 0; x < 5; ++x) {
    const uint8_t bits = cols[x];
    for (uint8_t y = 0; y < 7; ++y) {
      if (bits & (1u << y)) fillPlayerRect(p, u + x*scale, v + y*scale, scale, scale, color);
    }
  }
}

void drawGlyphBanded(PlayerId p, char c, int16_t u, int16_t v, uint8_t scale,
                     uint16_t topColor, uint16_t bottomColor) {
  const uint8_t *cols = FONT5X7[(uint8_t)glyphIndex(c)];
  for (uint8_t x = 0; x < 5; ++x) {
    const uint8_t bits = cols[x];
    for (uint8_t y = 0; y < 7; ++y) {
      if (bits & (1u << y)) {
        fillPlayerRect(p, u + x*scale, v + y*scale, scale, scale, y < 2 ? topColor : bottomColor);
      }
    }
  }
}

int16_t measureText(const char *text, uint8_t scale) {
  int16_t len = 0;
  while (text[len]) ++len;
  if (!len) return 0;
  return (int16_t)(len * 6 * scale - scale);
}

void drawCenteredText(PlayerId p, const char *text, int16_t v, uint8_t scale, uint16_t color) {
  int16_t u = (int16_t)((320 - measureText(text, scale)) / 2);
  for (uint16_t i = 0; text[i]; ++i) drawGlyph(p, text[i], (int16_t)(u + i*6*scale), v, scale, color);
}

void drawCenteredBandedText(PlayerId p, const char *text, int16_t v, uint8_t scale,
                            uint16_t topColor, uint16_t bottomColor) {
  const int16_t width = measureText(text, scale);
  const int16_t u = (int16_t)((320 - width) / 2);
  // one local-pixel dark offset shadow
  for (uint16_t i = 0; text[i]; ++i) drawGlyph(p, text[i], (int16_t)(u + i*6*scale + 1), v+1, scale, C_BG);
  for (uint16_t i = 0; text[i]; ++i) drawGlyphBanded(p, text[i], (int16_t)(u + i*6*scale), v, scale, topColor, bottomColor);
}

// ============================================================
// SCENE DRAWING
// ============================================================

void drawBackground() {
  // Scratch is already filled with C_BG. Add deterministic stars.
  for (uint8_t i = 0; i < 12; ++i) {
    const StarPoint &s = STAR_PAIRS[i];
    const uint16_t color = s.bright ? C_STAR_BRIGHT : C_STAR_DIM;
    putPixelPhysical(s.x, s.y, color);
    putPixelPhysical((int16_t)(239 - s.x), s.y, color);
  }

  // Four tiny neutral center hints, deliberately not a continuous divider.
  const uint16_t ys[4] = {58, 124, 196, 262};
  for (uint8_t i = 0; i < 4; ++i) {
    putPixelPhysical(119, ys[i], C_STRUCTURE);
    putPixelPhysical(120, ys[i], C_STRUCTURE);
  }
}

void drawBunkers() {
  for (uint8_t bi = 0; bi < MAX_BUNKERS; ++bi) {
    const Bunker &b = g.bunkers[bi];
    const uint16_t body = b.owner == PlayerId::Left ? C_LEFT_BUNKER : C_RIGHT_BUNKER;
    const uint16_t edge = b.owner == PlayerId::Left ? C_LEFT_SHADOW : C_RIGHT_ALT;

    for (uint8_t r = 0; r < 12; ++r) {
      for (uint8_t c = 0; c < 4; ++c) {
        if (!isCellOccupied(b,r,c)) continue;
        RectI cell = bunkerCellRect(b,r,c);
        fillRectPhysical(cell, body);

        // Only exposed leading surfaces get a restrained accent; no black cell grid.
        if (b.owner == PlayerId::Left) {
          if (c == 3 || !isCellOccupied(b,r,(uint8_t)(c+1)))
            drawVLinePhysical((int16_t)(cell.x+3), cell.y, cell.h, edge);
        } else {
          if (c == 0 || !isCellOccupied(b,r,(uint8_t)(c-1)))
            drawVLinePhysical(cell.x, cell.y, cell.h, edge);
        }
      }
    }
  }
}

void drawScorePips() {
  const int16_t cy[3] = {147,159,171};
  for (uint8_t p = 0; p < 2; ++p) {
    const int16_t cx = p == 0 ? 6 : 233;
    const uint16_t fill = p == 0 ? C_LEFT : C_RIGHT;
    for (uint8_t i = 0; i < 3; ++i) {
      fillRectPhysical({(int16_t)(cx-3),(int16_t)(cy[i]-3),7,7}, C_STRUCTURE);
      fillRectPhysical({(int16_t)(cx-2),(int16_t)(cy[i]-2),5,5}, i < g.players[p].wins ? fill : C_BG);
    }
  }
}

uint8_t shipMaterial(int8_t dx, int8_t dy) {
  const int8_t ay = dy < 0 ? -dy : dy;
  bool occupied = false;
  if (ay <= 1) occupied = dx >= -8 && dx <= 9;
  else if (ay <= 3) occupied = dx >= -6 && dx <= 7;
  else if (ay <= 5) occupied = dx >= -5 && dx <= 5;
  else if (ay <= 7) occupied = dx >= -7 && dx <= 2;
  else if (ay <= 9) occupied = dx >= -5 && dx <= 0;
  else occupied = dx >= -2 && dx <= -1;
  if (!occupied) return 0;
  if (ay <= 1 && dx >= 2 && dx <= 4) return 3; // cockpit
  if (dx <= -4) return 1;                      // shadow/rear structure
  return 2;                                    // primary hull
}

void drawShip(PlayerId p, uint32_t nowMs) {
  const uint8_t pi = playerIndex(p);
  const Player &pl = g.players[pi];
  if (!pl.alive) return;
  const int16_t anchorX = p == PlayerId::Left ? (int16_t)LEFT_SHIP_X : (int16_t)RIGHT_SHIP_X;
  const int16_t anchorY = iround(pl.centerY);
  const uint16_t shadow = p == PlayerId::Left ? C_LEFT_SHADOW : C_RIGHT_ALT;
  const uint16_t primary = p == PlayerId::Left ? C_LEFT : C_RIGHT;

  for (int8_t dy = -10; dy <= 10; ++dy) {
    for (int8_t dx = -8; dx <= 9; ++dx) {
      const uint8_t mat = shipMaterial(dx,dy);
      if (!mat) continue;
      const int8_t sx = p == PlayerId::Left ? dx : -dx;
      const uint16_t color = mat == 1 ? shadow : (mat == 3 ? C_WHITE : primary);
      putPixelPhysical((int16_t)(anchorX + sx), (int16_t)(anchorY + dy), color);
    }
  }

  // Restrained engine accent. Moving ships pulse at 8 Hz without changing bounds.
  const bool engineBright = pl.movingSign != 0 && ((nowMs / 125u) & 1u);
  const int16_t engineX = p == PlayerId::Left ? (int16_t)(anchorX - 9) : (int16_t)(anchorX + 8);
  fillRectPhysical({engineX,(int16_t)(anchorY-1),2,3}, engineBright ? primary : shadow);

  if (pl.muzzleActive) {
    if (p == PlayerId::Left) {
      fillRectPhysical({(int16_t)(anchorX+10),(int16_t)(anchorY-1),5,3}, primary);
      putPixelPhysical((int16_t)(anchorX+15), anchorY, C_WHITE);
    } else {
      fillRectPhysical({(int16_t)(anchorX-14),(int16_t)(anchorY-1),5,3}, primary);
      putPixelPhysical((int16_t)(anchorX-15), anchorY, C_WHITE);
    }
  }
}

void drawShips(uint32_t nowMs) {
  drawShip(PlayerId::Left, nowMs);
  drawShip(PlayerId::Right, nowMs);
}

RectI projectileVisualBounds(const Projectile &s) {
  const int16_t x = iround(s.centerX), y = iround(s.centerY);
  return {(int16_t)(x-3),(int16_t)(y-1),7,3};
}

void drawProjectiles() {
  for (uint8_t i = 0; i < MAX_PROJECTILES; ++i) {
    const Projectile &s = g.shots[i];
    if (!s.active) continue;
    const int16_t x = iround(s.centerX), y = iround(s.centerY);
    const uint16_t body = s.owner == PlayerId::Left ? C_LEFT : C_RIGHT;
    fillRectPhysical({(int16_t)(x-3),(int16_t)(y-1),7,3}, body);
    if (s.owner == PlayerId::Left) drawHLinePhysical((int16_t)(x-1), y, 4, C_WHITE);
    else drawHLinePhysical((int16_t)(x-2), y, 4, C_WHITE);
  }
}

void drawEffects(uint32_t nowMs) {
  for (uint8_t i = 0; i < MAX_EFFECTS; ++i) {
    const VisualEffect &e = g.effects[i];
    if (!e.active) continue;
    const uint32_t age = elapsedMs(nowMs, e.startMs);
    if (age >= e.durationMs) continue;
    const int16_t x = iround(e.x), y = iround(e.y);
    const uint16_t own = e.owner == PlayerId::Left ? C_LEFT : C_RIGHT;

    if (e.type == EffectType::Impact) {
      putPixelPhysical(x,y,C_WHITE);
      drawHLinePhysical((int16_t)(x-2), y, 5, own);
      drawVLinePhysical(x,(int16_t)(y-2),5,C_GOLD);
    } else if (e.type == EffectType::Intercept) {
      putPixelPhysical(x,y,C_WHITE);
      drawHLinePhysical((int16_t)(x-3), y, 7, C_GOLD);
      drawVLinePhysical(x,(int16_t)(y-3),7,C_FATAL);
    } else {
      const uint8_t stage = age < 45 ? 0 : 1;
      if (stage == 0) {
        fillRectPhysical({(int16_t)(x-3),(int16_t)(y-3),7,7}, C_WHITE);
        drawHLinePhysical((int16_t)(x-6),y,13,C_GOLD);
      } else {
        const int16_t radius = (int16_t)(4 + ((age-45) * 8) / (DEATH_BURST_MS-45));
        for (uint8_t d = 0; d < 8; ++d) {
          const int16_t x1 = (int16_t)(x + EXPLOSION_DIRS[d][0] * (radius-2));
          const int16_t y1 = (int16_t)(y + EXPLOSION_DIRS[d][1] * (radius-2));
          putPixelPhysical(x1,y1,(d&1) ? C_GOLD : own);
          putPixelPhysical((int16_t)(x1+EXPLOSION_DIRS[d][0]),
                           (int16_t)(y1+EXPLOSION_DIRS[d][1]), C_FATAL);
        }
      }
    }
  }
}

RectI playerPanelRect(PlayerId p) {
  return mapPlayerRect(p, 8, 136, 304, 78);
}

void drawPlayerPanel(PlayerId p) {
  fillPlayerRect(p, 8, 136, 304, 78, C_PANEL);
  const uint16_t accent = p == PlayerId::Left ? C_LEFT : C_RIGHT;
  fillPlayerRect(p, 8, 136, 304, 2, accent);
  // small geometric corners, not a thick frame
  fillPlayerRect(p, 8, 140, 2, 10, C_STRUCTURE);
  fillPlayerRect(p, 302, 140, 2, 10, C_STRUCTURE);
  fillPlayerRect(p, 8, 202, 2, 8, C_STRUCTURE);
  fillPlayerRect(p, 302, 202, 2, 8, C_STRUCTURE);
}

const char* fatalitySubtitle(PlayerId p) {
  if (g.match.result == RoundResult::Draw) return "DOUBLE KO";
  const bool leftWon = g.match.result == RoundResult::LeftWin;
  const bool won = (p == PlayerId::Left) == leftWon;
  return won ? "ROUND WON" : "ROUND LOST";
}

bool playerWonMatch(PlayerId p) {
  return g.players[playerIndex(p)].wins >= 3;
}

void drawPhaseOverlay() {
  if (g.match.phase == Phase::FightSplash && g.match.overlayVisible) {
    char roundText[18];
    snprintf(roundText, sizeof(roundText), "ROUND %u", (unsigned)g.match.attemptNumber);
    for (uint8_t pi = 0; pi < 2; ++pi) {
      PlayerId p = pi == 0 ? PlayerId::Left : PlayerId::Right;
      drawPlayerPanel(p);
      drawCenteredText(p, roundText, 141, 1, C_MUTED_TEXT);
      drawCenteredText(p, "FIGHT!", 159, 3, C_GOLD);
      drawCenteredText(p, "DESTROY ALL BASES", 190, 1, C_WHITE);
      drawCenteredText(p, g.match.fightReady[pi] ? "READY" : "PRESS FIRE", 205, 1,
                       g.match.fightReady[pi] ? (p == PlayerId::Left ? C_LEFT : C_RIGHT) : C_MUTED_TEXT);
    }
  } else if (g.match.phase == Phase::Fatality) {
    for (uint8_t pi = 0; pi < 2; ++pi) {
      PlayerId p = pi == 0 ? PlayerId::Left : PlayerId::Right;
      drawPlayerPanel(p);
      drawCenteredBandedText(p, "FATALITY", 157, 3, C_GOLD, C_FATAL);
      drawCenteredText(p, fatalitySubtitle(p), 193, 1,
                       g.match.result == RoundResult::Draw ? C_MUTED_TEXT :
                       ((g.match.result == RoundResult::LeftWin) == (p == PlayerId::Left) ? C_GOLD : C_MUTED_TEXT));
    }
  } else if (g.match.phase == Phase::MatchOver) {
    for (uint8_t pi = 0; pi < 2; ++pi) {
      PlayerId p = pi == 0 ? PlayerId::Left : PlayerId::Right;
      drawPlayerPanel(p);
      const bool draw = g.match.result == RoundResult::Draw;
      const bool won = playerWonMatch(p);
      drawCenteredText(p, draw ? "DRAW" : (won ? "YOU WIN" : "YOU LOSE"),
                       151, 3, (draw || won) ? C_GOLD : C_WHITE);
      drawCenteredText(p, g.match.rematchReady[pi] ? "READY" : "PRESS FIRE", 191, 2,
                       g.match.rematchReady[pi] ? (p == PlayerId::Left ? C_LEFT : C_RIGHT) : C_MUTED_TEXT);
      if (won) {
        fillPlayerRect(p, 151, 143, 3, 3, C_GOLD);
        fillPlayerRect(p, 166, 145, 2, 2, C_GOLD);
      }
    }
  }
}

void composeRegion(RectI r, uint32_t nowMs) {
  r = clipRect(r);
  if (rectEmpty(r)) return;
  g.render.scratchRect = r;
  const uint32_t pixels = (uint32_t)r.w * r.h;
  for (uint32_t i = 0; i < pixels; ++i) scratchPixels[i] = C_BG;

  drawBackground();
  drawBunkers();
  drawScorePips();
  drawShips(nowMs);
  drawProjectiles();
  drawEffects(nowMs);
  drawPhaseOverlay();
}

void blitRegion(RectI r) {
  const uint32_t pixels = (uint32_t)r.w * r.h;
  if (!pixels || pixels > SCRATCH_PIXELS) return;
  display.drawRGBBitmap(r.x, r.y, scratchPixels, r.w, r.h);
  g.render.transferredPixels += pixels;
}

void composeAndBlit(RectI r, uint32_t nowMs) {
  r = clipRect(r);
  if (rectEmpty(r)) return;

  // Split into <=2048-pixel opaque subrectangles. Width is capped at 64 when useful.
  int16_t x = r.x;
  while (x < r.x + r.w) {
    const int16_t chunkW = imin16(64, (int16_t)(r.x + r.w - x));
    const int16_t maxH = (int16_t)(SCRATCH_PIXELS / chunkW);
    int16_t y = r.y;
    while (y < r.y + r.h) {
      const int16_t chunkH = imin16(maxH, (int16_t)(r.y + r.h - y));
      RectI sub{x,y,chunkW,chunkH};
      composeRegion(sub, nowMs);
      blitRegion(sub);
      y += chunkH;
    }
    x += chunkW;
  }
}

// ============================================================
// RENDER SNAPSHOTS / FRAME DAMAGE
// ============================================================

RectI shipVisualBounds(PlayerId p) {
  const int16_t y = iround(g.players[playerIndex(p)].centerY);
  // Conservative union includes sprite, engine and optional muzzle flash.
  if (p == PlayerId::Left) return {13,(int16_t)(y-11),27,23};
  return {200,(int16_t)(y-11),27,23};
}

uint16_t shipAppearance(PlayerId p, uint32_t nowMs) {
  const Player &pl = g.players[playerIndex(p)];
  uint16_t a = pl.alive ? 1 : 0;
  if (pl.muzzleActive) a |= 0x10;
  if (pl.movingSign != 0) a |= (uint16_t)(((nowMs / 125u) & 1u) ? 0x20 : 0x40);
  return a;
}

void invalidateSnapshots() {
  for (uint8_t p = 0; p < 2; ++p) {
    g.render.ship[p].visible = false;
    g.render.ship[p].bounds = {0,0,0,0};
    g.render.ship[p].appearance = 0;
    g.render.shownWins[p] = 255;
    g.render.shownReady[p] = !g.match.rematchReady[p];
  }
  for (uint8_t s = 0; s < MAX_PROJECTILES; ++s) {
    g.render.shot[s].visible = false;
    g.render.shot[s].bounds = {0,0,0,0};
  }
}

void captureRenderSnapshot(uint32_t nowMs) {
  for (uint8_t p = 0; p < 2; ++p) {
    PlayerId id = p == 0 ? PlayerId::Left : PlayerId::Right;
    g.render.ship[p].visible = g.players[p].alive;
    g.render.ship[p].bounds = shipVisualBounds(id);
    g.render.ship[p].appearance = shipAppearance(id, nowMs);
    g.render.shownWins[p] = g.players[p].wins;
    g.render.shownReady[p] = g.match.rematchReady[p];
  }
  for (uint8_t s = 0; s < MAX_PROJECTILES; ++s) {
    g.render.shot[s].visible = g.shots[s].active;
    g.render.shot[s].bounds = g.shots[s].active ? projectileVisualBounds(g.shots[s]) : RectI{0,0,0,0};
  }
}

void markScoreDirty(PlayerId p) {
  markDirty(p == PlayerId::Left ? RectI{2,142,9,35} : RectI{229,142,9,35});
}

void markPanelDirty(PlayerId p) { markDirty(playerPanelRect(p)); }

void collectDynamicDamage(uint32_t nowMs) {
  for (uint8_t p = 0; p < 2; ++p) {
    PlayerId id = p == 0 ? PlayerId::Left : PlayerId::Right;
    const bool visible = g.players[p].alive;
    const RectI cur = shipVisualBounds(id);
    const uint16_t app = shipAppearance(id, nowMs);
    const ShipRenderSnapshot &old = g.render.ship[p];
    if (old.visible != visible || (visible && (!rectEqual(old.bounds, cur) || old.appearance != app))) {
      if (old.visible) markDirty(old.bounds);
      if (visible) markDirty(cur);
    }

    if (g.render.shownWins[p] != g.players[p].wins) markScoreDirty(id);
    if (g.match.phase == Phase::MatchOver && g.render.shownReady[p] != g.match.rematchReady[p]) markPanelDirty(id);
  }

  for (uint8_t s = 0; s < MAX_PROJECTILES; ++s) {
    const bool visible = g.shots[s].active;
    const RectI cur = visible ? projectileVisualBounds(g.shots[s]) : RectI{0,0,0,0};
    const ProjectileRenderSnapshot &old = g.render.shot[s];
    if (old.visible != visible || (visible && !rectEqual(old.bounds,cur))) {
      if (old.visible) markDirty(old.bounds);
      if (visible) markDirty(cur);
    }
  }

  // Effects are time-varying; redraw their conservative bounds each visual frame.
  for (uint8_t i = 0; i < MAX_EFFECTS; ++i) if (g.effects[i].active) markDirty(effectBounds(g.effects[i]));
}

bool renderDue(uint32_t nowUs) {
  return elapsedUs(nowUs, g.render.lastRenderUs) >= RENDER_INTERVAL_US;
}

void requestSceneRebuild() {
  g.render.rebuilding = true;
  g.render.rebuildY = 0;
  ++g.render.rebuildRevision;
  g.render.dirtyCount = 0;
  invalidateSnapshots();
}

void onScenePresentationComplete(uint32_t nowMs) {
  if (g.match.phase == Phase::FightSplash || g.match.phase == Phase::Fatality || g.match.phase == Phase::MatchOver) {
    g.match.presentationReady = true;
    g.match.presentationStartedMs = nowMs;
    if (g.match.phase == Phase::MatchOver) {
      if (g.players[0].wins >= 3) Audio::matchVictory(PlayerId::Left);
      else if (g.players[1].wins >= 3) Audio::matchVictory(PlayerId::Right);
    }
  }
}

void renderRebuildSlice(uint32_t nowMs) {
  if (!g.render.rebuilding) return;
  for (uint8_t n = 0; n < REBUILD_STRIPS_PER_UPDATE && g.render.rebuildY < SCREEN_H; ++n) {
    const int16_t h = imin16(REBUILD_ROWS, (int16_t)(SCREEN_H - g.render.rebuildY));
    RectI strip{0,g.render.rebuildY,SCREEN_W,h};
    composeRegion(strip, nowMs);
    blitRegion(strip);
    g.render.rebuildY += h;
  }

  if (g.render.rebuildY >= SCREEN_H) {
    g.render.rebuilding = false;
    g.render.dirtyCount = 0;
    captureRenderSnapshot(nowMs);
    onScenePresentationComplete(nowMs);
  }
}

void startPlayingAfterOverlayRemoval(uint32_t nowMs, uint32_t nowUs);

void renderFrame(uint32_t nowMs, uint32_t nowUs) {
  collectDynamicDamage(nowMs);
  mergeDirtyRects();
  for (uint8_t i = 0; i < g.render.dirtyCount; ++i) composeAndBlit(g.render.dirty[i], nowMs);
  g.render.dirtyCount = 0;
  g.render.lastRenderUs = nowUs;
  captureRenderSnapshot(nowMs);

  if (g.match.phase == Phase::FightSplash && g.match.overlayRemoving) {
    startPlayingAfterOverlayRemoval(nowMs, nowUs);
  }
}

// ============================================================
// MATCH / PHASE STATE
// ============================================================

void setupBunkers() {
  // Fully alternating bunker rows.
  //
  // Each bunker is 48 px tall. These Y positions alternate every 48-56 px:
  //
  //   Y=8    LEFT
  //   Y=56   RIGHT
  //   Y=112  LEFT
  //   Y=160  RIGHT
  //   Y=216  LEFT
  //   Y=264  RIGHT
  //
  // The two three-bunker patterns are exact 180-degree mirrors:
  // a left bunker at y maps to right y = 320 - (y + 48).
  //
  // This gives each player a bunker close to their own local-left wall
  // while making the open spaces flip/interlock across the arena.
  const int16_t leftYs[3]  = {8,112,216};
  const int16_t rightYs[3] = {56,160,264};

  for (uint8_t i = 0; i < 3; ++i) {
    g.bunkers[i].owner = PlayerId::Left;
    g.bunkers[i].x = 49;
    g.bunkers[i].y = leftYs[i];
    resetBunker(g.bunkers[i]);

    g.bunkers[i+3].owner = PlayerId::Right;
    g.bunkers[i+3].x = 175;
    g.bunkers[i+3].y = rightYs[i];
    resetBunker(g.bunkers[i+3]);
  }
}

void clearProjectiles() {
  for (uint8_t i = 0; i < MAX_PROJECTILES; ++i) g.shots[i].active = false;
}

void clearEventQueue() { g.eventCount = 0; }

void synchronizeSimulationClock(uint32_t nowUs) {
  g.previousSimUs = nowUs;
  g.simAccumulator = 0;
}

void prepareRound(uint32_t nowMs, uint32_t nowUs) {
  g.players[0].centerY = START_Y;
  g.players[1].centerY = START_Y;
  g.players[0].previousCenterY = START_Y;
  g.players[1].previousCenterY = START_Y;
  g.players[0].alive = true;
  g.players[1].alive = true;
  g.players[0].movingSign = 0;
  g.players[1].movingSign = 0;
  g.players[0].muzzleActive = false;
  g.players[1].muzzleActive = false;

  clearProjectiles();
  // Bunkers intentionally survive ship deaths / new rounds. They are only
  // restored by resetMatch() when a completely new match begins.
  clearEffects();
  clearEventQueue();
  resetInputGates();

  g.match.phase = Phase::FightSplash;
  g.match.result = RoundResult::None;
  g.match.fightReady[0] = false;
  g.match.fightReady[1] = false;
  g.match.phaseStartedMs = nowMs;
  g.match.presentationReady = false;
  g.match.overlayVisible = true;
  g.match.overlayRemoving = false;
  ++g.match.phaseGeneration;
  synchronizeSimulationClock(nowUs);
  requestSceneRebuild();
}

void resetMatch(uint32_t nowMs, uint32_t nowUs) {
  g.players[0].wins = 0;
  g.players[1].wins = 0;
  g.match.attemptNumber = 1;
  g.match.rematchReady[0] = false;
  g.match.rematchReady[1] = false;
  setupBunkers();
  prepareRound(nowMs, nowUs);
}

void startPlayingAfterOverlayRemoval(uint32_t nowMs, uint32_t nowUs) {
  g.match.phase = Phase::Playing;
  g.match.phaseStartedMs = nowMs;
  g.match.presentationReady = false;
  g.match.overlayRemoving = false;
  g.match.overlayVisible = false;
  resetInputGates();
  synchronizeSimulationClock(nowUs);
}

void beginFatality(uint32_t nowMs) {
  // Hide residual frozen shots and explosion effects under the result composition.
  clearProjectiles();
  clearEffects();
  resetInputGates();
  g.match.phase = Phase::Fatality;
  g.match.phaseStartedMs = nowMs;
  g.match.presentationReady = false;
  g.match.overlayVisible = true;
  g.match.overlayRemoving = false;
  ++g.match.phaseGeneration;
  requestSceneRebuild();
}

void enterMatchOver(uint32_t nowMs) {
  g.match.phase = Phase::MatchOver;
  g.match.phaseStartedMs = nowMs;
  g.match.presentationReady = false;
  g.match.overlayVisible = true;
  g.match.overlayRemoving = false;
  g.match.rematchReady[0] = false;
  g.match.rematchReady[1] = false;
  ++g.match.phaseGeneration;
  resetInputGates();
  requestSceneRebuild();
}

void finishFatality(uint32_t nowMs, uint32_t nowUs) {
  if (g.players[0].wins >= 3 || g.players[1].wins >= 3) {
    enterMatchOver(nowMs);
  } else {
    ++g.match.attemptNumber;
    prepareRound(nowMs, nowUs);
  }
}

void commitBaseVictory(RoundResult result, uint32_t nowMs) {
  if (g.match.phase != Phase::Playing || g.match.result != RoundResult::None ||
      result == RoundResult::None) return;

  g.match.result = result;

  // Reuse the existing three-pip match victory presentation/audio. A base clear
  // is an immediate match win, regardless of the current ship-round score.
  if (result == RoundResult::LeftWin) {
    g.players[0].wins = 3;
    markScoreDirty(PlayerId::Left);
  } else if (result == RoundResult::RightWin) {
    g.players[1].wins = 3;
    markScoreDirty(PlayerId::Right);
  }

  clearProjectiles();
  resetInputGates();
  g.simAccumulator = 0;
  enterMatchOver(nowMs);
}

void commitRoundResult(RoundResult result, uint32_t nowMs) {
  if (g.match.phase != Phase::Playing || g.match.result != RoundResult::None) return;
  g.match.result = result;

  if (result == RoundResult::LeftWin) {
    if (g.players[0].wins < 3) ++g.players[0].wins;
    emitEvent(EventType::ShipDestroyed, PlayerId::Right, RIGHT_SHIP_X, g.players[1].centerY);
    g.players[1].alive = false;
    markScoreDirty(PlayerId::Left);
  } else if (result == RoundResult::RightWin) {
    if (g.players[1].wins < 3) ++g.players[1].wins;
    emitEvent(EventType::ShipDestroyed, PlayerId::Left, LEFT_SHIP_X, g.players[0].centerY);
    g.players[0].alive = false;
    markScoreDirty(PlayerId::Right);
  } else {
    emitEvent(EventType::ShipDestroyed, PlayerId::Left, LEFT_SHIP_X, g.players[0].centerY);
    emitEvent(EventType::ShipDestroyed, PlayerId::Right, RIGHT_SHIP_X, g.players[1].centerY);
    g.players[0].alive = false;
    g.players[1].alive = false;
  }

  g.match.phase = Phase::DeathBurst;
  g.match.phaseStartedMs = nowMs;
  g.match.presentationReady = false;
  ++g.match.phaseGeneration;
  resetInputGates();
  g.simAccumulator = 0;
}

void registerFightReady(PlayerId p, uint32_t nowMs) {
  const uint8_t i = playerIndex(p);
  if (g.match.phase != Phase::FightSplash || !g.match.presentationReady ||
      g.match.fightReady[i]) return;

  g.match.fightReady[i] = true;
  markPanelDirty(p);
  emitEvent(EventType::PlayerReady, p, 0, 0);

  if (g.match.fightReady[0] && g.match.fightReady[1]) {
    // Both players explicitly armed the round. Start the short FIGHT hold now;
    // startPlayingAfterOverlayRemoval() will re-arm buttons only after release,
    // so these ready presses can never leak through as opening shots.
    g.match.presentationStartedMs = nowMs;
    Audio::fight();
  }
}

void registerRematchReady(PlayerId p, uint32_t nowMs) {
  const uint8_t i = playerIndex(p);
  if (g.match.phase != Phase::MatchOver || g.match.rematchReady[i]) return;
  g.match.rematchReady[i] = true;
  markPanelDirty(p);
  emitEvent(EventType::PlayerReady, p, 0, 0);

  if (g.match.rematchReady[0] && g.match.rematchReady[1]) {
    // Start a fresh match immediately; prepareRound disarms held buttons again.
    resetMatch(nowMs, micros());
  }
}

void updatePhase(uint32_t nowMs, uint32_t nowUs) {
  if (g.match.phase == Phase::FightSplash) {
    if (g.match.presentationReady && g.match.fightReady[0] && g.match.fightReady[1] &&
        !g.match.overlayRemoving &&
        elapsedMs(nowMs, g.match.presentationStartedMs) >= FIGHT_HOLD_MS) {
      g.match.overlayVisible = false;
      g.match.overlayRemoving = true;
      markPanelDirty(PlayerId::Left);
      markPanelDirty(PlayerId::Right);
    }
  } else if (g.match.phase == Phase::DeathBurst) {
    if (elapsedMs(nowMs, g.match.phaseStartedMs) >= DEATH_BURST_MS) beginFatality(nowMs);
  } else if (g.match.phase == Phase::Fatality) {
    if (g.match.presentationReady && elapsedMs(nowMs, g.match.presentationStartedMs) >= FATALITY_HOLD_MS) {
      finishFatality(nowMs, nowUs);
    }
  }
}

// ============================================================
// PUBLIC ENTRY POINTS
// ============================================================

void enter() {
  const uint32_t nowMs = millis();
  const uint32_t nowUs = micros();

  display.setRotation(0);
  display.setTextWrap(false);

  // Reset bounded state without touching launcher-owned hardware initialization.
  g = GameState{};
  g.initialized = true;
  g.render.lastRenderUs = nowUs;
  invalidateSnapshots();
  Audio::cancel();
  resetMatch(nowMs, nowUs);
}

void update(const GameInput &input) {
  const uint32_t nowUs = micros();
  const uint32_t nowMs = millis();

  pollInput(input, nowMs);
  updatePhase(nowMs, nowUs);

  if (g.match.phase == Phase::Playing) {
    uint32_t elapsed = elapsedUs(nowUs, g.previousSimUs);
    g.previousSimUs = nowUs;
    if (elapsed > MAX_ELAPSED_US) elapsed = MAX_ELAPSED_US;
    g.simAccumulator += (uint64_t)elapsed * SIM_SCALE;

    uint8_t ticks = 0;
    while (g.simAccumulator >= SIM_TICK_UNITS && ticks < MAX_CATCHUP_TICKS && g.match.phase == Phase::Playing) {
      stepSimulation(SIM_DT, nowMs);
      consumeEvents(nowMs);
      g.simAccumulator -= SIM_TICK_UNITS;
      ++ticks;
    }

    if (g.simAccumulator >= SIM_TICK_UNITS) {
      ++g.diag.droppedTickDebt;
      g.simAccumulator %= SIM_TICK_UNITS;
    }
  } else {
    synchronizeSimulationClock(nowUs);
  }

  consumeEvents(nowMs); // presentation / result events
  updateEffects(nowMs);

  if (g.render.rebuilding) {
    renderRebuildSlice(nowMs);
  } else {
    const bool importantDirty = g.render.dirtyCount > 0 &&
      (g.match.phase != Phase::Playing || g.match.overlayRemoving);
    if (renderDue(nowUs) || importantDirty) renderFrame(nowMs, nowUs);
  }
}

} // namespace SpaceEvadersV2

