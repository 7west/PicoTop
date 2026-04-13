#pragma once

#include "calc_types.h"





// saves calc work to file system, will prompt user for file name
bool calc_save_file(calc_cell_t *cells, calc_state_t state);

// loads calc work from file system, will prompt user for file name
bool calc_load_file(calc_cell_t *cells, calc_state_t *state);