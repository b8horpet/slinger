#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG

#include <iostream>
#include <regex>

#include <spdlog/spdlog.h>
#include <pugixml.hpp>

#include "map_maker.h"
#include "body_builder.h"
#include "input_manager.h"

const std::regex PathBuilder::PATH_REGEX = std::regex(R"(\s?([^\d])(?:\s(-?\d+\.?\d+)(?:,(-?\d+\.?\d*))?)?)");

namespace {
    sf::Color RED = sf::Color(255, 100, 50);
}

Dimensions::Dimensions(const pugi::xml_node &node) {
    width = node.attribute("width").as_float();
    height = node.attribute("height").as_float();
    x = node.attribute("x").as_float();
    y = node.attribute("y").as_float() * -1.f;
    x += width/2.f;
    y -= height/2.f;
}

MapMaker::MapMaker(entt::registry &registry, Physics &physics): mapShapeBuilder_(MapShapeBuilder(registry, physics))
{

}

void MapMaker::make(const std::string& path)
{
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(path.c_str());

    if (!result) {
        throw std::runtime_error("could not find file: " + path);
    }

    auto walls = doc.select_nodes("/svg/g[@inkscape:label='walls']/*");
    for (const auto& wall: walls) {
        mapShapeBuilder_.makeWall(wall.node());
    }
    SPDLOG_DEBUG("Added {} walls from {}", walls.size(), path);

    auto deathZones = doc.select_nodes("/svg/g[@inkscape:label='death_zones']/*");
    SPDLOG_DEBUG("Added {} death zones from {}", deathZones.size(), path);
    for (const auto& zone: deathZones) {
        mapShapeBuilder_.makeDeathZone(zone.node());
    }

    auto checkPoints = doc.select_nodes("/svg/g[@inkscape:label='checkpoints']/rect");
    SPDLOG_DEBUG("Added {} checkpoints from {}", checkPoints.size(), path);
    for (const auto& zone: checkPoints) {
        mapShapeBuilder_.makeCheckpoint(zone.node());
    }

    auto playerNode = doc.select_node("/svg/g[@inkscape:label='objects']/rect[@id='player']");
    if (!playerNode) {
        throw std::runtime_error("Could not find player in svg " + path);
    }

    mapShapeBuilder_.makePlayer(playerNode.node());

    SPDLOG_INFO("Successfully loaded {} as the current level", path);
}

void MapShapeBuilder::makeWall(const pugi::xml_node &node) {
    if (strcmp(node.name(), "rect") == 0) {
        makeRect(node);
    }
    else if (strcmp(node.name(), "path") == 0) {
        makePolygon(node);
    } else {
        throw std::runtime_error("Unsupported element type for wall");
    }
}


MapShapeBuilder::MapShapeBuilder(entt::registry &registry, Physics &physics):
    registry_(registry),
    physics_(physics)
{

}

void MapShapeBuilder::makePlayer(const pugi::xml_node &node) {
    Dimensions dimensions(node);

    // The rectangle player
    auto player = BodyBuilder(registry_, physics_)
        .setPos(dimensions.x, dimensions.y)
        .setFixedRotation(true)
        .addRect(1, 2)
            .setColor(sf::Color(100, 200, 50))
            .makeFixture()
            .draw()
            .setZIndex(0)
            .create()
        .addRect(0.8f, 0.1f)
            .setPos(0, -1)
            .setSensor()
            .setColor(sf::Color(255, 255, 255))
            .makeFixture()
            .setFootSensor()
            .draw()
            .setZIndex(1)
            .create()
        .create();

    // Follow the player and enable checkpoints
    registry_.emplace<Follow>(player);
    registry_.emplace<Respawnable>(player, sf::Vector2f(dimensions.x, dimensions.y), sf::seconds(2));

    // Add movement to player
    registry_.emplace<Movement>(player);
    registry_.emplace<InputComponent>(
        player, InputComponent {
            {sf::Keyboard::Key::A, InputAction::WALK_LEFT},
            {sf::Keyboard::Key::D, InputAction::WALK_RIGHT},
            {JustPressed(sf::Keyboard::Space), Jump { 8 } }
        });

    // The players arm
    auto arm = BodyBuilder(registry_, physics_)
        .setPos(dimensions.x, dimensions.y)
        .addRect(0.3f, 1)
            .setColor(sf::Color(255, 255, 255))
            .setSensor()
            .makeFixture()
            .draw()
            .setZIndex(1)
            .setDensity(0)
            .setPos(0, 0.4f)
            .create()
        .attach(player, 0, 0.7f, 0, 0)
        .setFixedRotation(true)
        .create();

    // Add arm inputs
    registry_.emplace<entt::tag<"rotate_to_mouse"_hs>>(arm);
    registry_.emplace<InputComponent>(arm, InputComponent {
        {
            JustPressed(sf::Mouse::Left), FireRope {
            sf::Vector2f(0, 0.5f),
            sf::Vector2f(0, 0.7f)
        }
        }
    });
}


