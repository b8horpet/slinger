//
// Created by derek on 19/09/20.
//

#include "illustrator.h"

#include <iostream>
#include <stdio.h>

#include <entt/entity/helper.hpp>

Illustrator::Illustrator(sf::RenderWindow &window, entt::registry &registry, entt::dispatcher &dispatcher) :
    window_(window),
    registry_(registry),
    dispatcher_(dispatcher),
    camera_(sf::Vector2f(0.f, 0.f), sf::Vector2f(80.f, -60.f) / 2.f)
{
    dispatcher_.sink<Event<FireRope>>().connect<&Illustrator::addRope>(*this);
    dispatcher_.sink<Event<Death>>().connect<&Illustrator::onPlayerDeath>(*this);

    registry_.on_construct<Drawable>().connect<&Illustrator::onAddDrawable>(this);
    registry_.on_destroy<Drawable>().connect<&Illustrator::onAddDrawable>(this);

    if (!font_.loadFromFile("data/LiberationMono-Regular.ttf"))
    {
        throw std::runtime_error("Could not locate font");
    }

    text_.setFont(font_);
    text_.setCharacterSize(24);
    text_.setFillColor(sf::Color::White);
}

void Illustrator::onAddDrawable(entt::registry& registry, entt::entity entity) {
    // Sort drawable entities by z index
    registry_.sort<Drawable>(
        [](const auto &lhs, const auto &rhs) {
            return lhs.zIndex < rhs.zIndex;
        }
    );
}



void Illustrator::draw(entt::registry &registry) {
    window_.clear(sf::Color::Black);
    window_.setFramerateLimit(60);

    registry.view<Follow, Position>().each(
        [this](const auto entity, const Follow& follow, const Position& position) {
            this->camera_.setCenter(position.value);
        }
    );

    window_.setView(camera_);

    registry.view<Drawable>().each(
        [this, &registry](const auto entity, const Drawable &drawable) {
            auto &pos = drawable.value->getPosition();

            if (registry.has<entt::tag<"wrapView"_hs>>(entity)
                && absolute(camera_.getCenter() - pos) > absolute(camera_.getSize() / 2.f)) {
                drawable.value->setPosition(pos + 2.f * (camera_.getCenter() - pos));
            }

            window_.draw(*drawable.value);
        }
    );

    window_.setView(window_.getDefaultView());
    registry.view<Follow, Position>().each(
        [this](const auto entity, const Follow& follow, const Position& position) {
            text_.setString("(" + std::to_string(position.value.x) + ", " + std::to_string(position.value.y) + ")");
            text_.setPosition(0, 0);
            text_.setScale(1.f, 1.f);
            window_.draw(text_);
        }
    );

    window_.display();
    window_.setView(camera_);
}

float Illustrator::round(float number, float multiple) {
    return ((number + multiple / 2) / multiple) * multiple;
}

Drawable Illustrator::makeRectangle(const sf::Vector2f &size, const sf::Vector2f &pos) const {
    Drawable d{std::make_unique<sf::RectangleShape>(sf::RectangleShape(size))};
    d.value->setPosition(pos);
    return d;
}

Drawable Illustrator::makeCircle(float radius, const sf::Vector2f &pos) const {
    Drawable d{std::make_unique<sf::CircleShape>(sf::CircleShape(radius))};
    d.value->setPosition(pos);
    return d;
}

sf::Vector2f Illustrator::absolute(const sf::Vector2f &vec) {
    return sf::Vector2f(abs(vec.x), abs(vec.y));
}

void Illustrator::addRope(const Event<FireRope> &event) {
}

void Illustrator::onPlayerDeath(const Event<Death> &event) {
    registry_.remove_if_exists<Follow>(event.entity);
}

bool operator>(const sf::Vector2f &lhs, const sf::Vector2f &rhs) {
    return lhs.x > rhs.x || lhs.y > rhs.y;
}