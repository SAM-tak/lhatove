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

// love.thread for L^: Threads running an L^ unit on a machine of their own,
// and Channels carrying values between machines. The references are
// wrap_ThreadModule.cpp, wrap_LuaThread.cpp and wrap_Channel.cpp beside this
// file.
//
// A thread body is a file (checked as a unit of the program, so its
// mistakes are diagnosed at newThread) or code in a string (loaded as a
// script). It reads its arguments as `...`. Values cross a Channel the way
// lh::variantOf says: scalars and LOVE objects as themselves, tables and
// closures as a carried copy -- a table holding a LOVE object cannot cross.

#include "ThreadModule.h"
#include "LhThread.h"
#include "Channel.h"
#include "lh/lh.h"

#include "modules/filesystem/File.h"
#include "modules/filesystem/FileData.h"

#include <mutex>

namespace love
{
namespace thread
{

#define instance() (Module::getInstance<ThreadModule>(Module::M_THREAD))

struct ThreadBinding
{
	lh::Errors *errors = nullptr;
	lh::TypeRegistry *registry = nullptr;
	LhatProgram *program = nullptr;
};

static ThreadBinding binding;

static LhThread *checkThread(LhatMachine *machine, const LhatValue *args, size_t count)
{
	LhThread *thread = count > 0 ? lh::checkObject<LhThread>(args[0], *binding.registry) : nullptr;
	if (thread == nullptr)
		lh::raise(machine, "Expected a Thread");
	return thread;
}

static Channel *checkChannel(LhatMachine *machine, const LhatValue *args, size_t count)
{
	Channel *channel = count > 0 ? lh::checkObject<Channel>(args[0], *binding.registry) : nullptr;
	if (channel == nullptr)
		lh::raise(machine, "Expected a Channel");
	return channel;
}

// ---------------------------------------------------------------------------
// love.thread
// ---------------------------------------------------------------------------

// The unit's diagnostics, one per line, for the raise.
static std::string diagnosticsOf(const LhatUnit *unit)
{
	std::string out;
	for (size_t i = 0; unit != nullptr && i < lhat_unit_diagnostic_count(unit); i++)
	{
		std::vector<char> room(1024);
		size_t needed = lhat_unit_diagnostic_write(unit, i, true, room.data(), room.size());
		if (needed >= room.size())
		{
			room.resize(needed + 1);
			lhat_unit_diagnostic_write(unit, i, true, room.data(), room.size());
		}
		if (!out.empty())
			out += "\n";
		out += room.data();
	}
	return out;
}

// A thread body from a file the loader can read: checked and compiled as a
// unit of the program, which the worker then runs.
static LhatValue threadFromFile(LhatMachine *machine, const std::string &path)
{
	const LhatUnit *unit = nullptr;
	{
		std::lock_guard<std::mutex> hold(lh::programMutex());
		unit = lhat_program_check(binding.program, path.c_str());
		if (unit == nullptr)
		{
			lh::raise(machine, "Could not read the thread file " + path);
			return lhat_nil();
		}
		if (!lhat_unit_ok(unit))
		{
			lh::raise(machine, "The thread file " + path + " did not check:\n" + diagnosticsOf(unit));
			return lhat_nil();
		}
		if (!lhat_program_compile(binding.program))
		{
			lh::raise(machine, "The thread file " + path + " did not compile");
			return lhat_nil();
		}
	}
	StrongRef<LhThread> thread(new LhThread(path, binding.program, lhat_unit_proto(unit), nullptr, binding.registry), Acquire::NORETAIN);
	return lh::pushObject(machine, *binding.registry, thread.get());
}

// A thread body from code: loaded as a script the thread owns.
static LhatValue threadFromText(LhatMachine *machine, const std::string &name, const char *text, size_t length)
{
	LhatProto *proto = nullptr;
	{
		std::lock_guard<std::mutex> hold(lh::programMutex());
		switch (lhat_program_load_text(binding.program, name.c_str(), text, length, &proto))
		{
		case LHAT_LOAD_OK:
			break;
		case LHAT_LOAD_REJECTED:
		{
			const char *why = lhat_program_load_failure(binding.program);
			{
				lh::raise(machine, "The thread code did not check:\n" + std::string(why != nullptr ? why : "(no diagnostic)"));
				return lhat_nil();
			}
		}
		default:
			{
				lh::raise(machine, "Could not load the thread code");
				return lhat_nil();
			}
		}
	}
	StrongRef<LhThread> thread(new LhThread(name, binding.program, proto, proto, binding.registry), Acquire::NORETAIN);
	return lh::pushObject(machine, *binding.registry, thread.get());
}

// newThread(path | code | File | FileData): wrap_ThreadModule's rule -- a
// string without a newline is a file name (".lh" here), anything else is
// the code itself.
static void lh_newThread(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						 LhatValue *answers, int *answerCount)
{
	(void) context;
	if (count < 1)
	{
		lh::raise(machine, "newThread needs a file name, code, a File or a FileData");
		return;
	}

	size_t length = 0;
	const char *text = lh::stringOf(args[0], &length);
	if (text != nullptr)
	{
		std::string s(text, length);
		bool isFile = s.find('\n') == std::string::npos && s.size() > 3 && s.compare(s.size() - 3, 3, ".lh") == 0;
		if (isFile)
		{
			answers[0] = threadFromFile(machine, s);
			*answerCount = 1;
			return;
		}
		answers[0] = threadFromText(machine, "thread", text, length);
		*answerCount = 1;
		return;
	}

	auto *file = lh::checkObject<love::filesystem::File>(args[0], *binding.registry);
	if (file != nullptr)
	{
		lh::guard(machine, [&]() {
			StrongRef<love::filesystem::FileData> data(file->read(), Acquire::NORETAIN);
			answers[0] = threadFromText(machine, data->getFilename(), (const char *) data->getData(), data->getSize());
			*answerCount = 1;
			return;
		});
		return;
	}
	auto *data = lh::checkObject<love::filesystem::FileData>(args[0], *binding.registry);
	if (data != nullptr)
	{
		answers[0] = threadFromText(machine, data->getFilename(), (const char *) data->getData(), data->getSize());
		*answerCount = 1;
		return;
	}

	{
		lh::raise(machine, "newThread needs a file name, code, a File or a FileData");
		return;
	}
}

static void lh_newChannel(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						 LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) args;
	(void) count;
	StrongRef<Channel> channel(instance()->newChannel(), Acquire::NORETAIN);
	answers[0] = lh::pushObject(machine, *binding.registry, channel.get());
	*answerCount = 1;
	return;
}

static void lh_getChannel(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						 LhatValue *answers, int *answerCount)
{
	(void) context;
	std::string name = lh::optString(args, count, 0, "");
	answers[0] = lh::pushObject(machine, *binding.registry, instance()->getChannel(name));
	*answerCount = 1;
	return;
}

// ---------------------------------------------------------------------------
// Thread
// ---------------------------------------------------------------------------

// start(...): the arguments cross as Variants; one that cannot raises.
static void lh_Thread_start(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						 LhatValue *answers, int *answerCount)
{
	(void) context;
	LhThread *thread = checkThread(machine, args, count);
	if (thread == nullptr)
		return;
	std::vector<Variant> carried;
	for (size_t i = 1; i < count; i++)
	{
		Variant v;
		std::string why;
		if (!lh::variantOf(machine, *binding.registry, args[i], v, why))
		{
			lh::raise(machine, "Thread argument " + std::to_string(i) + " cannot cross to the thread: " + why);
			return;
		}
		carried.push_back(v);
	}
	thread->start(carried);
	return;
}

static void lh_Thread_wait(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						 LhatValue *answers, int *answerCount)
{
	(void) context;
	LhThread *thread = checkThread(machine, args, count);
	if (thread != nullptr)
		thread->wait();
	return;
}

static void lh_Thread_getError(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						 LhatValue *answers, int *answerCount)
{
	(void) context;
	LhThread *thread = checkThread(machine, args, count);
	if (thread == nullptr || !thread->hasError())
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	LhatValue out = lhat_nil();
	lh::makeString(machine, thread->getError(), &out);
	answers[0] = out;
	*answerCount = 1;
	return;
}

static void lh_Thread_isRunning(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						 LhatValue *answers, int *answerCount)
{
	(void) context;
	LhThread *thread = checkThread(machine, args, count);
	answers[0] = lhat_bool(thread != nullptr && thread->isRunning());
	*answerCount = 1;
	return;
}

// ---------------------------------------------------------------------------
// Channel
// ---------------------------------------------------------------------------

#define CHANNEL_SELF() Channel *c = checkChannel(machine, args, count); if (c == nullptr) return

static bool variantArg(LhatMachine *machine, const LhatValue *args, size_t count, size_t index, Variant &out)
{
	std::string why;
	if (index >= count)
	{
		out = Variant();
		return true;
	}
	if (!lh::variantOf(machine, *binding.registry, args[index], out, why))
	{
		lh::raise(machine, "The value cannot cross a Channel: " + why);
		return false;
	}
	return true;
}

static void lh_Channel_push(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						 LhatValue *answers, int *answerCount)
{
	(void) context;
	CHANNEL_SELF();
	Variant v;
	if (!variantArg(machine, args, count, 1, v))
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	answers[0] = lhat_integer((int64_t) c->push(v));
	*answerCount = 1;
	return;
}

// supply(value[, timeout]) -> whether it was read.
static void lh_Channel_supply(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						 LhatValue *answers, int *answerCount)
{
	(void) context;
	CHANNEL_SELF();
	Variant v;
	if (!variantArg(machine, args, count, 1, v))
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	if (count >= 3)
	{
		answers[0] = lhat_bool(c->supply(v, lh::optNumber(args, count, 2, 0)));
		*answerCount = 1;
		return;
	}
	answers[0] = lhat_bool(c->supply(v));
	*answerCount = 1;
	return;
}

static void lh_Channel_pop(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						 LhatValue *answers, int *answerCount)
{
	(void) context;
	CHANNEL_SELF();
	Variant v;
	if (!c->pop(&v))
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	answers[0] = lh::pushVariant(machine, *binding.registry, v);
	*answerCount = 1;
	return;
}

// demand([timeout]) -> the value, nil when the timeout passed.
static void lh_Channel_demand(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						 LhatValue *answers, int *answerCount)
{
	(void) context;
	CHANNEL_SELF();
	Variant v;
	bool got = count >= 2 ? c->demand(&v, lh::optNumber(args, count, 1, 0)) : c->demand(&v);
	if (!got)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	answers[0] = lh::pushVariant(machine, *binding.registry, v);
	*answerCount = 1;
	return;
}

static void lh_Channel_peek(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						 LhatValue *answers, int *answerCount)
{
	(void) context;
	CHANNEL_SELF();
	Variant v;
	if (!c->peek(&v))
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	answers[0] = lh::pushVariant(machine, *binding.registry, v);
	*answerCount = 1;
	return;
}

static void lh_Channel_getCount(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						 LhatValue *answers, int *answerCount)
{
	(void) context;
	CHANNEL_SELF();
	answers[0] = lhat_integer(c->getCount());
	*answerCount = 1;
	return;
}

static void lh_Channel_hasRead(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						 LhatValue *answers, int *answerCount)
{
	(void) context;
	CHANNEL_SELF();
	answers[0] = lhat_bool(c->hasRead((uint64) lh::optNumber(args, count, 1, 0)));
	*answerCount = 1;
	return;
}

static void lh_Channel_clear(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						 LhatValue *answers, int *answerCount)
{
	(void) context;
	CHANNEL_SELF();
	c->clear();
	return;
}

// performAtomic(fn, ...): fn(channel, ...) runs with the channel locked.
static void lh_Channel_performAtomic(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						 LhatValue *answers, int *answerCount)
{
	(void) context;
	CHANNEL_SELF();
	if (count < 2 || !lhat_is_object_kind(args[1], LHAT_OBJECT_SUBROUTINE))
	{
		lh::raise(machine, "performAtomic needs a procedure to call");
		return;
	}
	std::vector<LhatValue> passed;
	passed.push_back(args[0]);
	for (size_t i = 2; i < count; i++)
		passed.push_back(args[i]);
	c->lockMutex();
	LhatRunResult ran = lhat_machine_call(machine, args[1], passed.data(), passed.size());
	c->unlockMutex();
	(void) ran; // a fault ends the run the host function was called from
	return;
}

} // thread
} // love

