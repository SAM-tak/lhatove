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

#include "lh.h"

#include <cstdio>

namespace love
{
namespace lh
{

bool Context::func(const char *module, const char *name, const char *signature, LhatHostFn fn, void *ctx) const
{
	return lhat_register_func(program, module, name, signature, fn, ctx);
}

bool Context::global(const char *name, const char *signature, LhatHostFn fn, void *ctx) const
{
	return lhat_register_global(program, name, signature, fn, ctx);
}

bool Context::bind(const char *name, const char *member) const
{
	return lhat_bind_initial(program, name, member);
}

Runtime::Runtime(LhatProgramLoader loader, void *loaderContext)
	: program_(nullptr)
	, machine_(nullptr)
{
	// 03 の 3.1: a file defaults to strict.
	program_ = lhat_program_new(true, loader, loaderContext);
}

Runtime::~Runtime()
{
	// The machine first: its heap holds values whose release callbacks reach
	// into the engine, and the program's registrations must still exist then.
	if (machine_ != nullptr)
		lhat_machine_dispose(machine_);
	if (program_ != nullptr)
		lhat_program_free(program_);
}

bool Runtime::registerAll(const Registrar *registrars, size_t count)
{
	if (program_ == nullptr)
		return false;

	Context ctx;
	ctx.program = program_;

	for (Phase phase : {Phase::TYPES, Phase::MEMBERS})
	{
		ctx.phase = phase;
		for (size_t i = 0; i < count; i++)
		{
			if (!registrars[i](ctx))
				return false;
		}
	}
	return true;
}

const LhatUnit *Runtime::check(const char *path)
{
	if (program_ == nullptr)
		return nullptr;
	return lhat_program_check(program_, path);
}

bool Runtime::ok() const
{
	if (program_ == nullptr || lhat_program_has_errors(program_))
		return false;
	for (const LhatUnit *unit = lhat_program_units(program_); unit != nullptr; unit = lhat_unit_next(unit))
	{
		if (!lhat_unit_ok(unit))
			return false;
	}
	return true;
}

std::string Runtime::diagnostics() const
{
	std::string out;
	if (program_ == nullptr)
		return "lhat: could not create a program\n";

	// 05 の 6.2: the program's own diagnostics are about a unit rather than
	// a place inside one (unreadable, a cycle), so they have no position.
	size_t own = lhat_program_diagnostic_count(program_);
	for (size_t i = 0; i < own; i++)
	{
		const LhatProgramDiagnostic *d = lhat_program_diagnostic(program_, i);
		out += d->path;
		out += ": error: ";
		out += lhat_program_error_message(d->code);
		out += "\n";
	}

	std::vector<char> room(1024);
	for (const LhatUnit *unit = lhat_program_units(program_); unit != nullptr; unit = lhat_unit_next(unit))
	{
		size_t count = lhat_unit_diagnostic_count(unit);
		for (size_t i = 0; i < count; i++)
		{
			size_t needed = lhat_unit_diagnostic_write(unit, i, true, room.data(), room.size());
			if (needed >= room.size())
			{
				room.resize(needed + 1);
				lhat_unit_diagnostic_write(unit, i, true, room.data(), room.size());
			}
			out += room.data();
			out += "\n";
		}
	}
	return out;
}

bool Runtime::compile()
{
	if (program_ == nullptr || !lhat_program_compile(program_))
		return false;

	if (machine_ == nullptr)
	{
		machine_ = lhat_machine_new();
		if (machine_ == nullptr)
			return false;
	}

	// Puts what was registered into L^.modules so an import^ finds it.
	return lhat_program_install(program_, machine_);
}

std::string Runtime::describe(const LhatRunResult &ran) const
{
	std::string text = lhat_run_status_message(ran.status);
	if (ran.status == LHAT_RUN_PANIC)
		text += ": " + valueText(ran.value);
	if (ran.line > 0)
		text = "line " + std::to_string(ran.line) + ": " + text;

	// 04 の 11.6改: the frames still standing, readable until the machine is
	// run again or disposed.
	if (machine_ != nullptr && lhat_machine_fault_depth(machine_) >= 2)
	{
		size_t needed = lhat_machine_traceback(machine_, nullptr, 0);
		std::vector<char> spelt(needed + 1);
		lhat_machine_traceback(machine_, spelt.data(), spelt.size());
		text += "\n";
		text += spelt.data();
	}
	return text;
}

std::string valueText(LhatValue value)
{
	size_t needed = lhat_value_text(value, nullptr, 0);
	std::vector<char> text(needed + 1);
	lhat_value_text(value, text.data(), text.size());
	return std::string(text.data(), needed);
}

bool makeString(LhatMachine *machine, const std::string &text, LhatValue *out)
{
	return lhat_machine_make_string(machine, text.data(), text.size(), out);
}

bool makeTuple(LhatMachine *machine, const LhatValue *values, size_t count, LhatValue *out)
{
	return lhat_make_tuple(machine, values, count, out);
}

} // lh
} // love
