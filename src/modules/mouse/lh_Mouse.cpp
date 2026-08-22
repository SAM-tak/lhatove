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

// love.mouse for L^. The reference is wrap_Mouse.cpp beside this file.

#include "Mouse.h"
#include "lh/lh.h"

#include <vector>

namespace love
{
namespace mouse
{

#define instance() (Module::getInstance<Mouse>(Module::M_MOUSE))

static LhatValue lh_getPosition(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	(void) arguments;
	(void) count;
	double x = 0.0, y = 0.0;
	instance()->getPosition(x, y);
	LhatValue parts[2] = {lhat_real(x), lhat_real(y)};
	LhatValue out = lhat_nil();
	lh::makeTuple(machine, parts, 2, &out);
	return out;
}

static LhatValue lh_getX(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	double x = 0.0, y = 0.0;
	instance()->getPosition(x, y);
	return lhat_real(x);
}

static LhatValue lh_getY(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	double x = 0.0, y = 0.0;
	instance()->getPosition(x, y);
	return lhat_real(y);
}

static LhatValue lh_setPosition(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	instance()->setPosition(lh::optNumber(arguments, count, 0, 0.0), lh::optNumber(arguments, count, 1, 0.0));
	return lhat_nil();
}

// isDown(button, ...): 1-based button numbers; true if any is down.
static LhatValue lh_isDown(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	std::vector<int> buttons;
	buttons.reserve(count);
	for (size_t i = 0; i < count; i++)
		buttons.push_back((int) lh::optNumber(arguments, count, i, 0.0));
	return lhat_bool(instance()->isDown(buttons));
}

static LhatValue lh_setVisible(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	instance()->setVisible(lh::optBool(arguments, count, 0, true));
	return lhat_nil();
}

static LhatValue lh_isVisible(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	return lhat_bool(instance()->isVisible());
}

} // mouse

namespace lh
{

bool lhopen_love_mouse(Context &ctx)
{
	if (ctx.types())
		return true;

	using namespace love::mouse;
	const char *m = "love.mouse";
	return ctx.func(m, "getPosition", "f^ -> (number^, number^);", lh_getPosition, nullptr)
		&& ctx.func(m, "getX", "f^ -> number^;", lh_getX, nullptr)
		&& ctx.func(m, "getY", "f^ -> number^;", lh_getY, nullptr)
		&& ctx.func(m, "setPosition", "p^number^, number^;", lh_setPosition, nullptr)
		&& ctx.func(m, "isDown", "f^number^, ... -> bool^;", lh_isDown, nullptr)
		&& ctx.func(m, "setVisible", "p^bool^;", lh_setVisible, nullptr)
		&& ctx.func(m, "isVisible", "f^ -> bool^;", lh_isVisible, nullptr);
}

} // lh
} // love
