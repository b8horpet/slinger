//
// Created by derek on 20/09/20.
//

#include <iostream>
#include "input_manager.h"
#include "misc_components.h"

InputManager::InputManager(sf::Window &window, entt::dispatcher& dispatcher) :
    window_(window), dispatcher_(dispatcher) {

}

UIAction InputManager::handleInput(entt::registry &registry, sf::Window &window) {
    // Remove all key releases from the previous frame
    firstTimeKeyPresses_.clear();
    firstTimeButtonPresses_.clear();

    // Process events
    sf::Event event;
    while (window.pollEvent(event)) {
        // Close window: exit
        if (event.type == sf::Event::Closed ||
            (event.type == sf::Event::KeyReleased && event.key.code == sf::Keyboard::Escape)) {
            return UIAction::CLOSE_GAME;
        }

        if (event.type == sf::Event::KeyPressed) {
            firstTimeKeyPresses_.insert(event.key.code);
        }

        if (event.type == sf::Event::MouseButtonPressed) {
            firstTimeButtonPresses_.insert(event.mouseButton.button);
        }
    }

    // Handle walking
    auto view = registry.view<InputComponent>();
    for (auto entity : view) {
        for (const auto &kv : registry.get<InputComponent>(entity)) {
            if(!std::visit(*this, kv.first)) {
                continue;
            }

            InputAction action = kv.second;

            if (Movement* movement = registry.try_get<Movement>(entity)) {
                handleMovement(entity, action, *movement);
            }

            if (action == InputAction::FIRE_ROPE) {
                dispatcher_.enqueue(FireRopeEvent { entity });
            }
        }
    }

    return UIAction::NO_ACTION;
}

bool InputManager::operator() (sf::Keyboard::Key key) const {
    return sf::Keyboard::isKeyPressed(key);
}

bool InputManager::operator() (sf::Mouse::Button button) const {
    return sf::Mouse::isButtonPressed(button);
}

bool InputManager::operator() (JustPressed<sf::Keyboard::Key> key) const {
    return firstTimeKeyPresses_.contains(key.value);
}

bool InputManager::operator() (JustPressed<sf::Mouse::Button> button) const {
    return firstTimeButtonPresses_.contains(button.value);
}


void InputManager::handleMovement(entt::entity entity, InputAction action, Movement &movement) {
    if (action == InputAction::WALK_RIGHT) {
        movement.direction += 1;
    }

    if (action == InputAction::WALK_LEFT) {
        movement.direction -= 1;
    }

    if (action == InputAction::JUMP) {
        dispatcher_.enqueue(JumpEvent { entity });
    }
}