#include <SFML/Graphics.hpp>
#include "FastNoiseLite.h"
#include <optional>
#include <map>
#include <cmath>
#include <cstdint>
#include <climits>

constexpr int Chunk_Size = 32;
constexpr int Tile_Size = 8;
constexpr int Chunk_Unload_Radius = 8;
constexpr float Camera_Zoom = 3.f;
constexpr int Tree_Cell = 6;
constexpr float TreeSpacing = 32.f;

sf::Color LerpColor(const sf::Color &a,
                    const sf::Color &b,
                    float t)
{
    t = std::clamp(t, 0.f, 1.f);

    return sf::Color(
        static_cast<uint8_t>(a.r + (b.r - a.r) * t),
        static_cast<uint8_t>(a.g + (b.g - a.g) * t),
        static_cast<uint8_t>(a.b + (b.b - a.b) * t));
}

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
    sf::Color color;
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



Chunk generateChunk(int chunkX, int chunkY, FastNoiseLite& noise, FastNoiseLite& moistureNoise, FastNoiseLite& temperatureNoise, FastNoiseLite& continentNoise, FastNoiseLite& treeGenerator,FastNoiseLite& biomeNoise)
{
    Chunk chunk;

    for (int localY = 0; localY < Chunk_Size; ++localY)
    {
        for (int localX = 0; localX < Chunk_Size; ++localX)
        {
            int worldTileX = chunkX * Chunk_Size + localX;
            int worldTileY = chunkY * Chunk_Size + localY;

            Tile tile;

            
            float continent =
                (continentNoise.GetNoise(
                     worldTileX * 0.005f,
                     worldTileY * 0.005f) +
                 1.f) *
                0.5f;
            tile.height = (noise.GetNoise(
                static_cast<float>(worldTileX) * 0.05f,
                static_cast<float>(worldTileY) * 0.05f) *0.7f + continent * 0.3f + 1.f) * 0.5f;
            float variation =
                (biomeNoise.GetNoise(
                     worldTileX * 0.05f,
                     worldTileY * 0.05f) +
                 1.f) *
                0.5f;

           
            tile.moisture =
                (moistureNoise.GetNoise(
                     worldTileX * 0.03f,
                     worldTileY * 0.03f) +
                 1.f) *
                0.5f;

            float largeTemp =

                (temperatureNoise.GetNoise(

                     worldTileX * 0.01f,

                     worldTileY * 0.01f) +
                 1.f) *
                0.5f;

            float smallTemp =

                (temperatureNoise.GetNoise(

                     worldTileX * 0.03f,

                     worldTileY * 0.03f) +
                 1.f) *
                0.5f;

            tile.temperature =

                tile.temperature =

                    largeTemp * 0.7f +

                    smallTemp * 0.2f + continent * 0.1f;

            tile.moisture += (variation - 0.5f) * 0.2f;
            tile.temperature += (variation - 0.5f) * 0.2f;

            tile.moisture = std::clamp(tile.moisture, 0.f, 1.f);

            tile.temperature = std::clamp(tile.temperature, 0.f, 1.f);

            tile.fertility = 0.f;
            if (tile.height < 0.30f)
            {
                tile.biome = BiomeType::WaterBody;
            }
            else if (tile.height > 0.75f)
            {
                tile.biome = BiomeType::Mountain;
            }
            else
            {
                float t = tile.temperature;
                float m = tile.moisture;

                if (t < 0.3f)
                {
                    if (m > 0.65f)
                        tile.biome = BiomeType::Swamp;
                    else
                        tile.biome = BiomeType::Tundra;
                }
                else if (t > 0.7f)
                {
                    if (m < 0.3f)
                        tile.biome = BiomeType::Desert;
                    else if (m > 0.7f)
                        tile.biome = BiomeType::Forest;
                    else
                        tile.biome = BiomeType::Grassland;
                }
                else
                {
                    if (m > 0.7f)
                        tile.biome = BiomeType::Swamp;
                    else if (m > 0.5f)
                        tile.biome = BiomeType::Forest;
                    else
                        tile.biome = BiomeType::Grassland;
                }
            }
            chunk.tiles[localY][localX] = tile;
        }
    }

    return chunk;
}

