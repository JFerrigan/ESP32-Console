/*
  JAKEBOY ARCADE: HOW TO ADD A GAME

  Add a new .ino tab in this folder (for example Game_MyGame.ino).
  Include GameAPI.h. Put your game state and functions in a unique namespace.
  Provide exactly these two functions:
    void MyGame::enter();                 // reset state and draw initial screen
    void MyGame::update(const GameInput&); // advance game each launcher loop
  At file scope register the game exactly once:
    REGISTER_GAME(30, "MY GAME", MyGame::enter, MyGame::update);
  Use a unique order number (lower = higher in menu). That is all the
  launcher needs: no edit to its menu, setup(), or loop().

  Details: GameInput contains leftButton/rightButton held and leftPressed/
  rightPressed edges. Keep update() non-blocking. Use millis() for timing.
  The launcher owns GPIO, menu selection, and screen transitions. Your game
  may draw to its own Adafruit_ST7789 display or use its own helper code.
  The launcher calls enter() anew on every launch. Both buttons held together
  for 700 ms return to the menu. GPIO assignments are in Hardware.h.
  Existing large games put gameplay in their .h file and register from a
  matching Game_*.ino tab to avoid Arduino's automatic prototype generation.
  A simple game may live entirely in its Game_*.ino tab. See GAME_TEMPLATE.txt.
  Capacity is MAX_GAMES in GameAPI.h; increase that if adding >12 games.
  Music/Settings/Hardware Test are built-in menu entries after the games.
*/
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "Hardware.h"
#include "GameAPI.h"
#include "MusicPlayer.h"

// Registration happens during startup. The fixed array avoids allocation.
GameModule registeredGames[MAX_GAMES];
uint8_t registeredGameCount = 0;
bool registerGame(const GameModule &game) {
  if (registeredGameCount >= MAX_GAMES || !game.title || !game.enter || !game.update)
    return false;
  uint8_t slot = registeredGameCount++;
  while (slot > 0 && registeredGames[slot-1].order > game.order) {
    registeredGames[slot] = registeredGames[slot-1];
    --slot;
  }
  registeredGames[slot] = game;
  return true;
}
uint8_t gameCount() { return registeredGameCount; }
const GameModule *gameAt(uint8_t index) {
  return index < registeredGameCount ? &registeredGames[index] : nullptr;
}

// ============================================================
// DISPLAY
// ============================================================


Adafruit_ST7789 display =
  Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);


// ============================================================
// 3-POSITION SWITCHES
// ============================================================

// LEFT SWITCH

// RIGHT SWITCH


// ============================================================
// BUTTONS
// ============================================================

// Physical RIGHT button
// Button 1

// Physical LEFT button
// Button 2


// ============================================================
// BUZZERS
// ============================================================


#define BUZZER_1_FREQ 440
#define BUZZER_2_FREQ 660


// ============================================================
// SCREEN / APPLICATION STATES
// ============================================================

enum AppScreen {
  SCREEN_BOOT,
  SCREEN_MENU,
  SCREEN_GAME,
  SCREEN_MUSIC,
  SCREEN_SETTINGS,
  SCREEN_TEST
};

AppScreen currentScreen = SCREEN_BOOT;


// ============================================================
// MENU
// ============================================================

// New games appear before these built-in tools.
constexpr int BUILTIN_MENU_ITEMS = 3; // music, settings, hardware test
int menuCount() { return gameCount() + BUILTIN_MENU_ITEMS; }
const char *menuLabel(int index) {
  if (index < gameCount()) return gameAt(index)->title;
  switch (index - gameCount()) {
    case 0: return "MUSIC PLAYER";
    case 1: return "SETTINGS";
    default: return "HARDWARE TEST";
  }
}
const GameModule *activeGame = nullptr;
int menuTop = 0;
constexpr int VISIBLE_ROWS = 5;

int selectedMenuItem = 0;


// ============================================================
// INPUT STATE
// ============================================================

SwitchState leftSwitch = SWITCH_CENTER;
SwitchState rightSwitch = SWITCH_CENTER;

SwitchState previousLeftSwitch = SWITCH_CENTER;
SwitchState previousRightSwitch = SWITCH_CENTER;

bool leftButton = false;
bool rightButton = false;

bool previousLeftButton = false;
bool previousRightButton = false;


// ============================================================
// TIMING
// ============================================================

unsigned long lastInputTime = 0;

