#pragma once

#include "boards/pico2.h"

// pico_cmake_set_default PICO_FLASH_SIZE_BYTES = (16 * 1024 * 1024)

#undef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (16 * 1024 * 1024)