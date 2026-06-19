#include <iostream>
#include <SFML/Graphics.hpp>
#include "FastNoiseLite.h"
#include <optional>

int main()
{
    sf::RenderWindow window(sf::VideoMode({800, 600}), "AI Simulation");
    window.setFramerateLimit(60);

    while (window.isOpen()){
        while(std::optional event = window.pollEvent()){
            if (event->is<sf::Event::Closed>()){
                window.close();
            }

            window.clear(sf::Color::Black);
            window.display();

        }
    }

    return 0;
}