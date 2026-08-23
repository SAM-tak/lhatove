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

// love.physics.Contact for L^. The reference is wrap_Contact.cpp beside
// this file. A Contact is only valid while Box2D keeps the pair touching;
// every member but isDestroyed raises on a stale one.

#include "lh_Physics.h"

namespace love
{
namespace physics
{
namespace box2d
{

static Contact *self(LhatMachine *machine, const LhatValue *args, size_t count)
{
	Contact *contact = checkContact(machine, args, count, 0);
	if (contact != nullptr && !contact->isValid())
	{
		lh::raise(machine, "Attempt to use destroyed contact.");
		return nullptr;
	}
	return contact;
}

#define CONTACT_SELF() Contact *c = self(machine, args, count); if (c == nullptr) return lhat_nil()

#define CONTACT_NUMBER(name, expr) \
	static LhatValue lh_Contact_##name(LhatMachine *machine, void *context, const LhatValue *args, size_t count) \
	{ \
		(void) context; \
		CONTACT_SELF(); \
		return lhat_real(expr); \
	}

#define CONTACT_BOOL(name, expr) \
	static LhatValue lh_Contact_##name(LhatMachine *machine, void *context, const LhatValue *args, size_t count) \
	{ \
		(void) context; \
		CONTACT_SELF(); \
		return lhat_bool(expr); \
	}

#define CONTACT_SET(name, call) \
	static LhatValue lh_Contact_##name(LhatMachine *machine, void *context, const LhatValue *args, size_t count) \
	{ \
		(void) context; \
		CONTACT_SELF(); \
		float v = numberAt(args, count, 1); \
		bool flag = boolAt(args, count, 1); \
		(void) v; (void) flag; \
		c->call; \
		return lhat_nil(); \
	}

CONTACT_NUMBER(getFriction, c->getFriction())
CONTACT_NUMBER(getRestitution, c->getRestitution())
CONTACT_BOOL(isEnabled, c->isEnabled())
CONTACT_BOOL(isTouching, c->isTouching())
CONTACT_SET(setFriction, setFriction(v))
CONTACT_SET(setRestitution, setRestitution(v))
CONTACT_SET(setEnabled, setEnabled(flag))
CONTACT_SET(resetFriction, resetFriction())
CONTACT_SET(resetRestitution, resetRestitution())
CONTACT_SET(setTangentSpeed, setTangentSpeed(v))
CONTACT_NUMBER(getTangentSpeed, c->getTangentSpeed())

// getPositions() -> t^{...:number^}: the contact points as x, y pairs.
static LhatValue lh_Contact_getPositions(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	CONTACT_SELF();
	return numberList(machine, c->getPositions());
}

static LhatValue lh_Contact_getNormal(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	CONTACT_SELF();
	float out[2];
	c->getNormal(out[0], out[1]);
	return numbers(machine, out, 2);
}

// getChildren() -> (childA, childB), 1-based.
static LhatValue lh_Contact_getChildren(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	CONTACT_SELF();
	int a = 0, b = 0;
	c->getChildren(a, b);
	LhatValue items[2] = {lhat_integer(a + 1), lhat_integer(b + 1)};
	LhatValue out = lhat_nil();
	lh::makeTuple(machine, items, 2, &out);
	return out;
}

static LhatValue lh_Contact_getShapes(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	CONTACT_SELF();
	return lh::guard(machine, [&]() {
		Shape *a = nullptr, *b = nullptr;
		c->getShapes(a, b);
		LhatValue pair[2] = {pushShape(machine, a), pushShape(machine, b)};
		LhatValue out = lhat_nil();
		lh::makeTuple(machine, pair, 2, &out);
		return out;
	});
}

static LhatValue lh_Contact_isDestroyed(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	Contact *c = checkContact(machine, args, count, 0);
	if (c == nullptr)
		return lhat_nil();
	return lhat_bool(!c->isValid());
}

bool lhPhysicsContact(lh::Context &ctx)
{
	const char *m = LH_PHYSICS;
	if (!ctx.objectType(m, "Contact", Contact::type))
		return false;
	if (ctx.types())
		return true;

	const char *C = "Contact";
	return ctx.member(m, C, "getPositions", "f^self^ -> t^{...:number^};", lh_Contact_getPositions, nullptr)
		&& ctx.member(m, C, "getNormal", "f^self^ -> (number^, number^);", lh_Contact_getNormal, nullptr)
		&& ctx.member(m, C, "getFriction", "f^self^ -> number^;", lh_Contact_getFriction, nullptr)
		&& ctx.member(m, C, "getRestitution", "f^self^ -> number^;", lh_Contact_getRestitution, nullptr)
		&& ctx.member(m, C, "isEnabled", "f^self^ -> bool^;", lh_Contact_isEnabled, nullptr)
		&& ctx.member(m, C, "isTouching", "f^self^ -> bool^;", lh_Contact_isTouching, nullptr)
		&& ctx.member(m, C, "setFriction", "p^self^, number^;", lh_Contact_setFriction, nullptr)
		&& ctx.member(m, C, "setRestitution", "p^self^, number^;", lh_Contact_setRestitution, nullptr)
		&& ctx.member(m, C, "setEnabled", "p^self^, bool^;", lh_Contact_setEnabled, nullptr)
		&& ctx.member(m, C, "resetFriction", "p^self^;", lh_Contact_resetFriction, nullptr)
		&& ctx.member(m, C, "resetRestitution", "p^self^;", lh_Contact_resetRestitution, nullptr)
		&& ctx.member(m, C, "setTangentSpeed", "p^self^, number^;", lh_Contact_setTangentSpeed, nullptr)
		&& ctx.member(m, C, "getTangentSpeed", "f^self^ -> number^;", lh_Contact_getTangentSpeed, nullptr)
		&& ctx.member(m, C, "getChildren", "f^self^ -> (number^, number^);", lh_Contact_getChildren, nullptr)
		&& ctx.member(m, C, "getShapes", "p^self^ -> (love.physics.Shape, love.physics.Shape);", lh_Contact_getShapes, nullptr)
		&& ctx.member(m, C, "isDestroyed", "f^self^ -> bool^;", lh_Contact_isDestroyed, nullptr);
}

} // box2d
} // physics
} // love
