#include "HorizonBurn.h"

#if defined(GAME_API_LIFECYCLE_VERSION) && GAME_API_LIFECYCLE_VERSION >= 1
REGISTER_GAME_EX(70, "HORIZON BURN",
    HorizonBurn::enter, HorizonBurn::update,
    HorizonBurn::leave, HorizonBurn::allowMenuExit, true);
#else
REGISTER_GAME(70, "HORIZON BURN", HorizonBurn::enter, HorizonBurn::update);
#endif
