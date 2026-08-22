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

#include "common/Module.h"

// The modules milestone M1 brings up, in boot.lua's order.
#include "modules/timer/Timer.h"
#include "modules/event/sdl/Event.h"
#include "modules/keyboard/sdl/Keyboard.h"
#include "modules/mouse/sdl/Mouse.h"
#include "modules/font/freetype/Font.h"
#include "modules/window/sdl/Window.h"
#include "modules/graphics/Graphics.h"

// lhatstdlib, the modules the porting plan admits into the game's program:
// error kinds, std.debug, std.regex and std.load. std.io stays out --
// love.filesystem owns file access.
#include "stdlib/debug.h"
#include "stdlib/error.h"
#include "stdlib/load.h"
#include "stdlib/regex.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace love
{
namespace lh
{

// Registrars of the love.* modules, in the order they are declared. Each is
// run twice (types, then members); see lh::Runtime::registerAll.
bool lhopen_love(Context &ctx);
bool lhopen_love_timer(Context &ctx);
bool lhopen_love_event(Context &ctx);
bool lhopen_love_keyboard(Context &ctx);
bool lhopen_love_mouse(Context &ctx);
bool lhopen_love_window(Context &ctx);
bool lhopen_love_graphics(Context &ctx);
static bool lhopen_love_boot(Context &ctx);

static const Registrar registrars[] = {
	lhopen_love,
	lhopen_love_boot,
	lhopen_love_timer,
	lhopen_love_event,
	lhopen_love_keyboard,
	lhopen_love_mouse,
	lhopen_love_window,
	lhopen_love_graphics,
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

// ---------------------------------------------------------------------------
// love.boot -- what Boot.lh's run needs from the engine
// ---------------------------------------------------------------------------

// The callbacks a game may export, with the signature each has in the
// handlers table. The table always holds every one of them: the game's own
// member where it exported one, a no-op otherwise -- L^ has no way to write
// "call it if it is there" against a static table type, so that question is
// settled here, once, before the loop starts.
struct Callback
{
	const char *name;
	const char *signature;
};

static const Callback callbacks[] = {
	{"load", "p^;"},
	{"update", "p^number^;"},
	{"draw", "p^;"},
	{"quit", "p^ -> bool^;"},
	{"keypressed", "p^string^, string^, bool^;"},
	{"keyreleased", "p^string^, string^;"},
	{"textinput", "p^string^;"},
	{"mousemoved", "p^number^, number^, number^, number^, bool^;"},
	{"mousepressed", "p^number^, number^, number^, bool^, number^;"},
	{"mousereleased", "p^number^, number^, number^, bool^, number^;"},
	{"wheelmoved", "p^number^, number^, number^, number^, string^;"},
	{"resize", "p^number^, number^;"},
	{"focus", "p^bool^;"},
	{"mousefocus", "p^bool^;"},
	{"visible", "p^bool^;"},
};

struct BootState
{
	LhatValue handlers = lhat_nil(); // parked at L^.modules.love.boot.handlers
	std::string handlersSignature;   // outlives the program (program.h)
};

static LhatValue lh_boot_handlers(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) arguments;
	(void) count;
	return ((const BootState *) context)->handlers;
}

// The no-op a missing callback gets. Variadic, so it fits any of them.
static LhatValue lh_boot_noop(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	return lhat_nil();
}

// quit's no-op answers false: "I did not veto the quit".
static LhatValue lh_boot_noquit(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	return lhat_bool(false);
}

// One per process: a boot owns one program, and the registration context
// has to outlive it.
static BootState bootState;

static bool lhopen_love_boot(Context &ctx)
{
	if (ctx.types())
		return true;

	// The handlers table's type, spelled from the callback list so the two
	// cannot drift apart.
	std::string signature = "f^ -> t^{ ";
	for (size_t i = 0; i < sizeof(callbacks) / sizeof(callbacks[0]); i++)
	{
		if (i > 0)
			signature += ", ";
		signature += callbacks[i].name;
		signature += " : ";
		signature += callbacks[i].signature;
	}
	signature += " };";
	bootState.handlersSignature = signature;

	return ctx.func("love.boot", "handlers", bootState.handlersSignature.c_str(), lh_boot_handlers, &bootState);
}

// Builds the handlers table from what the game's module exported, parks it,
// and remembers it for love.boot.handlers().
static bool buildHandlers(LhatMachine *machine, LhatValue game)
{
	LhatValue table = lhat_nil();
	if (!lhat_machine_make_table(machine, &table))
		return false;
	LhatTable *t = (LhatTable *) lhat_as_object(table);
	const LhatTable *exported = lhat_is_object_kind(game, LHAT_OBJECT_TABLE) ? (const LhatTable *) lhat_as_object(game) : nullptr;

	for (const Callback &cb : callbacks)
	{
		LhatValue key = lhat_nil();
		if (!makeString(machine, cb.name, &key))
			return false;

		LhatValue member = exported != nullptr ? lhat_table_get(exported, key) : lhat_nil();
		if (!lhat_is_object_kind(member, LHAT_OBJECT_SUBROUTINE))
		{
			bool isQuit = strcmp(cb.name, "quit") == 0;
			if (!lhat_machine_make_host(machine, isQuit ? lh_boot_noquit : lh_boot_noop, nullptr, 0, true, false, false, nullptr, &member))
				return false;
		}

		bool refused = false;
		if (!lhat_table_set(t, key, member, &refused) || refused)
			return false;
	}

	// Parked: the host holds no GC roots of its own.
	if (!park(machine, "love.boot", "handlers", table))
		return false;
	bootState.handlers = table;
	return true;
}

// ---------------------------------------------------------------------------
// The boot sequence
// ---------------------------------------------------------------------------

// Module instances in boot.lua's order, released in reverse after the
// machine is gone -- GPU objects a wrapper's dispose releases need Graphics
// alive at that moment.
class Modules
{
public:

	~Modules()
	{
		for (auto it = held.rbegin(); it != held.rend(); ++it)
			(*it)->release();
	}

	template <typename T>
	T *add(T *module)
	{
		held.push_back(module);
		return module;
	}

private:

	std::vector<Module *> held;
};

static int exitCodeOf(LhatValue value)
{
	return lhat_is_integer(value) ? (int) lhat_as_integer(value) : 0;
}

static int boot(int argc, char **argv, bool console)
{
	consoleBuild = console;

	// Milestone M1: the game is a directory holding main.lh or a single .lh
	// file; without one the embedded hello unit runs. arg.lua's full option
	// handling (fused mode, .love archives) comes with the PhysFS loader.
	Loader loader;
	std::string mainUnit = "main.lh";
	loader.hold("Boot.lh", boot_lh);

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

	// Declared before the runtime so it is destroyed after it.
	Modules modules;

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

	const LhatUnit *bootUnit = runtime.check("Boot.lh");
	const LhatUnit *root = runtime.check(mainUnit.c_str());
	if (bootUnit == nullptr || root == nullptr || !runtime.ok())
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

	LhatMachine *machine = runtime.machine();

	// A script (the hello and probe units) answers its return^ and is done;
	// a module^ unit answers its public table, which is a game.
	LhatRunResult ran = lhat_run(machine, lhat_unit_proto(root));
	if (ran.status != LHAT_RUN_OK)
	{
		report("lhatove: error", runtime.describe(ran));
		return 1;
	}
	if (!lhat_is_object_kind(ran.value, LHAT_OBJECT_TABLE))
		return exitCodeOf(ran.value);

	LhatValue gameTable = ran.value;
	if (!park(machine, "love.boot", "game", gameTable))
	{
		report("lhatove", "Could not keep the game's module table.");
		return 1;
	}

	// The modules. conf.lh (milestone M2) will choose; until then all of M1's.
	try
	{
		modules.add(new love::timer::Timer());
		modules.add(new love::event::sdl::Event());
		modules.add(new love::keyboard::sdl::Keyboard());
		modules.add(new love::mouse::sdl::Mouse());
		modules.add(new love::font::freetype::Font());
		love::window::Window *window = modules.add(new love::window::sdl::Window());
		modules.add(love::graphics::Graphics::createInstance());

		window->setWindowTitle("Untitled");
		love::window::WindowSettings settings;
		if (!window->setWindow(800, 600, &settings))
		{
			report("lhatove", "Could not set window mode.");
			return 1;
		}
	}
	catch (const love::Exception &e)
	{
		report("lhatove", std::string("Could not start a module: ") + e.what());
		return 1;
	}

	// The first couple event pumps on some systems can take a while; better
	// here than in the first frames.
	for (int i = 0; i < 2; i++)
		Module::getInstance<love::event::Event>(Module::M_EVENT)->pump();
	Module::getInstance<love::timer::Timer>(Module::M_TIMER)->step();

	if (!buildHandlers(machine, gameTable))
	{
		report("lhatove", "Could not build the callback table.");
		return 1;
	}

	// run: the game's own if it exported one, else Boot.lh's.
	LhatValue run = lhat_nil();
	{
		LhatValue key = lhat_nil();
		makeString(machine, "run", &key);
		run = lhat_table_get((const LhatTable *) lhat_as_object(gameTable), key);
	}
	if (!lhat_is_object_kind(run, LHAT_OBJECT_SUBROUTINE))
	{
		LhatRunResult bootRan = lhat_run(machine, lhat_unit_proto(bootUnit));
		if (bootRan.status != LHAT_RUN_OK || !lhat_is_object_kind(bootRan.value, LHAT_OBJECT_SUBROUTINE))
		{
			report("lhatove: error", "Boot.lh did not answer a run procedure: " + runtime.describe(bootRan));
			return 1;
		}
		run = bootRan.value;
	}
	if (!park(machine, "love.boot", "run", run))
		return 1;

	// 02 の 15.5: calling a yieldable procedure answers its coroutine rather
	// than running it. A run that never yields answers its value at once.
	LhatRunResult started = lhat_machine_call(machine, run, nullptr, 0);
	if (started.status != LHAT_RUN_OK)
	{
		report("lhatove: error", runtime.describe(started));
		return 1;
	}
	if (!lhat_is_object_kind(started.value, LHAT_OBJECT_COROUTINE))
		return exitCodeOf(started.value);

	LhatValue coroutine = started.value;
	if (!park(machine, "love.boot", "coroutine", coroutine))
		return 1;

	// One resume per frame until the run returns.
	const char *gcstats = getenv("LHATOVE_GC_STATS");
	unsigned frame = 0;
	while (true)
	{
		LhatRunResult step = lhat_machine_resume(machine, coroutine, nullptr, 0);
		if (step.status != LHAT_RUN_OK)
		{
			report("lhatove: error", runtime.describe(step));
			return 1;
		}
		if (gcstats != nullptr && (++frame % 120) == 0)
			fprintf(stderr, "[gc] frame %u: collected %zu, live %zu\n", frame, step.collected, step.live);
		if (lhat_machine_coroutine_done(coroutine))
			return exitCodeOf(step.value);
	}
}

} // lh
} // love

int love_lh_boot(int argc, char **argv, bool console)
{
	return love::lh::boot(argc, argv, console);
}
