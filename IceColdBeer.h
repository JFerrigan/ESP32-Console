#pragma once
#include "Hardware.h"
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <math.h>

namespace IceColdBeer {

// ============================================================
// ESP32 Ice Cold Beer
// Complete single-sketch implementation for Arduino IDE.
// ============================================================

// ============================================================
// Compile-time options
// ============================================================
#define DEBUG_SERIAL 1
#define DEBUG_OVERLAY 0
#define SHOW_ORIENTATION_TEST 0

// Rotation must be 0 or 2 for 240x320 portrait on this display.
// The specification requires the edge that used to be landscape LEFT
// to become physical TOP. If the image is upside-down on your exact
// GMT020-02-8P module, change only this value from 0 to 2.
constexpr uint8_t TFT_ROTATION = 0;

// ============================================================
// Pin definitions - exact wiring from the specification
// ============================================================


// ============================================================
// Display / timing constants
// ============================================================
constexpr int16_t SCREEN_WIDTH  = 240;
constexpr int16_t SCREEN_HEIGHT = 320;

constexpr float FIXED_DT = 1.0f / 60.0f;
constexpr uint32_t SIMULATION_STEP_US = 16667UL; // ~60 Hz
constexpr uint32_t RENDER_STEP_US     = 33333UL; // ~30 Hz visual refresh
constexpr uint8_t MAX_SIM_STEPS_PER_LOOP = 5;
constexpr uint32_t MAX_ELAPSED_US = 250000UL;

constexpr uint32_t SWITCH_DEBOUNCE_MS = 20UL;
constexpr uint32_t SWITCH_ERROR_CENTER_MS = 120UL;

constexpr uint32_t BOOT_DURATION_MS = SHOW_ORIENTATION_TEST ? 3000UL : 1000UL;
constexpr uint32_t READY_DURATION_MS = 600UL;
constexpr uint32_t TARGET_HIT_DURATION_MS = 650UL;
constexpr uint32_t WRONG_HOLE_DURATION_MS = 650UL;
constexpr uint32_t COMPLETE_DURATION_MS = 2000UL;
constexpr uint32_t GAME_OVER_DURATION_MS = 2200UL;
constexpr uint32_t SINK_ANIMATION_MS = 350UL;
constexpr uint32_t DEBUG_INTERVAL_MS = 200UL;

// ============================================================
// Gameplay tuning constants
// ============================================================
constexpr float BAR_LEFT_X  = 10.0f;
constexpr float BAR_RIGHT_X = 230.0f;
constexpr float BAR_START_Y = 286.0f;

constexpr float BAR_TOP_LIMIT    = 28.0f;
constexpr float BAR_BOTTOM_LIMIT = 290.0f;
constexpr float BAR_MOVE_SPEED   = 75.0f;
constexpr float MAX_BAR_TILT_PIXELS = 75.0f;
constexpr int16_t BAR_THICKNESS = 4;

constexpr float BALL_RADIUS = 5.0f;
constexpr float BALL_GRAVITY = 900.0f;
constexpr float BALL_DAMPING = 0.992f;
constexpr float MAX_BALL_SPEED = 160.0f;
constexpr float BALL_LOSS_GRACE = 2.0f;

constexpr int TOTAL_TARGETS = 10;
constexpr int STARTING_LIVES = 3;
constexpr int BONUS_LIFE_TARGET = 7;

// RGB565 colors not provided directly by ST77xx headers.
constexpr uint16_t COLOR_GRAY      = 0x7BEF;
constexpr uint16_t COLOR_DARK_GRAY = 0x39E7;
constexpr uint16_t COLOR_VERY_DARK = 0x18C3;
constexpr uint16_t COLOR_CYAN      = 0x07FF;

// ============================================================
// Enums
// ============================================================
enum SwitchState {
  SWITCH_UP,
  SWITCH_CENTER,
  SWITCH_DOWN,
  SWITCH_ERROR
};

enum GameState {
  GAME_BOOT,
  GAME_READY,
  GAME_PLAYING,
  GAME_TARGET_HIT,
  GAME_WRONG_HOLE,
  GAME_COMPLETE,
  GAME_OVER
};

// ============================================================
// Structs
// ============================================================
struct DebouncedSwitch {
  SwitchState rawState;
  SwitchState stableState;
  SwitchState previousRawState;
  uint32_t rawChangedAt;
  uint32_t errorStartedAt;
  bool errorActive;
};

struct Hole {
  float x;
  float y;
  float radius;
  int targetNumber; // 0 = trap, 1..10 = numbered target
};

struct RectI {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
  bool valid;
};

struct VisualState {
  float leftBarY;
  float rightBarY;
  float ballX;
  float ballY;
  float ballRadius;
  bool ballVisible;
};

// ============================================================
// Display object
// ============================================================
Adafruit_ST7789 display = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// ============================================================
// Fixed board layout
// Numbered targets climb from bottom toward the top and alternate
// horizontally. All other holes are traps.
// ============================================================
Hole holes[] = {
  // Sequential targets 1 -> 10
  {145.0f, 250.0f, 10.0f, 1},
  { 78.0f, 226.0f, 10.0f, 2},
  {180.0f, 205.0f, 10.0f, 3},
  { 94.0f, 184.0f, 10.0f, 4},
  {142.0f, 161.0f, 10.0f, 5},
  { 60.0f, 139.0f, 10.0f, 6},
  {156.0f, 118.0f, 10.0f, 7},
  {200.0f,  96.0f, 10.0f, 8},
  {130.0f,  72.0f, 10.0f, 9},
  {118.0f,  44.0f, 10.0f, 10},

  // Trap holes
  { 35.0f, 247.0f, 10.0f, 0},
  {205.0f, 242.0f, 10.0f, 0},
  {170.0f, 230.0f, 10.0f, 0},
  { 40.0f, 211.0f, 10.0f, 0},
  {120.0f, 214.0f, 10.0f, 0},
  {215.0f, 189.0f, 10.0f, 0},
  { 52.0f, 171.0f, 10.0f, 0},
  {190.0f, 155.0f, 10.0f, 0},
  { 94.0f, 147.0f, 10.0f, 0},
  { 30.0f, 124.0f, 10.0f, 0},
  {210.0f, 127.0f, 10.0f, 0},
  { 84.0f, 105.0f, 10.0f, 0},
  {130.0f,  95.0f, 10.0f, 0},
  { 45.0f,  84.0f, 10.0f, 0},
  {190.0f,  70.0f, 10.0f, 0},
  { 74.0f,  56.0f, 10.0f, 0},
  {165.0f,  50.0f, 10.0f, 0}
};

constexpr int HOLE_COUNT = sizeof(holes) / sizeof(holes[0]);

// ============================================================
// Global game state
// ============================================================
GameState gameState = GAME_BOOT;
uint32_t stateStartedAt = 0;
int currentTarget = 1;
int capturedHoleIndex = -1;
int lives = STARTING_LIVES;
bool bonusLifeAwarded = false;
bool bonusLifeNotice = false;
uint8_t backgroundPhase = 18;

float leftBarY = BAR_START_Y;
float rightBarY = BAR_START_Y;
float ballX = SCREEN_WIDTH / 2.0f;
float ballY = BAR_START_Y - BALL_RADIUS;
float ballVelocityX = 0.0f;

float captureStartBallX = 0.0f;
float captureStartBallY = 0.0f;

// ============================================================
// Input state
// ============================================================
DebouncedSwitch leftSwitch;
DebouncedSwitch rightSwitch;
SwitchState leftSwitchState = SWITCH_CENTER;
SwitchState rightSwitchState = SWITCH_CENTER;

// ============================================================
// Main-loop timing
// ============================================================
uint32_t lastLoopMicros = 0;
uint32_t simulationAccumulatorUs = 0;
uint32_t lastRenderMicros = 0;
uint32_t lastDebugAt = 0;

// ============================================================
// Rendering state
// ============================================================
bool forceFullRedraw = true;
bool previousVisualValid = false;
VisualState previousVisual;

// ============================================================
// Forward declarations
// ============================================================
void initializeSerial();
void initializeDisplay();
void initializeInputs();
void initializeGame();

SwitchState readSwitch(int upPin, int downPin);
SwitchState readRawSwitch(int upPin, int downPin);
void initializeDebouncedSwitch(DebouncedSwitch &sw, int upPin, int downPin);
void updateDebouncedSwitch(DebouncedSwitch &sw, int upPin, int downPin, uint32_t nowMs);
void updateSwitchDebounce();
void updateInputs();

void enterGameState(GameState newState);
void resetBallAndBar();
void startCurrentTarget();
void completeGame();
void resetRun();
void awardBonusLifeIfNeeded();
void updateBackgroundForTarget();

void updateGame(float dt);
void updatePlaying(float dt);
void updateBar(float dt);
void updateBall(float dt);

float getBarYAtX(float x);
float getBarSlope();

void checkBallBounds();
void checkHoleCollisions();
bool ballCapturedByHole(const Hole &hole);
void handleTargetHit(int holeIndex);
void handleWrongHole(int holeIndex);

void renderGame();
void renderBootScreen();
void renderCompleteScreen();
void renderGameOverScreen();
void renderBoardFull();
void renderGradientBackground();
void drawGradientRect(const RectI &rect);
uint16_t gradientColorForY(int16_t y);
uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b);
void colorWheel(uint8_t pos, uint8_t &r, uint8_t &g, uint8_t &b);
void renderPlayfield();
void renderPlayfieldFrame();
void renderHoles();
void renderHole(const Hole &hole);
void renderBar();
void renderBall();
void renderBallVisual(const VisualState &visual);
void renderHUD();
void renderStateOverlay();
void renderOverlay();
void renderDebugOverlay();
void drawCenteredText(const char *text, int16_t y, uint8_t size, uint16_t color);

