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

#include "World.h"

#include "Shape.h"
#include "Contact.h"
#include "Physics.h"
#include "Body.h"
#include "Joint.h"

namespace love
{
namespace physics
{
namespace box2d
{

love::Type World::type("World", &Object::type);

World::ContactCallback::ContactCallback(World *world)
	: world(world)
{
}

World::ContactCallback::~ContactCallback()
{
}

void World::ContactCallback::process(b2Contact *contact, const b2ContactImpulse *impulse)
{
	if (listener.get() == nullptr)
		return;

	Shape *a = (Shape *)(contact->GetFixtureA()->GetUserData().pointer);
	Shape *b = (Shape *)(contact->GetFixtureB()->GetUserData().pointer);
	if (a == nullptr || b == nullptr)
		throw love::Exception("A Shape has escaped Memoizer!");

	StrongRef<Contact> cobj((Contact *) world->findObject(contact));
	if (cobj.get() == nullptr)
		cobj.set(new Contact(world, contact), Acquire::NORETAIN);

	std::vector<float> impulses;
	if (impulse != nullptr)
	{
		for (int c = 0; c < impulse->count; c++)
		{
			impulses.push_back(Physics::scaleUp(impulse->normalImpulses[c]));
			impulses.push_back(Physics::scaleUp(impulse->tangentImpulses[c]));
		}
	}

	listener->onContact(a, b, cobj.get(), impulses);
}

World::ContactFilter::ContactFilter()
{
}

World::ContactFilter::~ContactFilter()
{
}

bool World::ContactFilter::process(Shape *a, Shape *b)
{
	if (listener.get() != nullptr)
		return listener->shouldCollide(a, b);
	return true;
}

World::QueryCallback::QueryCallback(ShapeVisitor &visitor)
	: visitor(visitor)
{
}

World::QueryCallback::~QueryCallback()
{
}

bool World::QueryCallback::ReportFixture(b2Fixture *fixture)
{
	Shape *f = (Shape *)(fixture->GetUserData().pointer);
	if (!f)
		throw love::Exception("A Shape has escaped Memoizer!");
	return visitor.onShape(f);
}

World::CollectCallback::CollectCallback(uint16 categoryMask, std::vector<Shape *> &out)
	: categoryMask(categoryMask)
	, out(out)
{
}

World::CollectCallback::~CollectCallback()
{
}

bool World::CollectCallback::ReportFixture(b2Fixture *f)
{
	if (categoryMask != 0xFFFF && (categoryMask & f->GetFilterData().categoryBits) == 0)
		return true;

	Shape *shape = (Shape *)(f->GetUserData().pointer);
	if (!shape)
		throw love::Exception("A Shape has escaped Memoizer!");
	out.push_back(shape);
	return true;
}

World::RayCastCallback::RayCastCallback(RayCastVisitor &visitor)
	: visitor(visitor)
{
}

World::RayCastCallback::~RayCastCallback()
{
}

float World::RayCastCallback::ReportFixture(b2Fixture *fixture, const b2Vec2 &point, const b2Vec2 &normal, float fraction)
{
	Shape *f = (Shape *)(fixture->GetUserData().pointer);
	if (!f)
		throw love::Exception("A Shape has escaped Memoizer!");
	b2Vec2 scaledPoint = Physics::scaleUp(point);
	return visitor.onHit(f, scaledPoint.x, scaledPoint.y, normal.x, normal.y, fraction);
}

World::RayCastOneCallback::RayCastOneCallback(uint16 categoryMask, bool any)
	: hitFixture(nullptr)
	, hitPoint()
	, hitNormal()
	, hitFraction(1.0f)
	, categoryMask(categoryMask)
	, any(any)
{
}

float World::RayCastOneCallback::ReportFixture(b2Fixture *fixture, const b2Vec2 &point, const b2Vec2 &normal, float fraction)
{
	if (categoryMask != 0xFFFF && (categoryMask & fixture->GetFilterData().categoryBits) == 0)
		return -1;

	hitFixture = fixture;
	hitPoint = point;
	hitNormal = normal;
	hitFraction = fraction;

	// Returning the fraction makes sure it doesn't process anything farther away in subsequent iterations.
	return any ? 0 : fraction;
}

void World::SayGoodbye(b2Fixture *fixture)
{
	Shape *s = (Shape *)(fixture->GetUserData().pointer);
	// Hint implicit destruction with true.
	if (s) s->destroy(true);
}

void World::SayGoodbye(b2Joint *joint)
{
	Joint *j = (Joint *)(joint->GetUserData().pointer);
	// Hint implicit destruction with true.
	if (j) j->destroyJoint(true);
}

World::World()
	: world(nullptr)
	, destructWorld(false)
	, begin(this)
	, end(this)
	, presolve(this)
	, postsolve(this)
{
	world = new b2World(b2Vec2(0,0));
	world->SetAllowSleeping(true);
	world->SetContactListener(this);
	world->SetContactFilter(this);
	world->SetDestructionListener(this);
	b2BodyDef def;
	groundBody = world->CreateBody(&def);
	registerObject(world, this);
}

World::World(b2Vec2 gravity, bool sleep)
	: world(nullptr)
	, destructWorld(false)
	, begin(this)
	, end(this)
	, presolve(this)
	, postsolve(this)
{
	world = new b2World(Physics::scaleDown(gravity));
	world->SetAllowSleeping(sleep);
	world->SetContactListener(this);
	world->SetContactFilter(this);
	world->SetDestructionListener(this);
	b2BodyDef def;
	groundBody = world->CreateBody(&def);
	registerObject(world, this);
}

World::~World()
{
	destroy();
}

void World::update(float dt)
{
	update(dt, 8, 3); // Box2D 2.3's recommended defaults.
}

void World::update(float dt, int velocityIterations, int positionIterations)
{
	world->Step(dt, velocityIterations, positionIterations);

	// Destroy all objects marked during the time step.
	for (Body *b : destructBodies)
	{
		if (b->body != nullptr) b->destroy();
		// Release for reference in vector.
		b->release();
	}
	for (Shape *s : destructShapes)
	{
		if (s->isValid()) s->destroy();
		// Release for reference in vector.
		s->release();
	}
	for (Joint *j : destructJoints)
	{
		if (j->isValid()) j->destroyJoint();
		// Release for reference in vector.
		j->release();
	}
	destructBodies.clear();
	destructShapes.clear();
	destructJoints.clear();

	if (destructWorld)
		destroy();
}

void World::BeginContact(b2Contact *contact)
{
	begin.process(contact);
}

void World::EndContact(b2Contact *contact)
{
	end.process(contact);

	// Letting the Contact know that the b2Contact will be destroyed any second.
	Contact *c = (Contact *)findObject(contact);
	if (c != nullptr)
		c->invalidate();
}

void World::PreSolve(b2Contact *contact, const b2Manifold *oldManifold)
{
	B2_NOT_USED(oldManifold); // not sure what to do with this
	presolve.process(contact);
}

void World::PostSolve(b2Contact *contact, const b2ContactImpulse *impulse)
{
	postsolve.process(contact, impulse);
}

bool World::ShouldCollide(b2Fixture *fixtureA, b2Fixture *fixtureB)
{
	const b2Filter &filterA = fixtureA->GetFilterData();
	const b2Filter &filterB = fixtureB->GetFilterData();

	// From b2_world_callbacks.cpp
	// 0 is the default group index. If they're customized to be the same group,
	// allow collisions if it's positive and disallow if it's negative.
	if (filterA.groupIndex != 0 && filterA.groupIndex == filterB.groupIndex)
		return filterA.groupIndex > 0;

	if ((filterA.maskBits & filterB.categoryBits) == 0 || (filterA.categoryBits & filterB.maskBits) == 0)
		return false;

	// Shapes should be memoized, if we created them
	Shape *a = (Shape *)(fixtureA->GetUserData().pointer);
	Shape *b = (Shape *)(fixtureB->GetUserData().pointer);
	if (!a || !b)
		throw love::Exception("A Shape has escaped Memoizer!");

	return filter.process(a, b);
}

bool World::isValid() const
{
	return world != nullptr;
}

void World::setCallback(CallbackKind kind, ContactListener *listener)
{
	ContactCallback *slots[] = {&begin, &end, &presolve, &postsolve};
	if (kind < 0 || kind >= CALLBACK_MAX_ENUM)
		return;
	slots[kind]->listener.set(listener);
}

World::ContactListener *World::getCallback(CallbackKind kind) const
{
	const ContactCallback *slots[] = {&begin, &end, &presolve, &postsolve};
	if (kind < 0 || kind >= CALLBACK_MAX_ENUM)
		return nullptr;
	return slots[kind]->listener.get();
}

void World::setContactFilter(ContactFilterListener *listener)
{
	filter.listener.set(listener);
}

World::ContactFilterListener *World::getContactFilter() const
{
	return filter.listener.get();
}

void World::setGravity(float x, float y)
{
	world->SetGravity(Physics::scaleDown(b2Vec2(x, y)));
}

b2Vec2 World::getGravity() const
{
	return Physics::scaleUp(world->GetGravity());
}

void World::translateOrigin(float x, float y)
{
	world->ShiftOrigin(Physics::scaleDown(b2Vec2(x, y)));
}

void World::setSleepingAllowed(bool allow)
{
	world->SetAllowSleeping(allow);
}

bool World::isSleepingAllowed() const
{
	return world->GetAllowSleeping();
}

bool World::isLocked() const
{
	return world->IsLocked();
}

int World::getBodyCount() const
{
	return world->GetBodyCount()-1; // ignore the ground body
}

int World::getJointCount() const
{
	return world->GetJointCount();
}

int World::getContactCount() const
{
	return world->GetContactCount();
}

std::vector<Body *> World::getBodies() const
{
	std::vector<Body *> bodies;
	for (b2Body *b = world->GetBodyList(); b != nullptr; b = b->GetNext())
	{
		if (b == groundBody)
			continue;
		Body *body = (Body *)(b->GetUserData().pointer);
		if (!body)
			throw love::Exception("A body has escaped Memoizer!");
		bodies.push_back(body);
	}
	return bodies;
}

std::vector<Joint *> World::getJoints() const
{
	std::vector<Joint *> joints;
	for (b2Joint *j = world->GetJointList(); j != nullptr; j = j->GetNext())
	{
		Joint *joint = (Joint *)(j->GetUserData().pointer);
		if (!joint)
			throw love::Exception("A joint has escaped Memoizer!");
		joints.push_back(joint);
	}
	return joints;
}

std::vector<Contact *> World::getContacts()
{
	std::vector<Contact *> contacts;
	for (b2Contact *c = world->GetContactList(); c != nullptr; c = c->GetNext())
	{
		Contact *contact = (Contact *) findObject(c);
		if (!contact)
			contact = new Contact(this, c);
		else
			contact->retain();
		contacts.push_back(contact);
	}
	return contacts;
}

b2Body *World::getGroundBody() const
{
	return groundBody;
}

void World::queryShapesInArea(float lx, float ly, float ux, float uy, ShapeVisitor &visitor)
{
	b2AABB box;
	box.lowerBound = Physics::scaleDown(b2Vec2(lx, ly));
	box.upperBound = Physics::scaleDown(b2Vec2(ux, uy));
	QueryCallback query(visitor);
	world->QueryAABB(&query, box);
}

std::vector<Shape *> World::getShapesInArea(float lx, float ly, float ux, float uy, uint16 categoryMask)
{
	b2AABB box;
	box.lowerBound = Physics::scaleDown(b2Vec2(lx, ly));
	box.upperBound = Physics::scaleDown(b2Vec2(ux, uy));
	std::vector<Shape *> shapes;
	CollectCallback query(categoryMask, shapes);
	world->QueryAABB(&query, box);
	return shapes;
}

void World::rayCast(float x1, float y1, float x2, float y2, RayCastVisitor &visitor)
{
	b2Vec2 v1 = Physics::scaleDown(b2Vec2(x1, y1));
	b2Vec2 v2 = Physics::scaleDown(b2Vec2(x2, y2));
	RayCastCallback raycast(visitor);
	world->RayCast(&raycast, v1, v2);
}

static World::RayHit rayHitOf(const World::RayCastOneCallback &raycast)
{
	World::RayHit hit;
	if (raycast.hitFixture)
	{
		Shape *f = (Shape *)(raycast.hitFixture->GetUserData().pointer);
		if (f == nullptr)
			throw love::Exception("A Shape has escaped Memoizer!");
		b2Vec2 hitPoint = Physics::scaleUp(raycast.hitPoint);
		hit.shape = f;
		hit.x = hitPoint.x;
		hit.y = hitPoint.y;
		hit.nx = raycast.hitNormal.x;
		hit.ny = raycast.hitNormal.y;
		hit.fraction = raycast.hitFraction;
	}
	return hit;
}

World::RayHit World::rayCastAny(float x1, float y1, float x2, float y2, uint16 categoryMask)
{
	b2Vec2 v1 = Physics::scaleDown(b2Vec2(x1, y1));
	b2Vec2 v2 = Physics::scaleDown(b2Vec2(x2, y2));
	RayCastOneCallback raycast(categoryMask, true);
	world->RayCast(&raycast, v1, v2);
	return rayHitOf(raycast);
}

World::RayHit World::rayCastClosest(float x1, float y1, float x2, float y2, uint16 categoryMask)
{
	b2Vec2 v1 = Physics::scaleDown(b2Vec2(x1, y1));
	b2Vec2 v2 = Physics::scaleDown(b2Vec2(x2, y2));
	RayCastOneCallback raycast(categoryMask, false);
	world->RayCast(&raycast, v1, v2);
	return rayHitOf(raycast);
}

void World::destroy()
{
	if (world == nullptr)
		return;

	if (world->IsLocked())
	{
		destructWorld = true;
		return;
	}

	// Drop the listeners so nothing of the host outlives the world.
	begin.listener.set(nullptr);
	end.listener.set(nullptr);
	presolve.listener.set(nullptr);
	postsolve.listener.set(nullptr);
	filter.listener.set(nullptr);

	// Cleaning up the world.
	b2Body *b = world->GetBodyList();
	while (b)
	{
		b2Body *t = b;
		b = b->GetNext();
		if (t == groundBody)
			continue;
		Body *body = (Body *)(t->GetUserData().pointer);
		if (!body)
			throw love::Exception("A body has escaped Memoizer!");
		body->destroy();
	}

	world->DestroyBody(groundBody);
	unregisterObject(world);

	delete world;
	world = nullptr;
}

void World::registerObject(void *b2object, love::Object *object)
{
	box2dObjectMap[b2object] = object;
}

void World::unregisterObject(void *b2object)
{
	box2dObjectMap.erase(b2object);
}

love::Object *World::findObject(void *b2object) const
{
	auto it = box2dObjectMap.find(b2object);
	if (it != box2dObjectMap.end())
		return it->second;
	else
		return nullptr;
}

} // box2d
} // physics
} // love
