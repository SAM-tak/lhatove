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

// love.sensor for L^. The reference is wrap_Sensor.cpp beside this file.

#include "Sensor.h"
#include "lh/lh.h"

namespace love
{
namespace sensor
{

#define instance() (Module::getInstance<Sensor>(Module::M_SENSOR))

static bool sensorOf(LhatMachine *machine, const LhatValue *arguments, size_t count, Sensor::SensorType &type)
{
	std::string name = lh::optString(arguments, count, 0, "");
	if (!Sensor::getConstant(name.c_str(), type))
	{
		lh::raise(machine, "Invalid sensor type: " + name);
		return false;
	}
	return true;
}

static LhatValue lh_hasSensor(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	Sensor::SensorType type;
	if (!sensorOf(machine, arguments, count, type))
		return lhat_nil();
	return lhat_bool(instance()->hasSensor(type));
}

static LhatValue lh_isEnabled(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	Sensor::SensorType type;
	if (!sensorOf(machine, arguments, count, type))
		return lhat_nil();
	return lhat_bool(instance()->isEnabled(type));
}

static LhatValue lh_setEnabled(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	Sensor::SensorType type;
	if (!sensorOf(machine, arguments, count, type))
		return lhat_nil();
	return lh::guard(machine, [&]() {
		instance()->setEnabled(type, lh::optBool(arguments, count, 1, false));
		return lhat_nil();
	});
}

// getData(type) -> (x, y, z)
static LhatValue lh_getData(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	Sensor::SensorType type;
	if (!sensorOf(machine, arguments, count, type))
		return lhat_nil();
	return lh::guard(machine, [&]() {
		std::vector<float> data = instance()->getData(type);
		LhatValue parts[3] = {lhat_real(0), lhat_real(0), lhat_real(0)};
		for (size_t i = 0; i < data.size() && i < 3; i++)
			parts[i] = lhat_real(data[i]);
		LhatValue out = lhat_nil();
		lh::makeTuple(machine, parts, 3, &out);
		return out;
	});
}

} // sensor

namespace lh
{

bool lhopen_love_sensor(Context &ctx)
{
	if (ctx.types())
		return true;

	using namespace love::sensor;
	const char *m = "love.sensor";
	return ctx.func(m, "hasSensor", "f^string^ -> bool^;", lh_hasSensor, nullptr)
		&& ctx.func(m, "isEnabled", "f^string^ -> bool^;", lh_isEnabled, nullptr)
		&& ctx.func(m, "setEnabled", "p^string^, bool^;", lh_setEnabled, nullptr)
		&& ctx.func(m, "getData", "p^string^ -> (number^, number^, number^);", lh_getData, nullptr);
}

} // lh
} // love
