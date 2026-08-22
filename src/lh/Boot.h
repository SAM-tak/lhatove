/**
 * Copyright (c) 2006-2026 LOVE Development Team
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 **/

#ifndef LOVE_LH_BOOT_H
#define LOVE_LH_BOOT_H

#include "common/config.h"

#ifdef __cplusplus
extern "C"
{
#endif

// Runs a game: the L^ successor of boot.lua + callbacks.lua + the Lua-state
// setup that src/love.cpp used to do. Exported from liblove so the thin
// love/lovec executables stay in love.cpp.
//
// `console` says the caller is lovec (or any build that writes to a terminal
// of its own): problems go to stderr only, never to a message box.
//
// Answers the process exit code.
LOVE_EXPORT int love_lh_boot(int argc, char **argv, bool console);

#ifdef __cplusplus
}
#endif

#endif // LOVE_LH_BOOT_H
