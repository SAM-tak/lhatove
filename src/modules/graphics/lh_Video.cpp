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

// love.graphics.Video and love.video for L^. The references are
// wrap_Video.cpp, wrap_Graphics.lua's newVideo and wrap_VideoStream.cpp
// beside this file. newVideo(path) pairs the video with an audio Source
// decoded from the same file when love.audio can (as the Lua glue did),
// else syncs it to the clock.

#include "lh_Graphics.h"

#include "Video.h"
#include "modules/video/Video.h"
#include "modules/video/VideoStream.h"
#include "modules/filesystem/Filesystem.h"
#include "modules/filesystem/File.h"
#include "modules/audio/Audio.h"
#include "modules/audio/Source.h"
#include "modules/sound/Sound.h"
#include "modules/sound/Decoder.h"

namespace love
{
namespace graphics
{

#define instance() graphicsInstance()
#define binding graphicsBinding

static Video *checkVideo(LhatMachine *machine, const LhatValue *args, size_t count, size_t index)
{
	Video *video = index < count ? lh::checkObject<Video>(args[index], *binding.registry) : nullptr;
	if (video == nullptr)
		lh::raise(machine, "Expected a Video");
	return video;
}

#define VIDEO_SELF() Video *video = checkVideo(machine, args, count, 0); if (video == nullptr) return lhat_nil()

// An audio Source streaming the file, or nullptr when there is no audio
// module or no audio track.
static love::audio::Source *audioFor(const std::string &path)
{
	auto audio = Module::getInstance<love::audio::Audio>(Module::M_AUDIO);
	auto sound = Module::getInstance<love::sound::Sound>(Module::M_SOUND);
	auto fs = Module::getInstance<love::filesystem::Filesystem>(Module::M_FILESYSTEM);
	if (audio == nullptr || sound == nullptr || fs == nullptr)
		return nullptr;
	try
	{
		StrongRef<love::filesystem::File> file(fs->openFile(path.c_str(), love::filesystem::File::MODE_READ), Acquire::NORETAIN);
		StrongRef<love::sound::Decoder> decoder(sound->newDecoder(file.get(), love::sound::Decoder::DEFAULT_BUFFER_SIZE), Acquire::NORETAIN);
		return audio->newSource(decoder.get());
	}
	catch (const love::Exception &)
	{
		return nullptr;
	}
}

// newVideo(path[, settings]) where settings may say audio = false and
// dpiscale = n.
static LhatValue lh_newVideo(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	auto videomodule = Module::getInstance<love::video::Video>(Module::M_VIDEO);
	auto fs = Module::getInstance<love::filesystem::Filesystem>(Module::M_FILESYSTEM);
	if (videomodule == nullptr || fs == nullptr)
		return lh::raise(machine, "Cannot create videos without the love.video and love.filesystem modules.");
	std::string path = lh::optString(args, count, 0, "");
	bool wantAudio = true;
	float dpiscale = 1.0f;
	if (count >= 2 && lhat_is_object_kind(args[1], LHAT_OBJECT_TABLE))
	{
		wantAudio = lh::fieldBool(machine, args[1], "audio", true);
		dpiscale = (float) lh::fieldNumber(machine, args[1], "dpiscale", 1.0);
	}
	return lh::catchexcept(machine, binding.errors->io, [&]() {
		StrongRef<love::filesystem::File> file(fs->openFile(path.c_str(), love::filesystem::File::MODE_READ), Acquire::NORETAIN);
		StrongRef<love::video::VideoStream> stream(videomodule->newVideoStream(file.get()), Acquire::NORETAIN);
		StrongRef<Video> video(instance()->newVideo(stream.get(), dpiscale), Acquire::NORETAIN);
		StrongRef<love::audio::Source> source(wantAudio ? audioFor(path) : nullptr, Acquire::NORETAIN);
		if (source.get() != nullptr)
			video->setSource(source.get());
		else
		{
			StrongRef<love::video::VideoStream::FrameSync> sync(new love::video::VideoStream::DeltaSync(), Acquire::NORETAIN);
			stream->setSync(sync.get());
		}
		return lh::pushObject(machine, *binding.registry, video.get());
	});
}

#define VIDEO_DO(name, call) \
	static LhatValue lh_Video_##name(LhatMachine *machine, void *context, const LhatValue *args, size_t count) \
	{ \
		(void) context; \
		VIDEO_SELF(); \
		return lh::guard(machine, [&]() { video->getStream()->call; return lhat_nil(); }); \
	}

VIDEO_DO(play, play())
VIDEO_DO(pause, pause())
VIDEO_DO(rewind, seek(0))

static LhatValue lh_Video_seek(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	VIDEO_SELF();
	double offset = lh::optNumber(args, count, 1, 0);
	return lh::guard(machine, [&]() {
		video->getStream()->seek(offset);
		return lhat_nil();
	});
}

static LhatValue lh_Video_tell(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	VIDEO_SELF();
	return lhat_real(video->getStream()->tell());
}

static LhatValue lh_Video_isPlaying(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	VIDEO_SELF();
	return lhat_bool(video->getStream()->isPlaying());
}

static LhatValue lh_Video_getWidth(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	VIDEO_SELF();
	return lhat_integer(video->getWidth());
}

static LhatValue lh_Video_getHeight(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	VIDEO_SELF();
	return lhat_integer(video->getHeight());
}

static LhatValue lh_Video_getDimensions(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	VIDEO_SELF();
	float values[2] = {(float) video->getWidth(), (float) video->getHeight()};
	return numberTuple(machine, values, 2);
}

static LhatValue lh_Video_getSource(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	VIDEO_SELF();
	love::audio::Source *source = video->getSource();
	return source != nullptr ? lh::pushObject(machine, *binding.registry, source) : lhat_nil();
}

// setSource(source) / setSource() to detach the audio.
static LhatValue lh_Video_setSource(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	VIDEO_SELF();
	if (count < 2)
	{
		video->setSource(nullptr);
		return lhat_nil();
	}
	auto *source = lh::checkObject<love::audio::Source>(args[1], *binding.registry);
	if (source == nullptr)
		return lh::raise(machine, "Expected a Source");
	video->setSource(source);
	return lhat_nil();
}

static LhatValue lh_Video_getFilename(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	VIDEO_SELF();
	LhatValue out = lhat_nil();
	lh::makeString(machine, video->getStream()->getFilename(), &out);
	return out;
}

// setFilter(min[, mag])
static LhatValue lh_Video_setFilter(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	VIDEO_SELF();
	SamplerState s = video->getSamplerState();
	std::string minstr = lh::optString(args, count, 1, "linear");
	std::string magstr = lh::optString(args, count, 2, minstr);
	if (!SamplerState::getConstant(minstr.c_str(), s.minFilter))
		return lh::raise(machine, "Invalid filter mode: " + minstr);
	if (!SamplerState::getConstant(magstr.c_str(), s.magFilter))
		return lh::raise(machine, "Invalid filter mode: " + magstr);
	return lh::guard(machine, [&]() {
		video->setSamplerState(s);
		return lhat_nil();
	});
}

bool lhGraphicsVideo(lh::Context &ctx)
{
	const char *m = LH_GRAPHICS;
	if (!ctx.objectType(m, "Video", Video::type))
		return false;
	if (ctx.types())
		return true;
	const char *V = "Video";
	return ctx.func(m, "newVideo", "p^string^ -> love.graphics.Video|love.Error.IO;", lh_newVideo, nullptr)
		&& ctx.func(m, "newVideo", "p^string^, t^{} -> love.graphics.Video|love.Error.IO;", lh_newVideo, nullptr)
		&& ctx.member(m, V, "play", "p^self^;", lh_Video_play, nullptr)
		&& ctx.member(m, V, "pause", "p^self^;", lh_Video_pause, nullptr)
		&& ctx.member(m, V, "rewind", "p^self^;", lh_Video_rewind, nullptr)
		&& ctx.member(m, V, "seek", "p^self^, number^;", lh_Video_seek, nullptr)
		&& ctx.member(m, V, "tell", "f^self^ -> number^;", lh_Video_tell, nullptr)
		&& ctx.member(m, V, "isPlaying", "f^self^ -> bool^;", lh_Video_isPlaying, nullptr)
		&& ctx.member(m, V, "getWidth", "f^self^ -> number^;", lh_Video_getWidth, nullptr)
		&& ctx.member(m, V, "getHeight", "f^self^ -> number^;", lh_Video_getHeight, nullptr)
		&& ctx.member(m, V, "getDimensions", "f^self^ -> (number^, number^);", lh_Video_getDimensions, nullptr)
		&& ctx.member(m, V, "getSource", "p^self^ -> love.audio.Source|nil^;", lh_Video_getSource, nullptr)
		&& ctx.member(m, V, "setSource", "p^self^;", lh_Video_setSource, nullptr)
		&& ctx.member(m, V, "setSource", "p^self^, love.audio.Source;", lh_Video_setSource, nullptr)
		&& ctx.member(m, V, "getFilename", "f^self^ -> string^;", lh_Video_getFilename, nullptr)
		&& ctx.member(m, V, "setFilter", "p^self^, string^;", lh_Video_setFilter, nullptr)
		&& ctx.member(m, V, "setFilter", "p^self^, string^, string^;", lh_Video_setFilter, nullptr);
}

} // graphics
} // love
