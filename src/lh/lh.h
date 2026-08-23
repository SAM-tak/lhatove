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
#include "common/Exception.h"
#include "common/Object.h"
#include "common/Variant.h"
#include "common/int.h"

#include <lhat.h>

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace love
{
namespace lh
{

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

// 05 の 8.7: every registration belongs before the check, and a signature
// may only name types registered before it. Each module registrar is
// therefore called twice: once to declare its types, once to fill in
// members and functions against the now-known types.
enum class Phase
{
	TYPES,
	MEMBERS,
};

// The error kinds every binding answers with. Registered first of all, under
// "love", so a signature anywhere may name them.
struct Errors
{
	const LhatErrorKind *misuse = nullptr;       // bad argument, wrong state
	const LhatErrorKind *io = nullptr;           // a file or device said no
	const LhatErrorKind *notSupported = nullptr; // the platform cannot
};

// 05 の 8.8: one hostdata tag per LOVE object type. The tag is what a later
// call checks before reading the pointer as the type it expects; the
// love::Type beside it is what lets a union parameter (draw(Drawable)) be
// served by one check, through love::Type::isa.
class TypeRegistry
{
public:

	// The tag registered for exactly `type`, else for the nearest registered
	// type it derives from, else nullptr.
	const LhatHostDataTag *tagFor(love::Type &type) const;
	love::Type *typeFor(const LhatHostDataTag *tag) const;

	void add(love::Type &type, const LhatHostDataTag *tag);

private:

	std::map<love::Type *, const LhatHostDataTag *> tags;
	std::map<const LhatHostDataTag *, love::Type *> types;
};

class ParkingLot;

struct Context
{
	LhatProgram *program = nullptr;
	Phase phase = Phase::TYPES;
	Errors *errors = nullptr;
	TypeRegistry *registry = nullptr;
	ParkingLot *lot = nullptr;     // where a binding parks values it must keep
	std::string *failed = nullptr; // the registration that refused, for the report

	bool types() const { return phase == Phase::TYPES; }
	bool members() const { return phase == Phase::MEMBERS; }

	// Thin wrappers that keep the registration calls short at the call site.
	bool func(const char *module, const char *name, const char *signature, LhatHostFn fn, void *ctx) const;
	bool member(const char *module, const char *type, const char *name, const char *signature, LhatHostFn fn, void *ctx) const;
	bool global(const char *name, const char *signature, LhatHostFn fn, void *ctx) const;
	bool bind(const char *name, const char *member) const;

	// A LOVE object type as a hostdata type. Call in both phases: the TYPES
	// phase declares the tag, the MEMBERS phase adds what every object
	// answers (dispose, type, typeOf). Returns false if either refused.
	bool objectType(const char *module, const char *name, love::Type &type) const;
};

typedef bool (*Registrar)(Context &ctx);

// ---------------------------------------------------------------------------
// Program + machine
// ---------------------------------------------------------------------------

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
	Errors &errors() { return errors_; }
	TypeRegistry &registry() { return registry_; }
	ParkingLot *lot() const { return lot_.get(); }

	// Registers love.Error, then runs every registrar through both phases.
	// False if any refused.
	bool registerAll(const Registrar *registrars, size_t count);

	// Which registrar refused, for the report.
	const std::string &failedRegistrar() const { return failedRegistrar_; }

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
	Errors errors_;
	TypeRegistry registry_;
	StrongRef<ParkingLot> lot_;
	std::string failedRegistrar_;
};

// ---------------------------------------------------------------------------
// Parking
// ---------------------------------------------------------------------------

// The host holds no GC roots (05 の 8.8), so an L^ value the engine keeps
// across calls -- a Body's user data, a World's contact callback -- is parked
// in a table at L^.modules.love.registry under an integer slot. The lot owns
// the slots; a Parked object is one slot, given back when it dies. This is
// what common/Reference.h was for Lua.
class ParkingLot : public love::Object
{
public:

	static love::Type type;

	ParkingLot();
	virtual ~ParkingLot();

	// Makes the registry table on `machine` and parks it at love.registry.
	bool attach(LhatMachine *machine);

	// Forgets the machine: slots given back after this are not written.
	void detach();

	LhatMachine *machine() const { return machine_; }

	// A slot holding `value`, or 0 when the table refused. Runs the sweep
	// first; call only from a safe point (inside a host function).
	uint32 park(LhatValue value);

	// The value in `slot`; nil for 0 or when detached.
	LhatValue get(uint32 slot) const;

	// Gives the slot back at the next safe point. A wrapper's dispose may run
	// inside the collector, where the host must not write a table, so the
	// write waits for the next park() or sweep().
	void releaseLater(uint32 slot);

	// Writes nil into every slot given back since the last sweep.
	void sweep();

private:

	LhatMachine *machine_;
	LhatTable *table_;
	uint32 next_;
	std::vector<uint32> free_;
	std::vector<uint32> pending_;
};

// One parked value, as a love::Object so the engine can hold it the way it
// holds any other (StrongRef, userdata slots, listener fields).
class Parked : public love::Object
{
public:

	static love::Type type;

	Parked(ParkingLot *lot, LhatValue value);
	virtual ~Parked();

	// False when the lot refused the value.
	bool ok() const { return slot_ != 0; }

	LhatValue get() const;
	ParkingLot *lot() const { return lot_.get(); }

private:

	StrongRef<ParkingLot> lot_;
	uint32 slot_;
};

// ---------------------------------------------------------------------------
// Values
// ---------------------------------------------------------------------------

// Measure-then-fill helpers.
std::string valueText(LhatValue value);
bool makeString(LhatMachine *machine, const std::string &text, LhatValue *out);
bool makeTuple(LhatMachine *machine, const LhatValue *values, size_t count, LhatValue *out);

// The string a value holds, or nullptr when it is not a string.
const char *stringOf(LhatValue value, size_t *length = nullptr);

// Argument readers. A missing or mistyped argument answers the fallback; the
// checker has already refused a call whose declared arguments are wrong, so
// these only guard the variadic tail and the host's own mistakes.
double optNumber(const LhatValue *args, size_t count, size_t index, double fallback);
bool optBool(const LhatValue *args, size_t count, size_t index, bool fallback);
std::string optString(const LhatValue *args, size_t count, size_t index, const std::string &fallback);

// Table fields by name (a missing field answers nil / the fallback). The key
// string is made on `machine`; nothing here runs an instruction.
LhatValue field(LhatMachine *machine, LhatValue table, const char *name);
double fieldNumber(LhatMachine *machine, LhatValue table, const char *name, double fallback);
bool fieldBool(LhatMachine *machine, LhatValue table, const char *name, bool fallback);
std::string fieldString(LhatMachine *machine, LhatValue table, const char *name, const std::string &fallback);
bool fieldIs(LhatMachine *machine, LhatValue table, const char *name, LhatValueTag tag);

// A love::Variant (what events and channels carry) as a value on `machine`.
// Objects come back as fresh hostdata; tables are copied; light userdata is
// refused (nil).
LhatValue pushVariant(LhatMachine *machine, const TypeRegistry &registry, const Variant &v);

// ---------------------------------------------------------------------------
// Objects
// ---------------------------------------------------------------------------

// A fresh hostdata wrapper around `object` (retained; the wrapper's dispose
// releases it). 05 の 8.8: two wrappers of one object compare equal, so a
// fresh one per push is what the engine hands out. `type` is the most
// derived type the caller knows, as luax_pushtype took it; nil when neither
// it nor a type it derives from was registered.
LhatValue pushObject(LhatMachine *machine, const TypeRegistry &registry, love::Type &type, love::Object *object);

template <typename T>
LhatValue pushObject(LhatMachine *machine, const TypeRegistry &registry, T *object)
{
	return pushObject(machine, registry, T::type, object);
}

// The object a hostdata value wraps, as T, or nullptr when the value is not
// a wrapper of a T (derived types pass: love::Type::isa).
love::Object *checkObject(LhatValue value, const TypeRegistry &registry, love::Type &type);

template <typename T>
T *checkObject(LhatValue value, const TypeRegistry &registry)
{
	return (T *) checkObject(value, registry, T::type);
}

// ---------------------------------------------------------------------------
// Errors
// ---------------------------------------------------------------------------

// An error value of `kind` carrying `message`; nil when the machine could
// not make one (out of memory), which the VM reports as a type error.
LhatValue fail(LhatMachine *machine, const LhatErrorKind *kind, const std::string &message);

// A programmer error the API does not declare (a bad enum, a module that is
// not loaded, a GL failure). Lua raised these with lua_error; here it is
// 05 の 8.7改2's lhat_machine_panic: the run ends as if the call site had
// written panic^, traceback standing. The answer is nil for the caller to
// return -- it is dropped.
LhatValue raise(LhatMachine *machine, const std::string &message);

// Runs `body`; a love::Exception thrown inside is raised as above.
LhatValue guard(LhatMachine *machine, const std::function<LhatValue()> &body);

// Runs `body`; a love::Exception thrown inside becomes an error value of
// `kind`. The successor of luax_catchexcept -- errors are values here, so
// the body's own answer is returned when nothing was thrown.
LhatValue catchexcept(LhatMachine *machine, const LhatErrorKind *kind, const std::function<LhatValue()> &body);

// ---------------------------------------------------------------------------
// Calling back
// ---------------------------------------------------------------------------

// Calls table[name] if it holds a subroutine. Answers false when there was
// no such member; the run result is otherwise the callee's. A fault inside
// ends the run the host function was called from (vm.c's nested-fault rule).
bool callMember(LhatMachine *machine, LhatValue table, const char *name, const LhatValue *args, size_t count, LhatRunResult *out);

// A value kept alive at L^.modules.<module>.<name>: the host holds no GC
// roots of its own, so anything it needs across calls is parked there.
bool park(LhatMachine *machine, const char *module, const char *name, LhatValue value);

} // lh
} // love

#endif // LOVE_LH_LH_H
