#include "GameAPI.h"
#include "Tank.h"

namespace TankGame {
void enter() { Tank::enter(); }
void update(const GameInput &input) { Tank::tick(input); }
}

REGISTER_GAME(30, "TANK", TankGame::enter, TankGame::update);
