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

#ifndef LOVE_GRAPHICS_LH_GRAPHICS_H
#define LOVE_GRAPHICS_LH_GRAPHICS_H

// What the lh_*.cpp files of love.graphics share: the binding's registry,
// the readers every draw call uses, and the per-type halves of the
// registrar. The L^ side sees Texture (images and canvases alike), Font,
// Quad, Shader, Mesh, SpriteBatch, ParticleSystem, TextBatch and Video as
// hostdata types; a Drawable parameter is spelled as their union.

#include "Graphics.h"
#include "Texture.h"
#include "Quad.h"
#include "lh/lh.h"
#include "common/Matrix.h"
#include "common/Vector.h"

#include <string>
#include <vector>

namespace love
{
namespace graphics
{

#define LH_GRAPHICS "love.graphics"

struct GraphicsBinding
{
	lh::Errors *errors = nullptr;
	lh::TypeRegistry *registry = nullptr;
};

extern GraphicsBinding graphicsBinding;

#define graphicsInstance() (Module::getInstance<Graphics>(Module::M_GRAPHICS))

// The x, y, r, sx, sy, ox, oy, kx, ky tail of a draw call, from `first` on.
Matrix4 transformOf(const LhatValue *args, size_t count, size_t first);

// Readers that raise (and answer nullptr / false) on a miss.
Texture *checkTexture(LhatMachine *machine, const LhatValue *args, size_t count, size_t index);
Quad *checkQuad(LhatMachine *machine, const LhatValue *args, size_t count, size_t index);
bool drawModeOf(LhatMachine *machine, LhatValue value, Graphics::DrawMode &mode);

Colorf colorOf(const LhatValue *args, size_t count, size_t first);
void colorTuple(Colorf c, LhatValue *answers, int *answerCount);
// 05 の 8.7: writes `count` answers into the machine's room.
void numberTuple(const float *values, size_t count, LhatValue *answers, int *answerCount);

// x, y pairs from args[first..count) -- an odd tail drops its last number.
std::vector<Vector2> verticesOf(const LhatValue *args, size_t count, size_t first);

// Reads a table of numbers into `out`; false when the value is not one.
bool numbersOf(LhatValue table, std::vector<float> &out);

// The per-type halves of lhopen_love_graphics, each run in both phases.
bool lhGraphicsState(lh::Context &ctx);
bool lhGraphicsQuad(lh::Context &ctx);
bool lhGraphicsShader(lh::Context &ctx);
bool lhGraphicsMesh(lh::Context &ctx);
bool lhGraphicsSpriteBatch(lh::Context &ctx);
bool lhGraphicsParticleSystem(lh::Context &ctx);
bool lhGraphicsTextBatch(lh::Context &ctx);
bool lhGraphicsVideo(lh::Context &ctx);

} // graphics
} // love

#endif // LOVE_GRAPHICS_LH_GRAPHICS_H