const unsigned long INPUT_DEBOUNCE = 120;


// ============================================================
// COLORS
// ============================================================



// ============================================================
// READ SWITCH
// ============================================================

SwitchState readSwitch(int upPin, int downPin) {

  bool up =
    digitalRead(upPin) == LOW;

  bool down =
    digitalRead(downPin) == LOW;

  if (up && !down) {
    return SWITCH_UP;
  }

  if (!up && down) {
    return SWITCH_DOWN;
  }

  if (!up && !down) {
    return SWITCH_CENTER;
  }

  return SWITCH_ERROR;
}


// ============================================================
// SWITCH NAME
// ============================================================

const char* switchName(SwitchState state) {

  switch (state) {

    case SWITCH_UP:
      return "UP";

    case SWITCH_CENTER:
      return "CENTER";

    case SWITCH_DOWN:
      return "DOWN";

    default:
      return "ERROR";
  }
}


// ============================================================
// INPUT UPDATE
// ============================================================

void updateInputs() {

  previousLeftSwitch = leftSwitch;
  previousRightSwitch = rightSwitch;

  previousLeftButton = leftButton;
  previousRightButton = rightButton;

  leftSwitch =
    readSwitch(
      LEFT_UP_PIN,
      LEFT_DOWN_PIN
    );

  rightSwitch =
    readSwitch(
      RIGHT_UP_PIN,
      RIGHT_DOWN_PIN
    );

  leftButton =
    digitalRead(BUTTON_2_PIN) == LOW;

  rightButton =
    digitalRead(BUTTON_1_PIN) == LOW;
}


// ============================================================
// BUTTON EDGE DETECTION
// ============================================================

bool leftButtonPressed() {

  return
    leftButton &&
    !previousLeftButton;
}


bool rightButtonPressed() {

  return
    rightButton &&
    !previousRightButton;
}


// ============================================================
// SWITCH EDGE DETECTION
// ============================================================

bool leftSwitchMovedUp() {

  return
    leftSwitch == SWITCH_UP &&
    previousLeftSwitch != SWITCH_UP;
}


bool leftSwitchMovedDown() {

  return
    leftSwitch == SWITCH_DOWN &&
    previousLeftSwitch != SWITCH_DOWN;
}


// ============================================================
// BUZZER HELPERS
// ============================================================

void beepRight() {

  ledcWriteTone(
    BUZZER_1_PIN,
    700
  );

  delay(25);

  ledcWriteTone(
    BUZZER_1_PIN,
    0
  );
}


void beepLeft() {

  ledcWriteTone(
    BUZZER_2_PIN,
    500
  );

  delay(25);

  ledcWriteTone(
    BUZZER_2_PIN,
    0
  );
}


// ============================================================
// HEADER
// ============================================================

void drawHeader(const char* title) {

  display.fillScreen(ST77XX_BLACK);

  display.setTextSize(2);
  display.setTextColor(ST77XX_WHITE);

  display.setCursor(12, 12);
  display.println(title);

  display.drawFastHLine(
    0,
    42,
    240,
    ST77XX_WHITE
  );
}


// ============================================================
// BOOT SCREEN
// ============================================================

void drawBootScreen() {

  display.fillScreen(ST77XX_BLACK);

  display.setTextColor(ST77XX_CYAN);
  display.setTextSize(3);

  display.setCursor(38, 90);
  display.println("SharkBoy");

  display.setTextColor(ST77XX_WHITE);
  display.setTextSize(2);

  display.setCursor(38, 135);
  display.println("Shark Boy");

  display.drawRect(
    30,
    190,
    180,
    20,
    ST77XX_WHITE
  );

  // fake loading animation without clearing screen

  for (int i = 0; i <= 172; i += 4) {

    display.fillRect(
      34,
      194,
      i,
      12,
      ST77XX_GREEN
    );

    delay(10);
  }

  delay(400);
}


// ============================================================
// MENU DRAWING
// ============================================================

void drawMenuItem(
  int index,
  bool selected
) {

  if (index < menuTop || index >= menuTop + VISIBLE_ROWS) return;
  int y = 62 + (index - menuTop) * 42;

  // Only erase this row.
  display.fillRect(
    5,
    y - 6,
    230,
    38,
    ST77XX_BLACK
  );

  display.setTextSize(2);

  if (selected) {

    display.fillRect(
      8,
      y - 5,
      224,
      32,
      ST77XX_BLUE
    );

    display.setTextColor(
      ST77XX_WHITE
    );

    display.setCursor(
      14,
      y
    );

    display.print("> ");

  } else {

    display.setTextColor(
      ST77XX_WHITE
    );

    display.setCursor(
      14,
      y
    );

    display.print("  ");
  }

  display.println(
    menuLabel(index)
  );
}


