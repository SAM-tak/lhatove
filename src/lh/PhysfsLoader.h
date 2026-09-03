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
#include <set>
#include <string>

namespace love
{
namespace filesystem
{
class Filesystem;
}

namespace lh
{

// The program's LhatProgramLoader. The language takes its loader rather
// than defaulting to stdio (port.h), which is what lets units come from
// wherever LOVE mounts a game.
//
// Two sources, asked in order:
//   1. held units -- text the engine embeds (Boot.lh, nogame.lh) or
//      substitutes, served under a path the program asks for;
//   2. love.filesystem: the game's source (a directory, a .love archive or
//      the fused executable) and the save directory, one PhysFS namespace,
//      which is what lets one loader serve one program.
class Loader
{
public:

	// Serves `text` whenever the program asks for `path`.
	void hold(const std::string &path, const std::string &text);

	// 05 の 10 章: the same for bytes -- a unit this engine embeds compiled,
	// which is what a build without the front end holds (they are text in a
	// build that can read text). NUL is a byte like any other here, so the
	// length is given rather than measured.
	void hold(const std::string &path, const void *bytes, size_t length);
	bool isHeld(const std::string &path) const;

	// Reads game units through this module from now on.
	void setFilesystem(love::filesystem::Filesystem *fs) { filesystem = fs; }

	// Whether `path` can be read by this loader (held, or a file in the
	// mounted filesystem).
	bool exists(const std::string &path) const;

	// The callback handed to lhat_program_new, with `this` as its context.
	static char *load(void *context, const char *path, size_t *length);

	// 05 の 10 章: whether the unit at this path came over as compiled bytes
	// rather than text. A unit that shipped compiled has no checker type to
	// ask about, so what depends on one is skipped rather than answered
	// wrongly (Boot.cpp's callback check). Answered for held units too --
	// a build without the front end holds its own compiled.
	bool isBinary(const std::string &path) const
	{
		return binary.find(path) != binary.end();
	}

private:

	char *loadUnit(const char *path, size_t *length) const;

	std::map<std::string, std::string> held;
	love::filesystem::Filesystem *filesystem = nullptr;
	mutable std::set<std::string> binary;
};

} // lh
} // love

#endif // LOVE_LH_PHYSFSLOADER_H
