# JSON_C
JSON file parsing for c.

This is written in one file so that it can be ported easily to another project.
To do so put the file in the other project and before including it define JSON_IMPLEMENTATION
for the function definitions to be included.
Do not define JSON_IMPLEMENTATION in multiple files otherwise the functions will be defined multiple times which will lead to linker errors.

The best way to do it is to create a .c file in which the only thing you do is define JSON_IMPLEMENTATION and then include this file.
Or copy paste the following two lines :

#define JSON_IMPLEMENTATION
#include "json.h"