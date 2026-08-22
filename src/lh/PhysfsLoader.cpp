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

#include <cstdio>
#include <cstring>
#include <vector>

namespace love
{
namespace lh
{

// The program frees what the loader hands back with lhat_free, so the copy
// has to come from lhat_alloc. One byte past the end keeps it NUL terminated
// whatever it holds.
static char *copyOf(const char *text, size_t size, size_t *length)
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

void Loader::setBase(const std::string &newbase)
{
	base = newbase;
	if (!base.empty() && base.back() != '/' && base.back() != '\\')
		base += '/';
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

	if (base.empty())
		return nullptr;

	std::string full = base + path;
	FILE *file = fopen(full.c_str(), "rb");
	if (file == nullptr)
		return nullptr;

	std::vector<char> bytes;
	char chunk[4096];
	size_t got;
	while ((got = fread(chunk, 1, sizeof(chunk), file)) > 0)
		bytes.insert(bytes.end(), chunk, chunk + got);
	fclose(file);

	return copyOf(bytes.data(), bytes.size(), length);
}

} // lh
} // love