VisualState getCurrentVisualState();
bool visualStatesDiffer(const VisualState &a, const VisualState &b);
RectI invalidRect();
RectI clampRect(RectI rect);
RectI unionRect(const RectI &a, const RectI &b);
RectI barRectForVisual(const VisualState &visual);
RectI ballRectForVisual(const VisualState &visual);
bool rectIntersectsCircle(const RectI &rect, float cx, float cy, float radius);
void redrawStaticInsideRect(const RectI &rect);

float clampFloat(float value, float lo, float hi);
float lerpFloat(float a, float b, float t);
float switchVelocity(SwitchState state);
const char *switchStateName(SwitchState state);
const char *gameStateName(GameState state);
void updateDebugOutput();

// ============================================================
// Setup
// ============================================================
void enter() {
  initializeSerial();
  initializeInputs();
  initializeDisplay();
  initializeGame();

#if DEBUG_SERIAL
  Serial.println(F("Ready."));
#endif
}

// ============================================================
// Main loop
// ============================================================
void tick() {
  const uint32_t nowUs = micros();
  uint32_t elapsedUs = nowUs - lastLoopMicros;
  lastLoopMicros = nowUs;

  if (elapsedUs > MAX_ELAPSED_US) {
    elapsedUs = MAX_ELAPSED_US;
  }

  simulationAccumulatorUs += elapsedUs;

  // Inputs are sampled continuously, not only when a physics step happens.
  updateInputs();

  uint8_t steps = 0;
  while (simulationAccumulatorUs >= SIMULATION_STEP_US &&
         steps < MAX_SIM_STEPS_PER_LOOP) {
    updateGame(FIXED_DT);
    simulationAccumulatorUs -= SIMULATION_STEP_US;
    ++steps;
  }

  // If the system fell very far behind, drop excess backlog rather than
  // producing a long burst of stale simulation frames.
  if (steps == MAX_SIM_STEPS_PER_LOOP &&
      simulationAccumulatorUs >= SIMULATION_STEP_US) {
    simulationAccumulatorUs = 0;
  }

  if ((uint32_t)(nowUs - lastRenderMicros) >= RENDER_STEP_US) {
    lastRenderMicros = nowUs;
    renderGame();
  }

  updateDebugOutput();
}

