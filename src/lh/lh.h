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

#ifndef LOVE_LH_LH_H
#define LOVE_LH_LH_H

// The glue between LOVE and the L^ runtime. This takes over the role that
// src/common/runtime.h (the luax_* helpers) played for Lua: registering
// what the engine provides, converting values at the boundary, and owning
// the program/machine pair. Nothing here is part of the L^ language.

#include "common/config.h"

#include <lhat.h>

#include <string>
#include <vector>

namespace love
{
namespace lh
{

// 05 の 8.7: every registration belongs before the check, and a signature
// may only name types registered before it. Each module registrar is
// therefore called twice: once to declare its types, once to fill in
// members and functions against the now-known types.
enum class Phase
{
	TYPES,
	MEMBERS,
};

struct Context
{
	LhatProgram *program = nullptr;
	Phase phase = Phase::TYPES;

	bool types() const { return phase == Phase::TYPES; }
	bool members() const { return phase == Phase::MEMBERS; }

	// Thin wrappers that keep the registration calls short at the call site.
	bool func(const char *module, const char *name, const char *signature, LhatHostFn fn, void *ctx) const;
	bool global(const char *name, const char *signature, LhatHostFn fn, void *ctx) const;
	bool bind(const char *name, const char *member) const;
};

typedef bool (*Registrar)(Context &ctx);

// Owns one LhatProgram and, once compiled, one LhatMachine.
class Runtime
{
public:

	Runtime(LhatProgramLoader loader, void *loaderContext);
	~Runtime();

	Runtime(const Runtime &) = delete;
	Runtime &operator=(const Runtime &) = delete;

	LhatProgram *program() const { return program_; }
	LhatMachine *machine() const { return machine_; }

	// Runs every registrar through both phases. False if any refused.
	bool registerAll(const Registrar *registrars, size_t count);

	// lhat_program_check; the unit is returned even when it failed so the
	// caller can decide what to do with the diagnostics.
	const LhatUnit *check(const char *path);

	// True when the program and every unit checked without a diagnostic.
	bool ok() const;

	// Every diagnostic the program and its units recorded, one per line.
	std::string diagnostics() const;

	// lhat_program_compile + machine creation + install. False on failure;
	// compile may be called again after later checks (incremental).
	bool compile();

	// A run result rendered the way a driver reports it: status, panic value,
	// line, and the traceback when frames are standing.
	std::string describe(const LhatRunResult &ran) const;

private:

	LhatProgram *program_;
	LhatMachine *machine_;
};

// Value helpers (all measure-then-fill behind the scenes).
std::string valueText(LhatValue value);
bool makeString(LhatMachine *machine, const std::string &text, LhatValue *out);
bool makeTuple(LhatMachine *machine, const LhatValue *values, size_t count, LhatValue *out);

} // lh
} // love

#endif // LOVE_LH_LH_H