// ============================================================
// DRAW FULL MENU ONCE
// ============================================================

void drawMainMenu() {

  drawHeader(
    "MAIN MENU"
  );

  for (
    int i = 0;
    i < menuCount();
    i++
  ) {

    drawMenuItem(
      i,
      i == selectedMenuItem
    );
  }

  display.setTextSize(1);
  display.setTextColor(ST77XX_WHITE);

  display.setCursor(18, 267);
  display.print(selectedMenuItem + 1); display.print("/"); display.print(menuCount());

  display.setCursor(18, 285);
  display.println(
    "LEFT SWITCH: MOVE"
  );

  display.setCursor(18, 300);
  display.println(
    "RIGHT BUTTON: SELECT"
  );
}


// ============================================================
// MENU UPDATE
// ============================================================

void drawTestScreenStatic() {

  drawHeader(
    "HARDWARE TEST"
  );


  display.setTextSize(2);
  display.setTextColor(
    ST77XX_WHITE
  );


  display.setCursor(
    10,
    60
  );

  display.print(
    "L SW:"
  );


  display.setCursor(
    10,
    95
  );

  display.print(
    "R SW:"
  );


  display.drawFastHLine(
    0,
    128,
    240,
    ST77XX_WHITE
  );


  display.setCursor(
    10,
    145
  );

  display.print(
    "LEFT BTN:"
  );


  display.setCursor(
    10,
    180
  );

  display.print(
    "RIGHT BTN:"
  );


  display.drawFastHLine(
    0,
    215,
    240,
    ST77XX_WHITE
  );


  display.setTextSize(1);
  display.setTextColor(
    ST77XX_WHITE
  );

  display.setCursor(
    10,
    240
  );

  display.println(
    "LEFT BUTTON = BTN 2 / GPIO 27"
  );


  display.setCursor(
    10,
    255
  );

  display.println(
    "RIGHT BUTTON = BTN 1 / GPIO 14"
  );


  display.setCursor(
    10,
    285
  );

  display.println(
    "LEFT BTN + RIGHT DOWN = EXIT"
  );


  // Force first update

  previousLeftSwitch =
    SWITCH_ERROR;

  previousRightSwitch =
    SWITCH_ERROR;

  previousLeftButton =
    !leftButton;

  previousRightButton =
    !rightButton;
}


// ============================================================
// DRAW SWITCH VALUE
// ============================================================

void drawSwitchValue(
  int y,
  SwitchState state
) {

  display.fillRect(
    95,
    y,
    140,
    22,
    ST77XX_BLACK
  );

  display.setCursor(
    95,
    y
  );

  display.setTextSize(2);


  switch (state) {

    case SWITCH_UP:

      display.setTextColor(
        ST77XX_GREEN
      );

      break;


    case SWITCH_CENTER:

      display.setTextColor(
        ST77XX_YELLOW
      );

      break;


    case SWITCH_DOWN:

      display.setTextColor(
        ST77XX_CYAN
      );

      break;


    default:

      display.setTextColor(
        ST77XX_RED
      );

      break;
  }


  display.print(
    switchName(state)
  );
}


// ============================================================
// DRAW BUTTON VALUE
// ============================================================

void drawButtonValue(
  int y,
  bool pressed
) {

  display.fillRect(
    145,
    y,
    90,
    22,
    ST77XX_BLACK
  );

  display.setCursor(
    145,
    y
  );

  display.setTextSize(2);


  if (pressed) {

    display.setTextColor(
      ST77XX_GREEN
    );

    display.print(
      "DOWN"
    );

  } else {

    display.setTextColor(
      ST77XX_RED
    );

    display.print(
      "UP"
    );
  }
}


// ============================================================
// UPDATE TEST SCREEN
// ============================================================