// ============================================================
// Hardware initialization
// ============================================================
void initializeSerial() {
#if DEBUG_SERIAL
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("Ice Cold Beer ESP32"));
#endif
}

void initializeDisplay() {
#if DEBUG_SERIAL
  Serial.println(F("Initializing TFT..."));
#endif

  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  display.init(240, 320);
  display.setRotation(TFT_ROTATION);
  display.setTextWrap(false);
  display.fillScreen(ST77XX_BLACK);
}

void initializeInputs() {
#if DEBUG_SERIAL
  Serial.println(F("Initializing switches..."));
#endif

  pinMode(LEFT_UP_PIN, INPUT_PULLUP);
  pinMode(LEFT_DOWN_PIN, INPUT_PULLUP);
  pinMode(RIGHT_UP_PIN, INPUT_PULLUP);
  pinMode(RIGHT_DOWN_PIN, INPUT_PULLUP);

  initializeDebouncedSwitch(leftSwitch, LEFT_UP_PIN, LEFT_DOWN_PIN);
  initializeDebouncedSwitch(rightSwitch, RIGHT_UP_PIN, RIGHT_DOWN_PIN);

  leftSwitchState = leftSwitch.stableState;
  rightSwitchState = rightSwitch.stableState;
}

void initializeGame() {
  currentTarget = 1;
  capturedHoleIndex = -1;
  lives = STARTING_LIVES;
  bonusLifeAwarded = false;
  bonusLifeNotice = false;
  backgroundPhase = 18;
  resetBallAndBar();

  const uint32_t nowUs = micros();
  lastLoopMicros = nowUs;
  lastRenderMicros = nowUs;
  simulationAccumulatorUs = 0;
  lastDebugAt = millis();

  enterGameState(GAME_BOOT);
}

// ============================================================
// Input
// ============================================================
SwitchState readRawSwitch(int upPin, int downPin) {
  const int upValue = digitalRead(upPin);
  const int downValue = digitalRead(downPin);

  if (upValue == LOW && downValue == HIGH) {
    return SWITCH_UP;
  }
  if (upValue == HIGH && downValue == HIGH) {
    return SWITCH_CENTER;
  }
  if (upValue == HIGH && downValue == LOW) {
    return SWITCH_DOWN;
  }
  return SWITCH_ERROR; // LOW / LOW
}

SwitchState readSwitch(int upPin, int downPin) {
  return readRawSwitch(upPin, downPin);
}

void initializeDebouncedSwitch(DebouncedSwitch &sw, int upPin, int downPin) {
  const uint32_t nowMs = millis();
  const SwitchState initial = readRawSwitch(upPin, downPin);

  sw.rawState = initial;
  sw.previousRawState = initial;
  sw.rawChangedAt = nowMs;
  sw.errorStartedAt = nowMs;
  sw.errorActive = (initial == SWITCH_ERROR);
  sw.stableState = (initial == SWITCH_ERROR) ? SWITCH_CENTER : initial;
}

void updateDebouncedSwitch(DebouncedSwitch &sw,
                           int upPin,
                           int downPin,
                           uint32_t nowMs) {
  const SwitchState newRaw = readRawSwitch(upPin, downPin);
  sw.rawState = newRaw;

  // LOW/LOW is an invalid DPDT reading. Preserve the last valid stable
  // state briefly so ordinary contact bounce cannot kick the bar around.
  // If the error persists, fail safely to CENTER.
  if (newRaw == SWITCH_ERROR) {
    if (!sw.errorActive) {
      sw.errorActive = true;
      sw.errorStartedAt = nowMs;
    }

    if ((uint32_t)(nowMs - sw.errorStartedAt) >= SWITCH_ERROR_CENTER_MS) {
      sw.stableState = SWITCH_CENTER;
    }
    return;
  }

  const bool wasInError = sw.errorActive;
  sw.errorActive = false;

  // Require a fresh debounce interval when recovering from an invalid
  // LOW/LOW reading instead of immediately trusting the first valid sample.
  if (wasInError) {
    sw.previousRawState = newRaw;
    sw.rawChangedAt = nowMs;
    return;
  }

  if (newRaw != sw.previousRawState) {
    sw.previousRawState = newRaw;
    sw.rawChangedAt = nowMs;
  }

  if ((uint32_t)(nowMs - sw.rawChangedAt) >= SWITCH_DEBOUNCE_MS &&
      sw.stableState != newRaw) {
    sw.stableState = newRaw;
  }
}

void updateSwitchDebounce() {
  const uint32_t nowMs = millis();
  updateDebouncedSwitch(leftSwitch, LEFT_UP_PIN, LEFT_DOWN_PIN, nowMs);
  updateDebouncedSwitch(rightSwitch, RIGHT_UP_PIN, RIGHT_DOWN_PIN, nowMs);
}

void updateInputs() {
  updateSwitchDebounce();
  leftSwitchState = leftSwitch.stableState;
  rightSwitchState = rightSwitch.stableState;
}

// ============================================================
// Game lifecycle / state machine
// ============================================================
void enterGameState(GameState newState) {
  gameState = newState;
  stateStartedAt = millis();
  forceFullRedraw = true;
  previousVisualValid = false;
}

void resetBallAndBar() {
  leftBarY = BAR_START_Y;
  rightBarY = BAR_START_Y;
  ballX = SCREEN_WIDTH / 2.0f;
  ballVelocityX = 0.0f;
  ballY = getBarYAtX(ballX) - BALL_RADIUS;
  capturedHoleIndex = -1;
  captureStartBallX = ballX;
  captureStartBallY = ballY;
}

void updateBackgroundForTarget() {
  // Give every round a different dark neon gradient without continuously
  // repainting the whole TFT during play. That keeps the controls smooth.
  int targetForColor = currentTarget;
  if (targetForColor < 1) targetForColor = 1;
  if (targetForColor > TOTAL_TARGETS) targetForColor = TOTAL_TARGETS;
  backgroundPhase = (uint8_t)(18 + (targetForColor - 1) * 23);
}

void startCurrentTarget() {
  bonusLifeNotice = false;
  updateBackgroundForTarget();
  resetBallAndBar();
  enterGameState(GAME_READY);
}

