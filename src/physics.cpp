//
// Created by derek on 19/09/20.
//

#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/CircleShape.hpp>

#include <box2d/box2d.h>
#include <iostream>

#include "illustrator.h"
#include "physics.h"
#include "misc_components.h"


Physics::Physics(entt::registry &registry, entt::dispatcher &dispatcher) :
    registry_(registry), dispatcher_(dispatcher),
    world_(b2Vec2(0, -200.f)) {
    world_.SetContactListener(new ContactListener());

    dispatcher_.sink<Event<FireRope>>().connect<&Physics::fireRope>(*this);
    dispatcher_.sink<Event<Jump>>().connect<&Physics::jump>(*this);
}

const float Physics::TIME_STEP = 1 / 60.f;

void Physics::handlePhysics(entt::registry &registry, float delta, const sf::Vector2f &mousePos) {
    world_.Step(TIME_STEP, 6, 18);

    dispatcher_.update();

    registry.view<FixtureInfoPtr>().each(
        [delta, &registry, this](const auto entity, const FixtureInfoPtr &fixture) {
            b2Body *body = fixture->value->GetBody();

            if (Drawable *drawable = registry.try_get<Drawable>(entity)) {
                (*drawable).value->setPosition(
                    body->GetPosition().x,
                    body->GetPosition().y
                );

                (*drawable).value->setRotation((body->GetAngle() * 180.f / 3.145f) + fixture->angleOffset);
            }
        }
    );

    registry.view<BodyPtr>().each(
        [delta, mousePos, &registry, this](const auto entity, const BodyPtr &body) {
            if (Movement *movement = registry.try_get<Movement>(entity)) {


                this->manageMovement(entity, *body, *movement);
            }

            if (registry.has<entt::tag<"rotate_to_mouse"_hs>>(entity)) {
                this->rotateToMouse(*body, mousePos);
            }
        }
    );

    registry.view<JointPtr, Drawable>().each(
        [](const auto entity, const JointPtr &joint, Drawable &drawable) {
            const auto pointA = joint->GetAnchorA();
            const auto pointB = joint->GetAnchorB();
            const auto distance = pointB - pointA;

            const auto angle = atan2f(distance.y, distance.x);
            const auto length = distance.Length();

            // TODO: Make rope thinner the closer it is to max length

            drawable.value->setPosition(
                pointA.x,
                pointA.y
            );

            drawable.value->setRotation(angle * 180.f / 3.145f);

            auto* rect = dynamic_cast<sf::RectangleShape*>(drawable.value.get());
            rect->setSize(sf::Vector2f(length, rect->getSize().y));
        }
    );

}

BodyPtr Physics::makeBody(sf::Vector2f pos, float rot, b2BodyType bodyType) {
    b2BodyDef bodyDef;
    bodyDef.type = bodyType;
    bodyDef.position.Set(pos.x, pos.y);
    bodyDef.angle = rot;
    bodyDef.angularDamping = 0.01f;
    bodyDef.linearDamping = 0.01f;

    return BodyPtr(world_.CreateBody(&bodyDef));
}

FixtureInfoPtr& Physics::makeFixture(
    entt::entity entity,
    sf::Shape *shape,
    entt::registry &reg,
    entt::entity bodyEntity
)
{
    const BodyPtr *body = reg.try_get<BodyPtr>(bodyEntity);
    assert(body);

    auto *rect = dynamic_cast<sf::RectangleShape *>(shape);
    auto *circle = dynamic_cast<sf::CircleShape *>(shape);

    if (!(rect || circle)) {
        throw std::runtime_error("Can only make components from a rectangle and circle so far");
    }

    std::unique_ptr<b2Shape> fixtureShape;

    if (rect) {
        b2PolygonShape box;
        box.SetAsBox(
            rect->getSize().x / 2.f,
            rect->getSize().y / 2.f,
            b2Vec2(
                shape->getPosition().x,
                shape->getPosition().y
            ),
            shape->getRotation()
        );

        // Set the origin of the shape for rotations
        shape->setOrigin((rect->getSize() / 2.f) - shape->getPosition());

        fixtureShape = std::make_unique<b2PolygonShape>(box);
    }

    if (circle) {
        b2CircleShape circleShape;
        circleShape.m_radius = circle->getRadius();

        fixtureShape = std::make_unique<b2CircleShape>(circleShape);
        shape->setOrigin(sf::Vector2f(circle->getRadius(), circle->getRadius()));
    }

    b2FixtureDef fixtureDef;
    fixtureDef.shape = fixtureShape.get();
    fixtureDef.density = 1.0f;
    fixtureDef.friction = 0.0f;

    auto fix = std::make_shared<FixtureInfo>(
        FixtureInfo {
            FixturePtr((*body)->CreateFixture(&fixtureDef)),
            shape->getRotation(),
            shape->getPosition(),
            entity
        }
    );

    fix->value->SetUserData(static_cast<void *>(fix.get()));
    return registry_.emplace<FixtureInfoPtr>(entity, fix);
}


b2Vec2 Physics::tob2(const sf::Vector2f &vec) {
    return b2Vec2(vec.x, vec.y);
}

float Physics::toRadians(float deg) {
    return deg * Physics::PI / 180.f;
}

float Physics::toDegrees(float deg) {
    return deg * (180.f / Physics::PI);
}

