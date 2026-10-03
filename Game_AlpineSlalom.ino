// Jakeboy Arcade registration bridge for Alpine Slalom.
#include "AlpineSlalom.h"

#if defined(GAME_API_LIFECYCLE_VERSION) && GAME_API_LIFECYCLE_VERSION >= 1
REGISTER_GAME_EX(
  80,
  "ALPINE SLALOM",
  AlpineSlalom::enter,
  AlpineSlalom::update,
  AlpineSlalom::leave,
  AlpineSlalom::allowMenuExit,
  true
);
#else
REGISTER_GAME(
  80,
  "ALPINE SLALOM",
  AlpineSlalom::enter,
  AlpineSlalom::update
);
#endif
