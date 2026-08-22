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

#ifndef LOVE_LH_PHYSFSLOADER_H
#define LOVE_LH_PHYSFSLOADER_H

#include "common/config.h"

#include <lhat.h>

#include <map>
#include <string>

namespace love
{
namespace lh
{

// The program's LhatProgramLoader. The language takes its loader rather
// than defaulting to stdio (port.h), which is what lets units come from
// wherever LOVE mounts a game.
//
// Two sources, asked in order:
//   1. held units -- text the engine embeds (Boot.lh, nogame.lh) or
//      substitutes, served under a path the program asks for;
//   2. the game's files. For now this reads through stdio relative to a
//      base directory; milestone M2 routes it through love.filesystem
//      (PhysFS), which is why the class already carries that name.
class Loader
{
public:

	// Serves `text` whenever the program asks for `path`.
	void hold(const std::string &path, const std::string &text);

	// Directory the game's units are read from (empty: none).
	void setBase(const std::string &base);
	const std::string &getBase() const { return base; }

	// The callback handed to lhat_program_new, with `this` as its context.
	static char *load(void *context, const char *path, size_t *length);

private:

	char *loadUnit(const char *path, size_t *length) const;

	std::map<std::string, std::string> held;
	std::string base;
};

} // lh
} // love

#endif // LOVE_LH_PHYSFSLOADER_H
