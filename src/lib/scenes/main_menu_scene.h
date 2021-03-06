#ifndef SLINGER_MAIN_MENU_SCENE_H
#define SLINGER_MAIN_MENU_SCENE_H

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Text.hpp>
#include <variant>
#include <SFML/Graphics/RectangleShape.hpp>
#include <entt/signal/dispatcher.hpp>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>

#include "scene.h"

using MenuAction = std::variant<ExitGame, StartLevel, std::monostate, OpenTutorial>;

class MenuItem {
    const static float PADDING;
    const static float MARGIN;

    sf::Text label_;
    sf::RectangleShape border_;
    const bool title_;
    const MenuAction menuAction_;

public:
    explicit MenuItem(sf::Text label_, MenuAction action, bool title = false);

    float getWidth();
    float getHeight() const;
    void setPosition(float x, float y);
    void render(sf::RenderWindow& window);
    void handleMousePress(float x, float y, entt::dispatcher &dispatcher);

private:
    bool isClicked(float x, float y);
};

struct MenuSpacer {
    float height = 35.f;
};

class Menu {
    const sf::Font& font_;
    sf::RenderWindow& window_;
    std::vector<std::variant<MenuItem, MenuSpacer>> items_;
    bool repositioned = false;
    float textHeight_;

public:
    explicit Menu(const sf::Font& font, sf::RenderWindow& window);

    void addItem(const std::string& label, MenuAction action);
    void addTitle(const std::string& label);
    void addSpacer();
    void render();
    void reposition();
    void handleMousePress(int x, int y, entt::dispatcher &dispatcher);

private:
    float getHeight();
};

class LevelInfo {
    std::filesystem::path path_;
    std::string displayName_;
    std::optional<sf::Time> completionTime_;

public:
    explicit LevelInfo(std::filesystem::path path, std::optional<sf::Time> completionTime);
    [[nodiscard]] const std::filesystem::path &getPath() const;
    [[nodiscard]] const std::string &getDisplayName() const;
};

class MainMenuScene : public Scene {
    const static float MARGIN;

    const std::string& levelLocation_;
    sf::RenderWindow& window_;
    entt::dispatcher& sceneDispatcher_;

    sf::Font font_;
    sf::Text authorText_;
    sf::Text titleText_;
    Menu menu_;

public:
    explicit MainMenuScene(
        const std::string& levelLocation,
        sf::RenderWindow& window,
        entt::dispatcher& sceneDispatcher,
        const nlohmann::json& times
    );
    void step() override;

private:
    void reposition(int width, int height);
    std::vector<LevelInfo> getLevels(const std::string& levelsLoc, const nlohmann::json& times);
};

#endif //SLINGER_MAIN_MENU_SCENE_H
