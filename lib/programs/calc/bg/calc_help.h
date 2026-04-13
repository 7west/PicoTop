#pragma once

// lines cannot be longer than 80 char. This is 80 char:
//   12345678901234567890123456789012345678901234567890123456789012345678901234567890
static const char* CALC_HELP[28] = {
    " - Enter lets you switch between NAV and EDIT modes",
    " - available functions: sin, cos, tan, atan2, acos, asin, atan, ln, log10, log2,",
    "   sqrt, round, exp, abs",
    " - available constants: pi, e, ans (result from previous cell, starts at 0)",
    " - Cell Commands",
    "      ^A - Add cell after current cell, ^D - Delete current cell, ^Up/Down - ",
    "      move current cell up/down (NAV only), ^Z - Undo recent changes to cell",
    " - Other Commands",
    "      ^R - Radians/degrees mode, ^S - Save workbook, ^O - Open workbook",
    "      you can only save/load in NAV mode",
    " - Numbers can be entered in decimal, hex (0x), binary (0b), or scientific",
    "   notation (##e##)",
    " ",
    " ",
    " ",
    " ",
    " ",
    " ",
    " ",
    " ",
    " ",
    " ",
    " ",
    " ",
    " ",
    " ",
    " ",
    "^ means Ctrl, in case you didn't know",
};