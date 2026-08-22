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

#ifndef LOVE_LH_BOOT_LH_H
#define LOVE_LH_BOOT_LH_H

// L^ units the engine embeds and serves through the loader's hold table.
// Raw string literals, the way boot.lua used to be included.

namespace love
{
namespace lh
{

// Boot.lh: the default `run`, the successor of callbacks.lua's love.run. A
// script whose return^ is the run procedure; yieldable, so calling it from
// C answers a coroutine the engine resumes once per frame. The handlers
// table holds one member per callback -- the game's own or a no-op --
// built by the engine (Boot.cpp), so every call here is statically typed.
static const char boot_lh[] = R"lh(import^ love.boot
import^ love.event
import^ love.timer
import^ love.graphics

return^ p^ {
    let^ h = love.boot.handlers()
    h.load()

    # We don't want the first frame's dt to include time taken by load.
    love.timer.step()

    repeat^ {
        love.event.pump()
        let^ code = love.event.dispatch(h)
        if^ code isa^ number^ { return^ code }

        let^ dt = love.timer.step()
        h.update(dt)

        if^ love.graphics.isActive() {
            love.graphics.origin()
            love.graphics.clear()
            h.draw()
            love.graphics.present()
        }

        love.timer.sleep(0.001)
        yield^
    }
}
)lh";

// What runs when no game is given: a window that says so. nogame.lua's
// animated version is milestone M6.
static const char nogame_lh[] = R"lh(module^ nogame

import^ love.graphics
import^ love.event
import^ love.window

let^ usage = "lhatove -- LOVE with L^

No game was given.

usage:
    love path/to/gamedir            runs the game from the given directory which contains a main.lh file
    love path/to/packagedgame.love  runs the packaged game from the provided .love file
    love path/to/file.lh            runs the game from the given .lh file

Press Escape to quit."

public^let^ load = p^ {
    love.window.setTitle("lhatove")
    love.graphics.setBackgroundColor(0.26, 0.53, 0.96)
}

public^let^ draw = p^ {
    love.graphics.setColor(1, 1, 1)
    love.graphics.printf(usage, 40, 40, love.graphics.getWidth() - 80)
}

public^let^ keypressed = p^key:string^, scancode:string^, isrepeat:bool^ {
    if^ key = "escape" { love.event.quit() }
}
)lh";

// Milestone M0: what runs when no game is given. Exercises the boundary
// that every later module relies on -- a global, a module function, a
// tuple answer, and the two open questions from the porting plan:
//   U1: a host function answering an error value its signature declares;
//   U4: a host function answering a structural table type with p^ members.
static const char hello_main_lh[] = R"lh(import^ love

print("lhatove: hello from L^")

let^ major, minor, revision, codename = love.getVersion()
print($"love.getVersion() -> {major}.{minor}.{revision} {codename}")

let^ answer = love.probe.failing() catch^ -1
print($"U1 probe: failing() caught as {answer}")

return^ 0
)lh";

// `lovec --probe`: the U4 shape on its own -- a structural return type whose
// members are procedures, the shape love.boot.handlers() takes in M1.
static const char probe_main_lh[] = R"lh(import^ love

let^ h = love.probe.handlers()
h.update(0.5)
h.draw()
print("U4 probe: handlers table called back into the host")

return^ 0
)lh";

} // lh
} // love

#endif // LOVE_LH_BOOT_LH_H
