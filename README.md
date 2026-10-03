# Jakeboy Arcade

Open **Jakeboy_Arcade.ino** in Arduino IDE. Keep every file in this directory; Arduino compiles all `.ino` tabs as one program and includes the game headers. Install the ESP32 board package, Adafruit GFX, and Adafruit ST7789 libraries. Select your ESP32 DevKit/WROOM board and upload.

## Files

- `Jakeboy_Arcade.ino`: boot, menu, settings, hardware test, input and game registry.
- `GameAPI.h`: registration interface and `GameInput` contract.
- `Hardware.h`: shared pin assignments.
- `Game_DeepVector.ino` + `DeepVector.h`: Deep Vector registration and implementation.
- `Game_IceColdBeer.ino` + `IceColdBeer.h`: Ice Cold Beer registration and implementation.
- `AudioPlayer.ino` + `MusicPlayer.h` + `ArcadeSongs.h`: two-buzzer ABC player and 10-song library.
- `GAME_TEMPLATE.txt`: copy into a new `.ino` file to add a game.

To add a game, copy `GAME_TEMPLATE.txt` to `Game_YourName.ino` in this folder, give it a unique namespace, title and order, then implement `enter()` and `update(const GameInput&)`. Its `REGISTER_GAME` line adds it to the menu automatically. No edit to the main sketch is required until the 12 game slots are full. All files are part of one ESP32 firmware image, not separately uploaded programs.

## Controls

Either switch up/down: navigate the main menu. Right button: select. The left button does nothing on the main menu. In Settings, the left button goes back and the right button changes the selected setting. Hold both buttons together for 2 seconds in a game to return. The switches remain available to each game on GPIO 33/32 and 26/25. Left button GPIO 27; right button GPIO 14. Music uses buzzers GPIO 22 and 21. Hardware Test exits with left button held and right switch down. Settings control music on/off and volume.

The games retain their existing controls. Deep Vector uses either button to shoot. Ice Cold Beer uses its two three-position switches. Music defaults to ON but the main menu is silent. Each assigned game starts its own track. Selecting a song in Music Player keeps that song for later games. Turning Music OFF in Settings mutes songs and game effects; starting playback in Music Player turns the global setting back ON.

## Music Player

The Music Player has 10 original two-buzzer arrangements: D Minor Nocturne (classical, 76 BPM), Neon Sprint (synthwave, 148), Harbor Waltz (waltz, 96), Dust Road (country, 112), Midnight Train (blues, 132), Cloudstep (dance, 124), Cave Echo (ambient, 58), Pixel Parade (chiptune, 160), Sunset Bossa (bossa, 104), and Iron March (cinematic, 138).

Use the left switch to select Song, Playback, or Volume. Use the right switch to move between songs or change the selected setting. The right button advances the song, toggles playback, or raises volume. The left button returns to the menu. In the desktop simulator, the mouse wheel selects a row and a click changes it.

Volume changes in 5% steps. Its lower settings use a gentler output curve so quiet playback is easier to set.

Game themes: Ice Cold Beer → Harbor Waltz; Fishing Trawler → Midnight Train; Deep Vector → Cave Echo; Tank → Iron March; Space Evaders → Neon Sprint; Pong → Pixel Parade; Deep Hook → Sunset Bossa; Odd Stride → Dust Road; Meteor Sweep → Cloudstep; Skyhook and Scrap Claw → D Minor Nocturne. Horizon Burn plays its own beat-synchronized tracks: Easy → Afterglow Circuit (96 BPM), Normal → Rail Pulse (112 BPM), Hard → Steel Current (120 BPM), Expert → Signal Forge (140 BPM), and Master → Voltage Run (160 BPM). A song chosen in Music Player takes priority over these tracks.

The full arcade compiles for the ESP32 DevKit/WROOM profile with PSRAM disabled. Check timing, sound, and memory behavior on the physical board before treating the firmware as hardware validated.