void completeGame() {
  ballVelocityX = 0.0f;
  backgroundPhase = 205;
  enterGameState(GAME_COMPLETE);
}

void resetRun() {
  currentTarget = 1;
  lives = STARTING_LIVES;
  bonusLifeAwarded = false;
  bonusLifeNotice = false;
  capturedHoleIndex = -1;
  updateBackgroundForTarget();
  resetBallAndBar();
}

void awardBonusLifeIfNeeded() {
  // The bonus is earned by successfully sinking target 7, once per run.
  if (!bonusLifeAwarded && currentTarget == BONUS_LIFE_TARGET) {
    ++lives;
    bonusLifeAwarded = true;
    bonusLifeNotice = true;
  }
}

void updateGame(float dt) {
  const uint32_t elapsedStateMs = millis() - stateStartedAt;

  switch (gameState) {
    case GAME_BOOT:
      if (elapsedStateMs >= BOOT_DURATION_MS) {
        resetRun();
        startCurrentTarget();
      }
      break;

    case GAME_READY:
      // Controls remain live/debounced, but the bar does not move here.
      if (elapsedStateMs >= READY_DURATION_MS) {
        enterGameState(GAME_PLAYING);
      }
      break;

    case GAME_PLAYING:
      updatePlaying(dt);
      break;

    case GAME_TARGET_HIT:
      if (elapsedStateMs >= TARGET_HIT_DURATION_MS) {
        ++currentTarget;
        if (currentTarget > TOTAL_TARGETS) {
          completeGame();
        } else {
          startCurrentTarget();
        }
      }
      break;

    case GAME_WRONG_HOLE:
      if (elapsedStateMs >= WRONG_HOLE_DURATION_MS) {
        if (lives <= 0) {
          backgroundPhase = 246;
          enterGameState(GAME_OVER);
        } else {
          // Same target remains active.
          startCurrentTarget();
        }
      }
      break;

    case GAME_COMPLETE:
      if (elapsedStateMs >= COMPLETE_DURATION_MS) {
        resetRun();
        startCurrentTarget();
      }
      break;

    case GAME_OVER:
      if (elapsedStateMs >= GAME_OVER_DURATION_MS) {
        resetRun();
        startCurrentTarget();
      }
      break;
  }
}

void updatePlaying(float dt) {
  updateBar(dt);
  updateBall(dt);
  checkBallBounds();

  if (gameState != GAME_PLAYING) {
    return;
  }

  checkHoleCollisions();
}

// ============================================================
// Simulation
// ============================================================
float switchVelocity(SwitchState state) {
  switch (state) {
    case SWITCH_UP:
      return -BAR_MOVE_SPEED;
    case SWITCH_DOWN:
      return BAR_MOVE_SPEED;
    case SWITCH_CENTER:
    case SWITCH_ERROR:
    default:
      return 0.0f;
  }
}

void updateBar(float dt) {
  const float leftVelocity = switchVelocity(leftSwitchState);
  const float rightVelocity = switchVelocity(rightSwitchState);

  // Update each endpoint independently. Applying the tilt constraint after
  // each endpoint prevents a CENTERed side from being dragged around merely
  // because the opposite side has reached the tilt limit.
  float proposedLeft = leftBarY + leftVelocity * dt;
  proposedLeft = clampFloat(proposedLeft, BAR_TOP_LIMIT, BAR_BOTTOM_LIMIT);
  proposedLeft = clampFloat(
      proposedLeft,
      rightBarY - MAX_BAR_TILT_PIXELS,
      rightBarY + MAX_BAR_TILT_PIXELS);
  proposedLeft = clampFloat(proposedLeft, BAR_TOP_LIMIT, BAR_BOTTOM_LIMIT);
  leftBarY = proposedLeft;

  float proposedRight = rightBarY + rightVelocity * dt;
  proposedRight = clampFloat(proposedRight, BAR_TOP_LIMIT, BAR_BOTTOM_LIMIT);
  proposedRight = clampFloat(
      proposedRight,
      leftBarY - MAX_BAR_TILT_PIXELS,
      leftBarY + MAX_BAR_TILT_PIXELS);
  proposedRight = clampFloat(proposedRight, BAR_TOP_LIMIT, BAR_BOTTOM_LIMIT);
  rightBarY = proposedRight;
}

float getBarYAtX(float x) {
  float t = (x - BAR_LEFT_X) / (BAR_RIGHT_X - BAR_LEFT_X);
  t = clampFloat(t, 0.0f, 1.0f);
  return leftBarY + (rightBarY - leftBarY) * t;
}

float getBarSlope() {
  return (rightBarY - leftBarY) / (BAR_RIGHT_X - BAR_LEFT_X);
}

void updateBall(float dt) {
  const float normalizedSlope = getBarSlope();

  // Screen Y increases downward. A positive slope means the right end is
  // physically lower, so acceleration must be positive (to the right).
  const float acceleration = normalizedSlope * BALL_GRAVITY;

  ballVelocityX += acceleration * dt;
  ballVelocityX *= BALL_DAMPING;
  ballVelocityX = clampFloat(ballVelocityX, -MAX_BALL_SPEED, MAX_BALL_SPEED);

  ballX += ballVelocityX * dt;
  ballY = getBarYAtX(ballX) - BALL_RADIUS;
}

// ============================================================
// Collision
// ============================================================
void checkBallBounds() {
  if (gameState != GAME_PLAYING) {
    return;
  }

  if (ballX < BAR_LEFT_X - BALL_RADIUS - BALL_LOSS_GRACE ||
      ballX > BAR_RIGHT_X + BALL_RADIUS + BALL_LOSS_GRACE) {
    handleWrongHole(-1);
  }
}

bool ballCapturedByHole(const Hole &hole) {
  const float dx = ballX - hole.x;
  const float dy = ballY - hole.y;
  const float distanceSquared = dx * dx + dy * dy;

  float captureRadius = hole.radius - BALL_RADIUS * 0.25f;
  if (captureRadius < 2.0f) {
    captureRadius = 2.0f;
  }

  return distanceSquared < captureRadius * captureRadius;
}

