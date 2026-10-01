#include "GameAPI.h"
#include "OddStride.h"

REGISTER_GAME_EX(230, "ODD STRIDE",
                 OddStride::enter, OddStride::update,
                 OddStride::leave, OddStride::allowMenuExit, true);
