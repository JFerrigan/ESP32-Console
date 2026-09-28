# Space Evaders — Install

## Files to copy into `Jakeboy_Arcade/`

- `Game_SpaceEvaders.ino`
- `SpaceEvaders.h`

No launcher edit is required for the core game. `Game_SpaceEvaders.ino` registers the game as menu order **70**. If your current local sketch already uses order 70, change only that number in the registration line.

## Core integration

The game uses the launcher-owned global `display` from `GameAPI.h`. It does not create another ST7789 object, call `SPI.begin()`, call `display.init()`, define `setup()`, or define `loop()`.

The game reads the two three-position switches directly through `Hardware.h` and receives button state/edges through `GameInput`.

The launcher remains responsible for its global both-button exit gesture. Space Evaders does not alter the exit hold duration.

## Sound status

This strict drop-in build intentionally uses a silent internal audio adapter. The current launcher/music design has no shared SFX arbitration API, so writing to the buzzers directly from this game could fight `Music::tick()` or leave a tone behind after the launcher exits the game.

Gameplay, visuals, scoring, collision, rematch, and input are implemented without launcher changes. A later shared-audio integration can replace the no-op adapter without changing game rules.

## Device verification

After copying the two files:

1. Open `Jakeboy_Arcade.ino` in Arduino IDE.
2. Verify/compile the entire sketch.
3. Upload it to the ESP32.
4. Confirm `SPACE EVADERS` appears in the menu.
5. Confirm left UP moves toward negative raw Y and left DOWN toward positive raw Y.
6. Confirm right UP also moves toward negative raw Y and right DOWN toward positive raw Y; this is intentional because the two player views differ by 180 degrees.
7. Confirm holding a fire button does not repeat shots; a stable release and new click are required.
8. Confirm each player can have at most two active shots.
9. Confirm friendly and enemy shots both damage bunker cells.
10. Confirm opposing bolts can intercept each other.
11. Confirm a same-simulation-tick double hit produces a draw with no point.
12. Confirm first to three wins reaches match-over, and both players must independently click to ready the rematch.
13. Confirm the launcher exit gesture still works during every phase.
14. Watch for rendering artifacts or frame pacing on the physical TFT; desktop syntax checks cannot validate SPI/TFT performance.
