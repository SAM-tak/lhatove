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

// love.window for L^. The reference is wrap_Window.cpp beside this file.
// Milestone M1: the settings table of setMode/getMode waits for conf.lh.

#include "Window.h"
#include "lh/lh.h"

namespace love
{
namespace window
{

#define instance() (Module::getInstance<Window>(Module::M_WINDOW))

static LhatValue lh_setMode(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	int w = (int) lh::optNumber(arguments, count, 0, 800);
	int h = (int) lh::optNumber(arguments, count, 1, 600);
	return lh::guard(machine, [&]() {
		WindowSettings settings;
		return lhat_bool(instance()->setWindow(w, h, &settings));
	});
}

static LhatValue lh_getMode(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	(void) arguments;
	(void) count;
	int w = 0, h = 0;
	WindowSettings settings;
	instance()->getWindow(w, h, settings);
	LhatValue parts[2] = {lhat_integer(w), lhat_integer(h)};
	LhatValue out = lhat_nil();
	lh::makeTuple(machine, parts, 2, &out);
	return out;
}

static LhatValue lh_setTitle(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	instance()->setWindowTitle(lh::optString(arguments, count, 0, ""));
	return lhat_nil();
}

static LhatValue lh_getTitle(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	(void) arguments;
	(void) count;
	LhatValue out = lhat_nil();
	lh::makeString(machine, instance()->getWindowTitle(), &out);
	return out;
}

static LhatValue lh_isOpen(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	return lhat_bool(instance()->isOpen());
}

static LhatValue lh_close(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	(void) arguments;
	(void) count;
	return lh::guard(machine, [&]() {
		instance()->close();
		return lhat_nil();
	});
}

static LhatValue lh_setFullscreen(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	bool fullscreen = lh::optBool(arguments, count, 0, false);
	return lh::guard(machine, [&]() {
		return lhat_bool(instance()->setFullscreen(fullscreen));
	});
}

static LhatValue lh_getDPIScale(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	return lhat_real(instance()->getDPIScale());
}

static LhatValue lh_hasFocus(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	return lhat_bool(instance()->hasFocus());
}

static LhatValue lh_hasMouseFocus(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	return lhat_bool(instance()->hasMouseFocus());
}

static LhatValue lh_isVisible(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	return lhat_bool(instance()->isVisible());
}

static LhatValue lh_setVSync(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	instance()->setVSync((int) lh::optNumber(arguments, count, 0, 1));
	return lhat_nil();
}

static LhatValue lh_getVSync(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	return lhat_integer(instance()->getVSync());
}

} // window

namespace lh
{

bool lhopen_love_window(Context &ctx)
{
	if (ctx.types())
		return true;

	using namespace love::window;
	const char *m = "love.window";
	return ctx.func(m, "setMode", "p^number^, number^ -> bool^;", lh_setMode, nullptr)
		&& ctx.func(m, "getMode", "f^ -> (number^, number^);", lh_getMode, nullptr)
		&& ctx.func(m, "setTitle", "p^string^;", lh_setTitle, nullptr)
		&& ctx.func(m, "getTitle", "f^ -> string^;", lh_getTitle, nullptr)
		&& ctx.func(m, "isOpen", "f^ -> bool^;", lh_isOpen, nullptr)
		&& ctx.func(m, "close", "p^;", lh_close, nullptr)
		&& ctx.func(m, "setFullscreen", "p^bool^ -> bool^;", lh_setFullscreen, nullptr)
		&& ctx.func(m, "getDPIScale", "f^ -> number^;", lh_getDPIScale, nullptr)
		&& ctx.func(m, "hasFocus", "f^ -> bool^;", lh_hasFocus, nullptr)
		&& ctx.func(m, "hasMouseFocus", "f^ -> bool^;", lh_hasMouseFocus, nullptr)
		&& ctx.func(m, "isVisible", "f^ -> bool^;", lh_isVisible, nullptr)
		&& ctx.func(m, "setVSync", "p^number^;", lh_setVSync, nullptr)
		&& ctx.func(m, "getVSync", "f^ -> number^;", lh_getVSync, nullptr);
}

} // lh
} // love
