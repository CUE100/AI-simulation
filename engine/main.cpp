#include <SFML/Graphics.hpp>
#include "FastNoiseLite.h"
#include <optional>
#include <map>
#include <cmath>
#include <cstdint>

constexpr int Chunk_Size = 32;
constexpr int Tile_Size = 8;
constexpr int Chunk_Unload_Radius = 8;
constexpr float Camera_Zoom = 3.f;

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
    Swamp = 4,
    WaterBody = 5,
    Mountain = 6
};

struct Tile {
    float height;
    float moisture;
    float temperature;
    float fertility;
    BiomeType biome;
};

struct Tree
{
    sf::Vector2f position;
};

struct Chunk
{
    Tile tiles[Chunk_Size][Chunk_Size];

    std::vector<Tree> trees;
    bool treesGenerated = false;
};

struct ChunkCoord
{
    int x;
    int y;
};

int floorDiv(int value, int divisor)
{
    int quotient = value / divisor;
    int remainder = value % divisor;

    if (remainder != 0 && ((remainder < 0) != (divisor < 0)))
    {
        --quotient;
    }

    return quotient;
}

int floorTile(float worldPosition)
{
    return static_cast<int>(std::floor(worldPosition / static_cast<float>(Tile_Size)));
}



Chunk generateChunk(int chunkX, int chunkY, FastNoiseLite& noise, FastNoiseLite& moistureNoise, FastNoiseLite& temperatureNoise)
{
    Chunk chunk;

    for (int localY = 0; localY < Chunk_Size; ++localY)
    {
        for (int localX = 0; localX < Chunk_Size; ++localX)
        {
            int worldTileX = chunkX * Chunk_Size + localX;
            int worldTileY = chunkY * Chunk_Size + localY;

            Tile tile;
            tile.height = (noise.GetNoise(
                static_cast<float>(worldTileX) * 0.05f,
                static_cast<float>(worldTileY) * 0.05f) *0.7f + temperatureNoise.GetNoise(static_cast<float>(worldTileX) * 0.05f, static_cast<float>(worldTileY) * 0.05f) * 0.2f + moistureNoise.GetNoise(static_cast<float>(worldTileX) * 0.05f, static_cast<float>(worldTileY) * 0.05f) * 0.1f + 1.f) * 0.5f;
            tile.moisture =
                (moistureNoise.GetNoise(
                     worldTileX * 0.03f,
                     worldTileY * 0.03f) +
                 1.f) *
                0.5f;

            tile.temperature =
                (temperatureNoise.GetNoise(
                     worldTileX * 0.01f,
                     worldTileY * 0.01f) +
                 1.f) *
                0.5f;

            tile.fertility = 0.f;
            if (tile.height < 0.3f)
                tile.biome = BiomeType::WaterBody;
            else if (tile.height > 0.7f)
                tile.biome = BiomeType::Mountain;
            else if (tile.temperature > 0.7f &&
                     tile.moisture < 0.3f)
                tile.biome = BiomeType::Desert;
            else if (tile.temperature < 0.3f)
                tile.biome = BiomeType::Tundra;
            else if (tile.moisture > 0.7f)
                tile.biome = BiomeType::Swamp;
            else
                tile.biome = BiomeType::Forest;

            chunk.tiles[localY][localX] = tile;
        }
    }

    return chunk;
}

Chunk& getChunk(std::map<std::pair<int, int>, Chunk>& chunks, int chunkX, int chunkY, FastNoiseLite& noise, FastNoiseLite& moistureNoise, FastNoiseLite& temperatureNoise)
{
    auto key = std::make_pair(chunkX, chunkY);
    auto it = chunks.find(key);

    if (it == chunks.end())
    {
        it = chunks.emplace(key, generateChunk(chunkX, chunkY, noise, moistureNoise, temperatureNoise)).first;
    }

    return it->second;
}

