#pragma once

// lines cannot be longer than 80 char. This is 80 char:
//   12345678901234567890123456789012345678901234567890123456789012345678901234567890
static const char* CALC_HELP[28] = {
    " - Enter lets you switch between NAV and EDIT modes",
    " - available functions: sin, cos, tan, atan2 (real only), acos, asin, atan, ln,",
    "    log10, log2, sqrt, round, exp, abs, fact, gcd, lcm",
    " - ^ is bitwise XOR, ** is exponent (e.g. 2**3 = 8)",
    " - available constants: pi, e, i, ans (result from previous cell, starts at 0)",
    "      numbers must be multiplied by 'i' (e.g. 3*i = 3i)"
    " - Cell Commands",
    "      ^A - Add cell after current cell, ^D - Delete current cell, ^Up/Down - ",
    "      move current cell up/down (NAV only), ^Z - Undo recent changes to cell",
    " - Other Commands",
    "      ^R - Radians/degrees mode, ^S - Save workbook, ^O - Open workbook",
    "      you can only save/load in NAV mode",
    " - Numbers can be entered in decimal, hex (0x), binary (0b), or scientific",
    "   notation (##e##)",
    " - Commands:",
    "      let var = 4 + 2",
    "      nderiv equation, variable, at variable = #",
    "        ex: nderiv x**2, x, 1",
    "      solve equation equal to 0, variable, initial guess [optional]",
    "        ex: solve x**2 - 2, x",
    "        CANNOT solve for imaginary roots :(",
    "      nint equation, variable, lower bound, upper bound",
    "        ex: nint x**2, x, 0, 1",
    " ",
    " ",
    " ",
    " ",
    " ^ means Ctrl, in case you didn't know",
};