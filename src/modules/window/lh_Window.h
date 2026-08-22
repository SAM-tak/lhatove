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

#ifndef LOVE_WINDOW_LH_WINDOW_H
#define LOVE_WINDOW_LH_WINDOW_H

#include "Window.h"

#include <lhat.h>

#include <string>

namespace love
{
namespace window
{

// Fills `settings` from the fields of an L^ table (the third argument of
// love.window.setMode, and conf.lh's window table), as wrap_Window.cpp's
// readWindowSettings did. A field that is absent keeps the value already in
// `settings`. False, with `error` set, for a bad fullscreentype.
bool readWindowSettings(LhatMachine *machine, LhatValue table, WindowSettings &settings, std::string &error);

} // window
} // love

#endif // LOVE_WINDOW_LH_WINDOW_H