void updateTestScreen() {

  // Switch values only redraw when changed

  if (
    leftSwitch !=
    previousLeftSwitch
  ) {

    drawSwitchValue(
      60,
      leftSwitch
    );
  }


  if (
    rightSwitch !=
    previousRightSwitch
  ) {

    drawSwitchValue(
      95,
      rightSwitch
    );
  }


  // Buttons only redraw when changed

  if (
    leftButton !=
    previousLeftButton
  ) {

    drawButtonValue(
      145,
      leftButton
    );
  }


  if (
    rightButton !=
    previousRightButton
  ) {

    drawButtonValue(
      180,
      rightButton
    );
  }


  // Buzzers while buttons are held

  if (leftButton) {

    ledcWriteTone(
      BUZZER_2_PIN,
      BUZZER_2_FREQ
    );

  } else {

    ledcWriteTone(
      BUZZER_2_PIN,
      0
    );
  }


  if (rightButton) {

    ledcWriteTone(
      BUZZER_1_PIN,
      BUZZER_1_FREQ
    );

  } else {

    ledcWriteTone(
      BUZZER_1_PIN,
      0
    );
  }


  // Hold LEFT button + move RIGHT switch DOWN
  // to exit test screen.
  //
  // This avoids instantly leaving the test screen
  // when you're trying to test the left button.

  if (
    leftButton &&
    rightSwitch == SWITCH_DOWN
  ) {

    ledcWriteTone(
      BUZZER_1_PIN,
      0
    );

    ledcWriteTone(
      BUZZER_2_PIN,
      0
    );

    delay(150);

    currentScreen =
      SCREEN_MENU;

    drawMainMenu();
  }
}


// ============================================================
// SETUP
// ============================================================



