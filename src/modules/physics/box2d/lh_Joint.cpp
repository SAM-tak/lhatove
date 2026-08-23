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

// love.physics.Joint for L^: one hostdata type for every joint kind. The
// references are wrap_Joint.cpp and the wrap_*Joint.cpp files beside this
// file. A member a kind does not have raises when called on it; getType
// tells the kinds apart. Members several kinds share (setMotorSpeed,
// setStiffness, getLimits ...) dispatch on the kind.

#include "lh_Physics.h"

#include "DistanceJoint.h"
#include "FrictionJoint.h"
#include "GearJoint.h"
#include "MotorJoint.h"
#include "MouseJoint.h"
#include "PrismaticJoint.h"
#include "PulleyJoint.h"
#include "RevoluteJoint.h"
#include "RopeJoint.h"
#include "WeldJoint.h"
#include "WheelJoint.h"

namespace love
{
namespace physics
{
namespace box2d
{

static Joint *self(LhatMachine *machine, const LhatValue *args, size_t count)
{
	Joint *joint = checkJoint(machine, args, count, 0);
	if (joint != nullptr && !joint->isValid())
	{
		lh::raise(machine, "Attempt to use destroyed joint.");
		return nullptr;
	}
	return joint;
}

#define JOINT_SELF() Joint *j = self(machine, args, count); if (j == nullptr) return lhat_nil()

static LhatValue notAMember(LhatMachine *machine, Joint *j, const char *name)
{
	const char *have = "";
	Joint::getConstant(j->getType(), have);
	return lh::raise(machine, std::string(name) + " is not a member of a " + have + " joint.");
}

// The kind-dispatched accessors. Each case names the joint kind and the
// expression on `t`, the joint cast to that kind.
#define KIND(Kind, Type, expr) case Joint::Type: { Kind *t = (Kind *) j; (void) t; return expr; }

#define JOINT_GET_NUMBER(name, cases) \
	static LhatValue lh_Joint_##name(LhatMachine *machine, void *context, const LhatValue *args, size_t count) \
	{ \
		(void) context; \
		JOINT_SELF(); \
		float v = numberAt(args, count, 1); \
		(void) v; \
		return lh::guard(machine, [&]() -> LhatValue { \
			switch (j->getType()) \
			{ \
			cases \
			default: return notAMember(machine, j, #name); \
			} \
		}); \
	}

#define JOINT_SET(name, cases) \
	static LhatValue lh_Joint_##name(LhatMachine *machine, void *context, const LhatValue *args, size_t count) \
	{ \
		(void) context; \
		JOINT_SELF(); \
		float v = numberAt(args, count, 1); \
		float v2 = numberAt(args, count, 2); \
		bool flag = boolAt(args, count, 1); \
		(void) v; (void) v2; (void) flag; \
		return lh::guard(machine, [&]() -> LhatValue { \
			switch (j->getType()) \
			{ \
			cases \
			default: return notAMember(machine, j, #name); \
			} \
		}); \
	}

#define JOINT_PAIR(name, cases) \
	static LhatValue lh_Joint_##name(LhatMachine *machine, void *context, const LhatValue *args, size_t count) \
	{ \
		(void) context; \
		JOINT_SELF(); \
		return lh::guard(machine, [&]() -> LhatValue { \
			float out[2] = {0, 0}; \
			switch (j->getType()) \
			{ \
			cases \
			default: return notAMember(machine, j, #name); \
			} \
			return numbers(machine, out, 2); \
		}); \
	}

#define PAIR(Kind, Type, call) case Joint::Type: { Kind *t = (Kind *) j; t->call; break; }
#define SET(Kind, Type, call) case Joint::Type: { Kind *t = (Kind *) j; t->call; return lhat_nil(); }

// --- the base members ---

static LhatValue lh_Joint_getType(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	JOINT_SELF();
	const char *type = "";
	Joint::getConstant(j->getType(), type);
	LhatValue out = lhat_nil();
	lh::makeString(machine, type, &out);
	return out;
}

static LhatValue lh_Joint_getBodies(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	JOINT_SELF();
	return lh::guard(machine, [&]() {
		LhatValue pair[2] = {pushBody(machine, j->getBodyA()), pushBody(machine, j->getBodyB())};
		LhatValue out = lhat_nil();
		lh::makeTuple(machine, pair, 2, &out);
		return out;
	});
}

static LhatValue lh_Joint_getAnchors(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	JOINT_SELF();
	float out[4];
	j->getAnchors(out[0], out[1], out[2], out[3]);
	return numbers(machine, out, 4);
}

static LhatValue lh_Joint_getReactionForce(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	JOINT_SELF();
	float out[2];
	j->getReactionForce(numberAt(args, count, 1), out[0], out[1]);
	return numbers(machine, out, 2);
}

static LhatValue lh_Joint_getReactionTorque(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	JOINT_SELF();
	return lhat_real(j->getReactionTorque(numberAt(args, count, 1)));
}

static LhatValue lh_Joint_getCollideConnected(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	JOINT_SELF();
	return lhat_bool(j->getCollideConnected());
}

static LhatValue lh_Joint_setUserData(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	JOINT_SELF();
	if (count < 2 || lhat_is_nil(args[1]))
		j->setUserData(nullptr);
	else
	{
		StrongRef<lh::Parked> parked(new lh::Parked(lh::ParkingLot::lotOf(machine), args[1]), Acquire::NORETAIN);
		j->setUserData(parked.get());
	}
	return lhat_nil();
}

static LhatValue lh_Joint_getUserData(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	JOINT_SELF();
	lh::Parked *parked = dynamic_cast<lh::Parked *>(j->getUserData());
	return parked != nullptr ? parked->get() : lhat_nil();
}

static LhatValue lh_Joint_destroy(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	JOINT_SELF();
	return lh::guard(machine, [&]() {
		j->destroyJoint();
		return lhat_nil();
	});
}

static LhatValue lh_Joint_isDestroyed(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	Joint *j = checkJoint(machine, args, count, 0);
	if (j == nullptr)
		return lhat_nil();
	return lhat_bool(!j->isValid());
}

// --- members several kinds share, dispatched on the kind ---

JOINT_GET_NUMBER(getLength,
	KIND(DistanceJoint, JOINT_DISTANCE, lhat_real(t->getLength())))
JOINT_SET(setLength,
	SET(DistanceJoint, JOINT_DISTANCE, setLength(v)))

JOINT_GET_NUMBER(getStiffness,
	KIND(DistanceJoint, JOINT_DISTANCE, lhat_real(t->getStiffness()))
	KIND(MouseJoint, JOINT_MOUSE, lhat_real(t->getStiffness()))
	KIND(WeldJoint, JOINT_WELD, lhat_real(t->getStiffness()))
	KIND(WheelJoint, JOINT_WHEEL, lhat_real(t->getStiffness())))
JOINT_SET(setStiffness,
	SET(DistanceJoint, JOINT_DISTANCE, setStiffness(v))
	SET(MouseJoint, JOINT_MOUSE, setStiffness(v))
	SET(WeldJoint, JOINT_WELD, setStiffness(v))
	SET(WheelJoint, JOINT_WHEEL, setStiffness(v)))

JOINT_GET_NUMBER(getDamping,
	KIND(DistanceJoint, JOINT_DISTANCE, lhat_real(t->getDamping()))
	KIND(MouseJoint, JOINT_MOUSE, lhat_real(t->getDamping()))
	KIND(WeldJoint, JOINT_WELD, lhat_real(t->getDamping()))
	KIND(WheelJoint, JOINT_WHEEL, lhat_real(t->getDamping())))
JOINT_SET(setDamping,
	SET(DistanceJoint, JOINT_DISTANCE, setDamping(v))
	SET(MouseJoint, JOINT_MOUSE, setDamping(v))
	SET(WeldJoint, JOINT_WELD, setDamping(v))
	SET(WheelJoint, JOINT_WHEEL, setDamping(v)))

JOINT_GET_NUMBER(getMaxForce,
	KIND(FrictionJoint, JOINT_FRICTION, lhat_real(t->getMaxForce()))
	KIND(MotorJoint, JOINT_MOTOR, lhat_real(t->getMaxForce()))
	KIND(MouseJoint, JOINT_MOUSE, lhat_real(t->getMaxForce())))
JOINT_SET(setMaxForce,
	SET(FrictionJoint, JOINT_FRICTION, setMaxForce(v))
	SET(MotorJoint, JOINT_MOTOR, setMaxForce(v))
	SET(MouseJoint, JOINT_MOUSE, setMaxForce(v)))

JOINT_GET_NUMBER(getMaxTorque,
	KIND(FrictionJoint, JOINT_FRICTION, lhat_real(t->getMaxTorque()))
	KIND(MotorJoint, JOINT_MOTOR, lhat_real(t->getMaxTorque())))
JOINT_SET(setMaxTorque,
	SET(FrictionJoint, JOINT_FRICTION, setMaxTorque(v))
	SET(MotorJoint, JOINT_MOTOR, setMaxTorque(v)))

JOINT_GET_NUMBER(getRatio,
	KIND(GearJoint, JOINT_GEAR, lhat_real(t->getRatio()))
	KIND(PulleyJoint, JOINT_PULLEY, lhat_real(t->getRatio())))
JOINT_SET(setRatio,
	SET(GearJoint, JOINT_GEAR, setRatio(v)))

static LhatValue lh_Joint_getJoints(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	JOINT_SELF();
	if (j->getType() != Joint::JOINT_GEAR)
		return notAMember(machine, j, "getJoints");
	GearJoint *t = (GearJoint *) j;
	return lh::guard(machine, [&]() {
		LhatValue pair[2] = {pushJoint(machine, t->getJointA()), pushJoint(machine, t->getJointB())};
		LhatValue out = lhat_nil();
		lh::makeTuple(machine, pair, 2, &out);
		return out;
	});
}

JOINT_PAIR(getLinearOffset,
	PAIR(MotorJoint, JOINT_MOTOR, getLinearOffset(out[0], out[1])))
JOINT_SET(setLinearOffset,
	SET(MotorJoint, JOINT_MOTOR, setLinearOffset(v, v2)))
JOINT_GET_NUMBER(getAngularOffset,
	KIND(MotorJoint, JOINT_MOTOR, lhat_real(t->getAngularOffset())))
JOINT_SET(setAngularOffset,
	SET(MotorJoint, JOINT_MOTOR, setAngularOffset(v)))
JOINT_GET_NUMBER(getCorrectionFactor,
	KIND(MotorJoint, JOINT_MOTOR, lhat_real(t->getCorrectionFactor())))
JOINT_SET(setCorrectionFactor,
	SET(MotorJoint, JOINT_MOTOR, setCorrectionFactor(v)))

JOINT_PAIR(getTarget,
	PAIR(MouseJoint, JOINT_MOUSE, getTarget(out[0], out[1])))
JOINT_SET(setTarget,
	SET(MouseJoint, JOINT_MOUSE, setTarget(v, v2)))

JOINT_GET_NUMBER(getJointTranslation,
	KIND(PrismaticJoint, JOINT_PRISMATIC, lhat_real(t->getJointTranslation()))
	KIND(WheelJoint, JOINT_WHEEL, lhat_real(t->getJointTranslation())))
JOINT_GET_NUMBER(getJointSpeed,
	KIND(PrismaticJoint, JOINT_PRISMATIC, lhat_real(t->getJointSpeed()))
	KIND(RevoluteJoint, JOINT_REVOLUTE, lhat_real(t->getJointSpeed()))
	KIND(WheelJoint, JOINT_WHEEL, lhat_real(t->getJointSpeed())))
JOINT_GET_NUMBER(getJointAngle,
	KIND(RevoluteJoint, JOINT_REVOLUTE, lhat_real(t->getJointAngle())))

JOINT_SET(setMotorEnabled,
	SET(PrismaticJoint, JOINT_PRISMATIC, setMotorEnabled(flag))
	SET(RevoluteJoint, JOINT_REVOLUTE, setMotorEnabled(flag))
	SET(WheelJoint, JOINT_WHEEL, setMotorEnabled(flag)))
JOINT_GET_NUMBER(isMotorEnabled,
	KIND(PrismaticJoint, JOINT_PRISMATIC, lhat_bool(t->isMotorEnabled()))
	KIND(RevoluteJoint, JOINT_REVOLUTE, lhat_bool(t->isMotorEnabled()))
	KIND(WheelJoint, JOINT_WHEEL, lhat_bool(t->isMotorEnabled())))
JOINT_SET(setMotorSpeed,
	SET(PrismaticJoint, JOINT_PRISMATIC, setMotorSpeed(v))
	SET(RevoluteJoint, JOINT_REVOLUTE, setMotorSpeed(v))
	SET(WheelJoint, JOINT_WHEEL, setMotorSpeed(v)))
JOINT_GET_NUMBER(getMotorSpeed,
	KIND(PrismaticJoint, JOINT_PRISMATIC, lhat_real(t->getMotorSpeed()))
	KIND(RevoluteJoint, JOINT_REVOLUTE, lhat_real(t->getMotorSpeed()))
	KIND(WheelJoint, JOINT_WHEEL, lhat_real(t->getMotorSpeed())))

JOINT_SET(setMaxMotorForce,
	SET(PrismaticJoint, JOINT_PRISMATIC, setMaxMotorForce(v)))
JOINT_GET_NUMBER(getMaxMotorForce,
	KIND(PrismaticJoint, JOINT_PRISMATIC, lhat_real(t->getMaxMotorForce())))
JOINT_GET_NUMBER(getMotorForce,
	KIND(PrismaticJoint, JOINT_PRISMATIC, lhat_real(t->getMotorForce(v))))

JOINT_SET(setMaxMotorTorque,
	SET(RevoluteJoint, JOINT_REVOLUTE, setMaxMotorTorque(v))
	SET(WheelJoint, JOINT_WHEEL, setMaxMotorTorque(v)))
JOINT_GET_NUMBER(getMaxMotorTorque,
	KIND(RevoluteJoint, JOINT_REVOLUTE, lhat_real(t->getMaxMotorTorque()))
	KIND(WheelJoint, JOINT_WHEEL, lhat_real(t->getMaxMotorTorque())))
JOINT_GET_NUMBER(getMotorTorque,
	KIND(RevoluteJoint, JOINT_REVOLUTE, lhat_real(t->getMotorTorque(v)))
	KIND(WheelJoint, JOINT_WHEEL, lhat_real(t->getMotorTorque(v))))

JOINT_SET(setLimitsEnabled,
	SET(PrismaticJoint, JOINT_PRISMATIC, setLimitsEnabled(flag))
	SET(RevoluteJoint, JOINT_REVOLUTE, setLimitsEnabled(flag)))
JOINT_GET_NUMBER(areLimitsEnabled,
	KIND(PrismaticJoint, JOINT_PRISMATIC, lhat_bool(t->areLimitsEnabled()))
	KIND(RevoluteJoint, JOINT_REVOLUTE, lhat_bool(t->areLimitsEnabled())))
JOINT_SET(setUpperLimit,
	SET(PrismaticJoint, JOINT_PRISMATIC, setUpperLimit(v))
	SET(RevoluteJoint, JOINT_REVOLUTE, setUpperLimit(v)))
JOINT_SET(setLowerLimit,
	SET(PrismaticJoint, JOINT_PRISMATIC, setLowerLimit(v))
	SET(RevoluteJoint, JOINT_REVOLUTE, setLowerLimit(v)))
JOINT_SET(setLimits,
	SET(PrismaticJoint, JOINT_PRISMATIC, setLimits(v, v2))
	SET(RevoluteJoint, JOINT_REVOLUTE, setLimits(v, v2)))
JOINT_GET_NUMBER(getLowerLimit,
	KIND(PrismaticJoint, JOINT_PRISMATIC, lhat_real(t->getLowerLimit()))
	KIND(RevoluteJoint, JOINT_REVOLUTE, lhat_real(t->getLowerLimit())))
JOINT_GET_NUMBER(getUpperLimit,
	KIND(PrismaticJoint, JOINT_PRISMATIC, lhat_real(t->getUpperLimit()))
	KIND(RevoluteJoint, JOINT_REVOLUTE, lhat_real(t->getUpperLimit())))
JOINT_PAIR(getLimits,
	PAIR(PrismaticJoint, JOINT_PRISMATIC, getLimits(out[0], out[1]))
	PAIR(RevoluteJoint, JOINT_REVOLUTE, getLimits(out[0], out[1])))

JOINT_PAIR(getAxis,
	PAIR(PrismaticJoint, JOINT_PRISMATIC, getAxis(out[0], out[1]))
	PAIR(WheelJoint, JOINT_WHEEL, getAxis(out[0], out[1])))

JOINT_GET_NUMBER(getReferenceAngle,
	KIND(PrismaticJoint, JOINT_PRISMATIC, lhat_real(t->getReferenceAngle()))
	KIND(RevoluteJoint, JOINT_REVOLUTE, lhat_real(t->getReferenceAngle()))
	KIND(WeldJoint, JOINT_WELD, lhat_real(t->getReferenceAngle())))

static LhatValue lh_Joint_getGroundAnchors(LhatMachine *machine, void *context, const LhatValue *args, size_t count)
{
	(void) context;
	JOINT_SELF();
	if (j->getType() != Joint::JOINT_PULLEY)
		return notAMember(machine, j, "getGroundAnchors");
	float out[4];
	((PulleyJoint *) j)->getGroundAnchors(out[0], out[1], out[2], out[3]);
	return numbers(machine, out, 4);
}

JOINT_GET_NUMBER(getLengthA,
	KIND(PulleyJoint, JOINT_PULLEY, lhat_real(t->getLengthA())))
JOINT_GET_NUMBER(getLengthB,
	KIND(PulleyJoint, JOINT_PULLEY, lhat_real(t->getLengthB())))

JOINT_GET_NUMBER(getMaxLength,
	KIND(RopeJoint, JOINT_ROPE, lhat_real(t->getMaxLength())))
JOINT_SET(setMaxLength,
	SET(RopeJoint, JOINT_ROPE, setMaxLength(v)))

bool lhPhysicsJoint(lh::Context &ctx)
{
	const char *m = LH_PHYSICS;
	if (!ctx.objectType(m, "Joint", Joint::type))
		return false;
	if (ctx.types())
		return true;

	const char *J = "Joint";
	const char *num = "f^self^ -> number^;";
	const char *flag = "f^self^ -> bool^;";
	const char *setNum = "p^self^, number^;";
	const char *setFlag = "p^self^, bool^;";
	const char *setPair = "p^self^, number^, number^;";
	const char *pair = "f^self^ -> (number^, number^);";
	const char *numOfNum = "f^self^, number^ -> number^;";

	return ctx.member(m, J, "getType", "f^self^ -> string^;", lh_Joint_getType, nullptr)
		&& ctx.member(m, J, "getBodies", "p^self^ -> (love.physics.Body, love.physics.Body);", lh_Joint_getBodies, nullptr)
		&& ctx.member(m, J, "getAnchors", "f^self^ -> (number^, number^, number^, number^);", lh_Joint_getAnchors, nullptr)
		&& ctx.member(m, J, "getReactionForce", "f^self^, number^ -> (number^, number^);", lh_Joint_getReactionForce, nullptr)
		&& ctx.member(m, J, "getReactionTorque", numOfNum, lh_Joint_getReactionTorque, nullptr)
		&& ctx.member(m, J, "getCollideConnected", flag, lh_Joint_getCollideConnected, nullptr)
		&& ctx.member(m, J, "setUserData", "p^self^, any^;", lh_Joint_setUserData, nullptr)
		&& ctx.member(m, J, "getUserData", "f^self^ -> any^;", lh_Joint_getUserData, nullptr)
		&& ctx.member(m, J, "destroy", "p^self^;", lh_Joint_destroy, nullptr)
		&& ctx.member(m, J, "isDestroyed", flag, lh_Joint_isDestroyed, nullptr)
		// DistanceJoint
		&& ctx.member(m, J, "getLength", num, lh_Joint_getLength, nullptr)
		&& ctx.member(m, J, "setLength", setNum, lh_Joint_setLength, nullptr)
		// Distance / Mouse / Weld / Wheel
		&& ctx.member(m, J, "getStiffness", num, lh_Joint_getStiffness, nullptr)
		&& ctx.member(m, J, "setStiffness", setNum, lh_Joint_setStiffness, nullptr)
		&& ctx.member(m, J, "getDamping", num, lh_Joint_getDamping, nullptr)
		&& ctx.member(m, J, "setDamping", setNum, lh_Joint_setDamping, nullptr)
		// Friction / Motor / Mouse
		&& ctx.member(m, J, "getMaxForce", num, lh_Joint_getMaxForce, nullptr)
		&& ctx.member(m, J, "setMaxForce", setNum, lh_Joint_setMaxForce, nullptr)
		&& ctx.member(m, J, "getMaxTorque", num, lh_Joint_getMaxTorque, nullptr)
		&& ctx.member(m, J, "setMaxTorque", setNum, lh_Joint_setMaxTorque, nullptr)
		// Gear / Pulley
		&& ctx.member(m, J, "getRatio", num, lh_Joint_getRatio, nullptr)
		&& ctx.member(m, J, "setRatio", setNum, lh_Joint_setRatio, nullptr)
		&& ctx.member(m, J, "getJoints", "p^self^ -> (love.physics.Joint, love.physics.Joint);", lh_Joint_getJoints, nullptr)
		// MotorJoint
		&& ctx.member(m, J, "getLinearOffset", pair, lh_Joint_getLinearOffset, nullptr)
		&& ctx.member(m, J, "setLinearOffset", setPair, lh_Joint_setLinearOffset, nullptr)
		&& ctx.member(m, J, "getAngularOffset", num, lh_Joint_getAngularOffset, nullptr)
		&& ctx.member(m, J, "setAngularOffset", setNum, lh_Joint_setAngularOffset, nullptr)
		&& ctx.member(m, J, "getCorrectionFactor", num, lh_Joint_getCorrectionFactor, nullptr)
		&& ctx.member(m, J, "setCorrectionFactor", setNum, lh_Joint_setCorrectionFactor, nullptr)
		// MouseJoint
		&& ctx.member(m, J, "getTarget", pair, lh_Joint_getTarget, nullptr)
		&& ctx.member(m, J, "setTarget", setPair, lh_Joint_setTarget, nullptr)
		// Prismatic / Revolute / Wheel
		&& ctx.member(m, J, "getJointTranslation", num, lh_Joint_getJointTranslation, nullptr)
		&& ctx.member(m, J, "getJointSpeed", num, lh_Joint_getJointSpeed, nullptr)
		&& ctx.member(m, J, "getJointAngle", num, lh_Joint_getJointAngle, nullptr)
		&& ctx.member(m, J, "setMotorEnabled", setFlag, lh_Joint_setMotorEnabled, nullptr)
		&& ctx.member(m, J, "isMotorEnabled", flag, lh_Joint_isMotorEnabled, nullptr)
		&& ctx.member(m, J, "setMotorSpeed", setNum, lh_Joint_setMotorSpeed, nullptr)
		&& ctx.member(m, J, "getMotorSpeed", num, lh_Joint_getMotorSpeed, nullptr)
		&& ctx.member(m, J, "setMaxMotorForce", setNum, lh_Joint_setMaxMotorForce, nullptr)
		&& ctx.member(m, J, "getMaxMotorForce", num, lh_Joint_getMaxMotorForce, nullptr)
		&& ctx.member(m, J, "getMotorForce", numOfNum, lh_Joint_getMotorForce, nullptr)
		&& ctx.member(m, J, "setMaxMotorTorque", setNum, lh_Joint_setMaxMotorTorque, nullptr)
		&& ctx.member(m, J, "getMaxMotorTorque", num, lh_Joint_getMaxMotorTorque, nullptr)
		&& ctx.member(m, J, "getMotorTorque", numOfNum, lh_Joint_getMotorTorque, nullptr)
		&& ctx.member(m, J, "setLimitsEnabled", setFlag, lh_Joint_setLimitsEnabled, nullptr)
		&& ctx.member(m, J, "areLimitsEnabled", flag, lh_Joint_areLimitsEnabled, nullptr)
		&& ctx.member(m, J, "setUpperLimit", setNum, lh_Joint_setUpperLimit, nullptr)
		&& ctx.member(m, J, "setLowerLimit", setNum, lh_Joint_setLowerLimit, nullptr)
		&& ctx.member(m, J, "setLimits", setPair, lh_Joint_setLimits, nullptr)
		&& ctx.member(m, J, "getLowerLimit", num, lh_Joint_getLowerLimit, nullptr)
		&& ctx.member(m, J, "getUpperLimit", num, lh_Joint_getUpperLimit, nullptr)
		&& ctx.member(m, J, "getLimits", pair, lh_Joint_getLimits, nullptr)
		&& ctx.member(m, J, "getAxis", pair, lh_Joint_getAxis, nullptr)
		&& ctx.member(m, J, "getReferenceAngle", num, lh_Joint_getReferenceAngle, nullptr)
		// PulleyJoint
		&& ctx.member(m, J, "getGroundAnchors", "f^self^ -> (number^, number^, number^, number^);", lh_Joint_getGroundAnchors, nullptr)
		&& ctx.member(m, J, "getLengthA", num, lh_Joint_getLengthA, nullptr)
		&& ctx.member(m, J, "getLengthB", num, lh_Joint_getLengthB, nullptr)
		// RopeJoint
		&& ctx.member(m, J, "getMaxLength", num, lh_Joint_getMaxLength, nullptr)
		&& ctx.member(m, J, "setMaxLength", setNum, lh_Joint_setMaxLength, nullptr);
}

} // box2d
} // physics
} // love