entt::entity MapShapeBuilder::makeRect(const pugi::xml_node& node) {
    Dimensions dimensions(node);

    return BodyBuilder(registry_, physics_)
        .setPos(dimensions.x, dimensions.y)
        .setType(b2_staticBody)
        .addRect(dimensions.width, dimensions.height)
            .setColor(RED)
            .draw()
            .makeFixture()
            .setZIndex(0)
            .create()
        .create();
}

entt::entity MapShapeBuilder::makePolygon(const pugi::xml_node &node) {
    auto svgPoints = node.attribute("d").as_string();
    auto points = PathBuilder::build(svgPoints);

    SPDLOG_INFO("Would make a polygon with the following points: {}", svgPoints);

    return BodyBuilder(registry_, physics_)
        .setPos(0, 0)
        .setType(b2_staticBody)
        .addPolygon(points)
            .setColor(RED)
            .draw()
            .setZIndex(5)
            .makeFixture()
            .create()
        .create();
}

entt::entity MapShapeBuilder::makeDeathZone(const pugi::xml_node &node) {
    entt::entity entity;

    if (strcmp(node.name(), "rect") == 0) {
        Dimensions dimensions(node);
        entity = BodyBuilder(registry_, physics_)
            .setPos(dimensions.x, dimensions.y)
            .setType(b2_staticBody)
                .addRect(dimensions.width, dimensions.height)
                .setSensor()
                .makeFixture()
                .create()
            .create();

    } else if (strcmp(node.name(), "path") == 0) {
        auto svgPoints = node.attribute("d").as_string();
        auto points = PathBuilder::build(svgPoints);

        entity = BodyBuilder(registry_, physics_)
            .setPos(0, 0)
            .setType(b2_staticBody)
            .addPolygon(points)
                .setColor(RED)
                .setZIndex(5)
                .makeFixture()
                .setSensor()
                .create()
            .create();
    } else {
        throw std::runtime_error("Unsupported element type for death zone");
    }

    registry_.emplace<DeathZone>(entity);
    return entity;
}

void MapShapeBuilder::makeCheckpoint(const pugi::xml_node &node) {
    Dimensions dimensions(node);

    if (strcmp(node.name(), "rect") != 0) {
        throw std::runtime_error("Unsupported element type for checkpoint");
    }

    auto entity = BodyBuilder(registry_, physics_)
        .setPos(dimensions.x, dimensions.y)
        .setType(b2_staticBody)
        .addRect(dimensions.width, dimensions.height)
            .setSensor()
            .makeFixture()
            .create()
        .create();

    // Respawn at the bottom of the checkpoint with a small jump for the player
    auto respawnLoc = sf::Vector2f(dimensions.x, (dimensions.y - dimensions.height / 2.f) + 2.3f);

    registry_.emplace<Checkpoint>(entity, respawnLoc);
}

