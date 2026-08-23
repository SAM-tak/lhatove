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

#include "Body.h"

#include "common/math.h"

#include "Shape.h"
#include "World.h"
#include "Physics.h"

#include "Joint.h"
#include "Contact.h"

namespace love
{
namespace physics
{
namespace box2d
{

Body::Body(World *world, b2Vec2 p, Body::Type type)
	: world(world)
	, hasCustomMass(false)
{
	b2BodyDef def;
	def.position = Physics::scaleDown(p);
	def.userData.pointer = (uintptr_t)this;
	body = world->world->CreateBody(&def);
	// Box2D body holds a reference to the love Body.
	this->retain();
	this->setType(type);
}

Body::~Body()
{
}

float Body::getX()
{
	return Physics::scaleUp(body->GetPosition().x);
}

float Body::getY()
{
	return Physics::scaleUp(body->GetPosition().y);
}

void Body::getPosition(float &x_o, float &y_o)
{
	b2Vec2 v = Physics::scaleUp(body->GetPosition());
	x_o = v.x;
	y_o = v.y;
}

void Body::getLinearVelocity(float &x_o, float &y_o)
{
	b2Vec2 v = Physics::scaleUp(body->GetLinearVelocity());
	x_o = v.x;
	y_o = v.y;
}

float Body::getAngle()
{
	return body->GetAngle();
}

void Body::getWorldCenter(float &x_o, float &y_o)
{
	b2Vec2 v = Physics::scaleUp(body->GetWorldCenter());
	x_o = v.x;
	y_o = v.y;
}

void Body::getLocalCenter(float &x_o, float &y_o)
{
	b2Vec2 v = Physics::scaleUp(body->GetLocalCenter());
	x_o = v.x;
	y_o = v.y;
}

float Body::getAngularVelocity() const
{
	return body->GetAngularVelocity();
}

void Body::getKinematicState(b2Vec2 &pos_o, float &a_o, b2Vec2 &vel_o, float &da_o) const
{
	pos_o = Physics::scaleUp(body->GetPosition());
	a_o = body->GetAngle();
	vel_o = Physics::scaleUp(body->GetLinearVelocity());
	da_o = body->GetAngularVelocity();
}

float Body::getMass() const
{
	return body->GetMass();
}

float Body::getInertia() const
{
	return Physics::scaleUp(Physics::scaleUp(body->GetInertia()));
}

float Body::getAngularDamping() const
{
	return body->GetAngularDamping();
}

float Body::getLinearDamping() const
{
	return body->GetLinearDamping();
}

float Body::getGravityScale() const
{
	return body->GetGravityScale();
}

Body::Type Body::getType() const
{
	switch (body->GetType())
	{
	case b2_staticBody:
		return BODY_STATIC;
		break;
	case b2_dynamicBody:
		return BODY_DYNAMIC;
		break;
	case b2_kinematicBody:
		return BODY_KINEMATIC;
		break;
	default:
		return BODY_INVALID;
		break;
	}
}

void Body::applyLinearImpulse(float jx, float jy, bool wake)
{
	body->ApplyLinearImpulse(Physics::scaleDown(b2Vec2(jx, jy)), body->GetWorldCenter(), wake);
}

void Body::applyLinearImpulse(float jx, float jy, float rx, float ry, bool wake)
{
	body->ApplyLinearImpulse(Physics::scaleDown(b2Vec2(jx, jy)), Physics::scaleDown(b2Vec2(rx, ry)), wake);
}

void Body::applyAngularImpulse(float impulse, bool wake)
{
	// Angular impulse is in kg*m^2/s, meaning it needs to be scaled twice
	body->ApplyAngularImpulse(Physics::scaleDown(Physics::scaleDown(impulse)), wake);
}

void Body::applyTorque(float t, bool wake)
{
	// Torque is in N*m, or kg*m^2/s^2, meaning it also needs to be scaled twice
	body->ApplyTorque(Physics::scaleDown(Physics::scaleDown(t)), wake);
}

void Body::applyForce(float fx, float fy, float rx, float ry, bool wake)
{
	body->ApplyForce(Physics::scaleDown(b2Vec2(fx, fy)), Physics::scaleDown(b2Vec2(rx, ry)), wake);
}

void Body::applyForce(float fx, float fy, bool wake)
{
	body->ApplyForceToCenter(Physics::scaleDown(b2Vec2(fx, fy)), wake);
}

void Body::setX(float x)
{
	body->SetTransform(Physics::scaleDown(b2Vec2(x, getY())), getAngle());
}

void Body::setY(float y)
{
	body->SetTransform(Physics::scaleDown(b2Vec2(getX(), y)), getAngle());
}

void Body::setLinearVelocity(float x, float y)
{
	body->SetLinearVelocity(Physics::scaleDown(b2Vec2(x, y)));
}

void Body::setAngle(float d)
{
	body->SetTransform(body->GetPosition(), d);
}

void Body::setAngularVelocity(float r)
{
	body->SetAngularVelocity(r);
}

void Body::setKinematicState(b2Vec2 pos, float a, b2Vec2 vel, float da)
{
	body->SetTransform(Physics::scaleDown(pos), a);
	body->SetLinearVelocity(Physics::scaleDown(vel));
	body->SetAngularVelocity(da);
}

void Body::setPosition(float x, float y)
{
	body->SetTransform(Physics::scaleDown(b2Vec2(x, y)), body->GetAngle());
}

void Body::setAngularDamping(float d)
{
	body->SetAngularDamping(d);
}

void Body::setLinearDamping(float d)
{
	body->SetLinearDamping(d);
}

void Body::resetMassData()
{
	body->ResetMassData();
	hasCustomMass = false;
}

void Body::setMassData(float x, float y, float m, float i)
{
	b2MassData massData;
	massData.center = Physics::scaleDown(b2Vec2(x, y));
	massData.mass = m;
	massData.I = Physics::scaleDown(Physics::scaleDown(i));
	body->SetMassData(&massData);
	hasCustomMass = true;
}

void Body::setMass(float m)
{
	b2MassData data;
	body->GetMassData(&data);
	data.mass = m;
	body->SetMassData(&data);
	hasCustomMass = true;
}

void Body::setInertia(float i)
{
	b2MassData massData;
	massData.center = body->GetLocalCenter();
	massData.mass = body->GetMass();
	massData.I = Physics::scaleDown(Physics::scaleDown(i));
	body->SetMassData(&massData);
	hasCustomMass = true;
}

void Body::setGravityScale(float scale)
{
	body->SetGravityScale(scale);
}

void Body::setType(Body::Type type)
{
	switch (type)
	{
	case Body::BODY_STATIC:
		body->SetType(b2_staticBody);
		break;
	case Body::BODY_DYNAMIC:
		body->SetType(b2_dynamicBody);
		break;
	case Body::BODY_KINEMATIC:
		body->SetType(b2_kinematicBody);
		break;
	default:
		break;
	}
}

void Body::getWorldPoint(float x, float y, float &x_o, float &y_o)
{
	b2Vec2 v = Physics::scaleUp(body->GetWorldPoint(Physics::scaleDown(b2Vec2(x, y))));
	x_o = v.x;
	y_o = v.y;
}

void Body::getWorldVector(float x, float y, float &x_o, float &y_o)
{
	b2Vec2 v = Physics::scaleUp(body->GetWorldVector(Physics::scaleDown(b2Vec2(x, y))));
	x_o = v.x;
	y_o = v.y;
}

void Body::getLocalPoint(float x, float y, float &x_o, float &y_o)
{
	b2Vec2 v = Physics::scaleUp(body->GetLocalPoint(Physics::scaleDown(b2Vec2(x, y))));
	x_o = v.x;
	y_o = v.y;
}

void Body::getLocalVector(float x, float y, float &x_o, float &y_o)
{
	b2Vec2 v = Physics::scaleUp(body->GetLocalVector(Physics::scaleDown(b2Vec2(x, y))));
	x_o = v.x;
	y_o = v.y;
}

void Body::getLinearVelocityFromWorldPoint(float x, float y, float &x_o, float &y_o)
{
	b2Vec2 v = Physics::scaleUp(body->GetLinearVelocityFromWorldPoint(Physics::scaleDown(b2Vec2(x, y))));
	x_o = v.x;
	y_o = v.y;
}

void Body::getLinearVelocityFromLocalPoint(float x, float y, float &x_o, float &y_o)
{
	b2Vec2 v = Physics::scaleUp(body->GetLinearVelocityFromLocalPoint(Physics::scaleDown(b2Vec2(x, y))));
	x_o = v.x;
	y_o = v.y;
}

bool Body::isBullet() const
{
	return body->IsBullet();
}

void Body::setBullet(bool bullet)
{
	return body->SetBullet(bullet);
}

bool Body::isEnabled() const
{
	return body->IsEnabled();
}

bool Body::isAwake() const
{
	return body->IsAwake();
}

void Body::setSleepingAllowed(bool allow)
{
	body->SetSleepingAllowed(allow);
}

bool Body::isSleepingAllowed() const
{
	return body->IsSleepingAllowed();
}

void Body::setEnabled(bool enabled)
{
	body->SetEnabled(enabled);
}

void Body::setAwake(bool awake)
{
	body->SetAwake(awake);
}

void Body::setFixedRotation(bool fixed)
{
	body->SetFixedRotation(fixed);
}

bool Body::isFixedRotation() const
{
	return body->IsFixedRotation();
}

bool Body::isTouching(Body *other) const
{
	const b2ContactEdge *ce = body->GetContactList();
	b2Body *otherbody = other->body;

	while (ce != nullptr)
	{
		if (ce->other == otherbody && ce->contact != nullptr && ce->contact->IsTouching())
			return true;

		ce = ce->next;
	}

	return false;
}

World *Body::getWorld() const
{
	return world;
}

Shape *Body::getShape() const
{
	b2Fixture *f = body->GetFixtureList();
	if (f == nullptr)
		return nullptr;

	Shape *shape = (Shape *)(f->GetUserData().pointer);
	if (!shape)
		throw love::Exception("A Shape has escaped Memoizer!");

	return shape;
}

void Body::getMassData(float &x, float &y, float &mass, float &inertia)
{
	b2MassData data;
	body->GetMassData(&data);
	b2Vec2 center = Physics::scaleUp(data.center);
	x = center.x;
	y = center.y;
	mass = data.mass;
	inertia = Physics::scaleUp(Physics::scaleUp(data.I));
}

void Body::getWorldPoints(std::vector<float> &points)
{
	for (size_t i = 0; i + 1 < points.size(); i += 2)
	{
		b2Vec2 point = Physics::scaleUp(body->GetWorldPoint(Physics::scaleDown(b2Vec2(points[i], points[i + 1]))));
		points[i] = point.x;
		points[i + 1] = point.y;
	}
}

void Body::getLocalPoints(std::vector<float> &points)
{
	for (size_t i = 0; i + 1 < points.size(); i += 2)
	{
		b2Vec2 point = Physics::scaleUp(body->GetLocalPoint(Physics::scaleDown(b2Vec2(points[i], points[i + 1]))));
		points[i] = point.x;
		points[i + 1] = point.y;
	}
}

std::vector<Shape *> Body::getShapes() const
{
	std::vector<Shape *> shapes;
	for (b2Fixture *f = body->GetFixtureList(); f != nullptr; f = f->GetNext())
	{
		Shape *shape = (Shape *)(f->GetUserData().pointer);
		if (!shape)
			throw love::Exception("A Shape has escaped Memoizer!");
		shapes.push_back(shape);
	}
	return shapes;
}

std::vector<Joint *> Body::getJoints() const
{
	std::vector<Joint *> joints;
	for (const b2JointEdge *je = body->GetJointList(); je != nullptr; je = je->next)
	{
		Joint *joint = (Joint *) (je->joint->GetUserData().pointer);
		if (!joint)
			throw love::Exception("A joint has escaped Memoizer!");
		joints.push_back(joint);
	}
	return joints;
}

std::vector<Contact *> Body::getContacts() const
{
	std::vector<Contact *> contacts;
	for (const b2ContactEdge *ce = body->GetContactList(); ce != nullptr; ce = ce->next)
	{
		Contact *contact = (Contact *) world->findObject(ce->contact);
		if (!contact)
			contact = new Contact(world, ce->contact);
		else
			contact->retain();
		contacts.push_back(contact);
	}
	return contacts;
}

void Body::destroy()
{
	if (world->world->IsLocked())
	{
		// Called during time step. Save reference for destruction afterwards.
		this->retain();
		world->destructBodies.push_back(this);
		return;
	}

	world->world->DestroyBody(body);
	body = nullptr;

	// Drop the host's data so it does not outlive the body.
	userdata.set(nullptr);

	// Box2D body destroyed. Release its reference to the love Body.
	this->release();
}

void Body::setUserData(love::Object *data)
{
	userdata.set(data);
}

love::Object *Body::getUserData() const
{
	return userdata.get();
}

} // box2d
} // physics
} // love
