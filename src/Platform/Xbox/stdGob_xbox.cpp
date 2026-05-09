/* stdGob_xbox.cpp — compile stdGob.c as C++ with C linkage for Xbox
 * Location: src/Platform/Xbox/stdGob_xbox.cpp
 * VS2005 C89 mode can't handle mid-block declarations in stdGob.c.
 * This wrapper compiles it as C++ while preserving C linkage. */
extern "C" {
#include "../../Win95/stdGob.c"
}
