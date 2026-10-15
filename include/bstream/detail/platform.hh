#pragma once

// Keep Windows headers from defining macros that collide with the standard
// library. Handle the case where windows.h was included before bstream too.
#if defined(_WIN32) && !defined(NOMINMAX)
#define NOMINMAX
#endif

#if defined(min)
#undef min
#endif
#if defined(max)
#undef max
#endif
