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
#include "ErrorScreen.h"
#include "Watchdog.h"
#include "PhysfsLoader.h"
#include "lh.h"

#include "common/Module.h"
#include "common/version.h"
#include "modules/love/love.h"

// The modules, in boot.lua's order.
#include "modules/filesystem/physfs/Filesystem.h"
#include "modules/timer/Timer.h"
#include "modules/event/sdl/Event.h"
#include "modules/keyboard/sdl/Keyboard.h"
#include "modules/mouse/sdl/Mouse.h"
#include "modules/joystick/sdl/JoystickModule.h"
#include "modules/touch/sdl/Touch.h"
#include "modules/sound/lullaby/Sound.h"
#include "modules/system/sdl/System.h"
#include "modules/sensor/sdl/Sensor.h"
#include "modules/audio/openal/Audio.h"
#include "modules/audio/null/Audio.h"
#include "modules/data/DataModule.h"
#include "modules/math/MathModule.h"
#include "modules/physics/box2d/Physics.h"
#include "modules/thread/ThreadModule.h"
#include "modules/video/theora/Video.h"
#include "modules/image/Image.h"
#include "modules/font/freetype/Font.h"
#include "modules/window/sdl/Window.h"
#include "modules/window/lh_Window.h"
#include "modules/graphics/Graphics.h"

// lhatstdlib, the modules the porting plan admits into the game's program:
// error kinds, std.debug, std.regex, std.load, std.math (scalar maths; its
// angles are degrees, so love.graphics.rotate takes std.math.rad(a)) and
// std.lton, which is what conf.lton is written in and what a game reads its
// own data files with. std.io stays out -- love.filesystem owns file access
// -- and so does std.math.vector3, which LOVE's API has no use for.
#include "stdlib/debug.h"
#include "stdlib/error.h"
#include "stdlib/load.h"
#include "stdlib/lton.h"
#include "stdlib/math.h"
#include "stdlib/regex.h"

#include <SDL3/SDL.h>

#include <cstdint>
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
bool lhopen_love_filesystem(Context &ctx);
bool lhopen_love_image(Context &ctx);
bool lhopen_love_audio(Context &ctx);
bool lhopen_love_sound(Context &ctx);
bool lhopen_love_data(Context &ctx);
bool lhopen_love_math(Context &ctx);
bool lhopen_love_system(Context &ctx);
bool lhopen_love_touch(Context &ctx);
bool lhopen_love_sensor(Context &ctx);
bool lhopen_love_joystick(Context &ctx);
bool lhopen_love_physics(Context &ctx);
bool lhopen_love_thread(Context &ctx);
static bool lhopen_love_boot(Context &ctx);

static const Registrar registrars[] = {
	lhopen_love,
	lhopen_love_boot,
	lhopen_love_timer,
	lhopen_love_event,
	lhopen_love_keyboard,
	lhopen_love_mouse,
	lhopen_love_filesystem,
	lhopen_love_image,
	lhopen_love_sound,
	lhopen_love_audio,
	lhopen_love_data,
	lhopen_love_math,
	lhopen_love_system,
	lhopen_love_touch,
	lhopen_love_sensor,
	lhopen_love_joystick,
	lhopen_love_physics,
	lhopen_love_thread,
	lhopen_love_window,
	lhopen_love_graphics,
};

