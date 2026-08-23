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

// love.graphics for L^, the render state half: canvases (render targets),
// the active shader, blending, scissor, stencil, color mask, default
// filters, line style, the transform stack's extras, ellipses and arcs,
// renderer info and stats, and reading a texture back. The reference is
// wrap_Graphics.cpp beside this file.

#include "lh_Graphics.h"

#include "Shader.h"
#include "modules/image/Image.h"
#include "modules/image/ImageData.h"
#include "modules/math/Transform.h"

namespace love
{
namespace graphics
{

#define instance() graphicsInstance()
#define binding graphicsBinding

static LhatValue stringValue(LhatMachine *machine, const char *text)
{
	LhatValue out = lhat_nil();
	lh::makeString(machine, text != nullptr ? text : "", &out);
	return out;
}

// ---------------------------------------------------------------------------
// Canvases and shaders
// ---------------------------------------------------------------------------

// newCanvas([w, h[, settings]]): a 2D render-target texture at the screen's
// pixel density. `settings` may carry msaa, format, mipmaps ("none" /
// "auto" / "manual"), readable and dpiscale.
static LhatValue lh_newCanvas(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	Texture::Settings s;
	s.renderTarget = true;
	s.width = (int) lh::optNumber(args, count, 0, instance()->getWidth());
	s.height = (int) lh::optNumber(args, count, 1, instance()->getHeight());
	s.dpiScale = (float) instance()->getScreenDPIScale();

	if (count >= 3 && lhat_is_object_kind(args[2], LHAT_OBJECT_TABLE))
	{
		LhatValue settings = args[2];
		s.msaa = (int) lh::fieldNumber(machine, settings, "msaa", 1);
		std::string format = lh::fieldString(machine, settings, "format", "");
		if (!format.empty() && !getConstant(format.c_str(), s.format))
			return lh::raise(machine, "Invalid pixel format: " + format);
		std::string mipmaps = lh::fieldString(machine, settings, "mipmaps", "");
		if (!mipmaps.empty() && !Texture::getConstant(mipmaps.c_str(), s.mipmaps))
			return lh::raise(machine, "Invalid mipmap mode: " + mipmaps);
		if (lh::fieldIs(machine, settings, "readable", LHAT_VALUE_BOOL))
			s.readable.set(lh::fieldBool(machine, settings, "readable", true));
		s.dpiScale = (float) lh::fieldNumber(machine, settings, "dpiscale", s.dpiScale);
	}

	return lh::guard(machine, [&]() {
		StrongRef<Texture> texture(instance()->newTexture(s), Acquire::NORETAIN);
		return lh::pushObject(machine, *binding.registry, texture.get());
	});
}

// setCanvas(texture, ...[, depth[, stencil]]) / setCanvas(): the color
// targets, then two optional booleans asking for temporary depth and
// stencil buffers. One variadic arm, since a list-of-Texture arm is not
// told apart from the Texture one at registration.
static LhatValue lh_setCanvas(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	if (count == 0)
	{
		return lh::guard(machine, [&]() {
			instance()->setRenderTarget();
			return lhat_nil();
		});
	}

	Graphics::RenderTargets targets;
	size_t at = 0;
	for (; at < count && lhat_is_object_kind(args[at], LHAT_OBJECT_HOSTDATA); at++)
	{
		Texture *texture = checkTexture(machine, args, count, at);
		if (texture == nullptr)
			return lhat_nil();
		targets.colors.emplace_back(texture, 0);
	}
	if (targets.colors.empty())
		return lh::raise(machine, "setCanvas needs at least one Texture");
	if (lh::optBool(args, count, at, false))
		targets.temporaryRTFlags |= Graphics::TEMPORARY_RT_DEPTH;
	if (lh::optBool(args, count, at + 1, false))
		targets.temporaryRTFlags |= Graphics::TEMPORARY_RT_STENCIL;
	return lh::guard(machine, [&]() {
		instance()->setRenderTargets(targets);
		return lhat_nil();
	});
}

// getCanvas() -> the first color target, nil^ when drawing to the screen.
static LhatValue lh_getCanvas(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	(void) args;
	(void) count;
	Graphics::RenderTargets targets = instance()->getRenderTargets();
	if (targets.colors.empty() || targets.colors[0].texture == nullptr)
		return lhat_nil();
	return lh::pushObject(machine, *binding.registry, targets.colors[0].texture);
}

static LhatValue lh_setShader(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	if (count == 0)
	{
		instance()->setShader();
		return lhat_nil();
	}
	Shader *shader = lh::checkObject<Shader>(args[0], *binding.registry);
	if (shader == nullptr)
		return lh::raise(machine, "Expected a Shader");
	return lh::guard(machine, [&]() {
		instance()->setShader(shader);
		return lhat_nil();
	});
}

static LhatValue lh_getShader(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	(void) args;
	(void) count;
	Shader *shader = instance()->getShader();
	return shader != nullptr ? lh::pushObject(machine, *binding.registry, shader) : lhat_nil();
}

// ---------------------------------------------------------------------------
// Blending, scissor, stencil, masks
// ---------------------------------------------------------------------------

// setBlendMode(mode[, alphamode])
static LhatValue lh_setBlendMode(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	std::string modestr = lh::optString(args, count, 0, "alpha");
	BlendMode mode;
	if (!getConstant(modestr.c_str(), mode))
		return lh::raise(machine, "Invalid blend mode: " + modestr);
	BlendAlpha alpha = BLENDALPHA_MULTIPLY;
	if (count >= 2)
	{
		std::string alphastr = lh::optString(args, count, 1, "alphamultiply");
		if (!getConstant(alphastr.c_str(), alpha))
			return lh::raise(machine, "Invalid blend alpha mode: " + alphastr);
	}
	return lh::guard(machine, [&]() {
		instance()->setBlendMode(mode, alpha);
		return lhat_nil();
	});
}

static LhatValue lh_getBlendMode(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	(void) args;
	(void) count;
	BlendAlpha alpha = BLENDALPHA_MULTIPLY;
	BlendMode mode = instance()->getBlendMode(alpha);
	const char *modestr = "alpha";
	const char *alphastr = "alphamultiply";
	getConstant(mode, modestr);
	getConstant(alpha, alphastr);
	LhatValue items[2] = {stringValue(machine, modestr), stringValue(machine, alphastr)};
	LhatValue out = lhat_nil();
	lh::makeTuple(machine, items, 2, &out);
	return out;
}

// setScissor(x, y, w, h) / setScissor() to disable.
static LhatValue lh_setScissor(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	return lh::guard(machine, [&]() {
		if (count == 0)
			instance()->setScissor();
		else
		{
			FRect rect = {(float) lh::optNumber(args, count, 0, 0), (float) lh::optNumber(args, count, 1, 0), (float) lh::optNumber(args, count, 2, 0), (float) lh::optNumber(args, count, 3, 0)};
			if (rect.w < 0 || rect.h < 0)
				return lh::raise(machine, "Can't set scissor with negative width and/or height.");
			instance()->setScissor(rect);
		}
		return lhat_nil();
	});
}

static LhatValue lh_intersectScissor(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	FRect rect = {(float) lh::optNumber(args, count, 0, 0), (float) lh::optNumber(args, count, 1, 0), (float) lh::optNumber(args, count, 2, 0), (float) lh::optNumber(args, count, 3, 0)};
	if (rect.w < 0 || rect.h < 0)
		return lh::raise(machine, "Can't set scissor with negative width and/or height.");
	return lh::guard(machine, [&]() {
		instance()->intersectScissor(rect);
		return lhat_nil();
	});
}

// getScissor() -> (x, y, w, h), all zero when none is set.
static LhatValue lh_getScissor(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	(void) args;
	(void) count;
	// The rectangle Graphics keeps is the last one set, whether or not the
	// scissor is on; a scissor that is off answers zeros here.
	FRect rect = {0, 0, 0, 0};
	if (!instance()->getScissor(rect))
		rect = {0, 0, 0, 0};
	float values[4] = {rect.x, rect.y, rect.w, rect.h};
	return numberTuple(machine, values, 4);
}

// setStencilMode(mode[, value]) / setStencilMode() to disable.
static LhatValue lh_setStencilMode(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	return lh::guard(machine, [&]() {
		if (count == 0)
		{
			instance()->setStencilMode();
			return lhat_nil();
		}
		std::string modestr = lh::optString(args, count, 0, "off");
		StencilMode mode;
		if (!getConstant(modestr.c_str(), mode))
			return lh::raise(machine, "Invalid stencil mode: " + modestr);
		instance()->setStencilMode(mode, (int) lh::optNumber(args, count, 1, 1));
		return lhat_nil();
	});
}

static LhatValue lh_getStencilMode(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	(void) args;
	(void) count;
	int value = 0;
	StencilMode mode = instance()->getStencilMode(value);
	const char *modestr = "off";
	getConstant(mode, modestr);
	LhatValue items[2] = {stringValue(machine, modestr), lhat_integer(value)};
	LhatValue out = lhat_nil();
	lh::makeTuple(machine, items, 2, &out);
	return out;
}

// setColorMask(r, g, b, a) / setColorMask() for all.
static LhatValue lh_setColorMask(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	ColorChannelMask mask;
	if (count >= 4)
	{
		mask.r = lh::optBool(args, count, 0, true);
		mask.g = lh::optBool(args, count, 1, true);
		mask.b = lh::optBool(args, count, 2, true);
		mask.a = lh::optBool(args, count, 3, true);
	}
	return lh::guard(machine, [&]() {
		instance()->setColorMask(mask);
		return lhat_nil();
	});
}

static LhatValue lh_getColorMask(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	(void) args;
	(void) count;
	ColorChannelMask mask = instance()->getColorMask();
	LhatValue items[4] = {lhat_bool(mask.r), lhat_bool(mask.g), lhat_bool(mask.b), lhat_bool(mask.a)};
	LhatValue out = lhat_nil();
	lh::makeTuple(machine, items, 4, &out);
	return out;
}

// ---------------------------------------------------------------------------
// Defaults and line style
// ---------------------------------------------------------------------------

// setDefaultFilter(min[, mag[, anisotropy]])
static LhatValue lh_setDefaultFilter(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	SamplerState s = instance()->getDefaultSamplerState();
	std::string minstr = lh::optString(args, count, 0, "linear");
	std::string magstr = lh::optString(args, count, 1, minstr);
	if (!SamplerState::getConstant(minstr.c_str(), s.minFilter))
		return lh::raise(machine, "Invalid filter mode: " + minstr);
	if (!SamplerState::getConstant(magstr.c_str(), s.magFilter))
		return lh::raise(machine, "Invalid filter mode: " + magstr);
	s.maxAnisotropy = (uint8) std::min(std::max(1.0, lh::optNumber(args, count, 2, 1.0)), (double) LOVE_UINT8_MAX);
	instance()->setDefaultSamplerState(s);
	return lhat_nil();
}

static LhatValue lh_getDefaultFilter(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	(void) args;
	(void) count;
	const SamplerState &s = instance()->getDefaultSamplerState();
	const char *minstr = "linear";
	const char *magstr = "linear";
	SamplerState::getConstant(s.minFilter, minstr);
	SamplerState::getConstant(s.magFilter, magstr);
	LhatValue items[3] = {stringValue(machine, minstr), stringValue(machine, magstr), lhat_integer(s.maxAnisotropy)};
	LhatValue out = lhat_nil();
	lh::makeTuple(machine, items, 3, &out);
	return out;
}

static LhatValue lh_setLineStyle(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	std::string name = lh::optString(args, count, 0, "smooth");
	Graphics::LineStyle style;
	if (!Graphics::getConstant(name.c_str(), style))
		return lh::raise(machine, "Invalid line style: " + name);
	instance()->setLineStyle(style);
	return lhat_nil();
}

static LhatValue lh_getLineStyle(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	(void) args;
	(void) count;
	const char *name = "smooth";
	Graphics::getConstant(instance()->getLineStyle(), name);
	return stringValue(machine, name);
}

static LhatValue lh_setLineJoin(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	std::string name = lh::optString(args, count, 0, "miter");
	Graphics::LineJoin join;
	if (!Graphics::getConstant(name.c_str(), join))
		return lh::raise(machine, "Invalid line join: " + name);
	instance()->setLineJoin(join);
	return lhat_nil();
}

static LhatValue lh_getLineJoin(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	(void) args;
	(void) count;
	const char *name = "miter";
	Graphics::getConstant(instance()->getLineJoin(), name);
	return stringValue(machine, name);
}

static LhatValue lh_setWireframe(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	(void) machine;
	instance()->setWireframe(lh::optBool(args, count, 0, false));
	return lhat_nil();
}

static LhatValue lh_isWireframe(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	(void) machine;
	(void) args;
	(void) count;
	return lhat_bool(instance()->isWireframe());
}

static LhatValue lh_getPointSize(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	(void) machine;
	(void) args;
	(void) count;
	return lhat_real(instance()->getPointSize());
}

static LhatValue lh_reset(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	(void) args;
	(void) count;
	return lh::guard(machine, [&]() {
		instance()->reset();
		return lhat_nil();
	});
}

// ---------------------------------------------------------------------------
// Transforms
// ---------------------------------------------------------------------------

static LhatValue lh_shear(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	(void) machine;
	instance()->shear((float) lh::optNumber(args, count, 0, 0), (float) lh::optNumber(args, count, 1, 0));
	return lhat_nil();
}

static love::math::Transform *checkTransform(LhatMachine *machine, const LhatValue *args, size_t count, size_t index)
{
	auto *transform = index < count ? lh::checkObject<love::math::Transform>(args[index], *binding.registry) : nullptr;
	if (transform == nullptr)
		lh::raise(machine, "Expected a Transform");
	return transform;
}

static LhatValue lh_applyTransform(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	auto *transform = checkTransform(machine, args, count, 0);
	if (transform == nullptr)
		return lhat_nil();
	return lh::guard(machine, [&]() {
		instance()->applyTransform(transform->getMatrix());
		return lhat_nil();
	});
}

static LhatValue lh_replaceTransform(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	auto *transform = checkTransform(machine, args, count, 0);
	if (transform == nullptr)
		return lhat_nil();
	return lh::guard(machine, [&]() {
		instance()->replaceTransform(transform->getMatrix());
		return lhat_nil();
	});
}

static LhatValue lh_transformPoint(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	Vector2 p((float) lh::optNumber(args, count, 0, 0), (float) lh::optNumber(args, count, 1, 0));
	p = instance()->transformPoint(p);
	float values[2] = {p.x, p.y};
	return numberTuple(machine, values, 2);
}

static LhatValue lh_inverseTransformPoint(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	Vector2 p((float) lh::optNumber(args, count, 0, 0), (float) lh::optNumber(args, count, 1, 0));
	p = instance()->inverseTransformPoint(p);
	float values[2] = {p.x, p.y};
	return numberTuple(machine, values, 2);
}

static LhatValue lh_getStackDepth(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	(void) machine;
	(void) args;
	(void) count;
	return lhat_integer((int64_t) instance()->getStackDepth());
}

// ---------------------------------------------------------------------------
// More shapes
// ---------------------------------------------------------------------------

// ellipse(mode, x, y, a, b[, segments])
static LhatValue lh_ellipse(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	Graphics::DrawMode mode;
	if (count < 1 || !drawModeOf(machine, args[0], mode))
		return lhat_nil();
	float x = (float) lh::optNumber(args, count, 1, 0), y = (float) lh::optNumber(args, count, 2, 0);
	float a = (float) lh::optNumber(args, count, 3, 0), b = (float) lh::optNumber(args, count, 4, a);
	return lh::guard(machine, [&]() {
		if (count >= 6)
			instance()->ellipse(mode, x, y, a, b, (int) lh::optNumber(args, count, 5, 10));
		else
			instance()->ellipse(mode, x, y, a, b);
		return lhat_nil();
	});
}

// arc(mode[, arctype], x, y, r, angle1, angle2[, segments])
static LhatValue lh_arc(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	Graphics::DrawMode mode;
	if (count < 1 || !drawModeOf(machine, args[0], mode))
		return lhat_nil();
	Graphics::ArcMode arcmode = Graphics::ARC_PIE;
	size_t at = 1;
	if (count >= 2 && lh::stringOf(args[1]) != nullptr)
	{
		std::string name = lh::optString(args, count, 1, "pie");
		if (!Graphics::getConstant(name.c_str(), arcmode))
			return lh::raise(machine, "Invalid arc mode: " + name);
		at = 2;
	}
	float x = (float) lh::optNumber(args, count, at, 0), y = (float) lh::optNumber(args, count, at + 1, 0);
	float r = (float) lh::optNumber(args, count, at + 2, 0);
	float a1 = (float) lh::optNumber(args, count, at + 3, 0), a2 = (float) lh::optNumber(args, count, at + 4, 0);
	return lh::guard(machine, [&]() {
		if (count > at + 5)
			instance()->arc(mode, arcmode, x, y, r, a1, a2, (int) lh::optNumber(args, count, at + 5, 10));
		else
			instance()->arc(mode, arcmode, x, y, r, a1, a2);
		return lhat_nil();
	});
}

// ---------------------------------------------------------------------------
// Information
// ---------------------------------------------------------------------------

static LhatValue lh_getRendererInfo(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	(void) args;
	(void) count;
	Graphics::RendererInfo info = instance()->getRendererInfo();
	LhatValue items[4] = {stringValue(machine, info.name.c_str()), stringValue(machine, info.version.c_str()), stringValue(machine, info.vendor.c_str()), stringValue(machine, info.device.c_str())};
	LhatValue out = lhat_nil();
	lh::makeTuple(machine, items, 4, &out);
	return out;
}

// getStats() -> a table of the frame's counts.
static LhatValue lh_getStats(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	(void) args;
	(void) count;
	Graphics::Stats stats = instance()->getStats();
	LhatValue table = lhat_nil();
	if (!lhat_machine_make_table(machine, &table))
		return lhat_nil();
	LhatTable *t = (LhatTable *) lhat_as_object(table);
	struct { const char *name; int64_t value; } fields[] = {
		{"drawcalls", stats.drawCalls},
		{"drawcallsbatched", stats.drawCallsBatched},
		{"canvasswitches", stats.renderTargetSwitches},
		{"shaderswitches", stats.shaderSwitches},
		{"textures", stats.textures},
		{"fonts", stats.fonts},
		{"buffers", stats.buffers},
		{"texturememory", stats.textureMemory},
		{"buffermemory", stats.bufferMemory},
	};
	for (const auto &f : fields)
	{
		LhatValue key = lhat_nil();
		bool refused = false;
		if (lh::makeString(machine, f.name, &key))
			lhat_table_set(t, key, lhat_integer(f.value), &refused);
	}
	return table;
}

static LhatValue lh_getDPIScale(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	(void) machine;
	(void) args;
	(void) count;
	return lhat_real(instance()->getScreenDPIScale());
}

static LhatValue lh_getPixelDimensions(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	(void) args;
	(void) count;
	float values[2] = {(float) instance()->getPixelWidth(), (float) instance()->getPixelHeight()};
	return numberTuple(machine, values, 2);
}

// readbackTexture(texture[, x, y, w, h]) -> ImageData with the pixels.
static LhatValue lh_readbackTexture(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	Texture *t = checkTexture(machine, args, count, 0);
	if (t == nullptr)
		return lhat_nil();
	Rect rect = {0, 0, t->getPixelWidth(0), t->getPixelHeight(0)};
	if (count >= 5)
	{
		rect.x = (int) lh::optNumber(args, count, 1, 0);
		rect.y = (int) lh::optNumber(args, count, 2, 0);
		rect.w = (int) lh::optNumber(args, count, 3, rect.w);
		rect.h = (int) lh::optNumber(args, count, 4, rect.h);
	}
	return lh::guard(machine, [&]() {
		StrongRef<love::image::ImageData> data(instance()->readbackTexture(t, 0, 0, rect, nullptr, 0, 0), Acquire::NORETAIN);
		return lh::pushObject(machine, *binding.registry, data.get());
	});
}

// ---------------------------------------------------------------------------
// Texture extras
// ---------------------------------------------------------------------------

#define TEXTURE_SELF() Texture *t = checkTexture(machine, args, count, 0); if (t == nullptr) return lhat_nil()

static LhatValue lh_Texture_isCanvas(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	TEXTURE_SELF();
	return lhat_bool(t->isRenderTarget());
}

static LhatValue lh_Texture_isReadable(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	TEXTURE_SELF();
	return lhat_bool(t->isReadable());
}

static LhatValue lh_Texture_getFormat(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	TEXTURE_SELF();
	const char *name = "";
	getConstant(t->getPixelFormat(), name);
	return stringValue(machine, name);
}

static LhatValue lh_Texture_getDPIScale(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	TEXTURE_SELF();
	return lhat_real(t->getDPIScale());
}

static LhatValue lh_Texture_getPixelDimensions(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	TEXTURE_SELF();
	float values[2] = {(float) t->getPixelWidth(0), (float) t->getPixelHeight(0)};
	return numberTuple(machine, values, 2);
}

static LhatValue lh_Texture_getMipmapCount(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	TEXTURE_SELF();
	return lhat_integer(t->getMipmapCount());
}

static LhatValue lh_Texture_getFilter(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	TEXTURE_SELF();
	const SamplerState &s = t->getSamplerState();
	const char *minstr = "linear";
	const char *magstr = "linear";
	SamplerState::getConstant(s.minFilter, minstr);
	SamplerState::getConstant(s.magFilter, magstr);
	LhatValue items[3] = {stringValue(machine, minstr), stringValue(machine, magstr), lhat_integer(s.maxAnisotropy)};
	LhatValue out = lhat_nil();
	lh::makeTuple(machine, items, 3, &out);
	return out;
}

// setWrap(horiz[, vert])
static LhatValue lh_Texture_setWrap(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	TEXTURE_SELF();
	SamplerState s = t->getSamplerState();
	std::string u = lh::optString(args, count, 1, "clamp");
	std::string v = lh::optString(args, count, 2, u);
	if (!SamplerState::getConstant(u.c_str(), s.wrapU))
		return lh::raise(machine, "Invalid wrap mode: " + u);
	if (!SamplerState::getConstant(v.c_str(), s.wrapV))
		return lh::raise(machine, "Invalid wrap mode: " + v);
	return lh::guard(machine, [&]() {
		t->setSamplerState(s);
		return lhat_nil();
	});
}

static LhatValue lh_Texture_getWrap(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	TEXTURE_SELF();
	const SamplerState &s = t->getSamplerState();
	const char *u = "clamp";
	const char *v = "clamp";
	SamplerState::getConstant(s.wrapU, u);
	SamplerState::getConstant(s.wrapV, v);
	LhatValue items[2] = {stringValue(machine, u), stringValue(machine, v)};
	LhatValue out = lhat_nil();
	lh::makeTuple(machine, items, 2, &out);
	return out;
}

static LhatValue lh_Texture_generateMipmaps(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	TEXTURE_SELF();
	return lh::guard(machine, [&]() {
		t->generateMipmaps();
		return lhat_nil();
	});
}

// replacePixels(imagedata[, x, y]): writes the pixels into the texture.
static LhatValue lh_Texture_replacePixels(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	TEXTURE_SELF();
	auto *data = count > 1 ? lh::checkObject<love::image::ImageData>(args[1], *binding.registry) : nullptr;
	if (data == nullptr)
		return lh::raise(machine, "Expected an ImageData");
	int x = (int) lh::optNumber(args, count, 2, 0);
	int y = (int) lh::optNumber(args, count, 3, 0);
	return lh::guard(machine, [&]() {
		t->replacePixels(data, 0, 0, x, y, true);
		return lhat_nil();
	});
}

bool lhGraphicsState(lh::Context &ctx)
{
	if (ctx.types())
		return true;
	const char *m = LH_GRAPHICS;
	const char *T = "Texture";
	return ctx.func(m, "newCanvas", "p^ -> love.graphics.Texture;", lh_newCanvas, nullptr)
		&& ctx.func(m, "newCanvas", "p^number^, number^ -> love.graphics.Texture;", lh_newCanvas, nullptr)
		&& ctx.func(m, "newCanvas", "p^number^, number^, t^{ msaa : number^ } -> love.graphics.Texture;", lh_newCanvas, nullptr)
		&& ctx.func(m, "setCanvas", "p^...;", lh_setCanvas, nullptr)
		&& ctx.func(m, "getCanvas", "p^ -> love.graphics.Texture|nil^;", lh_getCanvas, nullptr)
		&& ctx.func(m, "setShader", "p^;", lh_setShader, nullptr)
		&& ctx.func(m, "setShader", "p^love.graphics.Shader;", lh_setShader, nullptr)
		&& ctx.func(m, "getShader", "p^ -> love.graphics.Shader|nil^;", lh_getShader, nullptr)
		&& ctx.func(m, "setBlendMode", "p^string^;", lh_setBlendMode, nullptr)
		&& ctx.func(m, "setBlendMode", "p^string^, string^;", lh_setBlendMode, nullptr)
		&& ctx.func(m, "getBlendMode", "f^ -> (string^, string^);", lh_getBlendMode, nullptr)
		&& ctx.func(m, "setScissor", "p^;", lh_setScissor, nullptr)
		&& ctx.func(m, "setScissor", "p^number^, number^, number^, number^;", lh_setScissor, nullptr)
		&& ctx.func(m, "intersectScissor", "p^number^, number^, number^, number^;", lh_intersectScissor, nullptr)
		&& ctx.func(m, "getScissor", "f^ -> (number^, number^, number^, number^);", lh_getScissor, nullptr)
		&& ctx.func(m, "setStencilMode", "p^;", lh_setStencilMode, nullptr)
		&& ctx.func(m, "setStencilMode", "p^string^;", lh_setStencilMode, nullptr)
		&& ctx.func(m, "setStencilMode", "p^string^, number^;", lh_setStencilMode, nullptr)
		&& ctx.func(m, "getStencilMode", "f^ -> (string^, number^);", lh_getStencilMode, nullptr)
		&& ctx.func(m, "setColorMask", "p^;", lh_setColorMask, nullptr)
		&& ctx.func(m, "setColorMask", "p^bool^, bool^, bool^, bool^;", lh_setColorMask, nullptr)
		&& ctx.func(m, "getColorMask", "f^ -> (bool^, bool^, bool^, bool^);", lh_getColorMask, nullptr)
		&& ctx.func(m, "setDefaultFilter", "p^string^;", lh_setDefaultFilter, nullptr)
		&& ctx.func(m, "setDefaultFilter", "p^string^, string^;", lh_setDefaultFilter, nullptr)
		&& ctx.func(m, "setDefaultFilter", "p^string^, string^, number^;", lh_setDefaultFilter, nullptr)
		&& ctx.func(m, "getDefaultFilter", "f^ -> (string^, string^, number^);", lh_getDefaultFilter, nullptr)
		&& ctx.func(m, "setLineStyle", "p^string^;", lh_setLineStyle, nullptr)
		&& ctx.func(m, "getLineStyle", "f^ -> string^;", lh_getLineStyle, nullptr)
		&& ctx.func(m, "setLineJoin", "p^string^;", lh_setLineJoin, nullptr)
		&& ctx.func(m, "getLineJoin", "f^ -> string^;", lh_getLineJoin, nullptr)
		&& ctx.func(m, "setWireframe", "p^bool^;", lh_setWireframe, nullptr)
		&& ctx.func(m, "isWireframe", "f^ -> bool^;", lh_isWireframe, nullptr)
		&& ctx.func(m, "getPointSize", "f^ -> number^;", lh_getPointSize, nullptr)
		&& ctx.func(m, "reset", "p^;", lh_reset, nullptr)
		&& ctx.func(m, "shear", "p^number^, number^;", lh_shear, nullptr)
		&& ctx.func(m, "applyTransform", "p^love.math.Transform;", lh_applyTransform, nullptr)
		&& ctx.func(m, "replaceTransform", "p^love.math.Transform;", lh_replaceTransform, nullptr)
		&& ctx.func(m, "transformPoint", "f^number^, number^ -> (number^, number^);", lh_transformPoint, nullptr)
		&& ctx.func(m, "inverseTransformPoint", "f^number^, number^ -> (number^, number^);", lh_inverseTransformPoint, nullptr)
		&& ctx.func(m, "getStackDepth", "f^ -> number^;", lh_getStackDepth, nullptr)
		&& ctx.func(m, "ellipse", "p^string^, number^, number^, number^, ...;", lh_ellipse, nullptr)
		&& ctx.func(m, "arc", "p^string^, number^, number^, number^, number^, number^, ...;", lh_arc, nullptr)
		&& ctx.func(m, "arc", "p^string^, string^, number^, number^, number^, number^, number^, ...;", lh_arc, nullptr)
		&& ctx.func(m, "getRendererInfo", "f^ -> (string^, string^, string^, string^);", lh_getRendererInfo, nullptr)
		&& ctx.func(m, "getStats", "f^ -> t^{ drawcalls : number^, drawcallsbatched : number^, canvasswitches : number^, shaderswitches : number^, textures : number^, fonts : number^, buffers : number^, texturememory : number^, buffermemory : number^ };", lh_getStats, nullptr)
		&& ctx.func(m, "getDPIScale", "f^ -> number^;", lh_getDPIScale, nullptr)
		&& ctx.func(m, "getPixelDimensions", "f^ -> (number^, number^);", lh_getPixelDimensions, nullptr)
		&& ctx.func(m, "readbackTexture", "p^love.graphics.Texture -> love.image.ImageData;", lh_readbackTexture, nullptr)
		&& ctx.func(m, "readbackTexture", "p^love.graphics.Texture, number^, number^, number^, number^ -> love.image.ImageData;", lh_readbackTexture, nullptr)
		&& ctx.member(m, T, "isCanvas", "f^self^ -> bool^;", lh_Texture_isCanvas, nullptr)
		&& ctx.member(m, T, "isReadable", "f^self^ -> bool^;", lh_Texture_isReadable, nullptr)
		&& ctx.member(m, T, "getFormat", "f^self^ -> string^;", lh_Texture_getFormat, nullptr)
		&& ctx.member(m, T, "getDPIScale", "f^self^ -> number^;", lh_Texture_getDPIScale, nullptr)
		&& ctx.member(m, T, "getPixelDimensions", "f^self^ -> (number^, number^);", lh_Texture_getPixelDimensions, nullptr)
		&& ctx.member(m, T, "getMipmapCount", "f^self^ -> number^;", lh_Texture_getMipmapCount, nullptr)
		&& ctx.member(m, T, "getFilter", "f^self^ -> (string^, string^, number^);", lh_Texture_getFilter, nullptr)
		&& ctx.member(m, T, "setWrap", "p^self^, string^;", lh_Texture_setWrap, nullptr)
		&& ctx.member(m, T, "setWrap", "p^self^, string^, string^;", lh_Texture_setWrap, nullptr)
		&& ctx.member(m, T, "getWrap", "f^self^ -> (string^, string^);", lh_Texture_getWrap, nullptr)
		&& ctx.member(m, T, "generateMipmaps", "p^self^;", lh_Texture_generateMipmaps, nullptr)
		&& ctx.member(m, T, "replacePixels", "p^self^, love.image.ImageData;", lh_Texture_replacePixels, nullptr)
		&& ctx.member(m, T, "replacePixels", "p^self^, love.image.ImageData, number^, number^;", lh_Texture_replacePixels, nullptr);
}

} // graphics
} // love
