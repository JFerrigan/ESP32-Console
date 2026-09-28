#include "GameAPI.h"
#include "Hardware.h"
#include "TankGame.h"

REGISTER_GAME(30, "TANK", tank::enter, tank::update);
