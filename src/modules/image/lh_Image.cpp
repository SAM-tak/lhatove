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

// love.image for L^ -- milestone M2: ImageData from a file or a size, and
// its dimensions. Pixel access (host value colors, mapPixel in C) is M3.
// The reference is wrap_Image.cpp / wrap_ImageData.cpp beside this file.

#include "Image.h"
#include "ImageData.h"
#include "lh/lh.h"

#include "modules/filesystem/Filesystem.h"
#include "modules/filesystem/FileData.h"

namespace love
{
namespace image
{

#define instance() (Module::getInstance<Image>(Module::M_IMAGE))

struct ImageBinding
{
	lh::Errors *errors;
	lh::TypeRegistry *registry;
};

static ImageData *checkImageData(LhatMachine *machine, const ImageBinding *b, const LhatValue *arguments, size_t count)
{
	ImageData *data = count > 0 ? lh::checkObject<ImageData>(arguments[0], *b->registry) : nullptr;
	if (data == nullptr)
		lh::raise(machine, "Expected an ImageData");
	return data;
}

// newImageData(path) -- decoded from the mounted filesystem (IO on failure);
// newImageData(width, height) -- blank RGBA8.
static LhatValue lh_newImageData(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	const ImageBinding *b = (const ImageBinding *) context;
	if (count >= 2 && lhat_is_number(arguments[0]))
	{
		int w = (int) lh::optNumber(arguments, count, 0, 0);
		int h = (int) lh::optNumber(arguments, count, 1, 0);
		if (w <= 0 || h <= 0)
			return lh::raise(machine, "Invalid image size.");
		return lh::guard(machine, [&]() {
			ImageData *data = instance()->newImageData(w, h);
			LhatValue out = lh::pushObject(machine, *b->registry, data);
			data->release();
			return out;
		});
	}

	std::string path = lh::optString(arguments, count, 0, "");
	auto fs = Module::getInstance<love::filesystem::Filesystem>(Module::M_FILESYSTEM);
	if (fs == nullptr)
		return lh::raise(machine, "love.filesystem is not loaded.");
	return lh::catchexcept(machine, b->errors->io, [&]() {
		StrongRef<love::filesystem::FileData> file(fs->read(path.c_str()), Acquire::NORETAIN);
		ImageData *data = instance()->newImageData(file.get());
		LhatValue out = lh::pushObject(machine, *b->registry, data);
		data->release();
		return out;
	});
}

static LhatValue lh_ImageData_getWidth(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	ImageData *data = checkImageData(machine, (const ImageBinding *) context, arguments, count);
	return lhat_integer(data != nullptr ? data->getWidth() : 0);
}

static LhatValue lh_ImageData_getHeight(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	ImageData *data = checkImageData(machine, (const ImageBinding *) context, arguments, count);
	return lhat_integer(data != nullptr ? data->getHeight() : 0);
}

static LhatValue lh_ImageData_getDimensions(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	ImageData *data = checkImageData(machine, (const ImageBinding *) context, arguments, count);
	if (data == nullptr)
		return lhat_nil();
	LhatValue parts[2] = {lhat_integer(data->getWidth()), lhat_integer(data->getHeight())};
	LhatValue out = lhat_nil();
	lh::makeTuple(machine, parts, 2, &out);
	return out;
}

// getPixel(x, y) -> (r, g, b, a); 0-based, as the Lua API. Out of bounds panics.
static LhatValue lh_ImageData_getPixel(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	ImageData *data = checkImageData(machine, (const ImageBinding *) context, arguments, count);
	if (data == nullptr)
		return lhat_nil();
	int x = (int) lh::optNumber(arguments, count, 1, 0);
	int y = (int) lh::optNumber(arguments, count, 2, 0);
	if (!data->inside(x, y))
		return lh::raise(machine, "Attempt to get out-of-range pixel!");
	Colorf c;
	data->getPixel(x, y, c);
	LhatValue parts[4] = {lhat_real(c.r), lhat_real(c.g), lhat_real(c.b), lhat_real(c.a)};
	LhatValue out = lhat_nil();
	lh::makeTuple(machine, parts, 4, &out);
	return out;
}

// setPixel(x, y, r, g, b[, a])
static LhatValue lh_ImageData_setPixel(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	ImageData *data = checkImageData(machine, (const ImageBinding *) context, arguments, count);
	if (data == nullptr)
		return lhat_nil();
	int x = (int) lh::optNumber(arguments, count, 1, 0);
	int y = (int) lh::optNumber(arguments, count, 2, 0);
	if (!data->inside(x, y))
		return lh::raise(machine, "Attempt to set out-of-range pixel!");
	Colorf c((float) lh::optNumber(arguments, count, 3, 0.0), (float) lh::optNumber(arguments, count, 4, 0.0), (float) lh::optNumber(arguments, count, 5, 0.0), (float) lh::optNumber(arguments, count, 6, 1.0));
	data->setPixel(x, y, c);
	return lhat_nil();
}

// mapPixel(fn): fn(x, y, r, g, b, a) -> (r, g, b, a) over every pixel. One
// nested call per pixel -- the FFI path Lua had is gone, and a bulk
// operation in C is what a hot loop should reach for.
static LhatValue lh_ImageData_mapPixel(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	ImageData *data = checkImageData(machine, (const ImageBinding *) context, arguments, count);
	if (data == nullptr)
		return lhat_nil();
	if (count < 2 || !lhat_is_object_kind(arguments[1], LHAT_OBJECT_SUBROUTINE))
		return lh::raise(machine, "Expected a function");
	LhatValue fn = arguments[1];
	int w = data->getWidth(), h = data->getHeight();
	for (int y = 0; y < h; y++)
	{
		for (int x = 0; x < w; x++)
		{
			Colorf c;
			data->getPixel(x, y, c);
			LhatValue args[6] = {lhat_integer(x), lhat_integer(y), lhat_real(c.r), lhat_real(c.g), lhat_real(c.b), lhat_real(c.a)};
			LhatRunResult ran = lhat_machine_call(machine, fn, args, 6);
			if (ran.status != LHAT_RUN_OK)
				return lhat_nil(); // the fault ends the run (vm.c)
			if (ran.position_count >= 3)
			{
				c.r = (float) lh::optNumber(ran.positions, ran.position_count, 0, c.r);
				c.g = (float) lh::optNumber(ran.positions, ran.position_count, 1, c.g);
				c.b = (float) lh::optNumber(ran.positions, ran.position_count, 2, c.b);
				c.a = (float) lh::optNumber(ran.positions, ran.position_count, 3, c.a);
				data->setPixel(x, y, c);
			}
		}
	}
	return lhat_nil();
}

static ImageBinding binding;

} // image

namespace lh
{

bool lhopen_love_image(Context &ctx)
{
	using namespace love::image;
	const char *m = "love.image";

	if (!ctx.objectType(m, "ImageData", ImageData::type))
		return false;
	if (ctx.types())
		return true;

	binding.errors = ctx.errors;
	binding.registry = ctx.registry;
	void *b = &binding;

	return ctx.func(m, "newImageData", "p^string^ -> love.image.ImageData|love.Error.IO;", lh_newImageData, b)
		&& ctx.func(m, "newImageData", "p^number^, number^ -> love.image.ImageData;", lh_newImageData, b)
		&& ctx.member(m, "ImageData", "getWidth", "f^self^ -> number^;", lh_ImageData_getWidth, b)
		&& ctx.member(m, "ImageData", "getHeight", "f^self^ -> number^;", lh_ImageData_getHeight, b)
		&& ctx.member(m, "ImageData", "getDimensions", "f^self^ -> (number^, number^);", lh_ImageData_getDimensions, b)
		&& ctx.member(m, "ImageData", "getPixel", "f^self^, number^, number^ -> (number^, number^, number^, number^);", lh_ImageData_getPixel, b)
		&& ctx.member(m, "ImageData", "setPixel", "p^self^, number^, number^, number^, number^, number^, ...;", lh_ImageData_setPixel, b)
		&& ctx.member(m, "ImageData", "mapPixel", "p^self^, f^number^, number^, number^, number^, number^, number^ -> (number^, number^, number^, number^);;", lh_ImageData_mapPixel, b);
}

} // lh
} // love
