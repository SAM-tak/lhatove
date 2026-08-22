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

#ifndef LOVE_LH_ERRORSCREEN_H
#define LOVE_LH_ERRORSCREEN_H

#include "common/config.h"

#include <string>

namespace love
{
namespace lh
{

// The blue screen: callbacks.lua's love.errorhandler, written against the
// engine directly since it must work when the L^ side is the thing that
// broke. Shows `message` in the window (opening one if needed) until the
// user closes it or presses Escape. Without a window module it only prints.
// Answers true when it could show the screen.
bool showErrorScreen(const std::string &message);

} // lh
} // love

#endif // LOVE_LH_ERRORSCREEN_H
