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

#include "Boot.h"
#include "Boot.lh.h"
#include "PhysfsLoader.h"
#include "lh.h"

// lhatstdlib, the modules the porting plan admits into the game's program:
// error kinds, std.debug, std.regex and std.load. std.io stays out --
// love.filesystem owns file access.
// The stdlib headers carry no extern "C" guard of their own (lhat.h does).
extern "C"
{
#include "stdlib/debug.h"
#include "stdlib/error.h"
#include "stdlib/load.h"
#include "stdlib/regex.h"
}

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace love
{
namespace lh
{

// Registrars of the love.* modules, in the order they are declared. Each is
// run twice (types, then members); see lh::Runtime::registerAll.
bool lhopen_love(Context &ctx);

static const Registrar registrars[] = {
	lhopen_love,
};

static bool registerStdlib(LhatProgram *program)
{
	return lhatstdlib_error_register(program)
		&& lhatstdlib_debug_register(program)
		&& lhatstdlib_regex_register(program)
		&& lhatstdlib_load_register(program);
}

// Something the user has to see. Text always; a message box as well for the
// windowed executable, since nothing else would show it.
static bool consoleBuild = false;

static void report(const std::string &title, const std::string &text)
{
	fputs(text.c_str(), stderr);
	if (!text.empty() && text.back() != '\n')
		fputc('\n', stderr);
	fflush(stderr);
	if (!consoleBuild)
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title.c_str(), text.c_str(), nullptr);
}

static bool endsWith(const std::string &s, const char *suffix)
{
	size_t n = strlen(suffix);
	return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
}

static int boot(int argc, char **argv, bool console)
{
	consoleBuild = console;

	// Milestone M0: the game is a directory holding main.lh or a single .lh
	// file; without one the embedded hello unit runs. arg.lua's full option
	// handling (fused mode, .love archives) comes with the PhysFS loader.
	Loader loader;
	std::string mainUnit = "main.lh";

	std::string game = argc > 1 ? argv[1] : "";
	if (game.empty())
	{
		loader.hold(mainUnit, hello_main_lh);
	}
	else if (game == "--probe")
	{
		loader.hold(mainUnit, probe_main_lh);
	}
	else if (endsWith(game, ".lh"))
	{
		size_t slash = game.find_last_of("/\\");
		if (slash == std::string::npos)
		{
			loader.setBase(".");
			mainUnit = game;
		}
		else
		{
			loader.setBase(game.substr(0, slash));
			mainUnit = game.substr(slash + 1);
		}
	}
	else
	{
		loader.setBase(game);
	}

	Runtime runtime(Loader::load, &loader);
	if (runtime.program() == nullptr)
	{
		report("lhatove", "Could not create the L^ program.");
		return 1;
	}

	if (!runtime.registerAll(registrars, sizeof(registrars) / sizeof(registrars[0])))
	{
		report("lhatove", "A love module refused to register its API.");
		return 1;
	}
	if (!registerStdlib(runtime.program()))
	{
		report("lhatove", "The L^ standard library refused to register.");
		return 1;
	}

	const LhatUnit *root = runtime.check(mainUnit.c_str());
	if (root == nullptr || !runtime.ok())
	{
		std::string said = runtime.diagnostics();
		if (said.empty())
			said = "Could not read " + mainUnit + " from " + (loader.getBase().empty() ? "the embedded units" : loader.getBase());
		report("lhatove: check failed", said);
		return 1;
	}

	if (!runtime.compile())
	{
		report("lhatove", "Could not compile the program.");
		return 1;
	}

	LhatRunResult ran = lhat_run(runtime.machine(), lhat_unit_proto(root));
	if (ran.status != LHAT_RUN_OK)
	{
		report("lhatove: error", runtime.describe(ran));
		return 1;
	}

	// A script's return^ is its exit code when it is a number.
	if (lhat_is_integer(ran.value))
		return (int) lhat_as_integer(ran.value);
	return 0;
}

} // lh
} // love

int love_lh_boot(int argc, char **argv, bool console)
{
	return love::lh::boot(argc, argv, console);
}
