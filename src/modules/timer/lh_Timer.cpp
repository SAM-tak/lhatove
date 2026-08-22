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

// love.timer for L^. The reference is wrap_Timer.cpp beside this file.

#include "Timer.h"
#include "lh/lh.h"

namespace love
{
namespace timer
{

#define instance() (Module::getInstance<Timer>(Module::M_TIMER))

static LhatValue lh_step(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	return lhat_real(instance()->step());
}

static LhatValue lh_getDelta(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	return lhat_real(instance()->getDelta());
}

static LhatValue lh_getFPS(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	return lhat_integer(instance()->getFPS());
}

static LhatValue lh_getAverageDelta(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	return lhat_real(instance()->getAverageDelta());
}

static LhatValue lh_sleep(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	instance()->sleep(lh::optNumber(arguments, count, 0, 0.0));
	return lhat_nil();
}

static LhatValue lh_getTime(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	return lhat_real(Timer::getTime());
}

} // timer

namespace lh
{

bool lhopen_love_timer(Context &ctx)
{
	if (ctx.types())
		return true;

	using namespace love::timer;
	const char *m = "love.timer";
	return ctx.func(m, "step", "p^ -> number^;", lh_step, nullptr)
		&& ctx.func(m, "getDelta", "f^ -> number^;", lh_getDelta, nullptr)
		&& ctx.func(m, "getFPS", "f^ -> number^;", lh_getFPS, nullptr)
		&& ctx.func(m, "getAverageDelta", "f^ -> number^;", lh_getAverageDelta, nullptr)
		&& ctx.func(m, "sleep", "p^number^;", lh_sleep, nullptr)
		&& ctx.func(m, "getTime", "f^ -> number^;", lh_getTime, nullptr);
}

} // lh
} // love