namespace love
{
namespace lh
{

bool lhopen_love_thread(Context &ctx)
{
	using namespace love::thread;
	const char *m = "love.thread";

	if (!ctx.objectType(m, "Thread", LhThread::type) || !ctx.objectType(m, "Channel", Channel::type))
		return false;
	if (ctx.types())
		return true;

	binding.errors = ctx.errors;
	binding.registry = ctx.registry;
	binding.program = ctx.program;

	return ctx.func(m, "newThread", "p^string^ -> love.thread.Thread;", lh_newThread, nullptr)
		&& ctx.func(m, "newThread", "p^love.filesystem.File -> love.thread.Thread;", lh_newThread, nullptr)
		&& ctx.func(m, "newThread", "p^love.filesystem.FileData -> love.thread.Thread;", lh_newThread, nullptr)
		&& ctx.func(m, "newChannel", "p^ -> love.thread.Channel;", lh_newChannel, nullptr)
		&& ctx.func(m, "getChannel", "p^string^ -> love.thread.Channel;", lh_getChannel, nullptr)
		&& ctx.member(m, "Thread", "start", "p^self^, ...;", lh_Thread_start, nullptr)
		&& ctx.member(m, "Thread", "wait", "p^self^;", lh_Thread_wait, nullptr)
		&& ctx.member(m, "Thread", "getError", "f^self^ -> string^|nil^;", lh_Thread_getError, nullptr)
		&& ctx.member(m, "Thread", "isRunning", "f^self^ -> bool^;", lh_Thread_isRunning, nullptr)
		&& ctx.member(m, "Channel", "push", "p^self^, any^ -> number^;", lh_Channel_push, nullptr)
		&& ctx.member(m, "Channel", "supply", "p^self^, any^ -> bool^;", lh_Channel_supply, nullptr)
		&& ctx.member(m, "Channel", "supply", "p^self^, any^, number^ -> bool^;", lh_Channel_supply, nullptr)
		&& ctx.member(m, "Channel", "pop", "p^self^ -> any^;", lh_Channel_pop, nullptr)
		&& ctx.member(m, "Channel", "demand", "p^self^ -> any^;", lh_Channel_demand, nullptr)
		&& ctx.member(m, "Channel", "demand", "p^self^, number^ -> any^;", lh_Channel_demand, nullptr)
		&& ctx.member(m, "Channel", "peek", "p^self^ -> any^;", lh_Channel_peek, nullptr)
		&& ctx.member(m, "Channel", "getCount", "f^self^ -> number^;", lh_Channel_getCount, nullptr)
		&& ctx.member(m, "Channel", "hasRead", "f^self^, number^ -> bool^;", lh_Channel_hasRead, nullptr)
		&& ctx.member(m, "Channel", "clear", "p^self^;", lh_Channel_clear, nullptr)
		// performAtomic(fn, ...): one arm per count of extra arguments, since a
		// procedure of fixed arity does not conform to a variadic type.
		&& ctx.member(m, "Channel", "performAtomic", "p^self^, p^love.thread.Channel;;", lh_Channel_performAtomic, nullptr)
		&& ctx.member(m, "Channel", "performAtomic", "p^self^, p^love.thread.Channel, any^;, any^;", lh_Channel_performAtomic, nullptr)
		&& ctx.member(m, "Channel", "performAtomic", "p^self^, p^love.thread.Channel, any^, any^;, any^, any^;", lh_Channel_performAtomic, nullptr)
		&& ctx.member(m, "Channel", "performAtomic", "p^self^, p^love.thread.Channel, any^, any^, any^;, any^, any^, any^;", lh_Channel_performAtomic, nullptr);
}

} // lh
} // love
