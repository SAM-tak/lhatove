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

// The `love` module itself and what the engine binds into L^ without a
// require^ (print). Successor of love.cpp's luaopen_love; that file stays in
// the tree as the reference for what still has to be ported.

#include "common/config.h"
#include "common/version.h"
#include "love.h"
#include "lh/lh.h"

#include <cstdio>
#include <string>

#ifdef LOVE_WINDOWS
#include <windows.h>
#endif

namespace love
{
namespace lh
{

// 05 の 8.2: print is what a script writer looks for first. Goes to stdout,
// which lovec shows and love.exe sends to the console it attached to.
static LhatValue lh_print(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	std::string line;
	for (size_t i = 0; i < count; i++)
	{
		if (i > 0)
			line += '\t';
		line += valueText(arguments[i]);
	}
	line += '\n';
	fputs(line.c_str(), stdout);
	fflush(stdout);
	return lhat_nil();
}

// love.getVersion() -> (major, minor, revision, codename)
static LhatValue lh_getVersion(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	(void) arguments;
	(void) count;
	LhatValue codename = lhat_nil();
	if (!makeString(machine, VERSION_CODENAME, &codename))
		return lhat_nil();
	LhatValue parts[4] = {
		lhat_integer(VERSION_MAJOR),
		lhat_integer(VERSION_MINOR),
		lhat_integer(VERSION_REV),
		codename,
	};
	LhatValue answer = lhat_nil();
	if (!makeTuple(machine, parts, 4, &answer))
		return lhat_nil();
	return answer;
}

// ---------------------------------------------------------------------------
// love.probe -- milestone M0 only. Answers the porting plan's open questions
// about the boundary and goes away once the real modules exist.
// ---------------------------------------------------------------------------

struct Probe
{
	const LhatErrorKind *failed = nullptr;
};

// U1: a host answering an error value its signature declares.
static LhatValue lh_probe_failing(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) arguments;
	(void) count;
	Probe *probe = (Probe *) context;
	LhatValue error = lhat_nil();
	if (!lhat_machine_make_error(machine, probe->failed, "failing on purpose", lhat_nil(), &error))
		return lhat_nil();
	return error;
}

static LhatValue lh_probe_update(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	double dt = count > 0 && lhat_is_number(arguments[0]) ? lhat_number_as_real(arguments[0]) : -1.0;
	printf("U4 probe: update(%g) reached the host\n", dt);
	return lhat_nil();
}

static LhatValue lh_probe_draw(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	printf("U4 probe: draw() reached the host\n");
	return lhat_nil();
}

// U4: a host answering a table whose members are procedures, under a
// structural signature -- the shape love.boot.handlers() will have.
static LhatValue lh_probe_handlers(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	(void) arguments;
	(void) count;

	// Nothing here runs an instruction, so the collector cannot step between
	// making the table and filling it (the table is not yet handed over).
	LhatValue table = lhat_nil();
	if (!lhat_machine_make_table(machine, &table))
		return lhat_nil();
	LhatTable *t = (LhatTable *) lhat_as_object(table);

	struct Entry
	{
		const char *name;
		LhatHostFn fn;
		uint8_t parameters;
	};
	const Entry entries[] = {
		{"update", lh_probe_update, 1},
		{"draw", lh_probe_draw, 0},
	};
	for (const Entry &e : entries)
	{
		LhatValue key = lhat_nil(), fn = lhat_nil();
		bool refused = false;
		if (!makeString(machine, e.name, &key)
			|| !lhat_machine_make_host(machine, e.fn, nullptr, e.parameters, false, false, false, nullptr, &fn)
			|| !lhat_table_set(t, key, fn, &refused) || refused)
			return lhat_nil();
	}
	return table;
}

static Probe probe;

bool lhopen_love(Context &ctx)
{
	if (ctx.types())
	{
		// love.Error is the error kind every binding answers with. It also has
		// to be the first registration under "love": when the "love" module
		// table is created implicitly by a submodule (love.probe) instead, a
		// structural return type reached through `import^ love` loses its
		// function-typed members (lhat HEAD ad39df0; see docs/porting/lhat-issues.md).
		static const char *const love_variants[] = {"Misuse", "IO", "NotSupported"};
		if (!lhat_register_error_kind(ctx.program, "love", "Error", love_variants, 3, nullptr, nullptr))
			return false;

		static const char *const variants[] = {"Failed"};
		const LhatErrorKind *kinds[1] = {nullptr};
		if (!lhat_register_error_kind(ctx.program, "love.probe", "Error", variants, 1, nullptr, kinds))
			return false;
		probe.failed = kinds[0];
		return true;
	}

	return ctx.global("print", "f^...->nil^;", lh_print, nullptr)
		&& ctx.bind("print", "L^.print")
		&& ctx.func("love", "getVersion", "f^ -> (number^, number^, number^, string^);", lh_getVersion, nullptr)
		&& ctx.func("love.probe", "failing", "f^ -> number^|love.probe.Error.Failed;", lh_probe_failing, &probe)
		&& ctx.func("love.probe", "handlers", "f^ -> t^{ update : p^number^ -> nil^;, draw : p^ -> nil^; };", lh_probe_handlers, &probe);
}

} // lh
} // love

// ---------------------------------------------------------------------------
// The C entry points love.h still exports.
// ---------------------------------------------------------------------------

const char *love_version()
{
	// Do not refer to love::VERSION here, the linker
	// will patch it back up to the executable's one..
	return LOVE_VERSION_STRING;
}

const char *love_codename()
{
	return love::VERSION_CODENAME;
}

#ifdef LOVE_LEGENDARY_CONSOLE_IO_HACK

bool love_openConsole(const char *&err)
{
	static bool is_open = false;
	err = nullptr;

	if (is_open)
		return true;

	is_open = true;

	if (!AttachConsole(ATTACH_PARENT_PROCESS))
	{
		DWORD winerr = GetLastError();

		if (winerr == ERROR_ACCESS_DENIED)
		{
			// The process is already attached to a console. We'll assume stdout
			// and friends are already being directed there.
			is_open = true;
			return is_open;
		}

		// Create our own console if we can't attach to an existing one.
		if (!AllocConsole())
		{
			is_open = false;
			err = "Could not create console.";
			return is_open;
		}

		SetConsoleTitle(TEXT("LOVE Console"));

		const int MAX_CONSOLE_LINES = 5000;
		CONSOLE_SCREEN_BUFFER_INFO console_info;

		// Set size.
		GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &console_info);
		console_info.dwSize.Y = MAX_CONSOLE_LINES;
		SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), console_info.dwSize);
	}

	FILE *fp;

	// Redirect stdout.
	fp = freopen("CONOUT$", "w", stdout);
	if (fp == NULL)
	{
		err = "Console redirection of stdout failed.";
		return is_open;
	}

	// Redirect stdin.
	fp = freopen("CONIN$", "r", stdin);
	if (fp == NULL)
	{
		err = "Console redirection of stdin failed.";
		return is_open;
	}

	// Redirect stderr.
	fp = freopen("CONOUT$", "w", stderr);
	if (fp == NULL)
	{
		err = "Console redirection of stderr failed.";
		return is_open;
	}

	return is_open;
}

#endif // LOVE_LEGENDARY_CONSOLE_IO_HACK