PathBuilder::CommandList PathBuilder::getCommands(const std::string &svgPath) {
    auto list = PathBuilder::CommandList();

    std::string searchString = svgPath;
    auto matchResults = std::smatch{};
    while(std::regex_search(searchString, matchResults, PathBuilder::PATH_REGEX))
    {
        if (matchResults.size() < 4) {
            throw std::runtime_error("SVG path string malformed, not enough arguments");
        }

        auto command = static_cast<PathBuilder::Command>(matchResults[1].str()[0]);

        // Default is no arguments
        Arguments args = std::monostate {};

        // Override the empty args if there are arguments
        if (matchResults[3].matched) {
            // There are two arguments, a vector
            args = sf::Vector2f(std::stof(matchResults[2]), std::stof(matchResults[3]));
        } else if (matchResults[2].matched) {
            // A single argument
            args = std::stof(matchResults[2]);
        }

        list.emplace_back(std::make_pair(command, args));

        // Remove the captured string from the path and try again
        searchString = matchResults.suffix();
    }

    return list;
}

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };

PointList PathBuilder::getPoints(const PathBuilder::CommandList &commands) {
    auto list = PointList();

    size_t index = 0;
    for (const auto& commandPair : commands) {
        Command command = commandPair.first;
        Arguments arguments = commandPair.second;

        if (command == Command::MoveTo || command == Command::RelativeMoveTo) {
            if (!std::holds_alternative<sf::Vector2f>(arguments)) {
                throw std::runtime_error("Unsupported arguments");
            }

            auto point = std::get<sf::Vector2f>(arguments);
            point.y *= -1.f;
            list.emplace_back(point);
        }

        if (command == Command::LineTo) {
            if (!std::holds_alternative<sf::Vector2f>(arguments)) {
                throw std::runtime_error("Unsupported arguments");
            }

            auto point = std::get<sf::Vector2f>(arguments);
            point.y *= -1.f;
            //point = list.at(index - 1) + point;
            list.emplace_back(point);
        }

        if (command == Command::RelativeLineTo) {
            if (!std::holds_alternative<sf::Vector2f>(arguments)) {
                throw std::runtime_error("Unsupported arguments");
            }

            auto point = std::get<sf::Vector2f>(arguments);
            point.y *= -1.f;
            point = list.at(index - 1) + point;
            list.emplace_back(point);
        }

        if (command == Command::HorizontalLineTo) {
            if (!std::holds_alternative<float>(arguments)) {
                throw std::runtime_error("Unsupported arguments");
            }

            auto point = sf::Vector2f(std::get<float>(arguments), list.at(index - 1).y);
            list.emplace_back(point);
        }

        if (command == Command::RelativeHorizontalLineTo) {
            if (!std::holds_alternative<float>(arguments)) {
                throw std::runtime_error("Unsupported arguments");
            }

            auto point = list.at(index - 1) + sf::Vector2f(std::get<float>(arguments), 0);
            list.emplace_back(point);
        }

        if (command == Command::VerticalLineTo) {
            if (!std::holds_alternative<float>(arguments)) {
                throw std::runtime_error("Unsupported arguments");
            }

            auto point = sf::Vector2f(list.at(index - 1).x, -std::get<float>(arguments));
            list.emplace_back(point);
        }

        if (command == Command::RelativeVerticalLineTo) {
            if (!std::holds_alternative<float>(arguments)) {
                throw std::runtime_error("Unsupported arguments");
            }

            auto point = list.at(index - 1) + sf::Vector2f(0, -std::get<float>(arguments));
            list.emplace_back(point);
        }

        if (command == Command::ClosePath) {
            if (!std::holds_alternative<std::monostate>(arguments)) {
                throw std::runtime_error("Unsupported arguments");
            }

            list.emplace_back(list.at(0));
        }

        index++;
    }

    return list;
}

PointList PathBuilder::build(const std::string &string) {
    auto commands = getCommands(string);

    return getPoints(commands);
}