static bool registerStdlib(LhatProgram *program)
{
	return lhatstdlib_error_register(program)
		&& lhatstdlib_debug_register(program)
		&& lhatstdlib_regex_register(program)
		&& lhatstdlib_load_register(program)
		&& lhatstdlib_lton_register(program)
		&& lhatstdlib_math_register(program);
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

// LHATOVE_TRACE=1: the boot sequence, step by step, on stderr.
static bool tracing = false;

static void trace(const char *step)
{
	if (tracing)
	{
		fprintf(stderr, "[boot] %s\n", step);
		fflush(stderr);
	}
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
	{"joystickpressed", "p^love.joystick.Joystick, number^;"},
	{"joystickreleased", "p^love.joystick.Joystick, number^;"},
	{"joystickaxis", "p^love.joystick.Joystick, number^, number^;"},
	{"joystickhat", "p^love.joystick.Joystick, number^, string^;"},
	{"joystickadded", "p^love.joystick.Joystick;"},
	{"joystickremoved", "p^love.joystick.Joystick;"},
	{"gamepadpressed", "p^love.joystick.Joystick, string^;"},
	{"gamepadreleased", "p^love.joystick.Joystick, string^;"},
	{"gamepadaxis", "p^love.joystick.Joystick, string^, number^;"},
	{"touchpressed", "p^number^, number^, number^, number^, number^, number^, ...;"},
	{"touchreleased", "p^number^, number^, number^, number^, number^, number^, ...;"},
	{"touchmoved", "p^number^, number^, number^, number^, number^, number^, ...;"},
	{"sensorupdated", "p^string^, number^, number^, number^;"},
	{"threaderror", "p^love.thread.Thread, string^;"},
	{"filedropped", "p^string^;"},
	{"directorydropped", "p^string^;"},
};

struct BootState
{
	LhatValue handlers = lhat_nil(); // parked at L^.modules.love.boot.handlers
	std::string handlersSignature;   // outlives the program (program.h)
	const TypeRegistry *registry = nullptr;
};

static LhatValue lh_boot_restartInto(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count);

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

static LhatValue lh_restartValue(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count);
static LhatValue lh_boot_restartInto(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count);

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
	bootState.registry = ctx.registry;

	return ctx.func("love.boot", "handlers", bootState.handlersSignature.c_str(), lh_boot_handlers, &bootState)
		&& ctx.func("love.boot", "restartInto", "p^string^;", lh_boot_restartInto, &bootState);
}

