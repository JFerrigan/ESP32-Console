#include "TripPong.h"

// Drop-in Jakeboy Arcade registration tab.
// The implementation lives in TripPong.h to avoid Arduino's automatic
// prototype generator placing prototypes ahead of the game's custom structs.
REGISTER_GAME(50, "PONG", Pong::enter, Pong::update);
