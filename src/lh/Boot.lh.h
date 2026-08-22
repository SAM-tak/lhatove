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

#ifndef LOVE_LH_BOOT_LH_H
#define LOVE_LH_BOOT_LH_H

// L^ units the engine embeds and serves through the loader's hold table.
// Raw string literals, the way boot.lua used to be included.

namespace love
{
namespace lh
{

// Milestone M0: what runs when no game is given. Exercises the boundary
// that every later module relies on -- a global, a module function, a
// tuple answer, and the two open questions from the porting plan:
//   U1: a host function answering an error value its signature declares;
//   U4: a host function answering a structural table type with p^ members.
static const char hello_main_lh[] = R"lh(import^ love

print("lhatove: hello from L^")

let^ major, minor, revision, codename = love.getVersion()
print($"love.getVersion() -> {major}.{minor}.{revision} {codename}")

let^ answer = love.probe.failing() catch^ -1
print($"U1 probe: failing() caught as {answer}")

return^ 0
)lh";

// `lovec --probe`: the U4 shape on its own. Kept apart from the hello unit
// because lhat HEAD ad39df0 frees the signature text while the structural
// type still points into it (docs/porting/lhat-issues.md), so this check
// passes or fails on what the freed bytes happen to hold.
static const char probe_main_lh[] = R"lh(import^ love

let^ h = love.probe.handlers()
h.update(0.5)
h.draw()
print("U4 probe: handlers table called back into the host")

return^ 0
)lh";

} // lh
} // love

#endif // LOVE_LH_BOOT_LH_H
