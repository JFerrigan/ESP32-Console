# Jakeboy Arcade

Open **Jakeboy_Arcade.ino** in Arduino IDE. Keep every file in this directory; Arduino compiles all `.ino` tabs as one program and includes the game headers. Install the ESP32 board package, Adafruit GFX, and Adafruit ST7789 libraries. Select your ESP32 DevKit/WROOM board and upload.

## Files

- `Jakeboy_Arcade.ino`: boot, menu, settings, hardware test, input and game registry.
- `GameAPI.h`: registration interface and `GameInput` contract.
- `Hardware.h`: shared pin assignments.
- `Game_DeepVector.ino` + `DeepVector.h`: Deep Vector registration and implementation.
- `Game_IceColdBeer.ino` + `IceColdBeer.h`: Ice Cold Beer registration and implementation.
- `AudioPlayer.ino` + `MusicPlayer.h`: ABC song parser/player.
- `GAME_TEMPLATE.txt`: copy into a new `.ino` file to add a game.

To add a game, copy `GAME_TEMPLATE.txt` to `Game_YourName.ino` in this folder, give it a unique namespace, title and order, then implement `enter()` and `update(const GameInput&)`. Its `REGISTER_GAME` line adds it to the menu automatically. No edit to the main sketch is required until the 12 game slots are full. All files are part of one ESP32 firmware image, not separately uploaded programs.

## Controls

Either switch up/down: navigate the main menu. Right button: select. The left button does nothing on the main menu. In Settings, the left button goes back and the right button changes the selected setting. Hold both buttons together for 2 seconds in a game to return. The switches remain available to each game on GPIO 33/32 and 26/25. Left button GPIO 27; right button GPIO 14. Music uses buzzers GPIO 22 and 21. Hardware Test exits with left button held and right switch down. Settings control music on/off and volume.

The games retain their existing controls. Deep Vector uses either button to shoot. Ice Cold Beer uses its two three-position switches.

The current ESP32 environment was unavailable here, so compile and on-device behavior should be checked in Arduino IDE before treating this as validated firmware.
