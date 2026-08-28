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
static void lh_print(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						 LhatValue *answers, int *answerCount)
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
	return;
}

// love.getVersion() -> (major, minor, revision, codename)
static void lh_getVersion(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						 LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) arguments;
	(void) count;
	LhatValue codename = lhat_nil();
	if (!makeString(machine, VERSION_CODENAME, &codename))
		return;
	answers[0] = lhat_integer(VERSION_MAJOR);
	answers[1] = lhat_integer(VERSION_MINOR);
	answers[2] = lhat_integer(VERSION_REV);
	answers[3] = codename;
	*answerCount = 4;
}

bool lhopen_love(Context &ctx)
{
	if (ctx.types())
		return true;

	return ctx.global("print", "f^...->nil^;", lh_print, nullptr)
		&& ctx.bind("print", "L^.print")
		&& ctx.func("love", "getVersion", "f^ -> (number^, number^, number^, string^);", lh_getVersion, nullptr);
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
