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

#ifndef LOVE_LH_WATCHDOG_H
#define LOVE_LH_WATCHDOG_H

namespace love
{
namespace lh
{

// A development aid for hangs: LHATOVE_WATCHDOG=<seconds> starts a thread
// that, once the main thread has been silent for that long, writes the
// main thread's stack to stderr (twice, a second apart) as module base +
// offset and ends the process. "Silent" means no call to kickWatchdog()
// -- the boot loop kicks once per frame. Windows x64 only; resolve the
// offsets with scripts/symbolize.c against a RelWithDebInfo build's .pdb.
void startWatchdog();
void kickWatchdog();

} // lh
} // love

#endif // LOVE_LH_WATCHDOG_H