// 05 の 5.6: a module^ unit's exports answer their types. Every callback the
// game exported has to conform to the signature the handlers table gives
// it, or the loop would call it wrongly at run time. Answers the problems
// found, one per line; empty when all is well.
static std::string checkCallbacks(const LhatUnit *unit)
{
	std::string problems;
	for (const Callback &cb : callbacks)
	{
		size_t needed = lhat_unit_export_type(unit, cb.name, nullptr, 0);
		if (needed == SIZE_MAX)
			continue; // not exported: the no-op takes its place
		if (lhat_unit_export_conforms(unit, cb.name, cb.signature))
			continue;
		std::vector<char> spelt(needed + 1);
		lhat_unit_export_type(unit, cb.name, spelt.data(), spelt.size());
		problems += std::string(lhat_unit_path(unit)) + ": error: " + cb.name + " is " + spelt.data() + ", but a love callback of that name is " + cb.signature + "\n";
	}
	return problems;
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
// conf.lton
// ---------------------------------------------------------------------------

// What conf.lton may say, with boot.lua's defaults. LTON is the inside of a
// table literal and nothing else (lhat DesignDocuments/08-lton.md), so the
// file is data rather than a script that answers data -- and 02 の 15.1 keeps
// it that way: the text is read as an f^ body, and an f^ may call only an f^,
// so nothing with an effect can be written there. Fields it does not write
// keep these.
struct Conf
{
	std::string identity;
	bool appendidentity = false;
	std::string version = LOVE_VERSION_STRING;
	bool console = false;
	struct
	{
		std::string title = "Untitled";
		int width = 800;
		int height = 600;
		LhatValue settings = lhat_nil(); // the window table itself, for readWindowSettings
		bool wanted = true;              // window = nil^ means no window
	} window;
	struct
	{
		bool audio = true, data = true, event = true, filesystem = true, font = true, graphics = true;
		bool image = true, joystick = true, keyboard = true, math = true, mouse = true, sensor = true;
		bool physics = true, sound = true, system = true, thread = true, timer = true, touch = true, video = true, window = true;
	} modules;
};

static void readConf(LhatMachine *machine, LhatValue table, Conf &conf)
{
	if (!lhat_is_object_kind(table, LHAT_OBJECT_TABLE))
		return;
	conf.identity = fieldString(machine, table, "identity", conf.identity);
	conf.appendidentity = fieldBool(machine, table, "appendidentity", conf.appendidentity);
	conf.version = fieldString(machine, table, "version", conf.version);
	conf.console = fieldBool(machine, table, "console", conf.console);

	LhatValue window = field(machine, table, "window");
	if (lhat_is_object_kind(window, LHAT_OBJECT_TABLE))
	{
		conf.window.title = fieldString(machine, window, "title", conf.window.title);
		conf.window.width = (int) fieldNumber(machine, window, "width", conf.window.width);
		conf.window.height = (int) fieldNumber(machine, window, "height", conf.window.height);
		conf.window.settings = window;
	}
	else if (fieldIs(machine, table, "window", LHAT_VALUE_NIL) && lhat_is_object_kind(field(machine, table, "modules"), LHAT_OBJECT_TABLE))
	{
		// window = nil^ is how boot.lua reads "no window".
	}

	LhatValue modules = field(machine, table, "modules");
	if (lhat_is_object_kind(modules, LHAT_OBJECT_TABLE))
	{
		conf.modules.audio = fieldBool(machine, modules, "audio", true);
		conf.modules.data = fieldBool(machine, modules, "data", true);
		conf.modules.event = fieldBool(machine, modules, "event", true);
		conf.modules.filesystem = fieldBool(machine, modules, "filesystem", true);
		conf.modules.font = fieldBool(machine, modules, "font", true);
		conf.modules.graphics = fieldBool(machine, modules, "graphics", true);
		conf.modules.image = fieldBool(machine, modules, "image", true);
		conf.modules.joystick = fieldBool(machine, modules, "joystick", true);
		conf.modules.keyboard = fieldBool(machine, modules, "keyboard", true);
		conf.modules.math = fieldBool(machine, modules, "math", true);
		conf.modules.mouse = fieldBool(machine, modules, "mouse", true);
		conf.modules.physics = fieldBool(machine, modules, "physics", true);
		conf.modules.sensor = fieldBool(machine, modules, "sensor", true);
		conf.modules.sound = fieldBool(machine, modules, "sound", true);
		conf.modules.system = fieldBool(machine, modules, "system", true);
		conf.modules.thread = fieldBool(machine, modules, "thread", true);
		conf.modules.timer = fieldBool(machine, modules, "timer", true);
		conf.modules.touch = fieldBool(machine, modules, "touch", true);
		conf.modules.video = fieldBool(machine, modules, "video", true);
		conf.modules.window = fieldBool(machine, modules, "window", true);
	}
}

// Why reading one did not answer a table, in the shape report() wants.
// REJECTED is where a text that tried to call a p^ arrives, and the program
// kept what the checker said; FAULTED ran and stopped, so the frames are
// still standing (04 の 11.6改).
static std::string describeLton(LhatMachine *machine, LhatProgram *program, LhatLtonStatus status)
{
	switch (status)
	{
	case LHAT_LTON_CANNOT_READ:
		return "could not be read";
	case LHAT_LTON_REJECTED:
	{
		const char *said = lhat_program_load_failure(program);
		return said != nullptr ? said : "refused";
	}
	case LHAT_LTON_FAULTED:
	{
		std::string text = "stopped while being read";
		size_t needed = lhat_machine_traceback(machine, nullptr, 0);
		if (needed > 0)
		{
			std::vector<char> spelt(needed + 1);
			lhat_machine_traceback(machine, spelt.data(), spelt.size());
			text += "\n";
			text += spelt.data();
		}
		return text;
	}
	case LHAT_LTON_OUT_OF_MEMORY:
		return "out of memory";
	default:
		return "refused";
	}
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

// What one run leaves for the next (Boot.h's contract). Process state on
// purpose: it has to outlive the runtime it came from.
struct RestartState
{
	Variant payload;       // what love.event.restart was given
	std::string gamepath;  // the no-game screen's drop, for the next boot
	bool asked = false;
};

static RestartState restartState;

void setRestartPayload(const Variant &payload)
{
	restartState.payload = payload;
	restartState.asked = true;
}

void setRestartGamePath(const std::string &path)
{
	restartState.gamepath = path;
	restartState.asked = true;
}

// A run's answer as the process exit code: a number is one, "restart" asks
// for another boot, anything else is 0.
static int exitCodeOf(LhatValue value)
{
	const char *text = lh::stringOf(value);
	if (text != nullptr && strcmp(text, "restart") == 0)
		return LOVE_LH_RESTART;
	return lhat_is_integer(value) ? (int) lhat_as_integer(value) : 0;
}

const Variant &restartPayload()
{
	return restartState.payload;
}

// love.boot.restartInto(path): the no-game screen restarting with a
// dropped game, as nogame.lua returned _noGameRestartInfo.
static LhatValue lh_boot_restartInto(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	std::string path = lh::optString(arguments, count, 0, "");
	auto event = Module::getInstance<love::event::Event>(Module::M_EVENT);
	if (event == nullptr)
		return lh::raise(machine, "love.event is not loaded");
	std::vector<Variant> args = {Variant("restart", 7), Variant(), Variant(path)};
	StrongRef<love::event::Message> message(new love::event::Message("quit", args), Acquire::NORETAIN);
	event->push(message);
	return lhat_nil();
}

// The last path component, as love.path.leaf.
static std::string leaf(const std::string &path)
{
	std::string p = path;
	while (!p.empty() && (p.back() == '/' || p.back() == '\\'))
		p.pop_back();
	size_t slash = p.find_last_of("/\\");
	return slash == std::string::npos ? p : p.substr(slash + 1);
}

// boot.lua's identity from a source name: leading dots stripped, the
// extension dropped, remaining dots made underscores.
static std::string identityOf(const std::string &name)
{
	std::string id = name;
	while (!id.empty() && id.front() == '.')
		id.erase(id.begin());
	size_t dot = id.find_last_of('.');
	if (dot != std::string::npos)
		id = id.substr(0, dot);
	for (char &c : id)
		if (c == '.')
			c = '_';
	return id.empty() ? "lovegame" : id;
}

// What the command line asks for. arg.lua's parser, reduced to what the
// engine reads itself: options first, then the game.
struct Arguments
{
	std::string game;     // directory, .love, or .lh file
	bool fused = false;   // --fused
	bool console = false; // --console
	bool dumpHostApi = false; // --dump-host-api [file]
	std::string dumpPath = "lhat-host.json";
};

static Arguments parseArguments(int argc, char **argv)
{
	Arguments args;
	for (int i = 1; i < argc; i++)
	{
		std::string a = argv[i];
		if (a == "--fused")
			args.fused = true;
		else if (a == "--console")
			args.console = true;
		else if (a == "--dump-host-api")
		{
			args.dumpHostApi = true;
			if (i + 1 < argc && argv[i + 1][0] != '-')
				args.dumpPath = argv[++i];
		}
		else if (a == "--")
			break;
		else if (args.game.empty() && (a.empty() || a[0] != '-'))
			args.game = a;
	}
	return args;
}

// Reports a problem the way the game's author should see it: the blue
// screen when a window can be had, text otherwise.
static void reportRuntime(const std::string &text)
{
	fputs(text.c_str(), stderr);
	if (!text.empty() && text.back() != '\n')
		fputc('\n', stderr);
	fflush(stderr);
	if (!showErrorScreen(text) && !consoleBuild)
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "lhatove: error", text.c_str(), nullptr);
}

static int boot(int argc, char **argv, bool console)
{
	consoleBuild = console;
	tracing = getenv("LHATOVE_TRACE") != nullptr;
	startWatchdog();
	Arguments args = parseArguments(argc, argv);

	// A restart with a game dropped on the no-game screen runs that game;
	// the payload stays for love.restartValue().
	if (!restartState.gamepath.empty())
	{
		args.game = restartState.gamepath;
		restartState.gamepath.clear();
	}
	restartState.asked = false;

#ifdef LOVE_LEGENDARY_CONSOLE_IO_HACK
	if (args.console)
	{
		const char *err = nullptr;
		love_openConsole(err);
	}
#endif

	// Declared before the runtime so it is destroyed after it.
	Modules modules;
	Loader loader;
	loader.hold("Boot.lh", boot_lh);

	// love.filesystem first of all: the loader reads through it. boot.lua's
	// love.boot, without arg.lua's URI handling.
	auto fs = modules.add(new love::filesystem::physfs::Filesystem());
	loader.setFilesystem(fs);
	try
	{
		fs->init(argv[0]);
	}
	catch (const love::Exception &e)
	{
		report("lhatove", std::string("Could not initialize the filesystem: ") + e.what());
		return 1;
	}

	std::string exepath = fs->getExecutablePath();
	if (exepath.empty())
		exepath = argv[0];

	// A fused game: the executable is the archive.
	bool canHasGame = false;
	try
	{
		canHasGame = fs->setSource(exepath.c_str());
	}
	catch (const love::Exception &)
	{
		canHasGame = false;
	}
	bool fused = canHasGame || args.fused;
	fs->setFused(fused);

	std::string mainUnit = "main.lh";
	std::string identity;
	std::string invalidGamePath;

	if (!canHasGame && !args.game.empty())
	{
		std::string source = args.game;
		if (endsWith(source, ".lh"))
		{
			mainUnit = leaf(source);
			size_t slash = source.find_last_of("/\\");
			source = slash == std::string::npos ? "." : source.substr(0, slash);
		}
		try
		{
			canHasGame = fs->setSource(source.c_str());
		}
		catch (const love::Exception &)
		{
			canHasGame = false;
		}
		if (!canHasGame)
			invalidGamePath = source;
		identity = leaf(source);
	}
	else
	{
		identity = leaf(exepath);
	}

	try
	{
		std::string realdir = fs->getRealDirectory(mainUnit.c_str());
		if (!realdir.empty())
			identity = leaf(realdir);
	}
	catch (const love::Exception &)
	{
		// Not on disk (or nothing mounted): the fallbacks above stand.
	}
	identity = identityOf(identity);
	fs->setIdentity(identity.c_str(), true);

	bool noGameCode = canHasGame && !loader.exists(mainUnit) && !loader.exists("conf.lton");

	if (!canHasGame || noGameCode)
	{
		if (!invalidGamePath.empty())
			fprintf(stderr, "Cannot load game at path '%s'.\nMake sure a folder exists at the specified path.\n", invalidGamePath.c_str());
		loader.hold(mainUnit, nogame_lh);
	}

	Runtime runtime(Loader::load, &loader);
	if (runtime.program() == nullptr)
	{
		report("lhatove", "Could not create the L^ program.");
		return 1;
	}

	trace("registering");
	if (!runtime.registerAll(registrars, sizeof(registrars) / sizeof(registrars[0])))
	{
		report("lhatove", "A love module refused to register its API: " + runtime.failedRegistrar());
		return 1;
	}
	trace("registering stdlib");
	if (!registerStdlib(runtime.program()))
	{
		report("lhatove", "The L^ standard library refused to register.");
		return 1;
	}

	// --dump-host-api: what the checker was told, as JSON for the language
	// server (lsp/workspace.c reads lhat-host.json at the workspace root).
	if (args.dumpHostApi)
	{
		size_t needed = lhat_program_dump_host_api(runtime.program(), nullptr, 0);
		std::vector<char> json(needed + 1);
		lhat_program_dump_host_api(runtime.program(), json.data(), json.size());
		FILE *out = fopen(args.dumpPath.c_str(), "wb");
		if (out == nullptr)
		{
			report("lhatove", "Could not write " + args.dumpPath);
			return 1;
		}
		fwrite(json.data(), 1, needed, out);
		fclose(out);
		printf("wrote %s (%zu bytes)\n", args.dumpPath.c_str(), needed);
		return 0;
	}

	// 05 の 8.7: everything is registered; now the units may be checked.
	// conf.lton is not among them: it is data, and lhatstdlib_lton_load reads
	// it below once there is a machine to build the table on.
	trace("checking Boot.lh");
	const LhatUnit *bootUnit = runtime.check("Boot.lh");
	trace("checking main");
	const LhatUnit *root = runtime.check(mainUnit.c_str());
	if (bootUnit == nullptr || root == nullptr || !runtime.ok())
	{
		std::string said = runtime.diagnostics();
		if (said.empty())
			said = "Could not read " + mainUnit;
		report("lhatove: check failed", said);
		return 1;
	}

	{
		std::string problems = checkCallbacks(root);
		if (!problems.empty())
		{
			report("lhatove: check failed", problems);
			return 1;
		}
	}

	trace("compiling");
	if (!runtime.compile())
	{
		report("lhatove", "Could not compile the program.");
		return 1;
	}

	LhatMachine *machine = runtime.machine();

	trace("reading conf.lton");
	// conf.lton: data read through the program's loader, so the same file is
	// found in a directory, a .love and a fused executable alike. What comes
	// back is the machine's and is not a root, so it is parked before
	// anything else runs (vm.h, and stdlib/lton.h says it again).
	Conf conf;
	if (loader.exists("conf.lton"))
	{
		LhatValue confTable = lhat_nil();
		LhatLtonStatus read = lhatstdlib_lton_load(machine, runtime.program(), "conf.lton", &confTable);
		if (read != LHAT_LTON_OK)
		{
			report("lhatove: error", "conf.lton: " + describeLton(machine, runtime.program(), read));
			return 1;
		}
		if (!park(machine, "love.boot", "conf", confTable))
			return 1;
		readConf(machine, confTable, conf);
		if (!conf.identity.empty())
			fs->setIdentity(conf.identity.c_str(), conf.appendidentity);
	}

#ifdef LOVE_LEGENDARY_CONSOLE_IO_HACK
	if (conf.console)
	{
		const char *err = nullptr;
		love_openConsole(err);
	}
#endif

	trace("creating modules");
	// The modules conf admits, in boot.lua's order. love.filesystem is
	// already up; the loader needed it.
	love::window::Window *window = nullptr;
	try
	{
		if (conf.modules.thread)
			modules.add(new love::thread::ThreadModule());
		if (conf.modules.timer)
			modules.add(new love::timer::Timer());
		if (conf.modules.event)
			modules.add(new love::event::sdl::Event());
		if (conf.modules.keyboard)
			modules.add(new love::keyboard::sdl::Keyboard());
		if (conf.modules.joystick)
			modules.add(new love::joystick::sdl::JoystickModule());
		if (conf.modules.mouse)
			modules.add(new love::mouse::sdl::Mouse());
		if (conf.modules.touch)
			modules.add(new love::touch::sdl::Touch());
		if (conf.modules.sound)
			modules.add(new love::sound::lullaby::Sound());
		if (conf.modules.system)
			modules.add(new love::system::sdl::System());
		if (conf.modules.sensor)
			modules.add(new love::sensor::sdl::Sensor());
		if (conf.modules.audio)
		{
			// OpenAL, else the silent module, as wrap_Audio.cpp falls back.
			try
			{
				modules.add(new love::audio::openal::Audio());
			}
			catch (const love::Exception &e)
			{
				fprintf(stderr, "%s\n", e.what());
				modules.add(new love::audio::null::Audio());
			}
		}
		if (conf.modules.image)
			modules.add(new love::image::Image());
		if (conf.modules.data)
			modules.add(new love::data::DataModule());
		if (conf.modules.video)
			modules.add(new love::video::theora::Video());
		if (conf.modules.font)
			modules.add(new love::font::freetype::Font());
		if (conf.modules.window)
			window = modules.add(new love::window::sdl::Window());
		if (conf.modules.graphics)
			modules.add(love::graphics::Graphics::createInstance());
		if (conf.modules.math)
			modules.add(new love::math::Math());
		if (conf.modules.physics)
			modules.add(new love::physics::box2d::Physics());

		if (window != nullptr && conf.window.wanted)
		{
			window->setWindowTitle(conf.window.title);
			love::window::WindowSettings settings;
			std::string error;
			if (!love::window::readWindowSettings(machine, conf.window.settings, settings, error))
			{
				report("lhatove: conf.lh", error);
				return 1;
			}
			if (!window->setWindow(conf.window.width, conf.window.height, &settings))
			{
				report("lhatove", "Could not set window mode.");
				return 1;
			}
		}
	}
	catch (const love::Exception &e)
	{
		report("lhatove", std::string("Could not start a module: ") + e.what());
		return 1;
	}

	// The first couple event pumps on some systems can take a while; better
	// here than in the first frames.
	auto event = Module::getInstance<love::event::Event>(Module::M_EVENT);
	auto timer = Module::getInstance<love::timer::Timer>(Module::M_TIMER);
	if (event != nullptr)
		for (int i = 0; i < 2; i++)
			event->pump();
	if (timer != nullptr)
		timer->step();

	trace("running main");
	// A script answers its return^ and is done;
	// a module^ unit answers its public table, which is a game.
	LhatRunResult ran = lhat_run(machine, lhat_unit_proto(root));
	if (ran.status != LHAT_RUN_OK)
	{
		reportRuntime(runtime.describe(ran));
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

	if (!buildHandlers(machine, gameTable))
	{
		report("lhatove", "Could not build the callback table.");
		return 1;
	}

	trace("starting run");
	// run: the game's own if it exported one, else Boot.lh's.
	LhatValue run = field(machine, gameTable, "run");
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
		reportRuntime(runtime.describe(started));
		return 1;
	}
	if (!lhat_is_object_kind(started.value, LHAT_OBJECT_COROUTINE))
		return exitCodeOf(started.value);

	LhatValue coroutine = started.value;
	if (!park(machine, "love.boot", "coroutine", coroutine))
		return 1;

	// One resume per frame until the run returns.
	// LHATOVE_GC_STATS=<n>: the collector's counts every n frames (1 = every frame).
	const char *gcstats = getenv("LHATOVE_GC_STATS");
	unsigned every = gcstats != nullptr && atoi(gcstats) > 0 ? (unsigned) atoi(gcstats) : 120;
	unsigned frame = 0;
	while (true)
	{
		kickWatchdog();
		// Slots given back by wrappers disposed in the collector are written
		// out here, between frames, where the host may touch the heap.
		runtime.lot()->sweep();
		LhatRunResult step = lhat_machine_resume(machine, coroutine, nullptr, 0);
		if (step.status != LHAT_RUN_OK)
		{
			reportRuntime(runtime.describe(step));
			return 1;
		}
		if (gcstats != nullptr && (++frame % every) == 0)
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

void love_lh_shutdown(void)
{
	// Only the process-wide registry is left to give back: a Runtime is a
	// stack object inside boot(), so every program is gone by now.
	lhat_registry_dispose();
}
