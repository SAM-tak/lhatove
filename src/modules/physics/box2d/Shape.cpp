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

#include "Shape.h"

// Module
#include "Body.h"
#include "World.h"
#include "Physics.h"

// STD

namespace love
{
namespace physics
{
namespace box2d
{

Shape::Shape(Body *body, const b2Shape &shape)
	: shape(nullptr)
	, own(false)
	, shapeType(SHAPE_INVALID)
	, body(body)
	, fixture(nullptr)
{
	if (body)
	{
		b2FixtureDef def;
		def.shape = &shape;
		def.userData.pointer = (uintptr_t)this;

		// 0 density stops CreateFixture from calling b2Body::ResetMassData().
		def.density = body->hasCustomMassData() ? 0.0f : 1.0f;

		fixture = body->body->CreateFixture(&def);
		this->shape = fixture->GetShape();

		if (body->hasCustomMassData())
			setDensity(1.0f);

		retain(); // Shape::destroy does the release().
	}
	else
	{
		// Path to support deprecated APIs.
		auto physics = Module::getInstance<Physics>(Module::M_PHYSICS);
		this->shape = shape.Clone(physics->getBlockAllocator());
		own = true;
	}

	switch (this->shape->GetType())
	{
	case b2Shape::e_circle:
		shapeType = SHAPE_CIRCLE;
		break;
	case b2Shape::e_polygon:
		shapeType = SHAPE_POLYGON;
		break;
	case b2Shape::e_edge:
		shapeType = SHAPE_EDGE;
		break;
	case b2Shape::e_chain:
		shapeType = SHAPE_CHAIN;
		break;
	default:
		shapeType = SHAPE_INVALID;
		break;
	}
}

Shape::~Shape()
{
	if (shape && own)
	{
		auto physics = Module::getInstance<Physics>(Module::M_PHYSICS);
		auto allocator = physics->getBlockAllocator();

		// Taken from b2Fixture::Destroy. Not very pretty...
		switch (shapeType)
		{
		case SHAPE_CIRCLE:
		{
			b2CircleShape *s = (b2CircleShape*)shape;
			s->~b2CircleShape();
			allocator->Free(s, sizeof(b2CircleShape));
			break;
		}
		case SHAPE_EDGE:
		{
			b2EdgeShape *s = (b2EdgeShape*)shape;
			s->~b2EdgeShape();
			allocator->Free(s, sizeof(b2EdgeShape));
			break;
		}
		case SHAPE_POLYGON:
		{
			b2PolygonShape *s = (b2PolygonShape*)shape;
			s->~b2PolygonShape();
			allocator->Free(s, sizeof(b2PolygonShape));
			break;
		}
		case SHAPE_CHAIN:
		{
			b2ChainShape *s = (b2ChainShape*)shape;
			s->~b2ChainShape();
			allocator->Free(s, sizeof(b2ChainShape));
			break;
		}
		default:
			break;
		}
	}
}

void Shape::destroy(bool implicit)
{
	if (fixture == nullptr)
		return;

	if (body->world->world->IsLocked())
	{
		// Called during time step. Save reference for destruction afterwards.
		this->retain();
		body->world->destructShapes.push_back(this);
		return;
	}

	if (!implicit && fixture != nullptr)
		body->body->DestroyFixture(fixture);

	fixture = nullptr;
	shape = nullptr;
	body = nullptr;

	// Drop the host's data so it does not outlive the fixture.
	userdata.set(nullptr);

	// Box2D fixture destroyed. Release its reference to the love Shape.
	release();
}

void Shape::throwIfFixtureNotValid() const
{
	if (fixture == nullptr)
		throw love::Exception("Shape must be active in the physics World to use this method.");
}

void Shape::throwIfShapeNotValid() const
{
	if (shape == nullptr)
		throw love::Exception("Cannot call this method on a destroyed Shape.");
}

Shape::Type Shape::getType() const
{
	return shapeType;
}

void Shape::setFriction(float friction)
{
	throwIfFixtureNotValid();
	fixture->SetFriction(friction);
}

void Shape::setRestitution(float restitution)
{
	throwIfFixtureNotValid();
	fixture->SetRestitution(restitution);
}

void Shape::setDensity(float density)
{
	throwIfFixtureNotValid();
	fixture->SetDensity(density);
	if (!body->hasCustomMassData())
		body->resetMassData();
}

void Shape::setSensor(bool sensor)
{
	throwIfFixtureNotValid();
	fixture->SetSensor(sensor);
}

float Shape::getFriction() const
{
	throwIfFixtureNotValid();
	return fixture->GetFriction();
}

float Shape::getRestitution() const
{
	throwIfFixtureNotValid();
	return fixture->GetRestitution();
}

float Shape::getDensity() const
{
	throwIfFixtureNotValid();
	return fixture->GetDensity();
}

bool Shape::isSensor() const
{
	throwIfFixtureNotValid();
	return fixture->IsSensor();
}

Body *Shape::getBody() const
{
	return body;
}

float Shape::getRadius() const
{
	throwIfShapeNotValid();
	return Physics::scaleUp(shape->m_radius);
}

int Shape::getChildCount() const
{
	throwIfShapeNotValid();
	return shape->GetChildCount();
}

void Shape::setFilterData(int *v)
{
	throwIfFixtureNotValid();
	b2Filter f;
	f.categoryBits = (uint16) v[0];
	f.maskBits = (uint16) v[1];
	f.groupIndex = (int16) v[2];
	fixture->SetFilterData(f);
}

void Shape::getFilterData(int *v)
{
	throwIfFixtureNotValid();
	b2Filter f = fixture->GetFilterData();
	v[0] = (int) f.categoryBits;
	v[1] = (int) f.maskBits;
	v[2] = (int) f.groupIndex;
}

void Shape::setCategory(uint16 bits)
{
	throwIfFixtureNotValid();
	b2Filter f = fixture->GetFilterData();
	f.categoryBits = bits;
	fixture->SetFilterData(f);
}

void Shape::setMask(uint16 bits)
{
	throwIfFixtureNotValid();
	b2Filter f = fixture->GetFilterData();
	f.maskBits = ~bits;
	fixture->SetFilterData(f);
}

void Shape::setGroupIndex(int index)
{
	throwIfFixtureNotValid();
	b2Filter f = fixture->GetFilterData();
	f.groupIndex = (uint16)index;
	fixture->SetFilterData(f);
}

int Shape::getGroupIndex() const
{
	throwIfFixtureNotValid();
	b2Filter f = fixture->GetFilterData();
	return f.groupIndex;
}

uint16 Shape::getCategory() const
{
	throwIfFixtureNotValid();
	return fixture->GetFilterData().categoryBits;
}

uint16 Shape::getMask() const
{
	throwIfFixtureNotValid();
	return ~(fixture->GetFilterData().maskBits);
}

void Shape::setUserData(love::Object *data)
{
	userdata.set(data);
}

love::Object *Shape::getUserData() const
{
	return userdata.get();
}

bool Shape::testPoint(float x, float y) const
{
	throwIfFixtureNotValid();
	return fixture->TestPoint(Physics::scaleDown(b2Vec2(x, y)));
}

bool Shape::testPoint(float x, float y, float r, float px, float py) const
{
	throwIfShapeNotValid();
	b2Vec2 point(px, py);
	b2Transform transform(Physics::scaleDown(b2Vec2(x, y)), b2Rot(r));
	return shape->TestPoint(transform, Physics::scaleDown(point));
}

bool Shape::rayCast(float x1, float y1, float x2, float y2, float maxFraction, int childIndex, float &nx, float &ny, float &fraction) const
{
	throwIfFixtureNotValid();
	b2RayCastInput input;
	b2RayCastOutput output;
	input.p1 = Physics::scaleDown(b2Vec2(x1, y1));
	input.p2 = Physics::scaleDown(b2Vec2(x2, y2));
	input.maxFraction = maxFraction;
	if (!fixture->RayCast(&output, input, childIndex))
		return false;
	nx = output.normal.x;
	ny = output.normal.y;
	fraction = output.fraction;
	return true;
}

bool Shape::rayCast(float x1, float y1, float x2, float y2, float maxFraction, float x, float y, float r, int childIndex, float &nx, float &ny, float &fraction) const
{
	throwIfShapeNotValid();
	b2RayCastInput input;
	b2RayCastOutput output;
	input.p1 = Physics::scaleDown(b2Vec2(x1, y1));
	input.p2 = Physics::scaleDown(b2Vec2(x2, y2));
	input.maxFraction = maxFraction;
	b2Transform transform(Physics::scaleDown(b2Vec2(x, y)), b2Rot(r));
	if (!shape->RayCast(&output, input, transform, childIndex))
		return false;
	nx = output.normal.x;
	ny = output.normal.y;
	fraction = output.fraction;
	return true;
}

void Shape::computeAABB(float x, float y, float r, int childIndex, float &lx, float &ly, float &ux, float &uy) const
{
	throwIfShapeNotValid();
	b2Transform transform(Physics::scaleDown(b2Vec2(x, y)), b2Rot(r));
	b2AABB box;
	shape->ComputeAABB(&box, transform, childIndex);
	box = Physics::scaleUp(box);
	lx = box.lowerBound.x;
	ly = box.lowerBound.y;
	ux = box.upperBound.x;
	uy = box.upperBound.y;
}

void Shape::computeMass(float density, float &cx, float &cy, float &mass, float &inertia) const
{
	throwIfShapeNotValid();
	b2MassData data;
	shape->ComputeMass(&data, density);
	b2Vec2 center = Physics::scaleUp(data.center);
	cx = center.x;
	cy = center.y;
	mass = data.mass;
	inertia = Physics::scaleUp(Physics::scaleUp(data.I));
}

void Shape::getBoundingBox(int childIndex, float &lx, float &ly, float &ux, float &uy) const
{
	throwIfFixtureNotValid();
	b2AABB box = fixture->GetAABB(childIndex);
	box = Physics::scaleUp(box);
	lx = box.lowerBound.x;
	ly = box.lowerBound.y;
	ux = box.upperBound.x;
	uy = box.upperBound.y;
}

void Shape::getMassData(float &cx, float &cy, float &mass, float &inertia) const
{
	throwIfFixtureNotValid();
	b2MassData data;
	fixture->GetMassData(&data);
	b2Vec2 center = Physics::scaleUp(data.center);
	cx = center.x;
	cy = center.y;
	mass = data.mass;
	inertia = data.I;
}

} // box2d
} // physics
} // love
