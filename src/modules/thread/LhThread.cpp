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

#include "LhThread.h"
#include "event/Event.h"

namespace love
{
namespace thread
{

love::Type LhThread::type("Thread", &Threadable::type);

LhThread::LhThread(const std::string &name, LhatProgram *program, const LhatProto *proto, LhatProto *owned, const lh::TypeRegistry *registry)
	: program(program)
	, proto(proto)
	, owned(owned)
	, registry(registry)
	, name(name)
	, haserror(false)
{
	threadName = name;
}

LhThread::~LhThread()
{
	// The OS thread retains this while it runs (sdl/Thread.cpp), so nothing
	// is running the proto when it is freed.
	if (owned != nullptr)
		lhat_proto_free(owned);
}

void LhThread::threadFunction()
{
	error.clear();
	haserror = false;

	// A machine of this thread's own, with the program installed; the main
	// machine's values reach it only as Variants (lh::pushVariant).
	LhatMachine *machine = lh::Runtime::spawnMachine(program);
	if (machine == nullptr)
	{
		error = "Could not start an L^ machine for the thread.";
		haserror = true;
		args.clear();
		onError();
		return;
	}

	std::vector<LhatValue> values;
	values.reserve(args.size());
	for (const Variant &v : args)
		values.push_back(lh::pushVariant(machine, *registry, v));
	args.clear();

	// 13.7: what the script reads as `...`.
	LhatRunResult ran = lhat_run_arguments(machine, proto, values.data(), values.size());
	if (ran.status != LHAT_RUN_OK)
	{
		error = lh::describeRun(machine, ran);
		haserror = true;
	}

	lh::Runtime::disposeMachine(machine);

	if (haserror)
		onError();
}

bool LhThread::start(const std::vector<Variant> &args)
{
	if (isRunning())
		return false;

	this->args = args;
	error.clear();
	haserror = false;

	return Threadable::start();
}

const std::string &LhThread::getError() const
{
	return error;
}

void LhThread::onError()
{
	auto eventmodule = Module::getInstance<event::Event>(Module::M_EVENT);
	if (!eventmodule)
		return;

	std::vector<Variant> vargs = {
		Variant(&LhThread::type, this),
		Variant(error.c_str(), error.length())
	};

	StrongRef<event::Message> msg(new event::Message("threaderror", vargs), Acquire::NORETAIN);
	eventmodule->push(msg);
}

} // thread
} // love