Chunk& getChunk(std::map<std::pair<int, int>, Chunk>& chunks, int chunkX, int chunkY, FastNoiseLite& noise, FastNoiseLite& moistureNoise, FastNoiseLite& temperatureNoise, FastNoiseLite& continentNoise, FastNoiseLite& treeGenerator,FastNoiseLite& biomeNoise)
{
    auto key = std::make_pair(chunkX, chunkY);
    auto it = chunks.find(key);

    if (it == chunks.end())
    {
        it = chunks.emplace(key, generateChunk(chunkX, chunkY, noise, moistureNoise, temperatureNoise, continentNoise, treeGenerator, biomeNoise)).first;
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
    FastNoiseLite moistureNoise;
    FastNoiseLite temperatureNoise;
    FastNoiseLite continentNoise;
    FastNoiseLite treeGenerator;
    FastNoiseLite biomeNoise;

    biomeNoise.SetSeed(777);
    noise.SetSeed(42);
    treeGenerator.SetSeed(789);
    continentNoise.SetSeed(999);
    moistureNoise.SetSeed(123);
    temperatureNoise.SetSeed(456);

    std::map<std::pair<int, int>, Chunk> chunks;

    sf::Color desert(220, 210, 120);
    sf::Color grass(110, 190, 70);
    sf::Color forest(40, 100, 50);
    sf::Color swamp(60, 90, 50);
    sf::Color tundra(190, 220, 220);

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

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) player.shape.move({0, -1 * 40});
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) player.shape.move({0, 1 * 40});
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) player.shape.move({-1 * 40, 0});
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) player.shape.move({1 * 40, 0});

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
                Chunk &chunk = getChunk(chunks, chunkX, chunkY, noise, moistureNoise, temperatureNoise, continentNoise, treeGenerator,biomeNoise);

                if (!chunk.treesGenerated)
                {
                    for (int y = 0; y < Chunk_Size; ++y)
                    {
                        for (int x = 0; x < Chunk_Size; ++x)
                        {
                            int worldX = chunkX * Chunk_Size + x;
                            int worldY = chunkY * Chunk_Size + y;

                            float height = chunk.tiles[y][x].height;
                            int cellX = floorDiv(worldX, Tree_Cell);
                            int cellY = floorDiv(worldY, Tree_Cell);

                            int seed =
                                cellX * 92837111 ^
                                cellY * 689287499;

                            float offsetX =
                                ((seed & 255) / 255.f - 0.5f) * TreeSpacing;

                            float offsetY =
                                (((seed >> 8) & 255) / 255.f - 0.5f) * TreeSpacing;

                            if (chunk.tiles[y][x].biome ==

                                BiomeType::Forest && height > 0.4f && height < 0.7f)
                            {
                                if(worldX%Tree_Cell == 0 && worldY % Tree_Cell == 0){
                                    float forestDensity =

                                        (treeGenerator.GetNoise(

                                            (chunkX * Chunk_Size + x) * 0.01f,

                                             (chunkY * Chunk_Size + y) * 0.01f) +
                                         1.f) *
                                        0.5f;

                                    float localVariation =

                                        (treeGenerator.GetNoise(

                                             (chunkX * Chunk_Size + x) * 0.3f,

                                             (chunkY * Chunk_Size + y) * 0.3f) +
                                         1.f) *
                                        0.5f;
                                    if ((forestDensity > 0.1f) && (localVariation > 0.3f))
                                    {
                                        Tree tree;

                                        tree.position = {
                                            float((chunkX * Chunk_Size + x) * Tile_Size)+ offsetX,
                                            float((chunkY * Chunk_Size + y) * Tile_Size)+ offsetY};

                                        bool canPlace = true;

                                        for (const Tree &other : chunk.trees)
                                        {
                                            float dx = tree.position.x - other.position.x;
                                            float dy = tree.position.y - other.position.y;

                                            if (dx * dx + dy * dy < TreeSpacing * TreeSpacing)
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

                        BiomeType biome = chunk.tiles[localY][localX].biome;

                        Tile &tile = chunk.tiles[localY][localX];

                        float t = tile.temperature;
                        float m = tile.moisture;
                        float h = tile.height;

                        sf::Color color;

                        if (h < 0.30f)
                        {
                            color = sf::Color(30, 100, 220); // water
                        }
                        else if (h > 0.75f)
                        {
                            if (h > 0.85f)
                                color = sf::Color::White;
                            else
                                color = sf::Color(120, 120, 120);
                        }
                        else
                        {
                            sf::Color cold(190, 220, 220);   // tundra
                            sf::Color grass(110, 190, 70);   // grassland
                            sf::Color forest(40, 100, 50);   // forest
                            sf::Color swamp(60, 90, 50);     // swamp
                            sf::Color desert(220, 210, 120); // desert

                            if (t < 0.3f)
                            {
                                color = LerpColor(cold, grass, t / 0.3f);
                            }
                            else if (t > 0.7f)
                            {
                                color = LerpColor(grass, desert,
                                                  (t - 0.7f) / 0.3f);
                            }
                            else
                            {
                                if (m > 0.7f)
                                {
                                    color = LerpColor(forest,
                                                      swamp,
                                                      (m - 0.7f) / 0.3f);
                                }
                                else
                                {
                                    color = LerpColor(grass,
                                                      forest,
                                                      m / 0.7f);
                                }
                            }
                        }

                        tileShape.setFillColor(color);

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