void checkHoleCollisions() {
  if (gameState != GAME_PLAYING) {
    return;
  }

  for (int i = 0; i < HOLE_COUNT; ++i) {
    if (!ballCapturedByHole(holes[i])) {
      continue;
    }

    if (holes[i].targetNumber == currentTarget) {
      handleTargetHit(i);
    } else {
      handleWrongHole(i);
    }
    return; // only the first capture in a frame counts
  }
}

void handleTargetHit(int holeIndex) {
  if (gameState != GAME_PLAYING) {
    return;
  }

  capturedHoleIndex = holeIndex;
  captureStartBallX = ballX;
  captureStartBallY = ballY;
  ballVelocityX = 0.0f;

  awardBonusLifeIfNeeded();
  enterGameState(GAME_TARGET_HIT);
}

void handleWrongHole(int holeIndex) {
  if (gameState != GAME_PLAYING) {
    return;
  }

  capturedHoleIndex = holeIndex;
  captureStartBallX = ballX;
  captureStartBallY = ballY;
  ballVelocityX = 0.0f;

  if (lives > 0) {
    --lives;
  }

  enterGameState(GAME_WRONG_HOLE);
}

// ============================================================
// Rendering - high-level
// ============================================================
void renderGame() {
  if (gameState == GAME_BOOT) {
    if (forceFullRedraw) {
      renderBootScreen();
      forceFullRedraw = false;
      previousVisualValid = false;
    }
    return;
  }

  if (gameState == GAME_COMPLETE) {
    if (forceFullRedraw) {
      renderCompleteScreen();
      forceFullRedraw = false;
      previousVisualValid = false;
    }
    return;
  }

  if (gameState == GAME_OVER) {
    if (forceFullRedraw) {
      renderGameOverScreen();
      forceFullRedraw = false;
      previousVisualValid = false;
    }
    return;
  }

  const VisualState currentVisual = getCurrentVisualState();

  if (forceFullRedraw || !previousVisualValid) {
    renderBoardFull();
    previousVisual = currentVisual;
    previousVisualValid = true;
    forceFullRedraw = false;
    return;
  }

  const bool barChanged =
      fabsf(currentVisual.leftBarY - previousVisual.leftBarY) > 0.05f ||
      fabsf(currentVisual.rightBarY - previousVisual.rightBarY) > 0.05f;

  const bool ballChanged =
      currentVisual.ballVisible != previousVisual.ballVisible ||
      fabsf(currentVisual.ballX - previousVisual.ballX) > 0.05f ||
      fabsf(currentVisual.ballY - previousVisual.ballY) > 0.05f ||
      fabsf(currentVisual.ballRadius - previousVisual.ballRadius) > 0.05f;

  if (!barChanged && !ballChanged) {
    return;
  }

  RectI dirty = invalidRect();

  if (barChanged) {
    dirty = unionRect(dirty, barRectForVisual(previousVisual));
    dirty = unionRect(dirty, barRectForVisual(currentVisual));
  }

  if (ballChanged) {
    dirty = unionRect(dirty, ballRectForVisual(previousVisual));
    dirty = unionRect(dirty, ballRectForVisual(currentVisual));
  }

  dirty = clampRect(dirty);

  if (dirty.valid) {
    redrawStaticInsideRect(dirty);

    // The dirty rectangle may have erased part of the bar, HUD, or overlay.
    // Repaint them in final z-order.
    renderBar();
    renderBallVisual(currentVisual);
    renderHUD();
    renderOverlay();
    renderDebugOverlay();
  }

  previousVisual = currentVisual;
  previousVisualValid = true;
}

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((uint16_t)(r & 0xF8) << 8) |
                    ((uint16_t)(g & 0xFC) << 3) |
                    ((uint16_t)b >> 3));
}

void colorWheel(uint8_t pos, uint8_t &r, uint8_t &g, uint8_t &b) {
  // Smooth three-segment RGB color wheel.
  if (pos < 85) {
    r = (uint8_t)(255 - pos * 3);
    g = (uint8_t)(pos * 3);
    b = 0;
  } else if (pos < 170) {
    pos = (uint8_t)(pos - 85);
    r = 0;
    g = (uint8_t)(255 - pos * 3);
    b = (uint8_t)(pos * 3);
  } else {
    pos = (uint8_t)(pos - 170);
    r = (uint8_t)(pos * 3);
    g = 0;
    b = (uint8_t)(255 - pos * 3);
  }
}

uint16_t gradientColorForY(int16_t y) {
  int32_t clampedY = y;
  if (clampedY < 0) clampedY = 0;
  if (clampedY >= SCREEN_HEIGHT) clampedY = SCREEN_HEIGHT - 1;

  // Sweep through a broad section of the color wheel vertically.
  const uint8_t verticalOffset =
      (uint8_t)((clampedY * 112L) / (SCREEN_HEIGHT - 1));
  const uint8_t wheelPos = (uint8_t)(backgroundPhase + verticalOffset);

  uint8_t r, g, b;
  colorWheel(wheelPos, r, g, b);

  // Keep the gradient dark and rich so the white ball/bar and bright target
  // colors stay extremely readable.
  r = (uint8_t)(4 + ((uint16_t)r * 50U) / 255U);
  g = (uint8_t)(4 + ((uint16_t)g * 48U) / 255U);
  b = (uint8_t)(7 + ((uint16_t)b * 62U) / 255U);

  return rgb565(r, g, b);
}

void drawGradientRect(const RectI &rect) {
  if (!rect.valid) {
    return;
  }

  const RectI clipped = clampRect(rect);
  if (!clipped.valid) {
    return;
  }

  for (int16_t y = clipped.y; y < clipped.y + clipped.h; ++y) {
    display.drawFastHLine(clipped.x, y, clipped.w, gradientColorForY(y));
  }
}

void renderGradientBackground() {
  RectI full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, true};
  drawGradientRect(full);
}

