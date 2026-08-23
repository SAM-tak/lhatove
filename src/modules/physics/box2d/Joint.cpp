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

#include "Joint.h"

// Module
#include "Body.h"
#include "World.h"
#include "Physics.h"


namespace love
{
namespace physics
{
namespace box2d
{

Joint::Joint(Body *body1)
	: world(body1->world)
	, body1(body1)
	, body2(nullptr)
{
}

Joint::Joint(Body *body1, Body *body2)
	: world(body1->world)
	, body1(body1)
	, body2(body2)
{
}

Joint::~Joint()
{
}

Joint::Type Joint::getType() const
{
	switch (joint->GetType())
	{
	case e_revoluteJoint:
		return JOINT_REVOLUTE;
	case e_prismaticJoint:
		return JOINT_PRISMATIC;
	case e_distanceJoint:
		return JOINT_DISTANCE;
	case e_pulleyJoint:
		return JOINT_PULLEY;
	case e_mouseJoint:
		return JOINT_MOUSE;
	case e_gearJoint:
		return JOINT_GEAR;
	case e_frictionJoint:
		return JOINT_FRICTION;
	case e_weldJoint:
		return JOINT_WELD;
	case e_wheelJoint:
		return JOINT_WHEEL;
	case e_ropeJoint:
		return JOINT_ROPE;
	case e_motorJoint:
		return JOINT_MOTOR;
	default:
		return JOINT_INVALID;
	}
}

Body *Joint::getBodyA() const
{
	b2Body *b2body = joint->GetBodyA();
	if (b2body == nullptr)
		return nullptr;

	Body *body = (Body *) (b2body->GetUserData().pointer);
	if (body == nullptr)
		throw love::Exception("A body has escaped Memoizer!");

	return body;
}

Body *Joint::getBodyB() const
{
	b2Body *b2body = joint->GetBodyB();
	if (b2body == nullptr)
		return nullptr;

	Body *body = (Body *) (b2body->GetUserData().pointer);
	if (body == nullptr)
		throw love::Exception("A body has escaped Memoizer!");

	return body;
}

bool Joint::isValid() const
{
	return joint != nullptr;
}

void Joint::getAnchors(float &x1, float &y1, float &x2, float &y2) const
{
	b2Vec2 a = Physics::scaleUp(joint->GetAnchorA());
	b2Vec2 b = Physics::scaleUp(joint->GetAnchorB());
	x1 = a.x;
	y1 = a.y;
	x2 = b.x;
	y2 = b.y;
}

void Joint::getReactionForce(float dt, float &x, float &y) const
{
	b2Vec2 v = Physics::scaleUp(joint->GetReactionForce(dt));
	x = v.x;
	y = v.y;
}

float Joint::getReactionTorque(float dt)
{
	return Physics::scaleUp(Physics::scaleUp(joint->GetReactionTorque(dt)));
}

b2Joint *Joint::createJoint(b2JointDef *def)
{
	def->userData.pointer = (uintptr_t)this;
	joint = world->world->CreateJoint(def);
	// Box2D joint has a reference to this love Joint.
	this->retain();
	return joint;
}

void Joint::destroyJoint(bool implicit)
{
	if (world->world->IsLocked())
	{
		// Called during time step. Save reference for destruction afterwards.
		this->retain();
		world->destructJoints.push_back(this);
		return;
	}

	if (!implicit && joint != nullptr)
		world->world->DestroyJoint(joint);
	joint = nullptr;

	// Drop the host data so it does not outlive the joint.
	userdata.set(nullptr);

	// Release the reference of the Box2D joint.
	this->release();
}

bool Joint::isEnabled() const
{
	return joint->IsEnabled();
}

bool Joint::getCollideConnected() const
{
	return joint->GetCollideConnected();
}

void Joint::setUserData(love::Object *data)
{
	userdata.set(data);
}

love::Object *Joint::getUserData() const
{
	return userdata.get();
}

} // box2d
} // physics
} // love
