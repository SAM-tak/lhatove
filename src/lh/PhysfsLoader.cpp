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

#include "PhysfsLoader.h"

#include "common/Exception.h"
#include "common/Object.h"
#include "modules/filesystem/Filesystem.h"
#include "modules/filesystem/FileData.h"

#include <cstring>

namespace love
{
namespace lh
{

// The program frees what the loader hands back with lhat_free, so the copy
// has to come from lhat_alloc. One byte past the end keeps it NUL terminated
// whatever it holds.
static char *copyOf(const void *text, size_t size, size_t *length)
{
	char *buffer = (char *) lhat_alloc(size + 1);
	if (buffer == nullptr)
		return nullptr;
	if (size > 0)
		memcpy(buffer, text, size);
	buffer[size] = '\0';
	*length = size;
	return buffer;
}

void Loader::hold(const std::string &path, const std::string &text)
{
	held[path] = text;
}

bool Loader::isHeld(const std::string &path) const
{
	return held.find(path) != held.end();
}

bool Loader::exists(const std::string &path) const
{
	if (isHeld(path))
		return true;
	if (filesystem == nullptr)
		return false;
	love::filesystem::Filesystem::Info info = {};
	return filesystem->getInfo(path.c_str(), info) && info.type == love::filesystem::Filesystem::FILETYPE_FILE;
}

char *Loader::load(void *context, const char *path, size_t *length)
{
	return ((const Loader *) context)->loadUnit(path, length);
}

char *Loader::loadUnit(const char *path, size_t *length) const
{
	auto it = held.find(path);
	if (it != held.end())
		return copyOf(it->second.data(), it->second.size(), length);

	if (filesystem == nullptr)
		return nullptr;

	try
	{
		StrongRef<love::filesystem::FileData> data(filesystem->read(path), Acquire::NORETAIN);
		return copyOf(data->getData(), data->getSize(), length);
	}
	catch (const love::Exception &)
	{
		// Not there, or not readable: the program reports the unit as
		// unreadable, which is the answer a missing file should get.
		return nullptr;
	}
}

} // lh
} // love