void renderBootScreen() {
  renderGradientBackground();

#if SHOW_ORIENTATION_TEST
  drawCenteredText("TOP", 5, 1, ST77XX_CYAN);
  drawCenteredText("BOTTOM", 300, 1, ST77XX_CYAN);
#endif

  drawCenteredText("ICE COLD", 120, 3, ST77XX_WHITE);
  drawCenteredText("BEER", 158, 4, ST77XX_YELLOW);
  drawCenteredText("ESP32", 214, 1, COLOR_GRAY);
}

void renderCompleteScreen() {
  renderGradientBackground();
  drawCenteredText("ALL 10", 105, 3, ST77XX_GREEN);
  drawCenteredText("COMPLETE", 145, 3, ST77XX_GREEN);
  drawCenteredText("ICE COLD BEER", 210, 2, ST77XX_WHITE);
}

void renderGameOverScreen() {
  renderGradientBackground();
  drawCenteredText("GAME OVER", 120, 3, ST77XX_RED);

  char targetText[24];
  snprintf(targetText, sizeof(targetText), "REACHED TARGET %d", currentTarget);
  drawCenteredText(targetText, 170, 1, ST77XX_WHITE);
  drawCenteredText("TRY AGAIN", 205, 2, ST77XX_YELLOW);
}

void renderBoardFull() {
  const VisualState visual = getCurrentVisualState();

  renderGradientBackground();
  renderPlayfield();
  renderBar();
  renderBallVisual(visual);
  renderHUD();
  renderOverlay();
  renderDebugOverlay();
}

void renderPlayfield() {
  renderPlayfieldFrame();
  renderHoles();
}

void renderPlayfieldFrame() {
  // Minimal border and HUD separator. Holes occupy the interior.
  display.drawFastHLine(0, 19, SCREEN_WIDTH, COLOR_DARK_GRAY);
  display.drawFastVLine(2, 20, SCREEN_HEIGHT - 22, COLOR_VERY_DARK);
  display.drawFastVLine(SCREEN_WIDTH - 3, 20, SCREEN_HEIGHT - 22, COLOR_VERY_DARK);
  display.drawFastHLine(2, SCREEN_HEIGHT - 3, SCREEN_WIDTH - 4, COLOR_VERY_DARK);
}

void renderHoles() {
  for (int i = 0; i < HOLE_COUNT; ++i) {
    renderHole(holes[i]);
  }
}

void renderHole(const Hole &hole) {
  const int16_t x = (int16_t)lroundf(hole.x);
  const int16_t y = (int16_t)lroundf(hole.y);
  const int16_t r = (int16_t)lroundf(hole.radius);

  bool isCapturedHole = false;
  if (capturedHoleIndex >= 0 && capturedHoleIndex < HOLE_COUNT) {
    isCapturedHole = (&hole == &holes[capturedHoleIndex]);
  }

  uint16_t outerColor = (hole.targetNumber > 0) ? COLOR_GRAY : COLOR_DARK_GRAY;

  if (hole.targetNumber == currentTarget) {
    outerColor = ST77XX_YELLOW;
  }
  if (isCapturedHole && gameState == GAME_TARGET_HIT) {
    outerColor = ST77XX_GREEN;
  }
  if (isCapturedHole && gameState == GAME_WRONG_HOLE) {
    outerColor = ST77XX_RED;
  }

  // Recess: dark interior plus two rings.
  display.fillCircle(x, y, r, ST77XX_BLACK);
  display.drawCircle(x, y, r, outerColor);
  if (r >= 3) {
    display.drawCircle(x, y, r - 2, COLOR_VERY_DARK);
  }

  // Active/captured holes get an additional outer ring.
  if (hole.targetNumber == currentTarget || isCapturedHole) {
    display.drawCircle(x, y, r + 2, outerColor);
    display.drawCircle(x, y, r + 3, outerColor);
  }

  if (hole.targetNumber > 0) {
    char numberBuffer[4];
    snprintf(numberBuffer, sizeof(numberBuffer), "%d", hole.targetNumber);

    display.setTextSize(1);
    display.setTextColor(
        (hole.targetNumber == currentTarget) ? ST77XX_YELLOW : ST77XX_WHITE,
        ST77XX_BLACK);

    int16_t x1, y1;
    uint16_t textW, textH;
    display.getTextBounds(numberBuffer, 0, 0, &x1, &y1, &textW, &textH);
    display.setCursor(x - (int16_t)textW / 2, y - (int16_t)textH / 2);
    display.print(numberBuffer);
  }
}

void renderBar() {
  const int16_t x1 = (int16_t)lroundf(BAR_LEFT_X);
  const int16_t x2 = (int16_t)lroundf(BAR_RIGHT_X);
  const int16_t y1 = (int16_t)lroundf(leftBarY);
  const int16_t y2 = (int16_t)lroundf(rightBarY);

  // leftBarY/rightBarY represent the TOP supporting edge of the bar.
  for (int16_t offset = 0; offset < BAR_THICKNESS; ++offset) {
    display.drawLine(x1, y1 + offset, x2, y2 + offset, ST77XX_WHITE);
  }
}

void renderBall() {
  renderBallVisual(getCurrentVisualState());
}

void renderBallVisual(const VisualState &visual) {
  if (!visual.ballVisible || visual.ballRadius <= 0.4f) {
    return;
  }

  const int16_t x = (int16_t)lroundf(visual.ballX);
  const int16_t y = (int16_t)lroundf(visual.ballY);
  const int16_t r = (int16_t)lroundf(visual.ballRadius);

  display.fillCircle(x, y, r, ST77XX_WHITE);
  if (r >= 3) {
    display.drawPixel(x - 1, y - 1, ST77XX_CYAN);
  }
}