void unloadFarChunks(std::map<std::pair<int, int>, Chunk>& chunks, int centerChunkX, int centerChunkY)
{
    for (auto it = chunks.begin(); it != chunks.end();)
    {
        int distanceX = std::abs(it->first.first - centerChunkX);
        int distanceY = std::abs(it->first.second - centerChunkY);

        if (distanceX > Chunk_Unload_Radius || distanceY > Chunk_Unload_Radius)
        {
            it = chunks.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void SpawnTree(sf::RenderWindow& window,sf::Vector2f position)
{
    sf::Color treeColor(54, 139, 70);
    sf::Vector2f center(position);
    float radius = 10.f;

    sf::VertexArray tree(sf::PrimitiveType::TriangleFan , 8);

    tree[0].position = center;
    tree[0].color = treeColor;

    for (size_t i = 0; i < 7; ++i)
    {
        // Convert index to radians (60 degrees per step)
        float angle = i * 2 * M_PI / 6;

        // Calculate X and Y coordinates relative to the center
        float x = center.x + radius * std::cos(angle);
        float y = center.y + radius * std::sin(angle);

        tree[i + 1].position = sf::Vector2f(x, y);
        tree[i + 1].color = treeColor;
    }


    window.draw(tree);
}

int main()
{
    sf::RenderWindow window(sf::VideoMode({800, 600}), "AI Simulation");
    window.setFramerateLimit(60);

    sf::RectangleShape tileShape(sf::Vector2f(Tile_Size, Tile_Size));

    Player player;
    player.shape.setPosition({200, 150});
    sf::View cam(sf::FloatRect({
        0.f,
        0.f
    }, {
        static_cast<float>(window.getSize().x) * Camera_Zoom,
        static_cast<float>(window.getSize().y) * Camera_Zoom
    }));
    cam.setCenter(player.shape.getPosition());

    FastNoiseLite noise;
    noise.SetSeed(42);
    FastNoiseLite moistureNoise;
    FastNoiseLite temperatureNoise;

    moistureNoise.SetSeed(123);
    temperatureNoise.SetSeed(456);

    std::map<std::pair<int, int>, Chunk> chunks;

    while (window.isOpen())
    {
        while (std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }

            if (const auto* resized = event->getIf<sf::Event::Resized>())
            {
                cam.setSize({
                    static_cast<float>(resized->size.x) * Camera_Zoom,
                    static_cast<float>(resized->size.y) * Camera_Zoom
                });
            }
        }

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) player.shape.move({0, -1 * 10});
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) player.shape.move({0, 1 * 10});
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) player.shape.move({-1 * 10, 0});
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) player.shape.move({1 * 10, 0});

        auto pos = player.shape.getPosition();

        cam.setCenter(pos);
        window.setView(cam);

        int playerTileX = floorTile(pos.x);
        int playerTileY = floorTile(pos.y);
        int playerChunkX = floorDiv(playerTileX, Chunk_Size);
        int playerChunkY = floorDiv(playerTileY, Chunk_Size);
        unloadFarChunks(chunks, playerChunkX, playerChunkY);

        float left = cam.getCenter().x - cam.getSize().x / 2.f;
        float right = cam.getCenter().x + cam.getSize().x / 2.f;
        float top = cam.getCenter().y - cam.getSize().y / 2.f;
        float bottom = cam.getCenter().y + cam.getSize().y / 2.f;

        int startTileX = floorTile(left);
        int endTileX = floorTile(right) + 1;
        int startTileY = floorTile(top);
        int endTileY = floorTile(bottom) + 1;

        int startChunkX = floorDiv(startTileX, Chunk_Size);
        int endChunkX = floorDiv(endTileX - 1, Chunk_Size);
        int startChunkY = floorDiv(startTileY, Chunk_Size);
        int endChunkY = floorDiv(endTileY - 1, Chunk_Size);

        window.clear(sf::Color::Black);

        for (int chunkY = startChunkY; chunkY <= endChunkY; ++chunkY)
        {
            for (int chunkX = startChunkX; chunkX <= endChunkX; ++chunkX)
            {
                Chunk &chunk = getChunk(chunks, chunkX, chunkY, noise, moistureNoise, temperatureNoise);

                if (!chunk.treesGenerated)
                {
                    for (int y = 0; y < Chunk_Size; ++y)
                    {
                        for (int x = 0; x < Chunk_Size; ++x)
                        {
                            float height = chunk.tiles[y][x].height;

                            if (height > 0.4f && height < 0.7f)
                            {
                                if (rand() % 100 < 1)
                                {
                                    Tree tree;

                                    tree.position = {
                                        float((chunkX * Chunk_Size + x) * Tile_Size),
                                        float((chunkY * Chunk_Size + y) * Tile_Size)};

                                    bool canPlace = true;

                                    for (const Tree &other : chunk.trees)
                                    {
                                        float dx = tree.position.x - other.position.x;
                                        float dy = tree.position.y - other.position.y;

                                        if (dx * dx + dy * dy < 16.f * 16.f)
                                        {
                                            canPlace = false;
                                            break;
                                        }
                                    }

                                    if (canPlace)
                                    {
                                        chunk.trees.push_back(tree);
                                    }
                                }
                            }
                        }
                    }

                    chunk.treesGenerated = true;
                }

                for (int localY = 0; localY < Chunk_Size; ++localY)
                {
                    for (int localX = 0; localX < Chunk_Size; ++localX)
                    {
                        int tileX = chunkX * Chunk_Size + localX;
                        int tileY = chunkY * Chunk_Size + localY;

                        if (tileX < startTileX ||
                            tileX >= endTileX ||
                            tileY < startTileY ||
                            tileY >= endTileY)
                        {
                            continue;
                        }

                        tileShape.setPosition({float(tileX * Tile_Size),
                                               float(tileY * Tile_Size)});

                        float height = chunk.tiles[localY][localX].height;

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

                bool canPlace = true;
                for (const Tree &tree : chunk.trees)
                {
                    SpawnTree(window, tree.position);
                }
            }
        }

        window.draw(player.shape);
        window.display();
    }

    return 0;
}
