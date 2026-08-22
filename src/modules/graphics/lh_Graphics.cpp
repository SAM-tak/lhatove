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

// love.graphics for L^ -- milestone M1: the immediate-mode subset a first
// game needs (clear, shapes, text, color and transform state). The reference
// is wrap_Graphics.cpp beside this file; objects (Image, Canvas, Font, ...)
// arrive with later milestones.
//
// Optional trailing arguments are spelled as overloads (one registration
// per arity, 02 の 14.12) where the set is small, and as a variadic tail
// where it is the nine-number transform every draw call takes.

#include "Graphics.h"
#include "lh/lh.h"

#include "common/Matrix.h"
#include "common/Vector.h"

#include <vector>

namespace love
{
namespace graphics
{

#define instance() (Module::getInstance<Graphics>(Module::M_GRAPHICS))

// The x, y, r, sx, sy, ox, oy, kx, ky tail of a draw call, from `first` on.
static Matrix4 transformOf(const LhatValue *args, size_t count, size_t first)
{
	float x = (float) lh::optNumber(args, count, first + 0, 0.0);
	float y = (float) lh::optNumber(args, count, first + 1, 0.0);
	float a = (float) lh::optNumber(args, count, first + 2, 0.0);
	float sx = (float) lh::optNumber(args, count, first + 3, 1.0);
	float sy = (float) lh::optNumber(args, count, first + 4, sx);
	float ox = (float) lh::optNumber(args, count, first + 5, 0.0);
	float oy = (float) lh::optNumber(args, count, first + 6, 0.0);
	float kx = (float) lh::optNumber(args, count, first + 7, 0.0);
	float ky = (float) lh::optNumber(args, count, first + 8, 0.0);
	return Matrix4(x, y, a, sx, sy, ox, oy, kx, ky);
}

static bool drawModeOf(LhatMachine *machine, LhatValue value, Graphics::DrawMode &mode)
{
	const char *name = lh::stringOf(value);
	if (name == nullptr || !Graphics::getConstant(name, mode))
	{
		lh::raise(machine, std::string("Invalid draw mode: ") + (name != nullptr ? name : "(not a string)"));
		return false;
	}
	return true;
}

static Colorf colorOf(const LhatValue *args, size_t count, size_t first)
{
	return Colorf(
		(float) lh::optNumber(args, count, first + 0, 0.0),
		(float) lh::optNumber(args, count, first + 1, 0.0),
		(float) lh::optNumber(args, count, first + 2, 0.0),
		(float) lh::optNumber(args, count, first + 3, 1.0));
}

static LhatValue colorTuple(LhatMachine *machine, Colorf c)
{
	LhatValue parts[4] = {lhat_real(c.r), lhat_real(c.g), lhat_real(c.b), lhat_real(c.a)};
	LhatValue out = lhat_nil();
	lh::makeTuple(machine, parts, 4, &out);
	return out;
}

// ---------------------------------------------------------------------------
// Frame
// ---------------------------------------------------------------------------

// clear() clears to the background color; clear(r, g, b[, a]) to that.
static LhatValue lh_clear(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	OptionalColorD color;
	if (count >= 3)
	{
		Colorf c = colorOf(arguments, count, 0);
		color.set(ColorD(c.r, c.g, c.b, c.a));
	}
	else
	{
		Colorf c = instance()->getBackgroundColor();
		color.set(ColorD(c.r, c.g, c.b, c.a));
	}
	return lh::guard(machine, [&]() {
		instance()->clear(color, OptionalInt(0), OptionalDouble(1.0));
		return lhat_nil();
	});
}

static LhatValue lh_present(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	(void) arguments;
	(void) count;
	return lh::guard(machine, [&]() {
		instance()->present(nullptr);
		return lhat_nil();
	});
}

static LhatValue lh_isActive(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	return lhat_bool(instance()->isActive());
}

static LhatValue lh_getWidth(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	return lhat_integer(instance()->getWidth());
}

static LhatValue lh_getHeight(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	return lhat_integer(instance()->getHeight());
}

static LhatValue lh_getDimensions(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	(void) arguments;
	(void) count;
	LhatValue parts[2] = {lhat_integer(instance()->getWidth()), lhat_integer(instance()->getHeight())};
	LhatValue out = lhat_nil();
	lh::makeTuple(machine, parts, 2, &out);
	return out;
}

// ---------------------------------------------------------------------------
// Color
// ---------------------------------------------------------------------------

static LhatValue lh_setColor(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	instance()->setColor(colorOf(arguments, count, 0));
	return lhat_nil();
}

static LhatValue lh_getColor(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	(void) arguments;
	(void) count;
	return colorTuple(machine, instance()->getColor());
}

static LhatValue lh_setBackgroundColor(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	instance()->setBackgroundColor(colorOf(arguments, count, 0));
	return lhat_nil();
}

static LhatValue lh_getBackgroundColor(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	(void) arguments;
	(void) count;
	return colorTuple(machine, instance()->getBackgroundColor());
}

static LhatValue lh_setLineWidth(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	instance()->setLineWidth((float) lh::optNumber(arguments, count, 0, 1.0));
	return lhat_nil();
}

static LhatValue lh_getLineWidth(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	return lhat_real(instance()->getLineWidth());
}

// ---------------------------------------------------------------------------
// Shapes
// ---------------------------------------------------------------------------

// rectangle(mode, x, y, w, h[, rx[, ry[, segments]]])
static LhatValue lh_rectangle(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	Graphics::DrawMode mode;
	if (count < 1 || !drawModeOf(machine, arguments[0], mode))
		return lhat_nil();
	float x = (float) lh::optNumber(arguments, count, 1, 0.0);
	float y = (float) lh::optNumber(arguments, count, 2, 0.0);
	float w = (float) lh::optNumber(arguments, count, 3, 0.0);
	float h = (float) lh::optNumber(arguments, count, 4, 0.0);
	return lh::guard(machine, [&]() {
		if (count <= 5)
			instance()->rectangle(mode, x, y, w, h);
		else
		{
			float rx = (float) lh::optNumber(arguments, count, 5, 0.0);
			float ry = (float) lh::optNumber(arguments, count, 6, rx);
			if (count <= 7)
				instance()->rectangle(mode, x, y, w, h, rx, ry);
			else
				instance()->rectangle(mode, x, y, w, h, rx, ry, (int) lh::optNumber(arguments, count, 7, 10));
		}
		return lhat_nil();
	});
}

// circle(mode, x, y, radius[, segments])
static LhatValue lh_circle(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	Graphics::DrawMode mode;
	if (count < 1 || !drawModeOf(machine, arguments[0], mode))
		return lhat_nil();
	float x = (float) lh::optNumber(arguments, count, 1, 0.0);
	float y = (float) lh::optNumber(arguments, count, 2, 0.0);
	float radius = (float) lh::optNumber(arguments, count, 3, 0.0);
	return lh::guard(machine, [&]() {
		if (count <= 4)
			instance()->circle(mode, x, y, radius);
		else
			instance()->circle(mode, x, y, radius, (int) lh::optNumber(arguments, count, 4, 10));
		return lhat_nil();
	});
}

// Reads x1, y1, x2, y2, ... from `first` on. An odd count drops the last.
static std::vector<Vector2> verticesOf(const LhatValue *args, size_t count, size_t first)
{
	std::vector<Vector2> vertices;
	for (size_t i = first; i + 1 < count; i += 2)
		vertices.emplace_back((float) lh::optNumber(args, count, i, 0.0), (float) lh::optNumber(args, count, i + 1, 0.0));
	return vertices;
}

// line(x1, y1, x2, y2, ...)
static LhatValue lh_line(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	std::vector<Vector2> vertices = verticesOf(arguments, count, 0);
	if (vertices.size() < 2)
		return lh::raise(machine, "Need at least two vertices to draw a line");
	return lh::guard(machine, [&]() {
		instance()->polyline(vertices.data(), vertices.size());
		return lhat_nil();
	});
}

// polygon(mode, x1, y1, x2, y2, ...)
static LhatValue lh_polygon(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	Graphics::DrawMode mode;
	if (count < 1 || !drawModeOf(machine, arguments[0], mode))
		return lhat_nil();
	std::vector<Vector2> vertices = verticesOf(arguments, count, 1);
	if (vertices.size() < 3)
		return lh::raise(machine, "Need at least three vertices to draw a polygon");
	// The closing vertex, as wrap_Graphics.cpp adds it.
	vertices.push_back(vertices[0]);
	return lh::guard(machine, [&]() {
		instance()->polygon(mode, vertices.data(), vertices.size());
		return lhat_nil();
	});
}

// points(x1, y1, x2, y2, ...)
static LhatValue lh_points(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	std::vector<Vector2> vertices = verticesOf(arguments, count, 0);
	if (vertices.empty())
		return lhat_nil();
	return lh::guard(machine, [&]() {
		instance()->points(vertices.data(), nullptr, vertices.size());
		return lhat_nil();
	});
}

static LhatValue lh_setPointSize(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	instance()->setPointSize((float) lh::optNumber(arguments, count, 0, 1.0));
	return lhat_nil();
}

// ---------------------------------------------------------------------------
// Text
// ---------------------------------------------------------------------------

// print(text, x, y, r, sx, sy, ox, oy, kx, ky) -- all but text optional.
static LhatValue lh_print(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	std::vector<love::font::ColoredString> text;
	text.push_back({lh::optString(arguments, count, 0, ""), instance()->getColor()});
	Matrix4 m = transformOf(arguments, count, 1);
	return lh::guard(machine, [&]() {
		instance()->print(text, m);
		return lhat_nil();
	});
}

// printf(text, x, y, limit[, align, r, sx, sy, ox, oy, kx, ky])
static LhatValue lh_printf(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	std::vector<love::font::ColoredString> text;
	text.push_back({lh::optString(arguments, count, 0, ""), instance()->getColor()});
	float x = (float) lh::optNumber(arguments, count, 1, 0.0);
	float y = (float) lh::optNumber(arguments, count, 2, 0.0);
	float wrap = (float) lh::optNumber(arguments, count, 3, 0.0);

	Font::AlignMode align = Font::ALIGN_LEFT;
	size_t rest = 4;
	if (count > 4 && lh::stringOf(arguments[4]) != nullptr)
	{
		const char *name = lh::stringOf(arguments[4]);
		if (!Font::getConstant(name, align))
			return lh::raise(machine, std::string("Invalid align mode: ") + name);
		rest = 5;
	}
	float a = (float) lh::optNumber(arguments, count, rest + 0, 0.0);
	float sx = (float) lh::optNumber(arguments, count, rest + 1, 1.0);
	float sy = (float) lh::optNumber(arguments, count, rest + 2, sx);
	float ox = (float) lh::optNumber(arguments, count, rest + 3, 0.0);
	float oy = (float) lh::optNumber(arguments, count, rest + 4, 0.0);
	float kx = (float) lh::optNumber(arguments, count, rest + 5, 0.0);
	float ky = (float) lh::optNumber(arguments, count, rest + 6, 0.0);
	Matrix4 m(x, y, a, sx, sy, ox, oy, kx, ky);
	return lh::guard(machine, [&]() {
		instance()->printf(text, wrap, align, m);
		return lhat_nil();
	});
}

// ---------------------------------------------------------------------------
// Transform
// ---------------------------------------------------------------------------

static LhatValue lh_push(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	(void) arguments;
	(void) count;
	return lh::guard(machine, [&]() {
		instance()->push();
		return lhat_nil();
	});
}

static LhatValue lh_pop(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	(void) arguments;
	(void) count;
	return lh::guard(machine, [&]() {
		instance()->pop();
		return lhat_nil();
	});
}

static LhatValue lh_translate(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	instance()->translate((float) lh::optNumber(arguments, count, 0, 0.0), (float) lh::optNumber(arguments, count, 1, 0.0));
	return lhat_nil();
}

static LhatValue lh_rotate(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	instance()->rotate((float) lh::optNumber(arguments, count, 0, 0.0));
	return lhat_nil();
}

static LhatValue lh_scale(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	float sx = (float) lh::optNumber(arguments, count, 0, 1.0);
	instance()->scale(sx, (float) lh::optNumber(arguments, count, 1, sx));
	return lhat_nil();
}

static LhatValue lh_origin(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	instance()->origin();
	return lhat_nil();
}

} // graphics

namespace lh
{

bool lhopen_love_graphics(Context &ctx)
{
	if (ctx.types())
		return true;

	using namespace love::graphics;
	const char *m = "love.graphics";
	return ctx.func(m, "clear", "p^;", lh_clear, nullptr)
		&& ctx.func(m, "clear", "p^number^, number^, number^;", lh_clear, nullptr)
		&& ctx.func(m, "clear", "p^number^, number^, number^, number^;", lh_clear, nullptr)
		&& ctx.func(m, "present", "p^;", lh_present, nullptr)
		&& ctx.func(m, "isActive", "f^ -> bool^;", lh_isActive, nullptr)
		&& ctx.func(m, "getWidth", "f^ -> number^;", lh_getWidth, nullptr)
		&& ctx.func(m, "getHeight", "f^ -> number^;", lh_getHeight, nullptr)
		&& ctx.func(m, "getDimensions", "f^ -> (number^, number^);", lh_getDimensions, nullptr)
		&& ctx.func(m, "setColor", "p^number^, number^, number^;", lh_setColor, nullptr)
		&& ctx.func(m, "setColor", "p^number^, number^, number^, number^;", lh_setColor, nullptr)
		&& ctx.func(m, "getColor", "f^ -> (number^, number^, number^, number^);", lh_getColor, nullptr)
		&& ctx.func(m, "setBackgroundColor", "p^number^, number^, number^;", lh_setBackgroundColor, nullptr)
		&& ctx.func(m, "setBackgroundColor", "p^number^, number^, number^, number^;", lh_setBackgroundColor, nullptr)
		&& ctx.func(m, "getBackgroundColor", "f^ -> (number^, number^, number^, number^);", lh_getBackgroundColor, nullptr)
		&& ctx.func(m, "setLineWidth", "p^number^;", lh_setLineWidth, nullptr)
		&& ctx.func(m, "getLineWidth", "f^ -> number^;", lh_getLineWidth, nullptr)
		&& ctx.func(m, "rectangle", "p^string^, number^, number^, number^, number^, ...;", lh_rectangle, nullptr)
		&& ctx.func(m, "circle", "p^string^, number^, number^, number^, ...;", lh_circle, nullptr)
		&& ctx.func(m, "line", "p^number^, number^, number^, number^, ...;", lh_line, nullptr)
		&& ctx.func(m, "polygon", "p^string^, number^, number^, number^, number^, number^, number^, ...;", lh_polygon, nullptr)
		&& ctx.func(m, "points", "p^number^, number^, ...;", lh_points, nullptr)
		&& ctx.func(m, "setPointSize", "p^number^;", lh_setPointSize, nullptr)
		&& ctx.func(m, "print", "p^string^, ...;", lh_print, nullptr)
		&& ctx.func(m, "printf", "p^string^, number^, number^, number^, ...;", lh_printf, nullptr)
		&& ctx.func(m, "push", "p^;", lh_push, nullptr)
		&& ctx.func(m, "pop", "p^;", lh_pop, nullptr)
		&& ctx.func(m, "translate", "p^number^, number^;", lh_translate, nullptr)
		&& ctx.func(m, "rotate", "p^number^;", lh_rotate, nullptr)
		&& ctx.func(m, "scale", "p^number^;", lh_scale, nullptr)
		&& ctx.func(m, "scale", "p^number^, number^;", lh_scale, nullptr)
		&& ctx.func(m, "origin", "p^;", lh_origin, nullptr);
}

} // lh
} // love
