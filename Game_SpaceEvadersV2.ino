#include "GameAPI.h"
#include "Hardware.h"
#include "SpaceEvadersV2.h"

// Drop-in Jakeboy Arcade registration. Change 70 only if that order is already used.
REGISTER_GAME(70, "SPACE EVADERS V2", SpaceEvadersV2::enter, SpaceEvadersV2::update);
