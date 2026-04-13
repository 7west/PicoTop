#include <stdio.h>

static const char info_doc_string[] = 
"WestOS Information File:\n"
"\n"
"WestOS is a keyboard-based operating system operating on an RP2350 microcontroller. Here are some basic tips:\n"
" - The 'help' command can be called to list all commands or appended to any command to see what it does.\n"
" - The file system is a STRICT FAT16 file system following the 8.3 file naming convention.\n"
"      - This means files and folders can only have 8 characters for the name and 3 for the extension\n"
"\n"
"Available programs:\n"
"shell - runs at startup, launches all other commands and programs\n"
"uEdit - basic text editor\n"
"calc - calculator workbook\n"
"hangman - Fun little hangman game";

#define INFO_DOC_LEN (sizeof(info_doc_string) - 1)