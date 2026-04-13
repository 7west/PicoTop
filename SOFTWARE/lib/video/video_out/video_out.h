// Handles all the HSTX, TMDS, DMA, and IRQ stuff on core1
// main calls core1, not here
#pragma once

#include <stdio.h>

#define VIDEO_H_RES 640
#define VIDEO_V_RES 480

void video_out_setup(void);

void video_out_pause(void);

void video_out_resume(void);