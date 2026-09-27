#pragma once
#include <Arduino.h>
#include <Adafruit_ST7789.h>

// The launcher owns the one TFT object used by games. Reuse it instead of
// creating another Adafruit_ST7789 for the same physical display.
extern Adafruit_ST7789 display;

// Snapshot passed to each game once per launcher loop.
struct GameInput {
  bool leftButton;          // GPIO 27 (button 2)
  bool rightButton;         // GPIO 14 (button 1)
  bool leftPressed;         // new press this update
  bool rightPressed;
};

struct GameModule {
  uint8_t order;            // menu order; lower numbers first
  const char *title;        // ideally <= 15 characters at text size 2
  void (*enter)();          // called whenever selected; reset/draw game
  void (*update)(const GameInput &); // called each launcher loop
};

constexpr uint8_t MAX_GAMES = 12;
bool registerGame(const GameModule &game);
uint8_t gameCount();
const GameModule *gameAt(uint8_t index);

// Each .ino game tab must contain exactly one REGISTER_GAME statement.
// A new game needs no launcher edits. Keep order values unique.
#define GAME_REGISTRY_JOIN_INNER(a, b) a##b
#define GAME_REGISTRY_JOIN(a, b) GAME_REGISTRY_JOIN_INNER(a, b)
#define REGISTER_GAME(ORDER, TITLE, ENTER, UPDATE) \
  static const bool GAME_REGISTRY_JOIN(registeredGame_, __COUNTER__) = \
    registerGame({ORDER, TITLE, ENTER, UPDATE})
