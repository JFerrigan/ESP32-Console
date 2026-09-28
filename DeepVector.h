#pragma once
#include "Hardware.h"
#include "GameRenderMemory.h"
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <math.h>
#include <esp_system.h>
#include <string.h>

namespace DeepVector {

// ============================================================
// DEEP VECTOR
// ESP32 / Arduino IDE / ST7789 240x320 / two 3-position switches
// Complete single-sketch implementation.
// ============================================================

// ---------------- Compile-time debugging ----------------
#define DEBUG_PERFORMANCE 0
#define DEBUG_INPUT 0

// ---------------- Fixed hardware mapping ----------------


Adafruit_ST7789 display = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// 0 gives a 240x320 portrait coordinate system on the common ST7789
// module. If this particular panel is physically upside-down, change
// ONLY this constant to 2. Do not change game coordinates.
constexpr uint8_t DISPLAY_ROTATION = 0;

// ---------------- Screen / viewport ----------------
constexpr int SCREEN_WIDTH  = 240;
constexpr int SCREEN_HEIGHT = 320;
constexpr int VIEWPORT_X = 0;
constexpr int VIEWPORT_Y = 0;
constexpr int VIEWPORT_W = 240;
constexpr int VIEWPORT_H = 288;
constexpr int VIEWPORT_MAX_X = VIEWPORT_X + VIEWPORT_W - 1;
constexpr int VIEWPORT_MAX_Y = VIEWPORT_Y + VIEWPORT_H - 1;
constexpr int CENTER_X = 120;
constexpr int CENTER_Y = 144;
constexpr int HUD_Y = 288;

// ---------------- Timing ----------------
constexpr float FIXED_DT = 1.0f / 60.0f;
constexpr uint32_t RENDER_INTERVAL_MS = 33;
constexpr int MAX_PHYSICS_CATCHUP_STEPS = 4;
constexpr uint32_t DEBOUNCE_MS = 20;
constexpr uint32_t SWITCH_ERROR_MS = 100;

// ---------------- Gameplay tuning ----------------
constexpr float PLAYER_BASE_SPEED = 22.0f;
constexpr float PLAYER_RADIUS = 1.75f;
constexpr float MAX_PITCH_RATE = 1.20f;
constexpr float MAX_YAW_RATE   = 1.20f;
constexpr float MAX_ROLL_RATE  = 1.80f;
constexpr float ANGULAR_ACCELERATION = 5.5f;
constexpr float ANGULAR_DAMPING = 7.0f;

// In this +X right, +Y up, +Z forward coordinate system, positive
// right-axis quaternion rotation points the nose down and positive
// forward-axis rotation raises the right side. These signs preserve
// the semantic meaning "positive pitch = nose up" and
// "positive roll = roll right" used by the control layer.
constexpr float PITCH_ROTATION_SIGN = -1.0f;
constexpr float YAW_ROTATION_SIGN   =  1.0f;
constexpr float ROLL_ROTATION_SIGN  = -1.0f;

constexpr float PROJECTILE_SPEED = 125.0f;
constexpr float PROJECTILE_RADIUS = 0.40f;
constexpr float PROJECTILE_LIFE = 1.80f;
constexpr uint32_t SHOT_COOLDOWN_MS = 180;

constexpr float NEAR_CLIP = 0.50f;
constexpr float FOCAL_LENGTH = 150.0f;

constexpr int MAX_ASTEROIDS   = 28;
constexpr int MAX_PROJECTILES = 10;
constexpr int MAX_PARTICLES   = 40;
constexpr int STAR_COUNT      = 90;
constexpr int MAX_RENDER_COMMANDS = 900;
constexpr int SPAWN_ATTEMPTS = 8;

constexpr int ASTEROID_VERTEX_COUNT = 12;
constexpr int ASTEROID_EDGE_COUNT = 30;

constexpr uint32_t PLAYER_INVULNERABILITY_MS = 500;
constexpr uint32_t DAMAGE_FLASH_MS = 180;
constexpr uint32_t STARTING_MS = 850;
constexpr uint32_t DESTROYED_DRIFT_MS = 800;
constexpr float WORLD_REBASE_DISTANCE = 10000.0f;

// ---------------- Colors ----------------
constexpr uint16_t COLOR_BACKGROUND = ST77XX_BLACK;
constexpr uint16_t COLOR_STAR_DIM    = 0x7BEF;
constexpr uint16_t COLOR_STAR        = ST77XX_WHITE;
constexpr uint16_t COLOR_ASTEROID    = ST77XX_CYAN;
constexpr uint16_t COLOR_PROJECTILE  = ST77XX_YELLOW;
constexpr uint16_t COLOR_PARTICLE    = ST77XX_WHITE;
constexpr uint16_t COLOR_HUD         = ST77XX_GREEN;
constexpr uint16_t COLOR_CROSSHAIR   = ST77XX_WHITE;
constexpr uint16_t COLOR_DAMAGE      = ST77XX_RED;

// ============================================================
// Data types
// ============================================================

struct Vec3 {
  float x;
  float y;
  float z;
};

struct Quaternion {
  float w;
  float x;
  float y;
  float z;
};

struct ScreenPoint {
  int16_t x;
  int16_t y;
  bool visible;
};

enum SwitchState {
  SWITCH_UP,
  SWITCH_CENTER,
  SWITCH_DOWN,
  SWITCH_ERROR
};

struct DebouncedSwitch {
  SwitchState rawState;
  SwitchState stableState;
  uint32_t rawChangedAt;
  bool fault;
};

enum ControlAction {
  ACTION_NONE,
  ACTION_PITCH_UP,
  ACTION_PITCH_DOWN,
  ACTION_YAW_LEFT,
  ACTION_YAW_RIGHT,
  ACTION_ROLL_LEFT,
  ACTION_ROLL_RIGHT
};

struct Player {
  Vec3 position;
  Quaternion orientation;
  Vec3 angularVelocity;
  float forwardSpeed;
  int shield;
  uint32_t lastShotTimeMs;
  uint32_t invulnerableUntilMs;
};

struct Asteroid {
  bool active;
  Vec3 position;
  Vec3 velocity;
  Quaternion orientation;
  Vec3 angularVelocity;
  float radius;
  int health;
  uint32_t seed;
  Vec3 vertices[ASTEROID_VERTEX_COUNT];
};

struct Projectile {
  bool active;
  Vec3 position;
  Vec3 velocity;
  float radius;
  float remainingLife;
};

struct Particle {
  bool active;
  Vec3 position;
  Vec3 velocity;
  float remainingLife;
  uint16_t color;
};

struct Star {
  Vec3 direction;
  uint8_t brightness;
};

struct Difficulty {
  int sector;
  int targetAsteroidCount;
  float asteroidSpeedMultiplier;
  float sizeMultiplier;
  float playerSpeedMultiplier;
};

enum AsteroidLOD {
  ASTEROID_LOD_FAR,
  ASTEROID_LOD_MEDIUM,
  ASTEROID_LOD_NEAR
};

enum RenderCommandType {
  RENDER_PIXEL,
  RENDER_LINE
};

struct RenderCommand {
  RenderCommandType type;
  int16_t x1;
  int16_t y1;
  int16_t x2;
  int16_t y2;
  uint16_t color;
};

enum GameState {
  GAME_BOOT,
  GAME_TITLE,
  GAME_STARTING,
  GAME_PLAYING,
  GAME_DESTROYED
};

// ============================================================
// Global game data
// ============================================================

Player player;
Asteroid asteroids[MAX_ASTEROIDS];
Projectile projectiles[MAX_PROJECTILES];
Particle particles[MAX_PARTICLES];
Star stars[STAR_COUNT];
Difficulty difficulty;
DebouncedSwitch leftSwitch;
DebouncedSwitch rightSwitch;
GameState gameState = GAME_BOOT;
bool fireButtonHeld = false;

static_assert(sizeof(RenderCommand) * MAX_RENDER_COMMANDS * 2 <= GameRenderMemory::CAPACITY,
              "render commands exceed shared memory");
RenderCommand *const previousCommands = reinterpret_cast<RenderCommand *>(GameRenderMemory::bytes);
RenderCommand *const currentCommands = previousCommands + MAX_RENDER_COMMANDS;
int previousCommandCount = 0;
int currentCommandCount = 0;

uint32_t score = 0;
float distanceTravelled = 0.0f;
uint32_t damageFlashUntilMs = 0;
float cameraShakeStrength = 0.0f;
float renderShakeX = 0.0f;
float renderShakeY = 0.0f;

bool titleLaunchArmed = false;
bool restartArmed = false;
uint32_t startingStartedAtMs = 0;
uint32_t destroyedAtMs = 0;
bool destroyedScreenShown = false;

uint32_t lastRenderMs = 0;
uint32_t lastLoopMicros = 0;
float physicsAccumulator = 0.0f;

uint32_t performanceWindowStartedMs = 0;
uint32_t performancePhysicsTicks = 0;
uint32_t performanceRenderFrames = 0;

int lastHudShield = -1;
uint32_t lastHudScore = 0xFFFFFFFFu;
int lastHudSector = -1;
int lastHudDistanceMeters = -1;
uint32_t lastHudDrawMs = 0;

ControlAction lastDebugAction = ACTION_NONE;
SwitchState lastDebugLeft = SWITCH_ERROR;
SwitchState lastDebugRight = SWITCH_ERROR;

// ============================================================
// Icosahedron source topology
// ============================================================

const uint8_t ASTEROID_EDGES[ASTEROID_EDGE_COUNT][2] = {
  {0,1}, {0,5}, {0,7}, {0,10}, {0,11},
  {1,5}, {1,7}, {1,8}, {1,9},
  {2,3}, {2,4}, {2,6}, {2,10}, {2,11},
  {3,4}, {3,6}, {3,8}, {3,9},
  {4,5}, {4,9}, {4,11},
  {5,9}, {5,11},
  {6,7}, {6,8}, {6,10},
  {7,8}, {7,10},
  {8,9}, {10,11}
};

// ============================================================
// Forward declarations
// ============================================================

void setupHardware();
void setupDisplay();
void setupInputs();
void initializeGame();
void resetGame();
void startGame(uint32_t nowMs);
void updateGame(float dt, uint32_t nowMs);
void updateTitle(uint32_t nowMs);
void updateStarting(uint32_t nowMs);
void updatePlaying(float dt, uint32_t nowMs);
void updateDestroyed(float dt, uint32_t nowMs);

SwitchState readSwitch(int upPin, int downPin);
void initSwitchInput(DebouncedSwitch& sw, int upPin, int downPin, uint32_t nowMs);
void updateSwitchInput(DebouncedSwitch& sw, int upPin, int downPin, uint32_t nowMs);
void updateInputs(uint32_t nowMs);
ControlAction decodeControls(SwitchState left, SwitchState right);

Vec3 vecAdd(Vec3 a, Vec3 b);
Vec3 vecSub(Vec3 a, Vec3 b);
Vec3 vecScale(Vec3 v, float s);
float vecDot(Vec3 a, Vec3 b);
Vec3 vecCross(Vec3 a, Vec3 b);
float vecLengthSquared(Vec3 v);
float vecLength(Vec3 v);
Vec3 vecNormalize(Vec3 v);
Quaternion quatIdentity();
Quaternion quatMultiply(Quaternion a, Quaternion b);
Quaternion quatConjugate(Quaternion q);
Quaternion quatNormalize(Quaternion q);
Quaternion quatFromAxisAngle(Vec3 axis, float radians);
Vec3 quatRotateVector(Quaternion q, Vec3 v);
float approach(float current, float target, float maxChange);

Vec3 worldToCamera(Vec3 worldPosition);
bool clipLineToNearPlane(Vec3& a, Vec3& b, float nearZ = NEAR_CLIP);
bool projectCameraPoint(Vec3 cameraPoint, ScreenPoint& result);
bool clipScreenLineToViewport(int& x0, int& y0, int& x1, int& y1);

void updatePlayerAngularVelocity(ControlAction action, float dt);
void integratePlayerRotation(float dt);
void updatePlayerPosition(float dt);

void tryFireWeapon(uint32_t nowMs);
bool spawnProjectile();
void updateProjectiles(float dt);

void clearAsteroids();
bool spawnAsteroidAhead(float minDistance, float maxDistance);
void generateAsteroidGeometry(Asteroid& asteroid);
void updateAsteroids(float dt);
void updateAsteroidRotation(Asteroid& asteroid, float dt);
void maintainAsteroidPopulation();
void cleanupAsteroids();
void destroyAsteroid(int index);
void fragmentAsteroid(const Asteroid& parent);
int asteroidHealthForRadius(float radius);

bool spheresOverlap(Vec3 aPos, float aRadius, Vec3 bPos, float bRadius);
void handleProjectileAsteroidCollisions();
void handlePlayerAsteroidCollisions(uint32_t nowMs);

void spawnExplosion(Vec3 position, int count, float force, uint16_t color = COLOR_PARTICLE);
void updateParticles(float dt);
void updateDifficulty();
void rebaseWorldIfNecessary();

void renderFrame(uint32_t nowMs);
void erasePreviousFrame();
void buildSceneCommands(uint32_t nowMs);
void renderStars();
void renderAsteroids();
void renderAsteroidsOfLOD(AsteroidLOD requestedLod);
void renderAsteroid(const Asteroid& asteroid);
void renderAsteroidWithLOD(const Asteroid& asteroid, AsteroidLOD lod);
AsteroidLOD chooseAsteroidLOD(float cameraDistance, float projectedRadius);
void renderProjectiles();
void renderProjectile(const Projectile& projectile);
void renderParticles();
void renderDamageBorder(uint32_t nowMs);
void renderCrosshair();
void renderHud(uint32_t nowMs, bool force = false);
void renderTitleScreen();
void renderStartingScreen(uint32_t nowMs);
void renderDestroyedScreen();
bool queuePixel(int x, int y, uint16_t color);
bool queueLine(int x1, int y1, int x2, int y2, uint16_t color);
void drawCurrentCommands();
void finishDynamicFrame();

float randomFloat(float minValue, float maxValue);
uint32_t seededNext(uint32_t& state);
float seededFloat(uint32_t& state, float minValue, float maxValue);
const char* switchStateName(SwitchState state);
const char* actionName(ControlAction action);
void updatePerformanceDebug(uint32_t nowMs);
void invalidateHud();

// ============================================================
// Arduino entry points
// ============================================================

void enter() {
  setupHardware();
  initializeGame();

  lastLoopMicros = micros();
  lastRenderMs = millis();
  performanceWindowStartedMs = millis();
}

void tick(bool shootHeld) {
  fireButtonHeld = shootHeld;
  const uint32_t nowMs = millis();
  const uint32_t nowMicros = micros();

  updateInputs(nowMs);

  uint32_t elapsedMicros = nowMicros - lastLoopMicros;
  lastLoopMicros = nowMicros;

  // Avoid an enormous catch-up after a debugger halt or temporary stall.
  if (elapsedMicros > 250000u) {
    elapsedMicros = 250000u;
  }

  physicsAccumulator += (float)elapsedMicros * 0.000001f;

  int steps = 0;
  while (physicsAccumulator >= FIXED_DT && steps < MAX_PHYSICS_CATCHUP_STEPS) {
    updateGame(FIXED_DT, nowMs);
    physicsAccumulator -= FIXED_DT;
    ++steps;
    ++performancePhysicsTicks;
  }

  if (steps == MAX_PHYSICS_CATCHUP_STEPS && physicsAccumulator >= FIXED_DT) {
    // Discard excessive backlog rather than entering a catch-up spiral.
    physicsAccumulator = fmodf(physicsAccumulator, FIXED_DT);
  }

  if ((uint32_t)(nowMs - lastRenderMs) >= RENDER_INTERVAL_MS) {
    lastRenderMs = nowMs;
    renderFrame(nowMs);
    ++performanceRenderFrames;
  }

  updatePerformanceDebug(nowMs);
}

// ============================================================
// Hardware
// ============================================================

void setupHardware() {
  Serial.begin(115200);
  setupInputs();
  setupDisplay();
}

void setupDisplay() {
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  display.init(240, 320);
  display.setRotation(DISPLAY_ROTATION);
  display.fillScreen(COLOR_BACKGROUND);

  if (display.width() != SCREEN_WIDTH || display.height() != SCREEN_HEIGHT) {
    Serial.println("DISPLAY ROTATION ERROR: expected 240x320 logical size");
    display.setTextColor(ST77XX_RED, ST77XX_BLACK);
    display.setTextSize(1);
    display.setCursor(8, 8);
    display.print("ROTATION ERROR");
  }
}

void setupInputs() {
  pinMode(LEFT_UP_PIN, INPUT_PULLUP);
  pinMode(LEFT_DOWN_PIN, INPUT_PULLUP);
  pinMode(RIGHT_UP_PIN, INPUT_PULLUP);
  pinMode(RIGHT_DOWN_PIN, INPUT_PULLUP);
}

// ============================================================
// Input and debounce
// ============================================================

SwitchState readSwitch(int upPin, int downPin) {
  const bool up = digitalRead(upPin) == LOW;
  const bool down = digitalRead(downPin) == LOW;

  if (up && !down) return SWITCH_UP;
  if (!up && down) return SWITCH_DOWN;
  if (!up && !down) return SWITCH_CENTER;
  return SWITCH_ERROR;
}

void initSwitchInput(DebouncedSwitch& sw, int upPin, int downPin, uint32_t nowMs) {
  SwitchState initial = readSwitch(upPin, downPin);
  sw.rawState = initial;
  sw.stableState = (initial == SWITCH_ERROR) ? SWITCH_CENTER : initial;
  sw.rawChangedAt = nowMs;
  sw.fault = false;
}

void updateSwitchInput(DebouncedSwitch& sw, int upPin, int downPin, uint32_t nowMs) {
  const SwitchState currentRaw = readSwitch(upPin, downPin);

  if (currentRaw != sw.rawState) {
    sw.rawState = currentRaw;
    sw.rawChangedAt = nowMs;
  }

  const uint32_t stableFor = nowMs - sw.rawChangedAt;

  if (sw.rawState == SWITCH_ERROR) {
    // A brief both-low state can occur mechanically while crossing detents.
    // Keep the last valid stable state until the fault persists.
    if (stableFor >= SWITCH_ERROR_MS) {
      sw.stableState = SWITCH_CENTER;
      sw.fault = true;
    }
    return;
  }

  if (stableFor >= DEBOUNCE_MS) {
    sw.stableState = sw.rawState;
    sw.fault = false;
  }
}

void updateInputs(uint32_t nowMs) {
  updateSwitchInput(leftSwitch, LEFT_UP_PIN, LEFT_DOWN_PIN, nowMs);
  updateSwitchInput(rightSwitch, RIGHT_UP_PIN, RIGHT_DOWN_PIN, nowMs);

#if DEBUG_INPUT
  const ControlAction action = decodeControls(leftSwitch.stableState, rightSwitch.stableState);
  if (leftSwitch.stableState != lastDebugLeft ||
      rightSwitch.stableState != lastDebugRight ||
      action != lastDebugAction) {
    Serial.print("LEFT ");
    Serial.print(switchStateName(leftSwitch.stableState));
    Serial.print("  RIGHT ");
    Serial.print(switchStateName(rightSwitch.stableState));
    Serial.print("  ACTION ");
    Serial.println(actionName(action));
    lastDebugLeft = leftSwitch.stableState;
    lastDebugRight = rightSwitch.stableState;
    lastDebugAction = action;
  }
#endif
}

ControlAction decodeControls(SwitchState left, SwitchState right) {
  if (left == SWITCH_ERROR) left = SWITCH_CENTER;
  if (right == SWITCH_ERROR) right = SWITCH_CENTER;

  if (left == SWITCH_CENTER && right == SWITCH_CENTER) return ACTION_NONE;

  // REVERSED
  if (left == SWITCH_UP   && right == SWITCH_UP)       return ACTION_PITCH_DOWN;
  if (left == SWITCH_DOWN && right == SWITCH_DOWN)     return ACTION_PITCH_UP;

  if (left == SWITCH_UP   && right == SWITCH_DOWN)     return ACTION_ROLL_RIGHT;
  if (left == SWITCH_DOWN && right == SWITCH_UP)       return ACTION_ROLL_LEFT;

  if (left == SWITCH_UP     && right == SWITCH_CENTER) return ACTION_YAW_RIGHT;
  if (left == SWITCH_CENTER && right == SWITCH_UP)     return ACTION_YAW_LEFT;

  // A switch pulled back while the other is centered turns the ship.
  if (left == SWITCH_DOWN   && right == SWITCH_CENTER) return ACTION_YAW_LEFT;
  if (left == SWITCH_CENTER && right == SWITCH_DOWN)   return ACTION_YAW_RIGHT;

  return ACTION_NONE;
}
// ============================================================
// Math
// ============================================================

Vec3 vecAdd(Vec3 a, Vec3 b) {
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 vecSub(Vec3 a, Vec3 b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 vecScale(Vec3 v, float s) {
  return {v.x * s, v.y * s, v.z * s};
}

float vecDot(Vec3 a, Vec3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 vecCross(Vec3 a, Vec3 b) {
  return {
    a.y * b.z - a.z * b.y,
    a.z * b.x - a.x * b.z,
    a.x * b.y - a.y * b.x
  };
}

float vecLengthSquared(Vec3 v) {
  return vecDot(v, v);
}

float vecLength(Vec3 v) {
  return sqrtf(vecLengthSquared(v));
}

Vec3 vecNormalize(Vec3 v) {
  const float lenSq = vecLengthSquared(v);
  if (lenSq <= 1.0e-12f) return {0.0f, 0.0f, 0.0f};
  const float invLen = 1.0f / sqrtf(lenSq);
  return vecScale(v, invLen);
}

Quaternion quatIdentity() {
  return {1.0f, 0.0f, 0.0f, 0.0f};
}

Quaternion quatMultiply(Quaternion a, Quaternion b) {
  return {
    a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z,
    a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
    a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
    a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w
  };
}

Quaternion quatConjugate(Quaternion q) {
  return {q.w, -q.x, -q.y, -q.z};
}

Quaternion quatNormalize(Quaternion q) {
  const float lenSq = q.w*q.w + q.x*q.x + q.y*q.y + q.z*q.z;
  if (lenSq <= 1.0e-12f) return quatIdentity();
  const float invLen = 1.0f / sqrtf(lenSq);
  return {q.w*invLen, q.x*invLen, q.y*invLen, q.z*invLen};
}

Quaternion quatFromAxisAngle(Vec3 axis, float radians) {
  axis = vecNormalize(axis);
  const float half = radians * 0.5f;
  const float s = sinf(half);
  return quatNormalize({cosf(half), axis.x*s, axis.y*s, axis.z*s});
}

Vec3 quatRotateVector(Quaternion q, Vec3 v) {
  // Optimized q * v * q^-1 form.
  const Vec3 qv = {q.x, q.y, q.z};
  const Vec3 t = vecScale(vecCross(qv, v), 2.0f);
  return vecAdd(v, vecAdd(vecScale(t, q.w), vecCross(qv, t)));
}

float approach(float current, float target, float maxChange) {
  if (current < target) {
    current += maxChange;
    if (current > target) current = target;
  } else if (current > target) {
    current -= maxChange;
    if (current < target) current = target;
  }
  return current;
}

// ============================================================
// Camera and projection
// ============================================================

Vec3 worldToCamera(Vec3 worldPosition) {
  const Vec3 relative = vecSub(worldPosition, player.position);
  return quatRotateVector(quatConjugate(player.orientation), relative);
}

bool clipLineToNearPlane(Vec3& a, Vec3& b, float nearZ) {
  const bool aBehind = a.z <= nearZ;
  const bool bBehind = b.z <= nearZ;

  if (aBehind && bBehind) return false;
  if (!aBehind && !bBehind) return true;

  const float dz = b.z - a.z;
  if (fabsf(dz) < 1.0e-8f) return false;

  const float t = (nearZ - a.z) / dz;
  const Vec3 hit = {
    a.x + (b.x - a.x) * t,
    a.y + (b.y - a.y) * t,
    nearZ
  };

  if (aBehind) a = hit;
  else b = hit;
  return true;
}
 
bool projectCameraPoint(Vec3 cameraPoint, ScreenPoint& result) {
  if (cameraPoint.z <= NEAR_CLIP) {
    result = {0, 0, false};
    return false;
  }

  const float invZ = 1.0f / cameraPoint.z;
  const float sx = (float)CENTER_X + cameraPoint.x * invZ * FOCAL_LENGTH + renderShakeX;
  const float sy = (float)CENTER_Y - cameraPoint.y * invZ * FOCAL_LENGTH + renderShakeY;

  // Keep values bounded before integer conversion.
  const float clampedX = fmaxf(-32760.0f, fminf(32760.0f, sx));
  const float clampedY = fmaxf(-32760.0f, fminf(32760.0f, sy));

  result.x = (int16_t)lroundf(clampedX);
  result.y = (int16_t)lroundf(clampedY);
  result.visible = result.x >= VIEWPORT_X && result.x <= VIEWPORT_MAX_X &&
                   result.y >= VIEWPORT_Y && result.y <= VIEWPORT_MAX_Y;
  return true;
}

static int screenOutCode(float x, float y) {
  int code = 0;
  if (x < VIEWPORT_X) code |= 1;
  else if (x > VIEWPORT_MAX_X) code |= 2;
  if (y < VIEWPORT_Y) code |= 4;
  else if (y > VIEWPORT_MAX_Y) code |= 8;
  return code;
}

bool clipScreenLineToViewport(int& x0, int& y0, int& x1, int& y1) {
  float ax = (float)x0;
  float ay = (float)y0;
  float bx = (float)x1;
  float by = (float)y1;

  int outA = screenOutCode(ax, ay);
  int outB = screenOutCode(bx, by);

  while (true) {
    if ((outA | outB) == 0) {
      x0 = (int)lroundf(ax);
      y0 = (int)lroundf(ay);
      x1 = (int)lroundf(bx);
      y1 = (int)lroundf(by);
      return true;
    }

    if (outA & outB) return false;

    const int out = outA ? outA : outB;
    float x = 0.0f;
    float y = 0.0f;

    if (out & 8) { // bottom
      const float denom = by - ay;
      if (fabsf(denom) < 1.0e-8f) return false;
      x = ax + (bx - ax) * ((float)VIEWPORT_MAX_Y - ay) / denom;
      y = (float)VIEWPORT_MAX_Y;
    } else if (out & 4) { // top
      const float denom = by - ay;
      if (fabsf(denom) < 1.0e-8f) return false;
      x = ax + (bx - ax) * ((float)VIEWPORT_Y - ay) / denom;
      y = (float)VIEWPORT_Y;
    } else if (out & 2) { // right
      const float denom = bx - ax;
      if (fabsf(denom) < 1.0e-8f) return false;
      y = ay + (by - ay) * ((float)VIEWPORT_MAX_X - ax) / denom;
      x = (float)VIEWPORT_MAX_X;
    } else { // left
      const float denom = bx - ax;
      if (fabsf(denom) < 1.0e-8f) return false;
      y = ay + (by - ay) * ((float)VIEWPORT_X - ax) / denom;
      x = (float)VIEWPORT_X;
    }

    if (out == outA) {
      ax = x;
      ay = y;
      outA = screenOutCode(ax, ay);
    } else {
      bx = x;
      by = y;
      outB = screenOutCode(bx, by);
    }
  }
}

// ============================================================
// Game initialization / states
// ============================================================

void initializeGame() {
  const uint32_t nowMs = millis();
  initSwitchInput(leftSwitch, LEFT_UP_PIN, LEFT_DOWN_PIN, nowMs);
  initSwitchInput(rightSwitch, RIGHT_UP_PIN, RIGHT_DOWN_PIN, nowMs);

  // Generate a fixed celestial sphere once. Translation never moves it.
  for (int i = 0; i < STAR_COUNT; ++i) {
    Vec3 v;
    do {
      v = {
        randomFloat(-1.0f, 1.0f),
        randomFloat(-1.0f, 1.0f),
        randomFloat(-1.0f, 1.0f)
      };
    } while (vecLengthSquared(v) < 0.05f || vecLengthSquared(v) > 1.0f);
    stars[i].direction = vecNormalize(v);
    stars[i].brightness = (uint8_t)randomFloat(110.0f, 255.0f);
  }

  resetGame();
  gameState = GAME_TITLE;
  titleLaunchArmed = false;
  restartArmed = false;
}

void resetGame() {
  player.position = {0.0f, 0.0f, 0.0f};
  player.orientation = quatIdentity();
  player.angularVelocity = {0.0f, 0.0f, 0.0f};
  player.forwardSpeed = PLAYER_BASE_SPEED;
  player.shield = 100;
  player.lastShotTimeMs = millis() - SHOT_COOLDOWN_MS;
  player.invulnerableUntilMs = 0;

  score = 0;
  distanceTravelled = 0.0f;
  damageFlashUntilMs = 0;
  cameraShakeStrength = 0.0f;

  difficulty.sector = 1;
  difficulty.targetAsteroidCount = 18;
  difficulty.asteroidSpeedMultiplier = 1.0f;
  difficulty.sizeMultiplier = 1.0f;
  difficulty.playerSpeedMultiplier = 1.0f;

  clearAsteroids();
  for (int i = 0; i < MAX_PROJECTILES; ++i) projectiles[i].active = false;
  for (int i = 0; i < MAX_PARTICLES; ++i) particles[i].active = false;

  previousCommandCount = 0;
  currentCommandCount = 0;
  destroyedScreenShown = false;
  invalidateHud();
}

void startGame(uint32_t nowMs) {
  resetGame();
  startingStartedAtMs = nowMs;
  gameState = GAME_STARTING;
}

void updateGame(float dt, uint32_t nowMs) {
  switch (gameState) {
    case GAME_BOOT:
      gameState = GAME_TITLE;
      break;
    case GAME_TITLE:
      updateTitle(nowMs);
      break;
    case GAME_STARTING:
      updateStarting(nowMs);
      break;
    case GAME_PLAYING:
      updatePlaying(dt, nowMs);
      break;
    case GAME_DESTROYED:
      updateDestroyed(dt, nowMs);
      break;
  }
}

void updateTitle(uint32_t nowMs) {
  const SwitchState left = leftSwitch.stableState;
  const SwitchState right = rightSwitch.stableState;

  if (!titleLaunchArmed && left == SWITCH_UP && right == SWITCH_UP) {
    titleLaunchArmed = true;
  }

  // Require neutral after the launch command so UP+UP cannot carry into
  // flight as an immediate pitch-up command.
  if (titleLaunchArmed && left == SWITCH_CENTER && right == SWITCH_CENTER) {
    titleLaunchArmed = false;
    startGame(nowMs);
  }
}

void updateStarting(uint32_t nowMs) {
  if ((uint32_t)(nowMs - startingStartedAtMs) >= STARTING_MS) {
    gameState = GAME_PLAYING;
    // Fill the initial local field immediately rather than waiting for
    // several visible frames of empty space.
    updateDifficulty();
    maintainAsteroidPopulation();
  }
}

void updatePlaying(float dt, uint32_t nowMs) {
  const ControlAction action = decodeControls(leftSwitch.stableState, rightSwitch.stableState);

  updatePlayerAngularVelocity(action, dt);
  integratePlayerRotation(dt);
  updatePlayerPosition(dt);

  if (fireButtonHeld) {
    tryFireWeapon(nowMs);
  }

  updateProjectiles(dt);
  updateAsteroids(dt);
  updateParticles(dt);

  handleProjectileAsteroidCollisions();
  handlePlayerAsteroidCollisions(nowMs);

  distanceTravelled += player.forwardSpeed * dt;
  updateDifficulty();
  cleanupAsteroids();
  maintainAsteroidPopulation();
  rebaseWorldIfNecessary();

  cameraShakeStrength = approach(cameraShakeStrength, 0.0f, 9.0f * dt);

  if (player.shield <= 0) {
    player.shield = 0;
    gameState = GAME_DESTROYED;
    destroyedAtMs = nowMs;
    destroyedScreenShown = false;
    restartArmed = false;
    spawnExplosion(player.position, 24, 12.0f, COLOR_DAMAGE);
  }
}

void updateDestroyed(float dt, uint32_t nowMs) {
  if ((uint32_t)(nowMs - destroyedAtMs) < DESTROYED_DRIFT_MS) {
    // No new controls. Existing angular motion damps while the field and
    // debris continue briefly, creating a visible destruction drift.
    player.angularVelocity.x = approach(player.angularVelocity.x, 0.0f, ANGULAR_DAMPING * 0.45f * dt);
    player.angularVelocity.y = approach(player.angularVelocity.y, 0.0f, ANGULAR_DAMPING * 0.45f * dt);
    player.angularVelocity.z = approach(player.angularVelocity.z, 0.0f, ANGULAR_DAMPING * 0.45f * dt);
    integratePlayerRotation(dt);
    updateAsteroids(dt);
    updateProjectiles(dt);
    updateParticles(dt);
    cameraShakeStrength = approach(cameraShakeStrength, 0.0f, 7.0f * dt);
  }

  // Do not accept a restart until the destruction drift has completed and
  // the final-result phase is available.
  if ((uint32_t)(nowMs - destroyedAtMs) >= DESTROYED_DRIFT_MS) {
    const SwitchState left = leftSwitch.stableState;
    const SwitchState right = rightSwitch.stableState;

    if (!restartArmed && left == SWITCH_UP && right == SWITCH_UP) {
      restartArmed = true;
    }

    if (restartArmed && left == SWITCH_CENTER && right == SWITCH_CENTER) {
      restartArmed = false;
      startGame(nowMs);
    }
  }
}

// ============================================================
// Player flight
// ============================================================

void updatePlayerAngularVelocity(ControlAction action, float dt) {
  float targetPitch = 0.0f;
  float targetYaw = 0.0f;
  float targetRoll = 0.0f;

  switch (action) {
    case ACTION_PITCH_UP:   targetPitch =  MAX_PITCH_RATE; break;
    case ACTION_PITCH_DOWN: targetPitch = -MAX_PITCH_RATE; break;
    case ACTION_YAW_LEFT:   targetYaw   = -MAX_YAW_RATE; break;
    case ACTION_YAW_RIGHT:  targetYaw   =  MAX_YAW_RATE; break;
    case ACTION_ROLL_LEFT:  targetRoll  = -MAX_ROLL_RATE; break;
    case ACTION_ROLL_RIGHT: targetRoll  =  MAX_ROLL_RATE; break;
    default: break;
  }

  const float pitchRate = (targetPitch == 0.0f ? ANGULAR_DAMPING : ANGULAR_ACCELERATION) * dt;
  const float yawRate   = (targetYaw   == 0.0f ? ANGULAR_DAMPING : ANGULAR_ACCELERATION) * dt;
  const float rollRate  = (targetRoll  == 0.0f ? ANGULAR_DAMPING : ANGULAR_ACCELERATION) * dt;

  player.angularVelocity.x = approach(player.angularVelocity.x, targetPitch, pitchRate);
  player.angularVelocity.y = approach(player.angularVelocity.y, targetYaw, yawRate);
  player.angularVelocity.z = approach(player.angularVelocity.z, targetRoll, rollRate);
}

void integratePlayerRotation(float dt) {
  Quaternion q = player.orientation;

  // Rotate about the spacecraft's current local axes expressed in world space.
  if (fabsf(player.angularVelocity.x) > 1.0e-5f) {
    const Vec3 right = quatRotateVector(q, {1.0f, 0.0f, 0.0f});
    const Quaternion dq = quatFromAxisAngle(right, player.angularVelocity.x * dt * PITCH_ROTATION_SIGN);
    q = quatMultiply(dq, q);
  }

  if (fabsf(player.angularVelocity.y) > 1.0e-5f) {
    const Vec3 up = quatRotateVector(q, {0.0f, 1.0f, 0.0f});
    const Quaternion dq = quatFromAxisAngle(up, player.angularVelocity.y * dt * YAW_ROTATION_SIGN);
    q = quatMultiply(dq, q);
  }

  if (fabsf(player.angularVelocity.z) > 1.0e-5f) {
    const Vec3 forward = quatRotateVector(q, {0.0f, 0.0f, 1.0f});
    const Quaternion dq = quatFromAxisAngle(forward, player.angularVelocity.z * dt * ROLL_ROTATION_SIGN);
    q = quatMultiply(dq, q);
  }

  player.orientation = quatNormalize(q);
}

void updatePlayerPosition(float dt) {
  const Vec3 forward = quatRotateVector(player.orientation, {0.0f, 0.0f, 1.0f});
  player.position = vecAdd(player.position, vecScale(forward, player.forwardSpeed * dt));
}

// ============================================================
// Weapon / projectile system
// ============================================================

void tryFireWeapon(uint32_t nowMs) {
  if ((uint32_t)(nowMs - player.lastShotTimeMs) < SHOT_COOLDOWN_MS) return;

  if (spawnProjectile()) {
    player.lastShotTimeMs = nowMs;
  }
}

bool spawnProjectile() {
  for (int i = 0; i < MAX_PROJECTILES; ++i) {
    if (projectiles[i].active) continue;

    const Vec3 forward = quatRotateVector(player.orientation, {0.0f, 0.0f, 1.0f});
    projectiles[i].active = true;
    projectiles[i].position = vecAdd(player.position, vecScale(forward, 2.0f));
    projectiles[i].velocity = vecScale(forward, PROJECTILE_SPEED);
    projectiles[i].radius = PROJECTILE_RADIUS;
    projectiles[i].remainingLife = PROJECTILE_LIFE;
    return true;
  }
  return false;
}

void updateProjectiles(float dt) {
  for (int i = 0; i < MAX_PROJECTILES; ++i) {
    Projectile& p = projectiles[i];
    if (!p.active) continue;

    p.position = vecAdd(p.position, vecScale(p.velocity, dt));
    p.remainingLife -= dt;
    if (p.remainingLife <= 0.0f) p.active = false;
  }
}

// ============================================================
// Asteroid system
// ============================================================

void clearAsteroids() {
  for (int i = 0; i < MAX_ASTEROIDS; ++i) asteroids[i].active = false;
}

int asteroidHealthForRadius(float radius) {
  if (radius < 3.8f) return 1;
  if (radius < 5.6f) return 2;
  return 3;
}

bool spawnAsteroidAhead(float minDistance, float maxDistance) {
  int slot = -1;
  for (int i = 0; i < MAX_ASTEROIDS; ++i) {
    if (!asteroids[i].active) {
      slot = i;
      break;
    }
  }
  if (slot < 0) return false;

  const Vec3 forward = quatRotateVector(player.orientation, {0.0f, 0.0f, 1.0f});
  const Vec3 right   = quatRotateVector(player.orientation, {1.0f, 0.0f, 0.0f});
  const Vec3 up      = quatRotateVector(player.orientation, {0.0f, 1.0f, 0.0f});

  for (int attempt = 0; attempt < SPAWN_ATTEMPTS; ++attempt) {
    const float distance = randomFloat(minDistance, maxDistance);
    const float spread = 0.42f * distance;
    const float horizontal = randomFloat(-spread, spread);
    const float vertical = randomFloat(-spread, spread);
    const float radius = randomFloat(2.3f, 6.7f) * difficulty.sizeMultiplier;

    // Nearer spawns that are exactly on the flight axis are rejected so a
    // newly-created body is never an immediate unavoidable collision.
    const float lateralSq = horizontal*horizontal + vertical*vertical;
    const float safeAxisRadius = radius + PLAYER_RADIUS + 3.0f;
    if (distance < 140.0f && lateralSq < safeAxisRadius * safeAxisRadius) {
      continue;
    }

    Vec3 position = player.position;
    position = vecAdd(position, vecScale(forward, distance));
    position = vecAdd(position, vecScale(right, horizontal));
    position = vecAdd(position, vecScale(up, vertical));

    bool overlapsBadly = false;
    for (int j = 0; j < MAX_ASTEROIDS; ++j) {
      if (!asteroids[j].active) continue;
      const float minSeparation = (radius + asteroids[j].radius) * 0.80f;
      if (vecLengthSquared(vecSub(position, asteroids[j].position)) < minSeparation * minSeparation) {
        overlapsBadly = true;
        break;
      }
    }
    if (overlapsBadly) continue;

    Asteroid& a = asteroids[slot];
    a.active = true;
    a.position = position;

    const float drift = 1.5f * difficulty.asteroidSpeedMultiplier;
    a.velocity = vecAdd(
      vecScale(right, randomFloat(-drift, drift)),
      vecScale(up, randomFloat(-drift, drift))
    );
    a.velocity = vecAdd(a.velocity, vecScale(forward, randomFloat(-0.5f, 0.6f) * difficulty.asteroidSpeedMultiplier));

    a.orientation = quatNormalize(quatMultiply(
      quatFromAxisAngle(vecNormalize({randomFloat(-1.0f, 1.0f), randomFloat(-1.0f, 1.0f), randomFloat(-1.0f, 1.0f)}), randomFloat(0.0f, 6.2831853f)),
      quatIdentity()
    ));
    a.angularVelocity = {
      randomFloat(-0.7f, 0.7f),
      randomFloat(-0.7f, 0.7f),
      randomFloat(-0.7f, 0.7f)
    };
    a.radius = radius;
    a.health = asteroidHealthForRadius(radius);
    a.seed = esp_random();
    generateAsteroidGeometry(a);
    return true;
  }

  return false;
}

void generateAsteroidGeometry(Asteroid& asteroid) {
  constexpr float PHI = 1.61803398875f;
  const Vec3 base[ASTEROID_VERTEX_COUNT] = {
    {-1, PHI, 0}, {1, PHI, 0}, {-1, -PHI, 0}, {1, -PHI, 0},
    {0, -1, PHI}, {0, 1, PHI}, {0, -1, -PHI}, {0, 1, -PHI},
    {PHI, 0, -1}, {PHI, 0, 1}, {-PHI, 0, -1}, {-PHI, 0, 1}
  };

  uint32_t rng = asteroid.seed ? asteroid.seed : 0xA341316Cu;
  for (int i = 0; i < ASTEROID_VERTEX_COUNT; ++i) {
    Vec3 v = vecNormalize(base[i]);
    const float irregularity = seededFloat(rng, 0.75f, 1.25f);
    asteroid.vertices[i] = vecScale(v, asteroid.radius * irregularity);
  }
}

void updateAsteroidRotation(Asteroid& asteroid, float dt) {
  Quaternion q = asteroid.orientation;

  if (fabsf(asteroid.angularVelocity.x) > 1.0e-5f) {
    q = quatMultiply(q, quatFromAxisAngle({1,0,0}, asteroid.angularVelocity.x * dt));
  }
  if (fabsf(asteroid.angularVelocity.y) > 1.0e-5f) {
    q = quatMultiply(q, quatFromAxisAngle({0,1,0}, asteroid.angularVelocity.y * dt));
  }
  if (fabsf(asteroid.angularVelocity.z) > 1.0e-5f) {
    q = quatMultiply(q, quatFromAxisAngle({0,0,1}, asteroid.angularVelocity.z * dt));
  }

  asteroid.orientation = quatNormalize(q);
}

void updateAsteroids(float dt) {
  for (int i = 0; i < MAX_ASTEROIDS; ++i) {
    Asteroid& a = asteroids[i];
    if (!a.active) continue;
    a.position = vecAdd(a.position, vecScale(a.velocity, dt));
    updateAsteroidRotation(a, dt);
  }
}

void maintainAsteroidPopulation() {
  int activeCount = 0;
  for (int i = 0; i < MAX_ASTEROIDS; ++i) {
    if (asteroids[i].active) ++activeCount;
  }

  int attempts = 0;
  while (activeCount < difficulty.targetAsteroidCount && attempts < MAX_ASTEROIDS * 2) {
    if (spawnAsteroidAhead(100.0f, 220.0f)) ++activeCount;
    ++attempts;
  }
}

void cleanupAsteroids() {
  constexpr float MAX_WORLD_DISTANCE = 430.0f;
  constexpr float MAX_WORLD_DISTANCE_SQ = MAX_WORLD_DISTANCE * MAX_WORLD_DISTANCE;

  for (int i = 0; i < MAX_ASTEROIDS; ++i) {
    Asteroid& a = asteroids[i];
    if (!a.active) continue;

    const Vec3 camera = worldToCamera(a.position);
    const float playerDistSq = vecLengthSquared(vecSub(a.position, player.position));
    if (camera.z < -30.0f || playerDistSq > MAX_WORLD_DISTANCE_SQ) {
      a.active = false;
    }
  }
}

void destroyAsteroid(int index) {
  if (index < 0 || index >= MAX_ASTEROIDS) return;
  Asteroid& a = asteroids[index];
  if (!a.active) return;

  const Asteroid parent = a;
  a.active = false;

  if (parent.radius < 3.8f) score += 100;
  else if (parent.radius < 5.6f) score += 200;
  else score += 400;

  spawnExplosion(parent.position, parent.radius >= 5.6f ? 12 : 8, 7.5f, COLOR_PARTICLE);

  if (parent.radius >= 5.8f) {
    fragmentAsteroid(parent);
  }
}

void fragmentAsteroid(const Asteroid& parent) {
  int freeSlots = 0;
  for (int i = 0; i < MAX_ASTEROIDS; ++i) {
    if (!asteroids[i].active) ++freeSlots;
  }
  if (freeSlots < 2) return;

  const int childCount = 2;
  for (int child = 0; child < childCount; ++child) {
    int slot = -1;
    for (int i = 0; i < MAX_ASTEROIDS; ++i) {
      if (!asteroids[i].active) {
        slot = i;
        break;
      }
    }
    if (slot < 0) return;

    Asteroid& a = asteroids[slot];
    const float childScale = randomFloat(0.55f, 0.68f);
    Vec3 outward = vecNormalize({
      randomFloat(-1.0f, 1.0f),
      randomFloat(-1.0f, 1.0f),
      randomFloat(-1.0f, 1.0f)
    });
    if (vecLengthSquared(outward) < 0.1f) outward = {1.0f, 0.0f, 0.0f};

    a.active = true;
    a.radius = parent.radius * childScale;
    a.position = vecAdd(parent.position, vecScale(outward, a.radius * 0.35f));
    a.velocity = vecAdd(parent.velocity, vecScale(outward, randomFloat(3.0f, 6.0f)));
    a.orientation = parent.orientation;
    a.angularVelocity = {
      randomFloat(-1.1f, 1.1f),
      randomFloat(-1.1f, 1.1f),
      randomFloat(-1.1f, 1.1f)
    };
    a.health = asteroidHealthForRadius(a.radius);
    a.seed = esp_random();
    generateAsteroidGeometry(a);
  }
}

// ============================================================
// Collision
// ============================================================

bool spheresOverlap(Vec3 aPos, float aRadius, Vec3 bPos, float bRadius) {
  const Vec3 delta = vecSub(aPos, bPos);
  const float combined = aRadius + bRadius;
  return vecLengthSquared(delta) < combined * combined;
}

void handleProjectileAsteroidCollisions() {
  for (int p = 0; p < MAX_PROJECTILES; ++p) {
    if (!projectiles[p].active) continue;

    for (int a = 0; a < MAX_ASTEROIDS; ++a) {
      if (!asteroids[a].active) continue;

      if (spheresOverlap(projectiles[p].position, projectiles[p].radius,
                         asteroids[a].position, asteroids[a].radius)) {
        projectiles[p].active = false;
        --asteroids[a].health;
        spawnExplosion(projectiles[p].position, 4, 3.2f, COLOR_PROJECTILE);

        if (asteroids[a].health <= 0) {
          destroyAsteroid(a);
        }
        break; // one projectile can damage only one asteroid per update
      }
    }
  }
}

void handlePlayerAsteroidCollisions(uint32_t nowMs) {
  if ((int32_t)(nowMs - player.invulnerableUntilMs) < 0) return;

  for (int i = 0; i < MAX_ASTEROIDS; ++i) {
    Asteroid& a = asteroids[i];
    if (!a.active) continue;

    if (spheresOverlap(player.position, PLAYER_RADIUS, a.position, a.radius)) {
      int damage;
      if (a.radius < 3.8f) damage = 20;
      else if (a.radius < 5.6f) damage = 35;
      else damage = 50;

      player.shield -= damage;
      if (player.shield < 0) player.shield = 0;
      player.invulnerableUntilMs = nowMs + PLAYER_INVULNERABILITY_MS;
      damageFlashUntilMs = nowMs + DAMAGE_FLASH_MS;
      cameraShakeStrength = fmaxf(cameraShakeStrength, 3.5f + a.radius * 0.35f);

      spawnExplosion(a.position, 10, 8.0f, COLOR_DAMAGE);
      a.active = false;
      return;
    }
  }
}

// ============================================================
// Particles
// ============================================================

void spawnExplosion(Vec3 position, int count, float force, uint16_t color) {
  for (int n = 0; n < count; ++n) {
    int slot = -1;
    for (int i = 0; i < MAX_PARTICLES; ++i) {
      if (!particles[i].active) {
        slot = i;
        break;
      }
    }
    if (slot < 0) return;

    Vec3 dir = vecNormalize({
      randomFloat(-1.0f, 1.0f),
      randomFloat(-1.0f, 1.0f),
      randomFloat(-1.0f, 1.0f)
    });
    if (vecLengthSquared(dir) < 0.01f) dir = {1.0f, 0.0f, 0.0f};

    Particle& p = particles[slot];
    p.active = true;
    p.position = position;
    p.velocity = vecScale(dir, randomFloat(force * 0.35f, force));
    p.remainingLife = randomFloat(0.20f, 0.75f);
    p.color = color;
  }
}

void updateParticles(float dt) {
  for (int i = 0; i < MAX_PARTICLES; ++i) {
    Particle& p = particles[i];
    if (!p.active) continue;
    p.position = vecAdd(p.position, vecScale(p.velocity, dt));
    p.remainingLife -= dt;
    if (p.remainingLife <= 0.0f) p.active = false;
  }
}

// ============================================================
// Difficulty and world maintenance
// ============================================================

void updateDifficulty() {
  int sector = 1 + (int)floorf(distanceTravelled / 500.0f);
  if (sector < 1) sector = 1;
  if (sector > 12) sector = 12;

  difficulty.sector = sector;
  difficulty.targetAsteroidCount = 18 + (sector - 1) * 2;
  if (difficulty.targetAsteroidCount > MAX_ASTEROIDS) {
    difficulty.targetAsteroidCount = MAX_ASTEROIDS;
  }

  difficulty.asteroidSpeedMultiplier = 1.0f + 0.06f * (sector - 1);
  if (difficulty.asteroidSpeedMultiplier > 1.60f) difficulty.asteroidSpeedMultiplier = 1.60f;

  difficulty.sizeMultiplier = 1.0f + 0.025f * (sector - 1);
  if (difficulty.sizeMultiplier > 1.25f) difficulty.sizeMultiplier = 1.25f;

  difficulty.playerSpeedMultiplier = 1.0f + 0.035f * (sector - 1);
  if (difficulty.playerSpeedMultiplier > 1.35f) difficulty.playerSpeedMultiplier = 1.35f;

  player.forwardSpeed = PLAYER_BASE_SPEED * difficulty.playerSpeedMultiplier;
}

void rebaseWorldIfNecessary() {
  const float thresholdSq = WORLD_REBASE_DISTANCE * WORLD_REBASE_DISTANCE;
  if (vecLengthSquared(player.position) <= thresholdSq) return;

  const Vec3 offset = player.position;
  player.position = {0.0f, 0.0f, 0.0f};

  for (int i = 0; i < MAX_ASTEROIDS; ++i) {
    if (asteroids[i].active) asteroids[i].position = vecSub(asteroids[i].position, offset);
  }
  for (int i = 0; i < MAX_PROJECTILES; ++i) {
    if (projectiles[i].active) projectiles[i].position = vecSub(projectiles[i].position, offset);
  }
  for (int i = 0; i < MAX_PARTICLES; ++i) {
    if (particles[i].active) particles[i].position = vecSub(particles[i].position, offset);
  }
}

// ============================================================
// Renderer command queue
// ============================================================

bool queuePixel(int x, int y, uint16_t color) {
  if (currentCommandCount >= MAX_RENDER_COMMANDS) return false;
  if (x < VIEWPORT_X || x > VIEWPORT_MAX_X || y < VIEWPORT_Y || y > VIEWPORT_MAX_Y) return false;

  RenderCommand& c = currentCommands[currentCommandCount++];
  c.type = RENDER_PIXEL;
  c.x1 = (int16_t)x;
  c.y1 = (int16_t)y;
  c.x2 = c.x1;
  c.y2 = c.y1;
  c.color = color;
  return true;
}

bool queueLine(int x1, int y1, int x2, int y2, uint16_t color) {
  if (currentCommandCount >= MAX_RENDER_COMMANDS) return false;
  if (!clipScreenLineToViewport(x1, y1, x2, y2)) return false;

  RenderCommand& c = currentCommands[currentCommandCount++];
  c.type = RENDER_LINE;
  c.x1 = (int16_t)x1;
  c.y1 = (int16_t)y1;
  c.x2 = (int16_t)x2;
  c.y2 = (int16_t)y2;
  c.color = color;
  return true;
}

void erasePreviousFrame() {
  display.startWrite();
  for (int i = 0; i < previousCommandCount; ++i) {
    const RenderCommand& c = previousCommands[i];
    if (c.type == RENDER_PIXEL) {
      display.writePixel(c.x1, c.y1, COLOR_BACKGROUND);
    } else {
      display.writeLine(c.x1, c.y1, c.x2, c.y2, COLOR_BACKGROUND);
    }
  }
  display.endWrite();
}

void drawCurrentCommands() {
  display.startWrite();
  for (int i = 0; i < currentCommandCount; ++i) {
    const RenderCommand& c = currentCommands[i];
    if (c.type == RENDER_PIXEL) {
      display.writePixel(c.x1, c.y1, c.color);
    } else {
      display.writeLine(c.x1, c.y1, c.x2, c.y2, c.color);
    }
  }
  display.endWrite();
}

void finishDynamicFrame() {
  previousCommandCount = currentCommandCount;
  if (previousCommandCount > 0) {
    memcpy(previousCommands, currentCommands, sizeof(RenderCommand) * previousCommandCount);
  }
  currentCommandCount = 0;
}

// ============================================================
// 3D rendering
// ============================================================

void buildSceneCommands(uint32_t nowMs) {
  currentCommandCount = 0;

  if (cameraShakeStrength > 0.05f) {
    renderShakeX = randomFloat(-cameraShakeStrength, cameraShakeStrength);
    renderShakeY = randomFloat(-cameraShakeStrength, cameraShakeStrength);
  } else {
    renderShakeX = 0.0f;
    renderShakeY = 0.0f;
  }

  // Priority order: damage / projectiles / near asteroids / medium
  // asteroids / particles / stars / far asteroid detail.
  renderDamageBorder(nowMs);
  renderProjectiles();
  renderAsteroidsOfLOD(ASTEROID_LOD_NEAR);
  renderAsteroidsOfLOD(ASTEROID_LOD_MEDIUM);
  renderParticles();
  renderStars();
  renderAsteroidsOfLOD(ASTEROID_LOD_FAR);
}

void renderStars() {
  for (int i = 0; i < STAR_COUNT; ++i) {
    Vec3 cameraDir = quatRotateVector(quatConjugate(player.orientation), stars[i].direction);
    if (cameraDir.z <= 0.03f) continue;

    // Treat the unit direction as a point on a distant sphere; translation
    // is intentionally ignored so stars respond only to orientation.
    const Vec3 cameraPoint = vecScale(cameraDir, 100.0f);
    ScreenPoint s;
    if (!projectCameraPoint(cameraPoint, s) || !s.visible) continue;

    const uint16_t color = stars[i].brightness > 190 ? COLOR_STAR : COLOR_STAR_DIM;
    if (!queuePixel(s.x, s.y, color)) return;

    if (stars[i].brightness > 238 && currentCommandCount < MAX_RENDER_COMMANDS - 1) {
      queuePixel(s.x + 1, s.y, color);
    }
  }
}

AsteroidLOD chooseAsteroidLOD(float cameraDistance, float projectedRadius) {
  if (cameraDistance > 150.0f || projectedRadius < 2.0f) return ASTEROID_LOD_FAR;
  if (cameraDistance > 50.0f || projectedRadius < 8.0f) return ASTEROID_LOD_MEDIUM;
  return ASTEROID_LOD_NEAR;
}

void renderAsteroids() {
  renderAsteroidsOfLOD(ASTEROID_LOD_NEAR);
  renderAsteroidsOfLOD(ASTEROID_LOD_MEDIUM);
  renderAsteroidsOfLOD(ASTEROID_LOD_FAR);
}

void renderAsteroidsOfLOD(AsteroidLOD requestedLod) {
  for (int i = 0; i < MAX_ASTEROIDS; ++i) {
    const Asteroid& a = asteroids[i];
    if (!a.active) continue;

    const Vec3 centerCam = worldToCamera(a.position);
    if (centerCam.z < -a.radius) continue;

    const float distance = vecLength(centerCam);
    const float safeZ = fmaxf(centerCam.z, NEAR_CLIP);
    const float projectedRadius = a.radius / safeZ * FOCAL_LENGTH;
    const AsteroidLOD lod = chooseAsteroidLOD(distance, projectedRadius);
    if (lod != requestedLod) continue;

    renderAsteroidWithLOD(a, lod);
    if (currentCommandCount >= MAX_RENDER_COMMANDS) return;
  }
}

void renderAsteroid(const Asteroid& asteroid) {
  const Vec3 centerCam = worldToCamera(asteroid.position);
  const float distance = vecLength(centerCam);
  const float projectedRadius = asteroid.radius / fmaxf(centerCam.z, NEAR_CLIP) * FOCAL_LENGTH;
  renderAsteroidWithLOD(asteroid, chooseAsteroidLOD(distance, projectedRadius));
}

void renderAsteroidWithLOD(const Asteroid& asteroid, AsteroidLOD lod) {
  const Vec3 centerCam = worldToCamera(asteroid.position);

  if (lod == ASTEROID_LOD_FAR) {
    if (centerCam.z <= NEAR_CLIP) return;
    ScreenPoint center;
    if (!projectCameraPoint(centerCam, center) || !center.visible) return;

    const float apparent = asteroid.radius / centerCam.z * FOCAL_LENGTH;
    if (apparent < 1.6f) {
      queuePixel(center.x, center.y, COLOR_ASTEROID);
    } else {
      const int r = apparent > 4.0f ? 3 : 2;
      queueLine(center.x, center.y - r, center.x + r, center.y, COLOR_ASTEROID);
      queueLine(center.x + r, center.y, center.x, center.y + r, COLOR_ASTEROID);
      queueLine(center.x, center.y + r, center.x - r, center.y, COLOR_ASTEROID);
      queueLine(center.x - r, center.y, center.x, center.y - r, COLOR_ASTEROID);
    }
    return;
  }

  Vec3 cameraVertices[ASTEROID_VERTEX_COUNT];
  for (int i = 0; i < ASTEROID_VERTEX_COUNT; ++i) {
    const Vec3 rotated = quatRotateVector(asteroid.orientation, asteroid.vertices[i]);
    cameraVertices[i] = worldToCamera(vecAdd(asteroid.position, rotated));
  }

  const int edgeLimit = (lod == ASTEROID_LOD_MEDIUM) ? 15 : ASTEROID_EDGE_COUNT;
  for (int e = 0; e < edgeLimit; ++e) {
    Vec3 a = cameraVertices[ASTEROID_EDGES[e][0]];
    Vec3 b = cameraVertices[ASTEROID_EDGES[e][1]];

    if (!clipLineToNearPlane(a, b)) continue;

    ScreenPoint sa;
    ScreenPoint sb;
    if (!projectCameraPoint(a, sa) || !projectCameraPoint(b, sb)) continue;

    if (!queueLine(sa.x, sa.y, sb.x, sb.y, COLOR_ASTEROID) &&
        currentCommandCount >= MAX_RENDER_COMMANDS) {
      return;
    }
  }
}

void renderProjectile(const Projectile& projectile) {
  Vec3 a = worldToCamera(projectile.position);
  const Vec3 backWorld = vecSub(projectile.position, vecScale(vecNormalize(projectile.velocity), 2.0f));
  Vec3 b = worldToCamera(backWorld);

  if (!clipLineToNearPlane(a, b)) return;

  ScreenPoint sa;
  ScreenPoint sb;
  if (!projectCameraPoint(a, sa) || !projectCameraPoint(b, sb)) return;
  queueLine(sa.x, sa.y, sb.x, sb.y, COLOR_PROJECTILE);
}

void renderProjectiles() {
  for (int i = 0; i < MAX_PROJECTILES; ++i) {
    if (!projectiles[i].active) continue;
    renderProjectile(projectiles[i]);
    if (currentCommandCount >= MAX_RENDER_COMMANDS) return;
  }
}

void renderParticles() {
  for (int i = 0; i < MAX_PARTICLES; ++i) {
    const Particle& p = particles[i];
    if (!p.active) continue;

    ScreenPoint s;
    const Vec3 camera = worldToCamera(p.position);
    if (!projectCameraPoint(camera, s) || !s.visible) continue;
    if (!queuePixel(s.x, s.y, p.color)) return;
  }
}

void renderDamageBorder(uint32_t nowMs) {
  if ((int32_t)(damageFlashUntilMs - nowMs) <= 0) return;
  queueLine(0, 0, VIEWPORT_MAX_X, 0, COLOR_DAMAGE);
  queueLine(VIEWPORT_MAX_X, 0, VIEWPORT_MAX_X, VIEWPORT_MAX_Y, COLOR_DAMAGE);
  queueLine(VIEWPORT_MAX_X, VIEWPORT_MAX_Y, 0, VIEWPORT_MAX_Y, COLOR_DAMAGE);
  queueLine(0, VIEWPORT_MAX_Y, 0, 0, COLOR_DAMAGE);
}

void renderCrosshair() {
  // Draw directly after the command buffer so the crosshair always wins
  // visibility priority. It is stationary, so it does not need an erase
  // command; any world erase that crosses it is followed by this redraw.
  display.startWrite();
  display.writeLine(CENTER_X, CENTER_Y - 7, CENTER_X, CENTER_Y - 3, COLOR_CROSSHAIR);
  display.writeLine(CENTER_X, CENTER_Y + 3, CENTER_X, CENTER_Y + 7, COLOR_CROSSHAIR);
  display.writeLine(CENTER_X - 7, CENTER_Y, CENTER_X - 3, CENTER_Y, COLOR_CROSSHAIR);
  display.writeLine(CENTER_X + 3, CENTER_Y, CENTER_X + 7, CENTER_Y, COLOR_CROSSHAIR);
  display.endWrite();
}

// ============================================================
// HUD and state screens
// ============================================================

void invalidateHud() {
  lastHudShield = -1;
  lastHudScore = 0xFFFFFFFFu;
  lastHudSector = -1;
  lastHudDistanceMeters = -1;
  lastHudDrawMs = 0;
}

void renderHud(uint32_t nowMs, bool force) {
  const int meters = (int)distanceTravelled;
  const bool importantChange = player.shield != lastHudShield ||
                               score != lastHudScore ||
                               difficulty.sector != lastHudSector;
  const bool distanceDue = meters != lastHudDistanceMeters &&
                           (uint32_t)(nowMs - lastHudDrawMs) >= 200u;

  if (!force && !importantChange && !distanceDue) return;

  display.fillRect(0, HUD_Y, SCREEN_WIDTH, SCREEN_HEIGHT - HUD_Y, COLOR_BACKGROUND);
  display.drawFastHLine(0, HUD_Y, SCREEN_WIDTH, COLOR_HUD);
  display.setTextWrap(false);
  display.setTextSize(1);
  display.setTextColor(COLOR_HUD, COLOR_BACKGROUND);

  char line[40];
  snprintf(line, sizeof(line), "SH %3d        %06lu", player.shield, (unsigned long)score);
  display.setCursor(4, 294);
  display.print(line);

  snprintf(line, sizeof(line), "SEC %2d         %5dm", difficulty.sector, meters);
  display.setCursor(4, 307);
  display.print(line);

  lastHudShield = player.shield;
  lastHudScore = score;
  lastHudSector = difficulty.sector;
  lastHudDistanceMeters = meters;
  lastHudDrawMs = nowMs;
}

void renderTitleScreen() {
  display.setTextWrap(false);
  display.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
  display.setTextSize(3);
  display.setCursor(28, 88);
  display.print("DEEP");
  display.setCursor(18, 120);
  display.print("VECTOR");

  display.setTextSize(1);
  display.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  display.setCursor(63, 190);
  display.print("FLIP BOTH UP");
  display.setCursor(70, 204);
  display.print("TO ARM LAUNCH");
  display.setCursor(43, 228);
  display.setTextColor(COLOR_HUD, ST77XX_BLACK);
  display.print("THEN RETURN TO CENTER");
}

void renderStartingScreen(uint32_t nowMs) {
  const uint32_t elapsed = nowMs - startingStartedAtMs;
  display.fillRect(0, 100, SCREEN_WIDTH, 100, COLOR_BACKGROUND);
  display.setTextWrap(false);
  display.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
  display.setTextSize(3);

  if (elapsed < 250) {
    display.setCursor(105, 132);
    display.print("3");
  } else if (elapsed < 500) {
    display.setCursor(105, 132);
    display.print("2");
  } else if (elapsed < 750) {
    display.setCursor(105, 132);
    display.print("1");
  } else {
    display.setTextSize(2);
    display.setCursor(82, 136);
    display.print("LAUNCH");
  }
}

void renderDestroyedScreen() {
  display.setTextWrap(false);
  display.setTextColor(ST77XX_RED, ST77XX_BLACK);
  display.setTextSize(2);
  display.setCursor(58, 66);
  display.print("SHIP LOST");

  display.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  display.setTextSize(1);
  char line[40];

  snprintf(line, sizeof(line), "SCORE  %lu", (unsigned long)score);
  display.setCursor(70, 120);
  display.print(line);

  snprintf(line, sizeof(line), "DIST   %.2f KM", distanceTravelled / 1000.0f);
  display.setCursor(70, 138);
  display.print(line);

  snprintf(line, sizeof(line), "SECTOR %d", difficulty.sector);
  display.setCursor(70, 156);
  display.print(line);

  display.setTextColor(COLOR_HUD, ST77XX_BLACK);
  display.setCursor(68, 205);
  display.print("BOTH UP TO ARM");
  display.setCursor(54, 219);
  display.print("THEN CENTER TO RESTART");
}

void renderFrame(uint32_t nowMs) {
  static GameState lastRenderedState = GAME_BOOT;
  const bool stateChanged = gameState != lastRenderedState;

  if (stateChanged) {
    if (previousCommandCount > 0) erasePreviousFrame();
    previousCommandCount = 0;
    currentCommandCount = 0;
    display.fillScreen(COLOR_BACKGROUND); // state transitions only
    invalidateHud();
    lastRenderedState = gameState;
  }

  switch (gameState) {
    case GAME_BOOT:
      break;

    case GAME_TITLE:
      if (stateChanged) renderTitleScreen();
      break;

    case GAME_STARTING:
      renderStartingScreen(nowMs);
      break;

    case GAME_PLAYING:
      erasePreviousFrame();
      buildSceneCommands(nowMs);
      drawCurrentCommands();
      renderCrosshair();
      renderHud(nowMs, stateChanged);
      finishDynamicFrame();
      break;

    case GAME_DESTROYED:
      if ((uint32_t)(nowMs - destroyedAtMs) < DESTROYED_DRIFT_MS) {
        erasePreviousFrame();
        buildSceneCommands(nowMs);
        drawCurrentCommands();
        renderCrosshair();
        renderHud(nowMs, stateChanged);
        finishDynamicFrame();
      } else if (!destroyedScreenShown) {
        if (previousCommandCount > 0) erasePreviousFrame();
        previousCommandCount = 0;
        currentCommandCount = 0;
        display.fillScreen(COLOR_BACKGROUND);
        renderDestroyedScreen();
        destroyedScreenShown = true;
      }
      break;
  }
}

// ============================================================
// Random / debugging utilities
// ============================================================

float randomFloat(float minValue, float maxValue) {
  const uint32_t r = esp_random();
  const float unit = (float)r / 4294967295.0f;
  return minValue + (maxValue - minValue) * unit;
}

uint32_t seededNext(uint32_t& state) {
  // Xorshift32 for deterministic per-asteroid geometry.
  if (state == 0) state = 0x6D2B79F5u;
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return state;
}

float seededFloat(uint32_t& state, float minValue, float maxValue) {
  const uint32_t r = seededNext(state);
  const float unit = (float)r / 4294967295.0f;
  return minValue + (maxValue - minValue) * unit;
}

const char* switchStateName(SwitchState state) {
  switch (state) {
    case SWITCH_UP: return "UP";
    case SWITCH_CENTER: return "CENTER";
    case SWITCH_DOWN: return "DOWN";
    case SWITCH_ERROR: return "ERROR";
  }
  return "?";
}

const char* actionName(ControlAction action) {
  switch (action) {
    case ACTION_NONE: return "COAST";
    case ACTION_PITCH_UP: return "PITCH UP";
    case ACTION_PITCH_DOWN: return "PITCH DOWN";
    case ACTION_YAW_LEFT: return "YAW LEFT";
    case ACTION_YAW_RIGHT: return "YAW RIGHT";
    case ACTION_ROLL_LEFT: return "ROLL LEFT";
    case ACTION_ROLL_RIGHT: return "ROLL RIGHT";
  }
  return "?";
}

void updatePerformanceDebug(uint32_t nowMs) {
#if DEBUG_PERFORMANCE
  if ((uint32_t)(nowMs - performanceWindowStartedMs) >= 1000u) {
    int activeAsteroids = 0;
    int activeProjectiles = 0;
    int activeParticles = 0;
    for (int i = 0; i < MAX_ASTEROIDS; ++i) if (asteroids[i].active) ++activeAsteroids;
    for (int i = 0; i < MAX_PROJECTILES; ++i) if (projectiles[i].active) ++activeProjectiles;
    for (int i = 0; i < MAX_PARTICLES; ++i) if (particles[i].active) ++activeParticles;

    Serial.print("FPS=");
    Serial.print(performanceRenderFrames);
    Serial.print(" PHYS=");
    Serial.print(performancePhysicsTicks);
    Serial.print(" AST=");
    Serial.print(activeAsteroids);
    Serial.print(" PROJ=");
    Serial.print(activeProjectiles);
    Serial.print(" PART=");
    Serial.print(activeParticles);
    Serial.print(" CMD=");
    Serial.print(previousCommandCount);
    Serial.print(" HEAP=");
    Serial.println(ESP.getFreeHeap());

    performanceWindowStartedMs = nowMs;
    performancePhysicsTicks = 0;
    performanceRenderFrames = 0;
  }
#else
  (void)nowMs;
#endif
}

}
