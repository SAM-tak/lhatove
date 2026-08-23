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

// love.event for L^. The reference is wrap_Event.cpp beside this file.
//
// Where Lua's love.run polled events and fanned them out through
// love.handlers in Lua, here the fan-out is `dispatch`: the run loop hands
// over its handlers table and the engine calls the member named after each
// event with typed arguments. That keeps Boot.lh free of the untyped
// "name, a, b, c..." shape an event has in Lua.

#include "Event.h"
#include "lh/lh.h"
#include "lh/Boot.h"

#include "common/Object.h"

#include <vector>

namespace love
{
namespace event
{

#define instance() (Module::getInstance<Event>(Module::M_EVENT))

struct EventBinding
{
	const lh::TypeRegistry *registry;
};

static LhatValue lh_pump(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	float timeout = (float) lh::optNumber(arguments, count, 0, 0.0);
	return lh::guard(machine, [&]() {
		instance()->pump(timeout);
		return lhat_nil();
	});
}

// dispatch(handlers) -> the exit code when a quit went unvetoed, else nil.
static LhatValue lh_dispatch(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	const EventBinding *binding = (const EventBinding *) context;
	if (count < 1)
		return lhat_nil();
	LhatValue handlers = arguments[0];

	Message *m = nullptr;
	while (instance()->poll(m) && m != nullptr)
	{
		StrongRef<Message> message(m, Acquire::NORETAIN);

		if (message->name == "quit")
		{
			// callbacks.lua: `if c or not love.quit or not love.quit() then
			// return a or 0, b` -- the third argument forces the quit. A
			// first argument "restart" asks for another boot, with the
			// second argument carried over (Boot.h); the no-game screen's
			// drop sends the game path third.
			const Variant &first = message->args.empty() ? Variant() : message->args[0];
			bool restart = first.getType() == Variant::STRING || first.getType() == Variant::SMALLSTRING;
			bool forced = message->args.size() > 2 && message->args[2].getType() == Variant::BOOLEAN && message->args[2].getData().boolean;
			LhatRunResult asked;
			bool vetoed = false;
			if (!forced && lh::callMember(machine, handlers, "quit", nullptr, 0, &asked))
			{
				if (asked.status != LHAT_RUN_OK)
					return lhat_nil(); // the fault ends the run (vm.c)
				vetoed = lhat_is_bool(asked.value) && lhat_as_bool(asked.value);
			}
			if (!vetoed)
			{
				if (restart)
				{
					lh::setRestartPayload(message->args.size() > 1 ? message->args[1] : Variant());
					if (message->args.size() > 2 && (message->args[2].getType() == Variant::STRING || message->args[2].getType() == Variant::SMALLSTRING))
					{
						const Variant::Data &d = message->args[2].getData();
						lh::setRestartGamePath(message->args[2].getType() == Variant::STRING ? std::string(d.string->str, d.string->len) : std::string(d.smallstring.str, d.smallstring.len));
					}
					LhatValue word = lhat_nil();
					lh::makeString(machine, "restart", &word);
					return word;
				}
				int code = 0;
				if (!message->args.empty() && message->args[0].getType() == Variant::NUMBER)
					code = (int) message->args[0].getData().number;
				return lhat_integer(code);
			}
			continue;
		}

		// 05 の 8.8: convert every argument, then call -- never interleave a
		// conversion with running.
		std::vector<LhatValue> args;
		args.reserve(message->args.size());
		for (const Variant &v : message->args)
			args.push_back(lh::pushVariant(machine, *binding->registry, v));

		LhatRunResult ran;
		if (lh::callMember(machine, handlers, message->name.c_str(), args.data(), args.size(), &ran) && ran.status != LHAT_RUN_OK)
			return lhat_nil(); // the fault ends the run (vm.c)
	}
	return lhat_nil();
}

static LhatValue lh_quit(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	return lh::guard(machine, [&]() {
		std::vector<Variant> args;
		const char *word = count > 0 ? lh::stringOf(arguments[0]) : nullptr;
		if (word != nullptr)
			args.emplace_back(std::string(word)); // "restart"
		else if (count > 0 && lhat_is_number(arguments[0]))
			args.emplace_back(lhat_number_as_real(arguments[0]));
		else
			args.emplace_back(0.0);
		StrongRef<Message> m(new Message("quit", args), Acquire::NORETAIN);
		instance()->push(m);
		return lhat_bool(true);
	});
}

// quit("restart") / restart([payload]): the run answers "restart" and the
// executable boots again; the payload comes back from love.restartValue().
// A LOVE object cannot cross (its module is gone by then); tables and
// closures cross as copies, but a closure's code dies with the program, so
// only data belongs in the payload.
static LhatValue lh_restart(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	const EventBinding *binding = (const EventBinding *) context;
	Variant payload;
	if (count > 0 && !lhat_is_nil(arguments[0]))
	{
		std::string why;
		if (!lh::variantOf(machine, *binding->registry, arguments[0], payload, why))
			return lh::raise(machine, "The restart payload cannot cross: " + why);
		if (payload.getType() == Variant::LOVEOBJECT && !payload.getData().objectproxy.type->isa(lh::Carried::type))
			return lh::raise(machine, "A LOVE object cannot be a restart payload");
	}
	return lh::guard(machine, [&]() {
		std::vector<Variant> args = {Variant("restart", 7), payload};
		StrongRef<Message> m(new Message("quit", args), Acquire::NORETAIN);
		instance()->push(m);
		return lhat_nil();
	});
}

// restartValue() -> what the run before handed restart(), nil on a first
// run. It lives here rather than on `love` itself: a member of the `love`
// table would need `import^ love`, which the submodule imports refuse.
static LhatValue lh_restartValue(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) arguments;
	(void) count;
	const EventBinding *binding = (const EventBinding *) context;
	return lh::pushVariant(machine, *binding->registry, lh::restartPayload());
}

static LhatValue lh_clear(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	(void) arguments;
	(void) count;
	return lh::guard(machine, [&]() {
		instance()->clear();
		return lhat_nil();
	});
}

static EventBinding binding;

} // event

namespace lh
{

bool lhopen_love_event(Context &ctx)
{
	if (ctx.types())
		return true;

	using namespace love::event;
	binding.registry = ctx.registry;
	const char *m = "love.event";
	return ctx.func(m, "pump", "p^;", lh_pump, nullptr)
		&& ctx.func(m, "pump", "p^number^;", lh_pump, nullptr)
		&& ctx.func(m, "dispatch", "p^t^{} -> number^|string^|nil^;", lh_dispatch, &binding)
		&& ctx.func(m, "quit", "p^ -> bool^;", lh_quit, nullptr)
		&& ctx.func(m, "quit", "p^number^ -> bool^;", lh_quit, nullptr)
		&& ctx.func(m, "quit", "p^string^ -> bool^;", lh_quit, nullptr)
		&& ctx.func(m, "restart", "p^;", lh_restart, &binding)
		&& ctx.func(m, "restart", "p^any^;", lh_restart, &binding)
		&& ctx.func(m, "restartValue", "p^ -> any^;", lh_restartValue, &binding)
		&& ctx.func(m, "clear", "p^;", lh_clear, nullptr);
}

} // lh
} // love