void renderHUD() {
  char leftText[18];
  char centerText[16];
  char rightText[12];

  int shownTarget = currentTarget;
  if (shownTarget < 1) shownTarget = 1;
  if (shownTarget > TOTAL_TARGETS) shownTarget = TOTAL_TARGETS;

  snprintf(leftText, sizeof(leftText), "TARGET %d", shownTarget);
  snprintf(centerText, sizeof(centerText), "LIVES %d", lives);
  snprintf(rightText, sizeof(rightText), "%d / %d", shownTarget, TOTAL_TARGETS);

  // Restore the gradient in the HUD band so text never smears.
  RectI hudRect = {0, 0, SCREEN_WIDTH, 19, true};
  drawGradientRect(hudRect);

  display.setTextSize(1);
  display.setTextColor(ST77XX_WHITE);
  display.setCursor(5, 6);
  display.print(leftText);

  int16_t x1, y1;
  uint16_t w, h;

  display.getTextBounds(centerText, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - (int16_t)w) / 2, 6);
  display.print(centerText);

  display.getTextBounds(rightText, 0, 0, &x1, &y1, &w, &h);
  display.setCursor(SCREEN_WIDTH - 5 - (int16_t)w, 6);
  display.print(rightText);

  display.drawFastHLine(0, 19, SCREEN_WIDTH, COLOR_DARK_GRAY);
}

void renderStateOverlay() {
  if (gameState == GAME_PLAYING) {
    return;
  }

  if (gameState == GAME_READY) {
    char targetText[20];
    snprintf(targetText, sizeof(targetText), "TARGET %d", currentTarget);

    display.fillRoundRect(48, 128, 144, 58, 7, ST77XX_BLACK);
    display.drawRoundRect(48, 128, 144, 58, 7, ST77XX_YELLOW);
    drawCenteredText(targetText, 140, 2, ST77XX_YELLOW);
    drawCenteredText("GET READY", 166, 1, ST77XX_WHITE);
    return;
  }

  if (gameState == GAME_TARGET_HIT) {
    char hitText[24];
    snprintf(hitText, sizeof(hitText), "TARGET %d HIT!", currentTarget);

    display.fillRoundRect(35, 132, 170, 52, 7, ST77XX_BLACK);
    display.drawRoundRect(35, 132, 170, 52, 7, ST77XX_GREEN);

    if (bonusLifeNotice) {
      drawCenteredText(hitText, 141, 2, ST77XX_GREEN);
      drawCenteredText("+1 LIFE!", 169, 1, ST77XX_YELLOW);
    } else {
      drawCenteredText(hitText, 148, 2, ST77XX_GREEN);
    }
    return;
  }

  if (gameState == GAME_WRONG_HOLE) {
    display.fillRoundRect(42, 128, 156, 62, 7, ST77XX_BLACK);
    display.drawRoundRect(42, 128, 156, 62, 7, ST77XX_RED);

    if (capturedHoleIndex < 0) {
      drawCenteredText("BALL LOST", 140, 2, ST77XX_RED);
    } else {
      drawCenteredText("MISS", 140, 3, ST77XX_RED);
    }

    char statusText[24];
    snprintf(statusText, sizeof(statusText), "%d %s LEFT",
             lives, (lives == 1) ? "LIFE" : "LIVES");
    drawCenteredText(statusText, 172, 1,
                     (lives > 0) ? ST77XX_WHITE : ST77XX_RED);
  }
}

void renderOverlay() {
  renderStateOverlay();
}

void renderDebugOverlay() {
#if DEBUG_OVERLAY
  char line[32];
  display.fillRect(132, 22, 105, 38, ST77XX_BLACK);
  display.drawRect(132, 22, 105, 38, COLOR_DARK_GRAY);
  display.setTextSize(1);
  display.setTextColor(ST77XX_CYAN, ST77XX_BLACK);

  snprintf(line, sizeof(line), "L:%.1f R:%.1f", leftBarY, rightBarY);
  display.setCursor(136, 26);
  display.print(line);

  snprintf(line, sizeof(line), "VX:%.1f", ballVelocityX);
  display.setCursor(136, 38);
  display.print(line);

  snprintf(line, sizeof(line), "X:%.1f Y:%.1f", ballX, ballY);
  display.setCursor(136, 50);
  display.print(line);
#endif
}

void drawCenteredText(const char *text, int16_t y, uint8_t size, uint16_t color) {
  display.setTextSize(size);
  display.setTextColor(color);

  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  const int16_t x = (SCREEN_WIDTH - (int16_t)w) / 2;
  display.setCursor(x, y);
  display.print(text);
}

// ============================================================
// Non-flickering dirty-region rendering helpers
// ============================================================
VisualState getCurrentVisualState() {
  VisualState visual;
  visual.leftBarY = leftBarY;
  visual.rightBarY = rightBarY;
  visual.ballX = ballX;
  visual.ballY = ballY;
  visual.ballRadius = BALL_RADIUS;
  visual.ballVisible = true;

  if (gameState == GAME_TARGET_HIT || gameState == GAME_WRONG_HOLE) {
    const uint32_t elapsedMs = millis() - stateStartedAt;
    float p = (float)elapsedMs / (float)SINK_ANIMATION_MS;
    p = clampFloat(p, 0.0f, 1.0f);

    if (capturedHoleIndex >= 0 && capturedHoleIndex < HOLE_COUNT) {
      const Hole &hole = holes[capturedHoleIndex];
      visual.ballX = lerpFloat(captureStartBallX, hole.x, p);
      visual.ballY = lerpFloat(captureStartBallY, hole.y, p);
      visual.ballRadius = BALL_RADIUS * (1.0f - 0.85f * p);
    } else {
      // Rolling off the end uses the same failure state, but visually lets
      // the ball fall away instead of moving toward a hole.
      visual.ballX = captureStartBallX;
      visual.ballY = captureStartBallY + 72.0f * p;
      visual.ballRadius = BALL_RADIUS * (1.0f - 0.75f * p);
    }

    if (p >= 1.0f) {
      visual.ballVisible = false;
    }
  }

  return visual;
}

bool visualStatesDiffer(const VisualState &a, const VisualState &b) {
  return
      fabsf(a.leftBarY - b.leftBarY) > 0.05f ||
      fabsf(a.rightBarY - b.rightBarY) > 0.05f ||
      a.ballVisible != b.ballVisible ||
      fabsf(a.ballX - b.ballX) > 0.05f ||
      fabsf(a.ballY - b.ballY) > 0.05f ||
      fabsf(a.ballRadius - b.ballRadius) > 0.05f;
}

RectI invalidRect() {
  RectI rect = {0, 0, 0, 0, false};
  return rect;
}

