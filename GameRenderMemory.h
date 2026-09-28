#pragma once
#include <stdint.h>

// These render buffers are used by one game at a time. Tank needs the
// largest combined frame/depth allocation; the other games reuse it.
namespace GameRenderMemory {
constexpr unsigned FRAME_BYTES = 120u * 160u;
constexpr unsigned DEPTH_BYTES = 120u * 128u;
constexpr unsigned CAPACITY = FRAME_BYTES + DEPTH_BYTES;
alignas(4) extern uint8_t bytes[CAPACITY];
}
