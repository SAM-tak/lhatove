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

// love.sound for L^: decoded audio as SoundData. The reference is
// wrap_Sound.cpp / wrap_SoundData.cpp beside this file. Decoder objects and
// per-sample access come later.

#include "Sound.h"
#include "SoundData.h"
#include "Decoder.h"
#include "lh/lh.h"

#include "modules/filesystem/Filesystem.h"
#include "modules/filesystem/File.h"

namespace love
{
namespace sound
{

#define instance() (Module::getInstance<Sound>(Module::M_SOUND))

struct SoundBinding
{
	lh::Errors *errors;
	lh::TypeRegistry *registry;
};

static SoundBinding binding;

static SoundData *checkSoundData(LhatMachine *machine, const LhatValue *arguments, size_t count)
{
	SoundData *data = count > 0 ? lh::checkObject<SoundData>(arguments[0], *binding.registry) : nullptr;
	if (data == nullptr)
		lh::raise(machine, "Expected a SoundData");
	return data;
}

// newSoundData(path) -- decoded whole; newSoundData(samples, rate, bits,
// channels) -- silence of that shape.
static LhatValue lh_newSoundData(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	if (count >= 1 && lhat_is_number(arguments[0]))
	{
		int samples = (int) lh::optNumber(arguments, count, 0, 0);
		int rate = (int) lh::optNumber(arguments, count, 1, Decoder::DEFAULT_SAMPLE_RATE);
		int bits = (int) lh::optNumber(arguments, count, 2, Decoder::DEFAULT_BIT_DEPTH);
		int channels = (int) lh::optNumber(arguments, count, 3, Decoder::DEFAULT_CHANNELS);
		return lh::guard(machine, [&]() {
			StrongRef<SoundData> data(instance()->newSoundData(samples, rate, bits, channels), Acquire::NORETAIN);
			return lh::pushObject(machine, *binding.registry, data.get());
		});
	}

	std::string path = lh::optString(arguments, count, 0, "");
	auto fs = Module::getInstance<love::filesystem::Filesystem>(Module::M_FILESYSTEM);
	if (fs == nullptr)
		return lh::raise(machine, "love.filesystem is not loaded.");
	return lh::catchexcept(machine, binding.errors->io, [&]() {
		StrongRef<love::filesystem::File> file(fs->openFile(path.c_str(), love::filesystem::File::MODE_READ), Acquire::NORETAIN);
		StrongRef<Decoder> decoder(instance()->newDecoder(file.get(), Decoder::DEFAULT_BUFFER_SIZE), Acquire::NORETAIN);
		StrongRef<SoundData> data(instance()->newSoundData(decoder.get()), Acquire::NORETAIN);
		return lh::pushObject(machine, *binding.registry, data.get());
	});
}

static LhatValue lh_SoundData_getDuration(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	SoundData *d = checkSoundData(machine, arguments, count);
	return lhat_real(d != nullptr ? d->getDuration() : 0.0f);
}

static LhatValue lh_SoundData_getSampleCount(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	SoundData *d = checkSoundData(machine, arguments, count);
	return lhat_integer(d != nullptr ? d->getSampleCount() : 0);
}

static LhatValue lh_SoundData_getSampleRate(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	SoundData *d = checkSoundData(machine, arguments, count);
	return lhat_integer(d != nullptr ? d->getSampleRate() : 0);
}

static LhatValue lh_SoundData_getChannelCount(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	SoundData *d = checkSoundData(machine, arguments, count);
	return lhat_integer(d != nullptr ? d->getChannelCount() : 0);
}

static LhatValue lh_SoundData_getBitDepth(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	SoundData *d = checkSoundData(machine, arguments, count);
	return lhat_integer(d != nullptr ? d->getBitDepth() : 0);
}

} // sound

namespace lh
{

bool lhopen_love_sound(Context &ctx)
{
	using namespace love::sound;
	const char *m = "love.sound";

	if (!ctx.objectType(m, "SoundData", SoundData::type))
		return false;
	if (ctx.types())
		return true;

	binding.errors = ctx.errors;
	binding.registry = ctx.registry;

	return ctx.func(m, "newSoundData", "p^string^ -> love.sound.SoundData|love.Error.IO;", lh_newSoundData, nullptr)
		&& ctx.func(m, "newSoundData", "p^number^, number^, number^, number^ -> love.sound.SoundData;", lh_newSoundData, nullptr)
		&& ctx.member(m, "SoundData", "getDuration", "f^self^ -> number^;", lh_SoundData_getDuration, nullptr)
		&& ctx.member(m, "SoundData", "getSampleCount", "f^self^ -> number^;", lh_SoundData_getSampleCount, nullptr)
		&& ctx.member(m, "SoundData", "getSampleRate", "f^self^ -> number^;", lh_SoundData_getSampleRate, nullptr)
		&& ctx.member(m, "SoundData", "getChannelCount", "f^self^ -> number^;", lh_SoundData_getChannelCount, nullptr)
		&& ctx.member(m, "SoundData", "getBitDepth", "f^self^ -> number^;", lh_SoundData_getBitDepth, nullptr);
}

} // lh
} // love