RectI clampRect(RectI rect) {
  if (!rect.valid) {
    return rect;
  }

  int16_t x1 = rect.x;
  int16_t y1 = rect.y;
  int16_t x2 = rect.x + rect.w - 1;
  int16_t y2 = rect.y + rect.h - 1;

  if (x2 < 0 || y2 < 0 || x1 >= SCREEN_WIDTH || y1 >= SCREEN_HEIGHT) {
    return invalidRect();
  }

  if (x1 < 0) x1 = 0;
  if (y1 < 0) y1 = 0;
  if (x2 >= SCREEN_WIDTH) x2 = SCREEN_WIDTH - 1;
  if (y2 >= SCREEN_HEIGHT) y2 = SCREEN_HEIGHT - 1;

  RectI result;
  result.x = x1;
  result.y = y1;
  result.w = x2 - x1 + 1;
  result.h = y2 - y1 + 1;
  result.valid = (result.w > 0 && result.h > 0);
  return result;
}

RectI unionRect(const RectI &a, const RectI &b) {
  if (!a.valid) return b;
  if (!b.valid) return a;

  const int16_t x1 = min(a.x, b.x);
  const int16_t y1 = min(a.y, b.y);
  const int16_t x2 = max((int16_t)(a.x + a.w - 1), (int16_t)(b.x + b.w - 1));
  const int16_t y2 = max((int16_t)(a.y + a.h - 1), (int16_t)(b.y + b.h - 1));

  RectI result;
  result.x = x1;
  result.y = y1;
  result.w = x2 - x1 + 1;
  result.h = y2 - y1 + 1;
  result.valid = true;
  return result;
}

RectI barRectForVisual(const VisualState &visual) {
  const float minY = fminf(visual.leftBarY, visual.rightBarY);
  const float maxY = fmaxf(visual.leftBarY, visual.rightBarY) + BAR_THICKNESS;
  constexpr int16_t pad = 3;

  RectI rect;
  rect.x = (int16_t)floorf(BAR_LEFT_X) - pad;
  rect.y = (int16_t)floorf(minY) - pad;
  rect.w = (int16_t)ceilf(BAR_RIGHT_X - BAR_LEFT_X) + pad * 2 + 1;
  rect.h = (int16_t)ceilf(maxY - minY) + pad * 2 + 1;
  rect.valid = true;
  return clampRect(rect);
}

RectI ballRectForVisual(const VisualState &visual) {
  if (!visual.ballVisible || visual.ballRadius <= 0.0f) {
    return invalidRect();
  }

  constexpr int16_t pad = 3;
  const float radius = visual.ballRadius;

  RectI rect;
  rect.x = (int16_t)floorf(visual.ballX - radius) - pad;
  rect.y = (int16_t)floorf(visual.ballY - radius) - pad;
  rect.w = (int16_t)ceilf(radius * 2.0f) + pad * 2 + 1;
  rect.h = (int16_t)ceilf(radius * 2.0f) + pad * 2 + 1;
  rect.valid = true;
  return clampRect(rect);
}

bool rectIntersectsCircle(const RectI &rect, float cx, float cy, float radius) {
  if (!rect.valid) {
    return false;
  }

  const float closestX = clampFloat(cx, (float)rect.x, (float)(rect.x + rect.w - 1));
  const float closestY = clampFloat(cy, (float)rect.y, (float)(rect.y + rect.h - 1));
  const float dx = cx - closestX;
  const float dy = cy - closestY;
  return dx * dx + dy * dy <= radius * radius;
}

void redrawStaticInsideRect(const RectI &rect) {
  if (!rect.valid) {
    return;
  }

  // Restore the exact same gradient pixels that were underneath the moving
  // ball/bar, then repaint any static geometry touched by this dirty region.
  drawGradientRect(rect);

  // Repaint any static hole whose visual footprint was touched.
  for (int i = 0; i < HOLE_COUNT; ++i) {
    const float visualRadius = holes[i].radius + 4.0f;
    if (rectIntersectsCircle(rect, holes[i].x, holes[i].y, visualRadius)) {
      renderHole(holes[i]);
    }
  }

  // Restore frame lines that may cross the dirty rectangle.
  renderPlayfieldFrame();
}

// ============================================================
// Debug output
// ============================================================
const char *switchStateName(SwitchState state) {
  switch (state) {
    case SWITCH_UP: return "UP";
    case SWITCH_CENTER: return "CENTER";
    case SWITCH_DOWN: return "DOWN";
    case SWITCH_ERROR: return "ERROR";
    default: return "?";
  }
}

const char *gameStateName(GameState state) {
  switch (state) {
    case GAME_BOOT: return "BOOT";
    case GAME_READY: return "READY";
    case GAME_PLAYING: return "PLAYING";
    case GAME_TARGET_HIT: return "TARGET_HIT";
    case GAME_WRONG_HOLE: return "WRONG_HOLE";
    case GAME_COMPLETE: return "COMPLETE";
    case GAME_OVER: return "GAME_OVER";
    default: return "?";
  }
}

void updateDebugOutput() {
#if DEBUG_SERIAL
  const uint32_t nowMs = millis();
  if ((uint32_t)(nowMs - lastDebugAt) < DEBUG_INTERVAL_MS) {
    return;
  }
  lastDebugAt = nowMs;

  Serial.printf(
      "L=%s(raw:%s) R=%s(raw:%s) LY=%.2f RY=%.2f X=%.2f Y=%.2f VX=%.2f target=%d lives=%d state=%s\n",
      switchStateName(leftSwitchState),
      switchStateName(leftSwitch.rawState),
      switchStateName(rightSwitchState),
      switchStateName(rightSwitch.rawState),
      leftBarY,
      rightBarY,
      ballX,
      ballY,
      ballVelocityX,
      currentTarget,
      lives,
      gameStateName(gameState));
#endif
}

// ============================================================
// Math helpers
// ============================================================
float clampFloat(float value, float lo, float hi) {
  if (value < lo) return lo;
  if (value > hi) return hi;
  return value;
}

float lerpFloat(float a, float b, float t) {
  return a + (b - a) * t;
}

}
