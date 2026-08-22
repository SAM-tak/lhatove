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

// love.keyboard for L^. The reference is wrap_Keyboard.cpp beside this file.

#include "Keyboard.h"
#include "lh/lh.h"

#include <vector>

namespace love
{
namespace keyboard
{

#define instance() (Module::getInstance<Keyboard>(Module::M_KEYBOARD))

// isDown(key, ...): every argument is a key constant; true if any is down.
static LhatValue lh_isDown(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	std::vector<Keyboard::Key> keys;
	keys.reserve(count);
	for (size_t i = 0; i < count; i++)
	{
		const char *name = lh::stringOf(arguments[i]);
		Keyboard::Key k;
		if (name == nullptr || !Keyboard::getConstant(name, k))
			return lh::raise(machine, std::string("Invalid key constant: ") + (name != nullptr ? name : "(not a string)"));
		keys.push_back(k);
	}
	return lhat_bool(instance()->isDown(keys));
}

static LhatValue lh_isScancodeDown(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	std::vector<Keyboard::Scancode> codes;
	codes.reserve(count);
	for (size_t i = 0; i < count; i++)
	{
		const char *name = lh::stringOf(arguments[i]);
		Keyboard::Scancode s;
		if (name == nullptr || !Keyboard::getConstant(name, s))
			return lh::raise(machine, std::string("Invalid scancode: ") + (name != nullptr ? name : "(not a string)"));
		codes.push_back(s);
	}
	return lhat_bool(instance()->isScancodeDown(codes));
}

static LhatValue lh_setKeyRepeat(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	instance()->setKeyRepeat(lh::optBool(arguments, count, 0, false));
	return lhat_nil();
}

static LhatValue lh_hasKeyRepeat(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	return lhat_bool(instance()->hasKeyRepeat());
}

static LhatValue lh_setTextInput(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	instance()->setTextInput(lh::optBool(arguments, count, 0, false));
	return lhat_nil();
}

static LhatValue lh_hasTextInput(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	return lhat_bool(instance()->hasTextInput());
}

} // keyboard

namespace lh
{

bool lhopen_love_keyboard(Context &ctx)
{
	if (ctx.types())
		return true;

	using namespace love::keyboard;
	const char *m = "love.keyboard";
	return ctx.func(m, "isDown", "f^string^, ... -> bool^;", lh_isDown, nullptr)
		&& ctx.func(m, "isScancodeDown", "f^string^, ... -> bool^;", lh_isScancodeDown, nullptr)
		&& ctx.func(m, "setKeyRepeat", "p^bool^;", lh_setKeyRepeat, nullptr)
		&& ctx.func(m, "hasKeyRepeat", "f^ -> bool^;", lh_hasKeyRepeat, nullptr)
		&& ctx.func(m, "setTextInput", "p^bool^;", lh_setTextInput, nullptr)
		&& ctx.func(m, "hasTextInput", "f^ -> bool^;", lh_hasTextInput, nullptr);
}

} // lh
} // love