// Launcher: left switch navigates; right button selects; left button exits.
// In games, hold both buttons for 700 ms to return; individual button taps
// remain available to the game. The same gesture works in the music screen.
uint32_t exitChordStarted = 0;
int settingsRow = 0;
int shownTestLeft = -1, shownTestRight = -1;
void drawMusicScreen() {
  drawHeader("MUSIC PLAYER");
  display.setTextSize(2);
  display.setTextColor(ST77XX_CYAN);
  display.setCursor(15, 85); display.print("ABC MUSIC");
  display.setTextColor(ST77XX_WHITE);
  display.setCursor(15, 150); display.print("Playback: ");
  display.print(Music::enabled ? "ON" : "OFF");
  display.setCursor(15, 185); display.print("Volume: ");
  display.print(Music::volume); display.print("%");
  display.setTextSize(1);
  display.setCursor(12, 275); display.print("Settings: change playback / volume");
  display.setCursor(12, 295); display.print("Hold BOTH buttons: menu");
}
void drawSettings() {
  drawHeader("SETTINGS");
  display.setTextSize(2);
  display.setCursor(12, 80);
  display.setTextColor(settingsRow == 0 ? ST77XX_YELLOW : ST77XX_WHITE);
  display.print("MUSIC: "); display.print(Music::enabled ? "ON " : "OFF");
  display.setCursor(12, 130);
  display.setTextColor(settingsRow == 1 ? ST77XX_YELLOW : ST77XX_WHITE);
  display.print("VOLUME: "); display.print(Music::volume); display.print("%   ");
  display.setTextSize(1); display.setTextColor(ST77XX_WHITE);
  display.setCursor(12, 215); display.print("LEFT SWITCH: choose setting");
  display.setCursor(12, 232); display.print("RIGHT SWITCH: change value");
  display.setCursor(12, 249); display.print("RIGHT BUTTON: toggle / volume +");
  display.setCursor(12, 266); display.print("LEFT BUTTON: menu");
}
void enterMenu() {
  const bool returningFromTest = currentScreen == SCREEN_TEST;
  activeGame = nullptr;
  currentScreen = SCREEN_MENU;
  exitChordStarted = 0;
  if (returningFromTest && Music::enabled) Music::startMusic();
  drawMainMenu();
}
void launch(int item) {
  if (item < gameCount()) {
    activeGame = gameAt(item);
    currentScreen = SCREEN_GAME;
    exitChordStarted = 0;
    activeGame->enter();
    return;
  }
  switch (item - gameCount()) {
    case 0: currentScreen = SCREEN_MUSIC; drawMusicScreen(); break;
    case 1: currentScreen = SCREEN_SETTINGS; drawSettings(); break;
    case 2:
      if (Music::enabled) Music::stopMusic();
      currentScreen = SCREEN_TEST;
      drawTestScreenStatic();
      drawSwitchValue(60, leftSwitch);
      drawSwitchValue(95, rightSwitch);
      drawButtonValue(145, leftButton);
      drawButtonValue(180, rightButton);
      break;
  }
}
void updateMainMenu() {
  int old = selectedMenuItem;
  if (leftSwitchMovedUp()) selectedMenuItem = (selectedMenuItem + menuCount() - 1) % menuCount();
  if (leftSwitchMovedDown()) selectedMenuItem = (selectedMenuItem + 1) % menuCount();
  if (old != selectedMenuItem) {
    int oldTop = menuTop;
    if (selectedMenuItem < menuTop) menuTop = selectedMenuItem;
    if (selectedMenuItem >= menuTop + VISIBLE_ROWS) menuTop = selectedMenuItem - VISIBLE_ROWS + 1;
    if (oldTop != menuTop) drawMainMenu();
    else {
      drawMenuItem(old, false);
      drawMenuItem(selectedMenuItem, true);
      display.fillRect(15, 264, 80, 14, ST77XX_BLACK);
      display.setTextSize(1); display.setTextColor(ST77XX_WHITE);
      display.setCursor(18, 267); display.print(selectedMenuItem + 1);
      display.print("/"); display.print(menuCount());
    }
  }
  if (rightButtonPressed()) launch(selectedMenuItem);
}
void setup() {
  Serial.begin(115200);
  pinMode(LEFT_UP_PIN, INPUT_PULLUP); pinMode(LEFT_DOWN_PIN, INPUT_PULLUP);
  pinMode(RIGHT_UP_PIN, INPUT_PULLUP); pinMode(RIGHT_DOWN_PIN, INPUT_PULLUP);
  pinMode(BUTTON_1_PIN, INPUT_PULLUP); pinMode(BUTTON_2_PIN, INPUT_PULLUP);
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  display.init(240,320); display.setRotation(0); display.setTextWrap(false);
  Music::enter();
  leftSwitch = readSwitch(LEFT_UP_PIN, LEFT_DOWN_PIN);
  rightSwitch = readSwitch(RIGHT_UP_PIN, RIGHT_DOWN_PIN);
  previousLeftSwitch = leftSwitch; previousRightSwitch = rightSwitch;
  leftButton = digitalRead(BUTTON_2_PIN) == LOW;
  rightButton = digitalRead(BUTTON_1_PIN) == LOW;
  previousLeftButton = leftButton; previousRightButton = rightButton;
  drawBootScreen(); enterMenu();
}
void loop() {
  updateInputs();
  if (Music::enabled && currentScreen != SCREEN_TEST) Music::tick();
  if (currentScreen == SCREEN_MENU) updateMainMenu();
  else if (currentScreen == SCREEN_SETTINGS) {
    int old = settingsRow;
    if (leftSwitchMovedUp()) settingsRow = 0;
    if (leftSwitchMovedDown()) settingsRow = 1;
    bool changed = old != settingsRow;
    if (settingsRow == 0 && (rightButtonPressed() || rightSwitch != previousRightSwitch && rightSwitch != SWITCH_CENTER)) {
      Music::setEnabled(!Music::enabled); changed = true;
    } else if (settingsRow == 1) {
      int delta = 0;
      if (rightSwitch == SWITCH_UP && previousRightSwitch != SWITCH_UP) delta = 10;
      if (rightSwitch == SWITCH_DOWN && previousRightSwitch != SWITCH_DOWN) delta = -10;
      if (rightButtonPressed()) delta = 10;
      if (delta) { Music::setVolume(constrain((int)Music::volume + delta, 0, 100)); changed = true; }
    }
    if (changed) drawSettings();
    if (leftButtonPressed()) enterMenu();
  } else if (currentScreen == SCREEN_TEST) {
    updateTestScreen();
    if (currentScreen == SCREEN_MENU) enterMenu();
  } else {
    // Both buttons held prevents a quick accidental exit while playing.
    if (leftButton && rightButton) {
      if (!exitChordStarted) exitChordStarted = millis();
      if (millis() - exitChordStarted >= 700) { enterMenu(); return; }
    } else exitChordStarted = 0;
    if (currentScreen == SCREEN_GAME && activeGame) {
      const GameInput input = {leftButton, rightButton,
                               leftButtonPressed(), rightButtonPressed()};
      activeGame->update(input);
    }
    if (currentScreen == SCREEN_MUSIC && leftButtonPressed()) enterMenu();
  }
  delay(2);
}
