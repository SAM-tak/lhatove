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

// love.physics.Shape for L^: one hostdata type for every shape kind. The
// references are wrap_Shape.cpp, wrap_CircleShape.cpp, wrap_PolygonShape.cpp,
// wrap_EdgeShape.cpp and wrap_ChainShape.cpp beside this file. Members a
// kind does not have raise when called on it; getType tells the kinds apart.

#include "lh_Physics.h"

#include "CircleShape.h"
#include "PolygonShape.h"
#include "EdgeShape.h"
#include "ChainShape.h"

#include <bitset>

namespace love
{
namespace physics
{
namespace box2d
{

static Shape *self(LhatMachine *machine, const LhatValue *args, size_t count)
{
	return checkShape(machine, args, count, 0);
}

#define SHAPE_SELF() Shape *s = self(machine, args, count); if (s == nullptr) return lhat_nil()

template <typename T>
static T *kindOf(LhatMachine *machine, Shape *s, Shape::Type type, const char *name)
{
	if (s->getType() != type)
	{
		const char *have = "";
		Shape::getConstant(s->getType(), have);
		lh::raise(machine, std::string(name) + " is not a member of a " + have + " shape.");
		return nullptr;
	}
	return (T *) s;
}

#define SHAPE_NUMBER(name, expr) \
	static LhatValue lh_Shape_##name(LhatMachine *machine, void *context, const LhatValue *args, size_t count) \
	{ \
		(void) context; \
		SHAPE_SELF(); \
		return lh::guard(machine, [&]() { return lhat_real(expr); }); \
	}

#define SHAPE_BOOL(name, expr) \
	static LhatValue lh_Shape_##name(LhatMachine *machine, void *context, const LhatValue *args, size_t count) \
	{ \
		(void) context; \
		SHAPE_SELF(); \
		return lh::guard(machine, [&]() { return lhat_bool(expr); }); \
	}

#define SHAPE_SET_NUMBER(name, call) \
	static LhatValue lh_Shape_##name(LhatMachine *machine, void *context, const LhatValue *args, size_t count) \
	{ \
		(void) context; \
		SHAPE_SELF(); \
		float v = numberAt(args, count, 1); \
		return lh::guard(machine, [&]() { s->call; return lhat_nil(); }); \
	}

SHAPE_NUMBER(getRadius, s->getRadius())
SHAPE_NUMBER(getChildCount, (float) s->getChildCount())
SHAPE_SET_NUMBER(setFriction, setFriction(v))
SHAPE_SET_NUMBER(setRestitution, setRestitution(v))
SHAPE_SET_NUMBER(setDensity, setDensity(v))
SHAPE_NUMBER(getFriction, s->getFriction())
SHAPE_NUMBER(getRestitution, s->getRestitution())
SHAPE_NUMBER(getDensity, s->getDensity())
SHAPE_BOOL(isSensor, s->isSensor())
SHAPE_NUMBER(getGroupIndex, (float) s->getGroupIndex())

static LhatValue lh_Shape_getType(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	SHAPE_SELF();
	const char *type = "";
	Shape::getConstant(s->getType(), type);
	LhatValue out = lhat_nil();
	lh::makeString(machine, type, &out);
	return out;
}

static LhatValue lh_Shape_setSensor(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	SHAPE_SELF();
	bool sensor = boolAt(args, count, 1);
	return lh::guard(machine, [&]() {
		s->setSensor(sensor);
		return lhat_nil();
	});
}

static LhatValue lh_Shape_setGroupIndex(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	SHAPE_SELF();
	int index = (int) numberAt(args, count, 1);
	return lh::guard(machine, [&]() {
		s->setGroupIndex(index);
		return lhat_nil();
	});
}

static LhatValue lh_Shape_getBody(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	SHAPE_SELF();
	Body *body = s->getBody();
	return body != nullptr ? pushBody(machine, body) : lhat_nil();
}

// testPoint(x, y) against the fixture; testPoint(x, y, r, px, py) against
// the bare shape placed at (x, y, r).
static LhatValue lh_Shape_testPoint(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	SHAPE_SELF();
	return lh::guard(machine, [&]() {
		if (count >= 6)
			return lhat_bool(s->testPoint(numberAt(args, count, 1), numberAt(args, count, 2), numberAt(args, count, 3), numberAt(args, count, 4), numberAt(args, count, 5)));
		return lhat_bool(s->testPoint(numberAt(args, count, 1), numberAt(args, count, 2)));
	});
}

// rayCast(x1, y1, x2, y2, maxFraction[, childIndex]) -> (hit, nx, ny, fraction)
// rayCast(x1, y1, x2, y2, maxFraction, x, y, r[, childIndex]) -> the same, against the bare shape
static LhatValue lh_Shape_rayCast(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	SHAPE_SELF();
	float x1 = numberAt(args, count, 1), y1 = numberAt(args, count, 2);
	float x2 = numberAt(args, count, 3), y2 = numberAt(args, count, 4);
	float maxFraction = numberAt(args, count, 5);
	return lh::guard(machine, [&]() {
		float nx = 0, ny = 0, fraction = 0;
		bool hit = false;
		if (count >= 9)
		{
			int childIndex = (int) numberAt(args, count, 9, 1) - 1;
			hit = s->rayCast(x1, y1, x2, y2, maxFraction, numberAt(args, count, 6), numberAt(args, count, 7), numberAt(args, count, 8), childIndex, nx, ny, fraction);
		}
		else
		{
			int childIndex = (int) numberAt(args, count, 6, 1) - 1;
			hit = s->rayCast(x1, y1, x2, y2, maxFraction, childIndex, nx, ny, fraction);
		}
		LhatValue items[4] = {lhat_bool(hit), lhat_real(nx), lhat_real(ny), lhat_real(fraction)};
		LhatValue out = lhat_nil();
		lh::makeTuple(machine, items, 4, &out);
		return out;
	});
}

// computeAABB(x, y, r[, childIndex]) -> (lx, ly, ux, uy)
static LhatValue lh_Shape_computeAABB(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	SHAPE_SELF();
	return lh::guard(machine, [&]() {
		float out[4];
		s->computeAABB(numberAt(args, count, 1), numberAt(args, count, 2), numberAt(args, count, 3), (int) numberAt(args, count, 4, 1) - 1, out[0], out[1], out[2], out[3]);
		return numbers(machine, out, 4);
	});
}

// computeMass(density) -> (x, y, mass, inertia)
static LhatValue lh_Shape_computeMass(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	SHAPE_SELF();
	return lh::guard(machine, [&]() {
		float out[4];
		s->computeMass(numberAt(args, count, 1), out[0], out[1], out[2], out[3]);
		return numbers(machine, out, 4);
	});
}

static LhatValue lh_Shape_setFilterData(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	SHAPE_SELF();
	int v[3] = {(int) numberAt(args, count, 1), (int) numberAt(args, count, 2), (int) numberAt(args, count, 3)};
	return lh::guard(machine, [&]() {
		s->setFilterData(v);
		return lhat_nil();
	});
}

static LhatValue lh_Shape_getFilterData(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	SHAPE_SELF();
	return lh::guard(machine, [&]() {
		int v[3];
		s->getFilterData(v);
		LhatValue items[3] = {lhat_integer(v[0]), lhat_integer(v[1]), lhat_integer(v[2])};
		LhatValue out = lhat_nil();
		lh::makeTuple(machine, items, 3, &out);
		return out;
	});
}

// Category and mask bits are spelled as 1-based bit indices, as in Lua.
static bool bitsOf(LhatMachine *machine, const LhatValue *args, size_t count, uint16 &out)
{
	std::bitset<16> b;
	for (size_t i = 1; i < count; i++)
	{
		double index = lh::optNumber(args, count, i, 0);
		if (index < 1 || index > 16)
		{
			lh::raise(machine, "Values must be in range 1-16.");
			return false;
		}
		b.set((size_t) index - 1, true);
	}
	out = (uint16) b.to_ulong();
	return true;
}

static LhatValue bitList(LhatMachine *machine, uint16 bits)
{
	std::bitset<16> b((int) bits);
	std::vector<float> indices;
	for (int i = 0; i < 16; i++)
		if (b.test(i))
			indices.push_back((float) (i + 1));
	return numberList(machine, indices);
}

static LhatValue lh_Shape_setCategory(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	SHAPE_SELF();
	uint16 bits = 0;
	if (!bitsOf(machine, args, count, bits))
		return lhat_nil();
	return lh::guard(machine, [&]() {
		s->setCategory(bits);
		return lhat_nil();
	});
}

static LhatValue lh_Shape_setMask(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	SHAPE_SELF();
	uint16 bits = 0;
	if (!bitsOf(machine, args, count, bits))
		return lhat_nil();
	return lh::guard(machine, [&]() {
		s->setMask(bits);
		return lhat_nil();
	});
}

static LhatValue lh_Shape_getCategory(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	SHAPE_SELF();
	return lh::guard(machine, [&]() { return bitList(machine, s->getCategory()); });
}

static LhatValue lh_Shape_getMask(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	SHAPE_SELF();
	return lh::guard(machine, [&]() { return bitList(machine, s->getMask()); });
}

static LhatValue lh_Shape_setUserData(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	SHAPE_SELF();
	if (count < 2 || lhat_is_nil(args[1]))
		s->setUserData(nullptr);
	else
	{
		StrongRef<lh::Parked> parked(new lh::Parked(physicsBinding.lot, args[1]), Acquire::NORETAIN);
		s->setUserData(parked.get());
	}
	return lhat_nil();
}

static LhatValue lh_Shape_getUserData(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	SHAPE_SELF();
	lh::Parked *parked = dynamic_cast<lh::Parked *>(s->getUserData());
	return parked != nullptr ? parked->get() : lhat_nil();
}

// getBoundingBox([childIndex]) -> (lx, ly, ux, uy)
static LhatValue lh_Shape_getBoundingBox(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	SHAPE_SELF();
	return lh::guard(machine, [&]() {
		float out[4];
		s->getBoundingBox((int) numberAt(args, count, 1, 1) - 1, out[0], out[1], out[2], out[3]);
		return numbers(machine, out, 4);
	});
}

static LhatValue lh_Shape_getMassData(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	SHAPE_SELF();
	return lh::guard(machine, [&]() {
		float out[4];
		s->getMassData(out[0], out[1], out[2], out[3]);
		return numbers(machine, out, 4);
	});
}

static LhatValue lh_Shape_destroy(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	SHAPE_SELF();
	return lh::guard(machine, [&]() {
		s->destroy();
		return lhat_nil();
	});
}

static LhatValue lh_Shape_isDestroyed(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	SHAPE_SELF();
	return lhat_bool(!s->isValid());
}

// --- CircleShape ---

static LhatValue lh_Shape_setRadius(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	SHAPE_SELF();
	CircleShape *c = kindOf<CircleShape>(machine, s, Shape::SHAPE_CIRCLE, "setRadius");
	if (c == nullptr)
		return lhat_nil();
	float r = numberAt(args, count, 1);
	return lh::guard(machine, [&]() {
		c->setRadius(r);
		return lhat_nil();
	});
}

// getPoint() -> (x, y) of a circle; getPoint(index) -> (x, y) of a chain's vertex.
static LhatValue lh_Shape_getPoint(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	SHAPE_SELF();
	return lh::guard(machine, [&]() {
		float out[2] = {0, 0};
		if (count >= 2)
		{
			ChainShape *chain = kindOf<ChainShape>(machine, s, Shape::SHAPE_CHAIN, "getPoint(index)");
			if (chain == nullptr)
				return lhat_nil();
			b2Vec2 v = chain->getPoint((int) numberAt(args, count, 1, 1) - 1);
			out[0] = v.x;
			out[1] = v.y;
		}
		else
		{
			CircleShape *c = kindOf<CircleShape>(machine, s, Shape::SHAPE_CIRCLE, "getPoint()");
			if (c == nullptr)
				return lhat_nil();
			c->getPoint(out[0], out[1]);
		}
		return numbers(machine, out, 2);
	});
}

static LhatValue lh_Shape_setPoint(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	SHAPE_SELF();
	CircleShape *c = kindOf<CircleShape>(machine, s, Shape::SHAPE_CIRCLE, "setPoint");
	if (c == nullptr)
		return lhat_nil();
	float x = numberAt(args, count, 1), y = numberAt(args, count, 2);
	return lh::guard(machine, [&]() {
		c->setPoint(x, y);
		return lhat_nil();
	});
}

// --- Polygon / Edge / Chain ---

// getPoints() -> t^{...:number^}: the vertices as x, y pairs, for polygon,
// edge and chain shapes alike.
static LhatValue lh_Shape_getPoints(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	SHAPE_SELF();
	return lh::guard(machine, [&]() {
		std::vector<float> points;
		switch (s->getType())
		{
		case Shape::SHAPE_POLYGON:
			points = ((PolygonShape *) s)->getPoints();
			break;
		case Shape::SHAPE_EDGE:
		{
			points.resize(4);
			((EdgeShape *) s)->getPoints(points[0], points[1], points[2], points[3]);
			break;
		}
		case Shape::SHAPE_CHAIN:
		{
			ChainShape *chain = (ChainShape *) s;
			const b2Vec2 *verts = chain->getPoints();
			int n = chain->getVertexCount();
			for (int i = 0; i < n; i++)
			{
				b2Vec2 v = Physics::scaleUp(verts[i]);
				points.push_back(v.x);
				points.push_back(v.y);
			}
			break;
		}
		default:
			return lh::raise(machine, "getPoints is not a member of a circle shape.");
		}
		return numberList(machine, points);
	});
}

// The ghost vertices of an edge or a chain.
typedef void (EdgeShape::*EdgeSetter)(float, float);
typedef b2Vec2 (EdgeShape::*EdgeGetter)() const;
typedef void (ChainShape::*ChainSetter)(float, float);
typedef b2Vec2 (ChainShape::*ChainGetter)() const;

static LhatValue setVertex(LhatMachine *machine, Shape *s, const LhatValue *args, size_t count, EdgeSetter edge, ChainSetter chain, const char *name)
{
	float x = numberAt(args, count, 1), y = numberAt(args, count, 2);
	return lh::guard(machine, [&]() {
		if (s->getType() == Shape::SHAPE_EDGE)
			(((EdgeShape *) s)->*edge)(x, y);
		else if (s->getType() == Shape::SHAPE_CHAIN)
			(((ChainShape *) s)->*chain)(x, y);
		else
			return lh::raise(machine, std::string(name) + " is only a member of edge and chain shapes.");
		return lhat_nil();
	});
}

static LhatValue getVertex(LhatMachine *machine, Shape *s, EdgeGetter edge, ChainGetter chain, const char *name)
{
	return lh::guard(machine, [&]() {
		b2Vec2 v;
		if (s->getType() == Shape::SHAPE_EDGE)
			v = (((EdgeShape *) s)->*edge)();
		else if (s->getType() == Shape::SHAPE_CHAIN)
			v = (((ChainShape *) s)->*chain)();
		else
			return lh::raise(machine, std::string(name) + " is only a member of edge and chain shapes.");
		float out[2] = {v.x, v.y};
		return numbers(machine, out, 2);
	});
}

static LhatValue lh_Shape_setNextVertex(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	SHAPE_SELF();
	return setVertex(machine, s, args, count, &EdgeShape::setNextVertex, &ChainShape::setNextVertex, "setNextVertex");
}

static LhatValue lh_Shape_setPreviousVertex(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	SHAPE_SELF();
	return setVertex(machine, s, args, count, &EdgeShape::setPreviousVertex, &ChainShape::setPreviousVertex, "setPreviousVertex");
}

static LhatValue lh_Shape_getNextVertex(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	SHAPE_SELF();
	return getVertex(machine, s, &EdgeShape::getNextVertex, &ChainShape::getNextVertex, "getNextVertex");
}

static LhatValue lh_Shape_getPreviousVertex(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	SHAPE_SELF();
	return getVertex(machine, s, &EdgeShape::getPreviousVertex, &ChainShape::getPreviousVertex, "getPreviousVertex");
}

static LhatValue lh_Shape_getVertexCount(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	SHAPE_SELF();
	ChainShape *chain = kindOf<ChainShape>(machine, s, Shape::SHAPE_CHAIN, "getVertexCount");
	if (chain == nullptr)
		return lhat_nil();
	return lh::guard(machine, [&]() { return lhat_integer(chain->getVertexCount()); });
}

bool lhPhysicsShape(lh::Context &ctx)
{
	const char *m = LH_PHYSICS;
	if (!ctx.objectType(m, "Shape", Shape::type))
		return false;
	if (ctx.types())
		return true;

	const char *S = "Shape";
	return ctx.member(m, S, "getType", "f^self^ -> string^;", lh_Shape_getType, nullptr)
		&& ctx.member(m, S, "getRadius", "f^self^ -> number^;", lh_Shape_getRadius, nullptr)
		&& ctx.member(m, S, "getChildCount", "f^self^ -> number^;", lh_Shape_getChildCount, nullptr)
		&& ctx.member(m, S, "setFriction", "p^self^, number^;", lh_Shape_setFriction, nullptr)
		&& ctx.member(m, S, "setRestitution", "p^self^, number^;", lh_Shape_setRestitution, nullptr)
		&& ctx.member(m, S, "setDensity", "p^self^, number^;", lh_Shape_setDensity, nullptr)
		&& ctx.member(m, S, "setSensor", "p^self^, bool^;", lh_Shape_setSensor, nullptr)
		&& ctx.member(m, S, "getFriction", "f^self^ -> number^;", lh_Shape_getFriction, nullptr)
		&& ctx.member(m, S, "getRestitution", "f^self^ -> number^;", lh_Shape_getRestitution, nullptr)
		&& ctx.member(m, S, "getDensity", "f^self^ -> number^;", lh_Shape_getDensity, nullptr)
		&& ctx.member(m, S, "getBody", "p^self^ -> love.physics.Body|nil^;", lh_Shape_getBody, nullptr)
		&& ctx.member(m, S, "isSensor", "f^self^ -> bool^;", lh_Shape_isSensor, nullptr)
		&& ctx.member(m, S, "testPoint", "f^self^, number^, number^ -> bool^;", lh_Shape_testPoint, nullptr)
		&& ctx.member(m, S, "testPoint", "f^self^, number^, number^, number^, number^, number^ -> bool^;", lh_Shape_testPoint, nullptr)
		&& ctx.member(m, S, "rayCast", "f^self^, number^, number^, number^, number^, number^ -> (bool^, number^, number^, number^);", lh_Shape_rayCast, nullptr)
		&& ctx.member(m, S, "rayCast", "f^self^, number^, number^, number^, number^, number^, number^ -> (bool^, number^, number^, number^);", lh_Shape_rayCast, nullptr)
		&& ctx.member(m, S, "rayCast", "f^self^, number^, number^, number^, number^, number^, number^, number^, number^ -> (bool^, number^, number^, number^);", lh_Shape_rayCast, nullptr)
		&& ctx.member(m, S, "rayCast", "f^self^, number^, number^, number^, number^, number^, number^, number^, number^, number^ -> (bool^, number^, number^, number^);", lh_Shape_rayCast, nullptr)
		&& ctx.member(m, S, "computeAABB", "f^self^, number^, number^, number^ -> (number^, number^, number^, number^);", lh_Shape_computeAABB, nullptr)
		&& ctx.member(m, S, "computeAABB", "f^self^, number^, number^, number^, number^ -> (number^, number^, number^, number^);", lh_Shape_computeAABB, nullptr)
		&& ctx.member(m, S, "computeMass", "f^self^, number^ -> (number^, number^, number^, number^);", lh_Shape_computeMass, nullptr)
		&& ctx.member(m, S, "setFilterData", "p^self^, number^, number^, number^;", lh_Shape_setFilterData, nullptr)
		&& ctx.member(m, S, "getFilterData", "f^self^ -> (number^, number^, number^);", lh_Shape_getFilterData, nullptr)
		&& ctx.member(m, S, "setCategory", "p^self^, number^, ...;", lh_Shape_setCategory, nullptr)
		&& ctx.member(m, S, "getCategory", "f^self^ -> t^{...:number^};", lh_Shape_getCategory, nullptr)
		&& ctx.member(m, S, "setMask", "p^self^, number^, ...;", lh_Shape_setMask, nullptr)
		&& ctx.member(m, S, "getMask", "f^self^ -> t^{...:number^};", lh_Shape_getMask, nullptr)
		&& ctx.member(m, S, "setUserData", "p^self^, any^;", lh_Shape_setUserData, nullptr)
		&& ctx.member(m, S, "getUserData", "f^self^ -> any^;", lh_Shape_getUserData, nullptr)
		&& ctx.member(m, S, "getBoundingBox", "f^self^ -> (number^, number^, number^, number^);", lh_Shape_getBoundingBox, nullptr)
		&& ctx.member(m, S, "getBoundingBox", "f^self^, number^ -> (number^, number^, number^, number^);", lh_Shape_getBoundingBox, nullptr)
		&& ctx.member(m, S, "getMassData", "f^self^ -> (number^, number^, number^, number^);", lh_Shape_getMassData, nullptr)
		&& ctx.member(m, S, "getGroupIndex", "f^self^ -> number^;", lh_Shape_getGroupIndex, nullptr)
		&& ctx.member(m, S, "setGroupIndex", "p^self^, number^;", lh_Shape_setGroupIndex, nullptr)
		&& ctx.member(m, S, "destroy", "p^self^;", lh_Shape_destroy, nullptr)
		&& ctx.member(m, S, "isDestroyed", "f^self^ -> bool^;", lh_Shape_isDestroyed, nullptr)
		// CircleShape
		&& ctx.member(m, S, "setRadius", "p^self^, number^;", lh_Shape_setRadius, nullptr)
		&& ctx.member(m, S, "getPoint", "f^self^ -> (number^, number^);", lh_Shape_getPoint, nullptr)
		&& ctx.member(m, S, "getPoint", "f^self^, number^ -> (number^, number^);", lh_Shape_getPoint, nullptr)
		&& ctx.member(m, S, "setPoint", "p^self^, number^, number^;", lh_Shape_setPoint, nullptr)
		// PolygonShape / EdgeShape / ChainShape
		&& ctx.member(m, S, "getPoints", "f^self^ -> t^{...:number^};", lh_Shape_getPoints, nullptr)
		&& ctx.member(m, S, "setNextVertex", "p^self^, number^, number^;", lh_Shape_setNextVertex, nullptr)
		&& ctx.member(m, S, "setPreviousVertex", "p^self^, number^, number^;", lh_Shape_setPreviousVertex, nullptr)
		&& ctx.member(m, S, "getNextVertex", "f^self^ -> (number^, number^);", lh_Shape_getNextVertex, nullptr)
		&& ctx.member(m, S, "getPreviousVertex", "f^self^ -> (number^, number^);", lh_Shape_getPreviousVertex, nullptr)
		&& ctx.member(m, S, "getVertexCount", "f^self^ -> number^;", lh_Shape_getVertexCount, nullptr);
}

} // box2d
} // physics
} // love
