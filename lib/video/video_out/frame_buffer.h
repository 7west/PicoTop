#pragma once

#include <stdint.h>

#ifndef _IMG_ASSET_SECTION
#define _IMG_ASSET_SECTION ".data"
#endif

extern uint8_t framebuf[];

#ifdef FRAME_BUFFER_IMPLEMENTATION 

// uint8_t __attribute__((aligned(4), section(_IMG_ASSET_SECTION ".mountains_640x480"))) 
// 		framebuf[640 * 480] = { 0x00 };

uint8_t __attribute__((section(".uninitialized_data"))) framebuf[640 * 480];

#endif