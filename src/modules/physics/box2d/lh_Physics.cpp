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

// love.physics for L^: the module functions (worlds, bodies, shapes, joints,
// the meter) and the helpers the other lh_*.cpp files in this directory use.
// The reference is wrap_Physics.cpp beside this file. The deprecated
// body-less shape constructors and newFixture are not carried over.

#include "lh_Physics.h"

#include "CircleShape.h"
#include "PolygonShape.h"
#include "EdgeShape.h"
#include "ChainShape.h"
#include "DistanceJoint.h"
#include "MouseJoint.h"
#include "RevoluteJoint.h"
#include "PrismaticJoint.h"
#include "PulleyJoint.h"
#include "GearJoint.h"
#include "FrictionJoint.h"
#include "WeldJoint.h"
#include "WheelJoint.h"
#include "RopeJoint.h"
#include "MotorJoint.h"

namespace love
{
namespace physics
{
namespace box2d
{

#define instance() (Module::getInstance<Physics>(Module::M_PHYSICS))

PhysicsBinding physicsBinding;

// ---------------------------------------------------------------------------
// Shared helpers
// ---------------------------------------------------------------------------

template <typename T>
static T *checkAt(LhatMachine *machine, const LhatValue *args, size_t count, size_t index, const char *what)
{
	T *object = index < count ? lh::checkObject<T>(args[index], *physicsBinding.registry) : nullptr;
	if (object == nullptr)
		lh::raise(machine, std::string("Expected a ") + what);
	return object;
}

World *checkWorld(LhatMachine *machine, const LhatValue *args, size_t count, size_t index)
{
	return checkAt<World>(machine, args, count, index, "World");
}

Body *checkBody(LhatMachine *machine, const LhatValue *args, size_t count, size_t index)
{
	return checkAt<Body>(machine, args, count, index, "Body");
}

Shape *checkShape(LhatMachine *machine, const LhatValue *args, size_t count, size_t index)
{
	return checkAt<Shape>(machine, args, count, index, "Shape");
}

Joint *checkJoint(LhatMachine *machine, const LhatValue *args, size_t count, size_t index)
{
	return checkAt<Joint>(machine, args, count, index, "Joint");
}

Contact *checkContact(LhatMachine *machine, const LhatValue *args, size_t count, size_t index)
{
	return checkAt<Contact>(machine, args, count, index, "Contact");
}

float numberAt(const LhatValue *args, size_t count, size_t index, float fallback)
{
	return (float) lh::optNumber(args, count, index, fallback);
}

bool boolAt(const LhatValue *args, size_t count, size_t index, bool fallback)
{
	return lh::optBool(args, count, index, fallback);
}

bool coordsOf(LhatMachine *machine, const LhatValue *args, size_t count, size_t from, std::vector<Vector2> &out)
{
	if (from > count || (count - from) % 2 != 0)
	{
		lh::raise(machine, "Number of vertex components must be a multiple of two.");
		return false;
	}
	for (size_t i = from; i + 1 < count; i += 2)
	{
		if (!lhat_is_number(args[i]) || !lhat_is_number(args[i + 1]))
		{
			lh::raise(machine, "Vertex components must be numbers.");
			return false;
		}
		out.emplace_back((float) lh::optNumber(args, count, i, 0), (float) lh::optNumber(args, count, i + 1, 0));
	}
	return true;
}

LhatValue pushWorld(LhatMachine *machine, World *world)
{
	return lh::pushObject(machine, *physicsBinding.registry, world);
}

LhatValue pushBody(LhatMachine *machine, Body *body)
{
	return lh::pushObject(machine, *physicsBinding.registry, body);
}

LhatValue pushShape(LhatMachine *machine, Shape *shape)
{
	return lh::pushObject(machine, *physicsBinding.registry, Shape::type, shape);
}

LhatValue pushJoint(LhatMachine *machine, Joint *joint)
{
	return lh::pushObject(machine, *physicsBinding.registry, Joint::type, joint);
}

LhatValue pushContact(LhatMachine *machine, Contact *contact)
{
	return lh::pushObject(machine, *physicsBinding.registry, contact);
}

LhatValue numbers(LhatMachine *machine, const float *values, size_t count)
{
	std::vector<LhatValue> items(count);
	for (size_t i = 0; i < count; i++)
		items[i] = lhat_real(values[i]);
	LhatValue out = lhat_nil();
	lh::makeTuple(machine, items.data(), count, &out);
	return out;
}

LhatValue numberList(LhatMachine *machine, const std::vector<float> &values)
{
	LhatValue table = lhat_nil();
	if (!lhat_machine_make_table(machine, &table))
		return lhat_nil();
	LhatTable *t = (LhatTable *) lhat_as_object(table);
	for (size_t i = 0; i < values.size(); i++)
	{
		bool refused = false;
		lhat_table_set(t, lhat_integer((int64_t) i + 1), lhat_real(values[i]), &refused);
	}
	return table;
}

bool callParked(LhatMachine *machine, lh::Parked *callee, const LhatValue *args, size_t count, LhatValue *result)
{
	if (callee == nullptr)
		return false;
	LhatValue fn = callee->get();
	if (!lhat_is_object_kind(fn, LHAT_OBJECT_SUBROUTINE) && !lhat_is_object_kind(fn, LHAT_OBJECT_HOST))
		return false;
	LhatRunResult ran = lhat_machine_call(machine, fn, args, count);
	if (ran.status != LHAT_RUN_OK)
		return false;
	if (result != nullptr)
		*result = ran.value;
	return true;
}

// ---------------------------------------------------------------------------
// Module functions
// ---------------------------------------------------------------------------

static bool bodyTypeAt(LhatMachine *machine, const LhatValue *args, size_t count, size_t index, Body::Type &out)
{
	std::string name = lh::optString(args, count, index, "static");
	if (!Body::getConstant(name.c_str(), out))
	{
		lh::raise(machine, "Invalid Body type: " + name);
		return false;
	}
	return true;
}

// Pushes (body, shape) and drops the references the factory handed over.
static LhatValue bodyAndShape(LhatMachine *machine, Body *body, Shape *shape)
{
	LhatValue pair[2] = {pushBody(machine, body), pushShape(machine, shape)};
	body->release();
	shape->release();
	LhatValue out = lhat_nil();
	lh::makeTuple(machine, pair, 2, &out);
	return out;
}

// newWorld([gx, gy[, sleep]])
static LhatValue lh_newWorld(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	float gx = numberAt(args, count, 0);
	float gy = numberAt(args, count, 1);
	bool sleep = boolAt(args, count, 2, true);
	return lh::guard(machine, [&]() {
		StrongRef<World> world(instance()->newWorld(gx, gy, sleep), Acquire::NORETAIN);
		return pushWorld(machine, world.get());
	});
}

// newBody(world[, x, y[, type]])
static LhatValue lh_newBody(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	World *world = checkWorld(machine, args, count, 0);
	if (world == nullptr)
		return lhat_nil();
	float x = numberAt(args, count, 1);
	float y = numberAt(args, count, 2);
	Body::Type btype = Body::BODY_STATIC;
	if (!bodyTypeAt(machine, args, count, 3, btype))
		return lhat_nil();
	return lh::guard(machine, [&]() {
		StrongRef<Body> body(instance()->newBody(world, x, y, btype), Acquire::NORETAIN);
		return pushBody(machine, body.get());
	});
}

// newCircleBody(world, type, x, y, radius) -> (Body, Shape)
static LhatValue lh_newCircleBody(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	World *world = checkWorld(machine, args, count, 0);
	Body::Type btype = Body::BODY_STATIC;
	if (world == nullptr || !bodyTypeAt(machine, args, count, 1, btype))
		return lhat_nil();
	float x = numberAt(args, count, 2);
	float y = numberAt(args, count, 3);
	float radius = numberAt(args, count, 4);
	return lh::guard(machine, [&]() {
		CircleShape *shape = nullptr;
		Body *body = instance()->newCircleBody(world, btype, x, y, radius, shape);
		return bodyAndShape(machine, body, shape);
	});
}

// newRectangleBody(world, type, x, y, w, h[, angle]) -> (Body, Shape)
static LhatValue lh_newRectangleBody(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	World *world = checkWorld(machine, args, count, 0);
	Body::Type btype = Body::BODY_STATIC;
	if (world == nullptr || !bodyTypeAt(machine, args, count, 1, btype))
		return lhat_nil();
	float x = numberAt(args, count, 2);
	float y = numberAt(args, count, 3);
	float w = numberAt(args, count, 4);
	float h = numberAt(args, count, 5);
	float angle = numberAt(args, count, 6);
	return lh::guard(machine, [&]() {
		PolygonShape *shape = nullptr;
		Body *body = instance()->newRectangleBody(world, btype, x, y, w, h, angle, shape);
		return bodyAndShape(machine, body, shape);
	});
}

// newPolygonBody(world, type, x1, y1, x2, y2, x3, y3, ...) -> (Body, Shape)
static LhatValue lh_newPolygonBody(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	World *world = checkWorld(machine, args, count, 0);
	Body::Type btype = Body::BODY_STATIC;
	if (world == nullptr || !bodyTypeAt(machine, args, count, 1, btype))
		return lhat_nil();
	std::vector<Vector2> coords;
	if (!coordsOf(machine, args, count, 2, coords))
		return lhat_nil();
	return lh::guard(machine, [&]() {
		PolygonShape *shape = nullptr;
		Body *body = instance()->newPolygonBody(world, btype, coords.data(), (int) coords.size(), shape);
		return bodyAndShape(machine, body, shape);
	});
}

// newEdgeBody(world, type, x1, y1, x2, y2[, prevx, prevy, nextx, nexty]) -> (Body, Shape)
static LhatValue lh_newEdgeBody(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	World *world = checkWorld(machine, args, count, 0);
	Body::Type btype = Body::BODY_STATIC;
	if (world == nullptr || !bodyTypeAt(machine, args, count, 1, btype))
		return lhat_nil();
	float x1 = numberAt(args, count, 2);
	float y1 = numberAt(args, count, 3);
	float x2 = numberAt(args, count, 4);
	float y2 = numberAt(args, count, 5);
	return lh::guard(machine, [&]() {
		EdgeShape *shape = nullptr;
		Body *body = nullptr;
		if (count >= 10)
		{
			float prevx = numberAt(args, count, 6);
			float prevy = numberAt(args, count, 7);
			float nextx = numberAt(args, count, 8);
			float nexty = numberAt(args, count, 9);
			body = instance()->newEdgeBody(world, btype, x1, y1, x2, y2, prevx, prevy, nextx, nexty, shape);
		}
		else
			body = instance()->newEdgeBody(world, btype, x1, y1, x2, y2, shape);
		return bodyAndShape(machine, body, shape);
	});
}

// newChainBody(world, type, loop, x1, y1, x2, y2, ...) -> (Body, Shape)
static LhatValue lh_newChainBody(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	World *world = checkWorld(machine, args, count, 0);
	Body::Type btype = Body::BODY_STATIC;
	if (world == nullptr || !bodyTypeAt(machine, args, count, 1, btype))
		return lhat_nil();
	bool loop = boolAt(args, count, 2);
	std::vector<Vector2> coords;
	if (!coordsOf(machine, args, count, 3, coords) || coords.empty())
		return coords.empty() ? lh::raise(machine, "A chain needs at least one vertex.") : lhat_nil();
	return lh::guard(machine, [&]() {
		ChainShape *shape = nullptr;
		Body *body = instance()->newChainBody(world, btype, loop, coords.data(), (int) coords.size(), shape);
		return bodyAndShape(machine, body, shape);
	});
}

// newCircleShape(body, radius) / newCircleShape(body, x, y, radius)
static LhatValue lh_newCircleShape(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	Body *body = checkBody(machine, args, count, 0);
	if (body == nullptr)
		return lhat_nil();
	float x = 0, y = 0, radius = 0;
	if (count >= 4)
	{
		x = numberAt(args, count, 1);
		y = numberAt(args, count, 2);
		radius = numberAt(args, count, 3);
	}
	else
		radius = numberAt(args, count, 1);
	return lh::guard(machine, [&]() {
		StrongRef<CircleShape> shape(instance()->newCircleShape(body, x, y, radius), Acquire::NORETAIN);
		return pushShape(machine, shape.get());
	});
}

// newRectangleShape(body, w, h) / newRectangleShape(body, x, y, w, h[, angle])
static LhatValue lh_newRectangleShape(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	Body *body = checkBody(machine, args, count, 0);
	if (body == nullptr)
		return lhat_nil();
	float x = 0, y = 0, w = 0, h = 0, angle = 0;
	if (count >= 5)
	{
		x = numberAt(args, count, 1);
		y = numberAt(args, count, 2);
		w = numberAt(args, count, 3);
		h = numberAt(args, count, 4);
		angle = numberAt(args, count, 5);
	}
	else
	{
		w = numberAt(args, count, 1);
		h = numberAt(args, count, 2);
	}
	return lh::guard(machine, [&]() {
		StrongRef<PolygonShape> shape(instance()->newRectangleShape(body, x, y, w, h, angle), Acquire::NORETAIN);
		return pushShape(machine, shape.get());
	});
}

// newEdgeShape(body, x1, y1, x2, y2[, prevx, prevy, nextx, nexty])
static LhatValue lh_newEdgeShape(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	Body *body = checkBody(machine, args, count, 0);
	if (body == nullptr)
		return lhat_nil();
	float x1 = numberAt(args, count, 1);
	float y1 = numberAt(args, count, 2);
	float x2 = numberAt(args, count, 3);
	float y2 = numberAt(args, count, 4);
	return lh::guard(machine, [&]() {
		EdgeShape *made = nullptr;
		if (count >= 9)
		{
			float prevx = numberAt(args, count, 5);
			float prevy = numberAt(args, count, 6);
			float nextx = numberAt(args, count, 7);
			float nexty = numberAt(args, count, 8);
			made = instance()->newEdgeShape(body, x1, y1, x2, y2, prevx, prevy, nextx, nexty);
		}
		else
			made = instance()->newEdgeShape(body, x1, y1, x2, y2);
		StrongRef<EdgeShape> shape(made, Acquire::NORETAIN);
		return pushShape(machine, shape.get());
	});
}

// newPolygonShape(body, x1, y1, x2, y2, x3, y3, ...)
static LhatValue lh_newPolygonShape(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	Body *body = checkBody(machine, args, count, 0);
	if (body == nullptr)
		return lhat_nil();
	std::vector<Vector2> coords;
	if (!coordsOf(machine, args, count, 1, coords))
		return lhat_nil();
	return lh::guard(machine, [&]() {
		StrongRef<PolygonShape> shape(instance()->newPolygonShape(body, coords.data(), (int) coords.size()), Acquire::NORETAIN);
		return pushShape(machine, shape.get());
	});
}

// newChainShape(body, loop, x1, y1, x2, y2, ...)
static LhatValue lh_newChainShape(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	Body *body = checkBody(machine, args, count, 0);
	if (body == nullptr)
		return lhat_nil();
	bool loop = boolAt(args, count, 1);
	std::vector<Vector2> coords;
	if (!coordsOf(machine, args, count, 2, coords))
		return lhat_nil();
	if (coords.empty())
		return lh::raise(machine, "A chain needs at least one vertex.");
	return lh::guard(machine, [&]() {
		StrongRef<ChainShape> shape(instance()->newChainShape(body, loop, coords.data(), (int) coords.size()), Acquire::NORETAIN);
		return pushShape(machine, shape.get());
	});
}

// Two bodies at args[0], args[1]; false (raised) when either is missing.
static bool twoBodies(LhatMachine *machine, const LhatValue *args, size_t count, Body *&a, Body *&b)
{
	a = checkBody(machine, args, count, 0);
	b = a != nullptr ? checkBody(machine, args, count, 1) : nullptr;
	return b != nullptr;
}

template <typename T>
static LhatValue pushNewJoint(LhatMachine *machine, const std::function<T *()> &make)
{
	return lh::guard(machine, [&]() {
		StrongRef<T> joint(make(), Acquire::NORETAIN);
		return pushJoint(machine, joint.get());
	});
}

// newDistanceJoint(b1, b2, x1, y1, x2, y2[, collideConnected])
static LhatValue lh_newDistanceJoint(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	Body *b1, *b2;
	if (!twoBodies(machine, args, count, b1, b2))
		return lhat_nil();
	float x1 = numberAt(args, count, 2), y1 = numberAt(args, count, 3);
	float x2 = numberAt(args, count, 4), y2 = numberAt(args, count, 5);
	bool collide = boolAt(args, count, 6);
	return pushNewJoint<DistanceJoint>(machine, [&]() { return instance()->newDistanceJoint(b1, b2, x1, y1, x2, y2, collide); });
}

// newMouseJoint(body, x, y)
static LhatValue lh_newMouseJoint(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	Body *body = checkBody(machine, args, count, 0);
	if (body == nullptr)
		return lhat_nil();
	float x = numberAt(args, count, 1), y = numberAt(args, count, 2);
	return pushNewJoint<MouseJoint>(machine, [&]() { return instance()->newMouseJoint(body, x, y); });
}

// The anchor arguments the revolute/weld/friction joints share:
// (b1, b2, x, y[, collide]) or (b1, b2, x1, y1, x2, y2[, collide[, referenceAngle]]).
struct Anchors
{
	float xA = 0, yA = 0, xB = 0, yB = 0;
	bool collide = false;
	bool hasReferenceAngle = false;
	float referenceAngle = 0;
};

static Anchors anchorsOf(const LhatValue *args, size_t count)
{
	Anchors a;
	a.xA = numberAt(args, count, 2);
	a.yA = numberAt(args, count, 3);
	if (count >= 6)
	{
		a.xB = numberAt(args, count, 4);
		a.yB = numberAt(args, count, 5);
		a.collide = boolAt(args, count, 6);
		a.hasReferenceAngle = count >= 8;
		a.referenceAngle = numberAt(args, count, 7);
	}
	else
	{
		a.xB = a.xA;
		a.yB = a.yA;
		a.collide = boolAt(args, count, 4);
	}
	return a;
}

static LhatValue lh_newRevoluteJoint(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	Body *b1, *b2;
	if (!twoBodies(machine, args, count, b1, b2))
		return lhat_nil();
	Anchors a = anchorsOf(args, count);
	return pushNewJoint<RevoluteJoint>(machine, [&]() {
		if (a.hasReferenceAngle)
			return instance()->newRevoluteJoint(b1, b2, a.xA, a.yA, a.xB, a.yB, a.collide, a.referenceAngle);
		return instance()->newRevoluteJoint(b1, b2, a.xA, a.yA, a.xB, a.yB, a.collide);
	});
}

static LhatValue lh_newWeldJoint(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	Body *b1, *b2;
	if (!twoBodies(machine, args, count, b1, b2))
		return lhat_nil();
	Anchors a = anchorsOf(args, count);
	return pushNewJoint<WeldJoint>(machine, [&]() {
		if (a.hasReferenceAngle)
			return instance()->newWeldJoint(b1, b2, a.xA, a.yA, a.xB, a.yB, a.collide, a.referenceAngle);
		return instance()->newWeldJoint(b1, b2, a.xA, a.yA, a.xB, a.yB, a.collide);
	});
}

static LhatValue lh_newFrictionJoint(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	Body *b1, *b2;
	if (!twoBodies(machine, args, count, b1, b2))
		return lhat_nil();
	Anchors a = anchorsOf(args, count);
	return pushNewJoint<FrictionJoint>(machine, [&]() { return instance()->newFrictionJoint(b1, b2, a.xA, a.yA, a.xB, a.yB, a.collide); });
}

// (b1, b2, x, y, ax, ay[, collide]) or (b1, b2, x1, y1, x2, y2, ax, ay[, collide[, referenceAngle]])
struct AxisAnchors
{
	Anchors anchors;
	float ax = 0, ay = 0;
};

static AxisAnchors axisAnchorsOf(const LhatValue *args, size_t count)
{
	AxisAnchors a;
	a.anchors.xA = numberAt(args, count, 2);
	a.anchors.yA = numberAt(args, count, 3);
	if (count >= 8)
	{
		a.anchors.xB = numberAt(args, count, 4);
		a.anchors.yB = numberAt(args, count, 5);
		a.ax = numberAt(args, count, 6);
		a.ay = numberAt(args, count, 7);
		a.anchors.collide = boolAt(args, count, 8);
		a.anchors.hasReferenceAngle = count >= 10;
		a.anchors.referenceAngle = numberAt(args, count, 9);
	}
	else
	{
		a.anchors.xB = a.anchors.xA;
		a.anchors.yB = a.anchors.yA;
		a.ax = numberAt(args, count, 4);
		a.ay = numberAt(args, count, 5);
		a.anchors.collide = boolAt(args, count, 6);
	}
	return a;
}

static LhatValue lh_newPrismaticJoint(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	Body *b1, *b2;
	if (!twoBodies(machine, args, count, b1, b2))
		return lhat_nil();
	AxisAnchors a = axisAnchorsOf(args, count);
	const Anchors &n = a.anchors;
	return pushNewJoint<PrismaticJoint>(machine, [&]() {
		if (n.hasReferenceAngle)
			return instance()->newPrismaticJoint(b1, b2, n.xA, n.yA, n.xB, n.yB, a.ax, a.ay, n.collide, n.referenceAngle);
		return instance()->newPrismaticJoint(b1, b2, n.xA, n.yA, n.xB, n.yB, a.ax, a.ay, n.collide);
	});
}

static LhatValue lh_newWheelJoint(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	Body *b1, *b2;
	if (!twoBodies(machine, args, count, b1, b2))
		return lhat_nil();
	AxisAnchors a = axisAnchorsOf(args, count);
	const Anchors &n = a.anchors;
	return pushNewJoint<WheelJoint>(machine, [&]() { return instance()->newWheelJoint(b1, b2, n.xA, n.yA, n.xB, n.yB, a.ax, a.ay, n.collide); });
}

// newPulleyJoint(b1, b2, gx1, gy1, gx2, gy2, x1, y1, x2, y2, ratio[, collide])
static LhatValue lh_newPulleyJoint(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	Body *b1, *b2;
	if (!twoBodies(machine, args, count, b1, b2))
		return lhat_nil();
	b2Vec2 g1(numberAt(args, count, 2), numberAt(args, count, 3));
	b2Vec2 g2(numberAt(args, count, 4), numberAt(args, count, 5));
	b2Vec2 a1(numberAt(args, count, 6), numberAt(args, count, 7));
	b2Vec2 a2(numberAt(args, count, 8), numberAt(args, count, 9));
	float ratio = numberAt(args, count, 10, 1.0f);
	bool collide = boolAt(args, count, 11, true);
	return pushNewJoint<PulleyJoint>(machine, [&]() { return instance()->newPulleyJoint(b1, b2, g1, g2, a1, a2, ratio, collide); });
}

// newGearJoint(j1, j2[, ratio[, collide]])
static LhatValue lh_newGearJoint(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	Joint *j1 = checkJoint(machine, args, count, 0);
	Joint *j2 = j1 != nullptr ? checkJoint(machine, args, count, 1) : nullptr;
	if (j2 == nullptr)
		return lhat_nil();
	float ratio = numberAt(args, count, 2, 1.0f);
	bool collide = boolAt(args, count, 3);
	return pushNewJoint<GearJoint>(machine, [&]() { return instance()->newGearJoint(j1, j2, ratio, collide); });
}

// newRopeJoint(b1, b2, x1, y1, x2, y2, maxLength[, collide])
static LhatValue lh_newRopeJoint(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	Body *b1, *b2;
	if (!twoBodies(machine, args, count, b1, b2))
		return lhat_nil();
	float x1 = numberAt(args, count, 2), y1 = numberAt(args, count, 3);
	float x2 = numberAt(args, count, 4), y2 = numberAt(args, count, 5);
	float maxLength = numberAt(args, count, 6);
	bool collide = boolAt(args, count, 7);
	return pushNewJoint<RopeJoint>(machine, [&]() { return instance()->newRopeJoint(b1, b2, x1, y1, x2, y2, maxLength, collide); });
}

// newMotorJoint(b1, b2[, correctionFactor[, collide]])
static LhatValue lh_newMotorJoint(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	Body *b1, *b2;
	if (!twoBodies(machine, args, count, b1, b2))
		return lhat_nil();
	return pushNewJoint<MotorJoint>(machine, [&]() {
		if (count >= 3)
			return instance()->newMotorJoint(b1, b2, numberAt(args, count, 2), boolAt(args, count, 3));
		return instance()->newMotorJoint(b1, b2);
	});
}

// getDistance(shapeA, shapeB) -> (distance, x1, y1, x2, y2)
static LhatValue lh_getDistance(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	Shape *a = checkShape(machine, args, count, 0);
	Shape *b = a != nullptr ? checkShape(machine, args, count, 1) : nullptr;
	if (b == nullptr)
		return lhat_nil();
	return lh::guard(machine, [&]() {
		float out[5];
		out[0] = Physics::getDistance(a, b, out[1], out[2], out[3], out[4]);
		return numbers(machine, out, 5);
	});
}

static LhatValue lh_getMeter(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) machine;
	(void) context;
	(void) args;
	(void) count;
	return lhat_real(Physics::getMeter());
}

static LhatValue lh_setMeter(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	float meter = numberAt(args, count, 0);
	return lh::guard(machine, [&]() {
		Physics::setMeter(meter);
		return lhat_nil();
	});
}

// The four stiffness/frequency conversions: (a, b, body1[, body2]) -> (x, y),
// the ground body standing in when body2 is not given.
typedef void (*Conversion)(float &, float &, float, float, b2Body *, b2Body *);

static LhatValue convert(LhatMachine *machine, const LhatValue *args, size_t count, Conversion conversion)
{
	float a = numberAt(args, count, 0);
	float b = numberAt(args, count, 1);
	Body *body1 = checkBody(machine, args, count, 2);
	if (body1 == nullptr)
		return lhat_nil();
	b2Body *other = nullptr;
	if (count >= 4)
	{
		Body *body2 = checkBody(machine, args, count, 3);
		if (body2 == nullptr)
			return lhat_nil();
		other = body2->body;
	}
	else
		other = body1->getWorld()->getGroundBody();
	float out[2];
	conversion(out[0], out[1], a, b, body1->body, other);
	return numbers(machine, out, 2);
}

static LhatValue lh_computeLinearStiffness(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	return convert(machine, args, count, Physics::computeLinearStiffness);
}

static LhatValue lh_computeLinearFrequency(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	return convert(machine, args, count, Physics::computeLinearFrequency);
}

static LhatValue lh_computeAngularStiffness(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	return convert(machine, args, count, Physics::computeAngularStiffness);
}

static LhatValue lh_computeAngularFrequency(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	return convert(machine, args, count, Physics::computeAngularFrequency);
}

} // box2d
} // physics
} // love