void Physics::manageMovement(entt::entity entity, b2Body &body, Movement &movement) {
    if (!isOnFloor(entity)) {
        movement.direction = 0;
        movement.jumping = false;
        return;
    }

    float vel = body.GetLinearVelocity().x;
    float desiredVel = 0;

    if (movement.direction == 0) {
        desiredVel = vel * movement.deceleration;
    }

    if (movement.direction > 0) {
        desiredVel = b2Min(vel + movement.acceleration, movement.vel());
    }

    if (movement.direction < 0) {
        desiredVel = b2Max(vel - movement.acceleration, movement.vel());
    }

    float velChange = desiredVel - body.GetLinearVelocity().x;
    float impulse = body.GetMass() * velChange;
    body.ApplyLinearImpulseToCenter(b2Vec2(impulse, 0), false);

    if (movement.jumping) {
        std::cout << "Gonna jump!" << std::endl;
        impulse = body.GetMass() * movement.jumpVel;
        body.ApplyLinearImpulseToCenter(b2Vec2(0, impulse), false);
    }

    movement.direction = 0;
    movement.jumping = false;
}

BodyPtr &Physics::makeBody(entt::entity entity, sf::Vector2f pos, float rot, b2BodyType type) {
    auto &body = registry_.emplace<BodyPtr>(entity, makeBody(pos, rot, type));
    return body;
}

b2World &Physics::getWorld() {
    return world_;
}

void Physics::rotateToMouse(b2Body &body, const sf::Vector2f &mousePos) {
    b2Vec2 toTarget = tob2(mousePos) - body.GetPosition();
    float desiredAngle = atan2f(-toTarget.x, toTarget.y);
    body.SetTransform(body.GetPosition(), desiredAngle);
}

void Physics::fireRope(Event<FireRope> event) {
    std::cout << "Firing a rope from physics!" << std::endl;

    rayCastEntity_ = event.entity;
    const auto &body = registry_.get<BodyPtr>(event.entity);
    auto startingPos = body->GetWorldPoint(tob2(event.eventDef.localPos));
    auto endingPos = tob2(event.eventDef.target);
    std::cout << endingPos.x << ", " << endingPos.y << std::endl;

    world_.RayCast(this, startingPos, endingPos);
}

// Called when the rope hits something
float Physics::ReportFixture(b2Fixture *fixture, const b2Vec2 &point, const b2Vec2 &normal, float fraction) {
    // Find the entity that fired the rope
    auto* fixInfo = static_cast<FixtureInfo *>(fixture->GetUserData());

    b2RopeJointDef jointDef;
    jointDef.bodyA = fixture->GetBody();
    jointDef.localAnchorA = fixture->GetBody()->GetLocalPoint(point);

    // TODO Figure out how to get the person who fired the rope here
    jointDef.bodyB = registry_.get<BodyPtr>(rayCastEntity_).get();
    // TODO Figure out how to get the arm position here from the event
    jointDef.localAnchorB = b2Vec2(0, 7);

    jointDef.maxLength = (point - jointDef.bodyB->GetWorldPoint(jointDef.localAnchorB)).Length();

    auto rope = registry_.create();

    auto joint = JointPtr(world_.CreateJoint(&jointDef));
    registry_.emplace<JointPtr>(rope, std::move(joint));

    auto width = 1.f;

    auto drawable = Drawable{
        std::make_unique<sf::RectangleShape>(sf::Vector2f(16, width)),
        3
    };
    drawable.value->setOrigin(0, width/2);
    registry_.emplace<Drawable>(rope, std::move(drawable));

    // Sort drawable entities by z index
    registry_.sort<Drawable>([](const auto &lhs, const auto &rhs) {
        return lhs.zIndex < rhs.zIndex;
    });

    return 0;
}

void Physics::jump(Event<Jump> event) {
    if (!isOnFloor(event.entity)) {
        return;
    }

    BodyPtr &body = registry_.get<BodyPtr>(event.entity);

    std::cout << "Gonna jump!" << std::endl;
    float impulse = body->GetMass() * event.eventDef.impulse;
    body->ApplyLinearImpulseToCenter(b2Vec2(0, impulse), false);
}

bool Physics::isOnFloor(entt::entity entity) {
    const auto *foot = registry_.try_get<FootSensor>(entity);

    if (!foot) {
        return true;
    }

    return foot->fixture->numberOfContacts > 0;
}

void BodyDeleter::operator()(b2Body *body) const {
    body->GetWorld()->DestroyBody(body);
}

void FixtureDeleter::operator()(b2Fixture *fixture) const {
    fixture->GetBody()->DestroyFixture(fixture);
}

void ContactListener::BeginContact(b2Contact *contact) {
    auto *fixA = static_cast<FixtureInfo *>(contact->GetFixtureA()->GetUserData());
    auto *fixB = static_cast<FixtureInfo *>(contact->GetFixtureB()->GetUserData());

    fixA->numberOfContacts += 1;
    fixB->numberOfContacts += 1;
}

void ContactListener::EndContact(b2Contact *contact) {
    auto *fixA = static_cast<FixtureInfo *>(contact->GetFixtureA()->GetUserData());
    auto *fixB = static_cast<FixtureInfo *>(contact->GetFixtureB()->GetUserData());

    fixA->numberOfContacts -= 1;
    fixB->numberOfContacts -= 1;
}

void JointDeleter::operator()(b2Joint *joint) const {
    joint->GetBodyA()->GetWorld()->DestroyJoint(joint);
}
