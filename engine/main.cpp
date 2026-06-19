#include <iostream>
#include <SFML/Graphics.hpp>
#include "FastNoiseLite.h"
#include <optional>

struct Player {
    sf::CircleShape shape;

    Player() {
        shape.setRadius(20.f);
        shape.setFillColor(sf::Color::Red);
    }
};
enum class BiomeType : uint8_t
{
    Desert = 0,
    Forest = 1,
    Tundra = 2,
    Grassland = 3,
    Swamp = 4
};

struct Tile {
    float height;
    float moisture;
    float temperature;
    float fertility;
    BiomeType biome;
};



int main()
{
    sf::RenderWindow window(sf::VideoMode({800, 600}), "AI Simulation");
    window.setFramerateLimit(60);

    sf::RectangleShape tileShape(sf::Vector2f(8, 8));
    tileShape.setSize({8, 8});

    Player player;
    player.shape.setPosition({200, 150});
    sf::View cam(sf::FloatRect({0.f, 0.f}, {1920, 1080}));
    cam.setCenter(player.shape.getPosition());

    FastNoiseLite noise;
    noise.SetSeed(42);

    int worldWidth = 1000;
    int worldHeight = 1000;
    std::vector<std::vector<Tile>> world(worldHeight, std::vector<Tile>(worldWidth));

    for (int y = 0; y < worldHeight; ++y) {
        for (int x = 0; x < worldWidth; ++x) {
            Tile tile;
            tile.height = ((noise.GetNoise(static_cast<float>(x) * 0.1f, static_cast<float>(y) * 0.1f))+1.0f)*0.5f; // Normalize to [0, 1]

            world[y][x] = tile;
        }
    }

        while (window.isOpen())
        {
            while (std::optional event = window.pollEvent())
            {
                if (event->is<sf::Event::Closed>())
                {
                    window.close();
                }

            }

            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) player.shape.move({0, -1*10});
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) player.shape.move({0, 1*10});
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) player.shape.move({-1*10, 0});
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) player.shape.move({1*10, 0});

            cam.setCenter(player.shape.getPosition());
            window.setView(cam);

            window.clear(sf::Color::Black);
            
            for (int y = 0; y < worldHeight; y++)
            {
                for (int x = 0; x < worldWidth; x++)
                {
                    tileShape.setPosition({static_cast<float>(x) * 8, static_cast<float>(y) * 8});
                    float height = world[y][x].height;

                    if (height < 0.3f)

                        tileShape.setFillColor(sf::Color::Blue);

                    else if (height < 0.4f)

                        tileShape.setFillColor(sf::Color::Yellow);

                    else if (height < 0.7f)

                        tileShape.setFillColor(sf::Color::Green);

                    else

                        tileShape.setFillColor(sf::Color(120, 120, 120));

                    window.draw(tileShape);
                }
            }
            window.draw(player.shape);
            window.display();
        }

    return 0;
}