namespace love
{
namespace lh
{

bool lhopen_love_physics(Context &ctx)
{
	using namespace love::physics::box2d;
	const char *m = LH_PHYSICS;

	// The types first (every signature below names them), then each type's
	// members, then the module functions.
	if (!lhPhysicsWorld(ctx) || !lhPhysicsBody(ctx) || !lhPhysicsShape(ctx) || !lhPhysicsJoint(ctx) || !lhPhysicsContact(ctx))
		return false;
	if (ctx.types())
		return true;

	physicsBinding.errors = ctx.errors;
	physicsBinding.registry = ctx.registry;

	const char *W = "love.physics.World";
	const char *B = "love.physics.Body";
	const char *S = "love.physics.Shape";
	const char *J = "love.physics.Joint";
	std::string bs = std::string(" -> (") + B + ", " + S + ");";

	return ctx.func(m, "newWorld", (std::string("p^ -> ") + W + ";").c_str(), lh_newWorld, nullptr)
		&& ctx.func(m, "newWorld", (std::string("p^number^, number^ -> ") + W + ";").c_str(), lh_newWorld, nullptr)
		&& ctx.func(m, "newWorld", (std::string("p^number^, number^, bool^ -> ") + W + ";").c_str(), lh_newWorld, nullptr)
		&& ctx.func(m, "newBody", (std::string("p^") + W + " -> " + B + ";").c_str(), lh_newBody, nullptr)
		&& ctx.func(m, "newBody", (std::string("p^") + W + ", number^, number^ -> " + B + ";").c_str(), lh_newBody, nullptr)
		&& ctx.func(m, "newBody", (std::string("p^") + W + ", number^, number^, string^ -> " + B + ";").c_str(), lh_newBody, nullptr)
		&& ctx.func(m, "newCircleBody", (std::string("p^") + W + ", string^, number^, number^, number^" + bs).c_str(), lh_newCircleBody, nullptr)
		&& ctx.func(m, "newRectangleBody", (std::string("p^") + W + ", string^, number^, number^, number^, number^" + bs).c_str(), lh_newRectangleBody, nullptr)
		&& ctx.func(m, "newRectangleBody", (std::string("p^") + W + ", string^, number^, number^, number^, number^, number^" + bs).c_str(), lh_newRectangleBody, nullptr)
		&& ctx.func(m, "newPolygonBody", (std::string("p^") + W + ", string^, number^, number^, number^, number^, number^, number^, ..." + bs).c_str(), lh_newPolygonBody, nullptr)
		&& ctx.func(m, "newEdgeBody", (std::string("p^") + W + ", string^, number^, number^, number^, number^" + bs).c_str(), lh_newEdgeBody, nullptr)
		&& ctx.func(m, "newEdgeBody", (std::string("p^") + W + ", string^, number^, number^, number^, number^, number^, number^, number^, number^" + bs).c_str(), lh_newEdgeBody, nullptr)
		&& ctx.func(m, "newChainBody", (std::string("p^") + W + ", string^, bool^, number^, number^, ..." + bs).c_str(), lh_newChainBody, nullptr)
		&& ctx.func(m, "newCircleShape", (std::string("p^") + B + ", number^ -> " + S + ";").c_str(), lh_newCircleShape, nullptr)
		&& ctx.func(m, "newCircleShape", (std::string("p^") + B + ", number^, number^, number^ -> " + S + ";").c_str(), lh_newCircleShape, nullptr)
		&& ctx.func(m, "newRectangleShape", (std::string("p^") + B + ", number^, number^ -> " + S + ";").c_str(), lh_newRectangleShape, nullptr)
		&& ctx.func(m, "newRectangleShape", (std::string("p^") + B + ", number^, number^, number^, number^ -> " + S + ";").c_str(), lh_newRectangleShape, nullptr)
		&& ctx.func(m, "newRectangleShape", (std::string("p^") + B + ", number^, number^, number^, number^, number^ -> " + S + ";").c_str(), lh_newRectangleShape, nullptr)
		&& ctx.func(m, "newEdgeShape", (std::string("p^") + B + ", number^, number^, number^, number^ -> " + S + ";").c_str(), lh_newEdgeShape, nullptr)
		&& ctx.func(m, "newEdgeShape", (std::string("p^") + B + ", number^, number^, number^, number^, number^, number^, number^, number^ -> " + S + ";").c_str(), lh_newEdgeShape, nullptr)
		&& ctx.func(m, "newPolygonShape", (std::string("p^") + B + ", number^, number^, number^, number^, number^, number^, ... -> " + S + ";").c_str(), lh_newPolygonShape, nullptr)
		&& ctx.func(m, "newChainShape", (std::string("p^") + B + ", bool^, number^, number^, ... -> " + S + ";").c_str(), lh_newChainShape, nullptr)
		&& ctx.func(m, "newDistanceJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^ -> " + J + ";").c_str(), lh_newDistanceJoint, nullptr)
		&& ctx.func(m, "newDistanceJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^, bool^ -> " + J + ";").c_str(), lh_newDistanceJoint, nullptr)
		&& ctx.func(m, "newMouseJoint", (std::string("p^") + B + ", number^, number^ -> " + J + ";").c_str(), lh_newMouseJoint, nullptr)
		&& ctx.func(m, "newRevoluteJoint", (std::string("p^") + B + ", " + B + ", number^, number^ -> " + J + ";").c_str(), lh_newRevoluteJoint, nullptr)
		&& ctx.func(m, "newRevoluteJoint", (std::string("p^") + B + ", " + B + ", number^, number^, bool^ -> " + J + ";").c_str(), lh_newRevoluteJoint, nullptr)
		&& ctx.func(m, "newRevoluteJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^ -> " + J + ";").c_str(), lh_newRevoluteJoint, nullptr)
		&& ctx.func(m, "newRevoluteJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^, bool^ -> " + J + ";").c_str(), lh_newRevoluteJoint, nullptr)
		&& ctx.func(m, "newRevoluteJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^, bool^, number^ -> " + J + ";").c_str(), lh_newRevoluteJoint, nullptr)
		&& ctx.func(m, "newPrismaticJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^ -> " + J + ";").c_str(), lh_newPrismaticJoint, nullptr)
		&& ctx.func(m, "newPrismaticJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^, bool^ -> " + J + ";").c_str(), lh_newPrismaticJoint, nullptr)
		&& ctx.func(m, "newPrismaticJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^, number^, number^ -> " + J + ";").c_str(), lh_newPrismaticJoint, nullptr)
		&& ctx.func(m, "newPrismaticJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^, number^, number^, bool^ -> " + J + ";").c_str(), lh_newPrismaticJoint, nullptr)
		&& ctx.func(m, "newPrismaticJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^, number^, number^, bool^, number^ -> " + J + ";").c_str(), lh_newPrismaticJoint, nullptr)
		&& ctx.func(m, "newPulleyJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^, number^, number^, number^, number^, number^ -> " + J + ";").c_str(), lh_newPulleyJoint, nullptr)
		&& ctx.func(m, "newPulleyJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^, number^, number^, number^, number^, number^, bool^ -> " + J + ";").c_str(), lh_newPulleyJoint, nullptr)
		&& ctx.func(m, "newGearJoint", (std::string("p^") + J + ", " + J + " -> " + J + ";").c_str(), lh_newGearJoint, nullptr)
		&& ctx.func(m, "newGearJoint", (std::string("p^") + J + ", " + J + ", number^ -> " + J + ";").c_str(), lh_newGearJoint, nullptr)
		&& ctx.func(m, "newGearJoint", (std::string("p^") + J + ", " + J + ", number^, bool^ -> " + J + ";").c_str(), lh_newGearJoint, nullptr)
		&& ctx.func(m, "newFrictionJoint", (std::string("p^") + B + ", " + B + ", number^, number^ -> " + J + ";").c_str(), lh_newFrictionJoint, nullptr)
		&& ctx.func(m, "newFrictionJoint", (std::string("p^") + B + ", " + B + ", number^, number^, bool^ -> " + J + ";").c_str(), lh_newFrictionJoint, nullptr)
		&& ctx.func(m, "newFrictionJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^ -> " + J + ";").c_str(), lh_newFrictionJoint, nullptr)
		&& ctx.func(m, "newFrictionJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^, bool^ -> " + J + ";").c_str(), lh_newFrictionJoint, nullptr)
		&& ctx.func(m, "newWeldJoint", (std::string("p^") + B + ", " + B + ", number^, number^ -> " + J + ";").c_str(), lh_newWeldJoint, nullptr)
		&& ctx.func(m, "newWeldJoint", (std::string("p^") + B + ", " + B + ", number^, number^, bool^ -> " + J + ";").c_str(), lh_newWeldJoint, nullptr)
		&& ctx.func(m, "newWeldJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^ -> " + J + ";").c_str(), lh_newWeldJoint, nullptr)
		&& ctx.func(m, "newWeldJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^, bool^ -> " + J + ";").c_str(), lh_newWeldJoint, nullptr)
		&& ctx.func(m, "newWeldJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^, bool^, number^ -> " + J + ";").c_str(), lh_newWeldJoint, nullptr)
		&& ctx.func(m, "newWheelJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^ -> " + J + ";").c_str(), lh_newWheelJoint, nullptr)
		&& ctx.func(m, "newWheelJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^, bool^ -> " + J + ";").c_str(), lh_newWheelJoint, nullptr)
		&& ctx.func(m, "newWheelJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^, number^, number^ -> " + J + ";").c_str(), lh_newWheelJoint, nullptr)
		&& ctx.func(m, "newWheelJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^, number^, number^, bool^ -> " + J + ";").c_str(), lh_newWheelJoint, nullptr)
		&& ctx.func(m, "newRopeJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^, number^ -> " + J + ";").c_str(), lh_newRopeJoint, nullptr)
		&& ctx.func(m, "newRopeJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^, number^, bool^ -> " + J + ";").c_str(), lh_newRopeJoint, nullptr)
		&& ctx.func(m, "newMotorJoint", (std::string("p^") + B + ", " + B + " -> " + J + ";").c_str(), lh_newMotorJoint, nullptr)
		&& ctx.func(m, "newMotorJoint", (std::string("p^") + B + ", " + B + ", number^ -> " + J + ";").c_str(), lh_newMotorJoint, nullptr)
		&& ctx.func(m, "newMotorJoint", (std::string("p^") + B + ", " + B + ", number^, bool^ -> " + J + ";").c_str(), lh_newMotorJoint, nullptr)
		&& ctx.func(m, "getDistance", (std::string("f^") + S + ", " + S + " -> (number^, number^, number^, number^, number^);").c_str(), lh_getDistance, nullptr)
		&& ctx.func(m, "getMeter", "f^ -> number^;", lh_getMeter, nullptr)
		&& ctx.func(m, "setMeter", "p^number^;", lh_setMeter, nullptr)
		&& ctx.func(m, "computeLinearStiffness", (std::string("f^number^, number^, ") + B + " -> (number^, number^);").c_str(), lh_computeLinearStiffness, nullptr)
		&& ctx.func(m, "computeLinearStiffness", (std::string("f^number^, number^, ") + B + ", " + B + " -> (number^, number^);").c_str(), lh_computeLinearStiffness, nullptr)
		&& ctx.func(m, "computeLinearFrequency", (std::string("f^number^, number^, ") + B + " -> (number^, number^);").c_str(), lh_computeLinearFrequency, nullptr)
		&& ctx.func(m, "computeLinearFrequency", (std::string("f^number^, number^, ") + B + ", " + B + " -> (number^, number^);").c_str(), lh_computeLinearFrequency, nullptr)
		&& ctx.func(m, "computeAngularStiffness", (std::string("f^number^, number^, ") + B + " -> (number^, number^);").c_str(), lh_computeAngularStiffness, nullptr)
		&& ctx.func(m, "computeAngularStiffness", (std::string("f^number^, number^, ") + B + ", " + B + " -> (number^, number^);").c_str(), lh_computeAngularStiffness, nullptr)
		&& ctx.func(m, "computeAngularFrequency", (std::string("f^number^, number^, ") + B + " -> (number^, number^);").c_str(), lh_computeAngularFrequency, nullptr)
		&& ctx.func(m, "computeAngularFrequency", (std::string("f^number^, number^, ") + B + ", " + B + " -> (number^, number^);").c_str(), lh_computeAngularFrequency, nullptr);
}

} // lh
} // love
