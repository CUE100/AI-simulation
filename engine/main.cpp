#include <SFML/Graphics.hpp>
#include "FastNoiseLite.h"
#include <optional>
#include <map>
#include <cmath>
#include <cstdint>
#include <climits>
#include <algorithm>
#include <iostream>

constexpr int Chunk_Size = 32;
constexpr int Tile_Size = 16;
constexpr int Chunk_Unload_Radius = 16;
constexpr float Camera_Zoom = 4.f;
constexpr int Tree_Cell = 6;

const sf::Color WaterColor(30, 100, 220);
const sf::Color BeachColor(235, 220, 150);
const sf::Color GrassColor(110, 190, 70);
const sf::Color ForestColor(40, 100, 50);
const sf::Color SwampColor(60, 90, 50);
const sf::Color DesertColor(220, 210, 120);
const sf::Color TundraColor(190, 220, 220);
const sf::Color MountainColor(120, 120, 120);
const sf::Color SnowColor(255, 255, 255);

sf::Color lerpColor(sf::Color a, sf::Color b, float t)
{
    t = std::clamp(t, 0.f, 1.f);

    return sf::Color(
        a.r + (b.r - a.r) * t,
        a.g + (b.g - a.g) * t,
        a.b + (b.b - a.b) * t);
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
    Mountain = 6,
    Beach = 7
};

struct Tile {
    float height;
    float moisture;
    float temperature;
    float fertility;
    sf::Color color;
    BiomeType biome;
};

float SmoothStep(float edge0,
                 float edge1,
                 float x)
{
    x = std::clamp(
        (x - edge0) / (edge1 - edge0),
        0.f,
        1.f);

    return x * x * (3.f - 2.f * x);
}

struct Tree
{
    sf::Vector2f position;
    sf::Color color;
    float radius;
};

struct Chunk
{
    Tile tiles[Chunk_Size][Chunk_Size];

    std::vector<Tree> trees;

    sf::VertexArray terrain{sf::PrimitiveType::Triangles};
    sf::VertexArray treeMesh{sf::PrimitiveType::Triangles};

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

sf::Color landBiomeColor(BiomeType biome)
{
    switch (biome)
    {
        case BiomeType::Desert:
            return DesertColor;
        case BiomeType::Beach:
            return BeachColor;
        case BiomeType::Forest:
            return ForestColor;
        case BiomeType::Tundra:
            return TundraColor;
        case BiomeType::Swamp:
            return SwampColor;
        case BiomeType::Grassland:
        default:
            return GrassColor;
    }
}

bool isLandBiome(BiomeType biome)
{
    return biome != BiomeType::WaterBody && biome != BiomeType::Mountain;
}

sf::Color terrainColorForTile(const Chunk& chunk, int localX, int localY)
{
    const Tile& tile = chunk.tiles[localY][localX];

    if (tile.biome == BiomeType::WaterBody || tile.height < 0.30f)
    {
        return WaterColor;
    }

    if (tile.biome == BiomeType::Mountain || tile.height > 0.75f)
    {
        return tile.height > 0.82f ? SnowColor : MountainColor;
    }

    sf::Color color = landBiomeColor(tile.biome);
    int red = 0;
    int green = 0;
    int blue = 0;
    int differentLandNeighbors = 0;

    for (int offsetY = -1; offsetY <= 1; ++offsetY)
    {
        for (int offsetX = -1; offsetX <= 1; ++offsetX)
        {
            if (offsetX == 0 && offsetY == 0)
            {
                continue;
            }

            int sampleX = localX + offsetX;
            int sampleY = localY + offsetY;

            if (sampleX < 0 ||
                sampleX >= Chunk_Size ||
                sampleY < 0 ||
                sampleY >= Chunk_Size)
            {
                continue;
            }

            const Tile& neighbor = chunk.tiles[sampleY][sampleX];

            if (neighbor.biome == tile.biome || !isLandBiome(neighbor.biome))
            {
                continue;
            }

            sf::Color neighborColor = landBiomeColor(neighbor.biome);
            red += neighborColor.r;
            green += neighborColor.g;
            blue += neighborColor.b;
            ++differentLandNeighbors;
        }
    }

    if (differentLandNeighbors == 0)
    {
        return color;
    }

    sf::Color neighborTint(
        red / differentLandNeighbors,
        green / differentLandNeighbors,
        blue / differentLandNeighbors);

    float edgeAmount =
        SmoothStep(0.f, 0.25f, differentLandNeighbors / 8.f) * 0.16f;

    return lerpColor(color, neighborTint, edgeAmount);
}

void cacheChunkTerrainColors(Chunk& chunk)
{
    for (int localY = 0; localY < Chunk_Size; ++localY)
    {
        for (int localX = 0; localX < Chunk_Size; ++localX)
        {
            chunk.tiles[localY][localX].color =
                terrainColorForTile(chunk, localX, localY);
        }
    }
}

void rebuildChunkTerrainMesh(Chunk& chunk, int chunkX, int chunkY)
{
    chunk.terrain.resize(Chunk_Size * Chunk_Size * 6);

    int vertexIndex = 0;

    for (int localY = 0; localY < Chunk_Size; ++localY)
    {
        for (int localX = 0; localX < Chunk_Size; ++localX)
        {
            float left =
                static_cast<float>((chunkX * Chunk_Size + localX) * Tile_Size);
            float top =
                static_cast<float>((chunkY * Chunk_Size + localY) * Tile_Size);
            float right = left + static_cast<float>(Tile_Size);
            float bottom = top + static_cast<float>(Tile_Size);
            sf::Color color = chunk.tiles[localY][localX].color;

            chunk.terrain[vertexIndex + 0].position = {left, top};
            chunk.terrain[vertexIndex + 1].position = {right, top};
            chunk.terrain[vertexIndex + 2].position = {right, bottom};
            chunk.terrain[vertexIndex + 3].position = {left, top};
            chunk.terrain[vertexIndex + 4].position = {right, bottom};
            chunk.terrain[vertexIndex + 5].position = {left, bottom};

            for (int i = 0; i < 6; ++i)
            {
                chunk.terrain[vertexIndex + i].color = color;
            }

            vertexIndex += 6;
        }
    }
}

float hashFloat(int x, int y, uint32_t salt)
{
    uint32_t hash =
        static_cast<uint32_t>(x) * 374761393u ^
        static_cast<uint32_t>(y) * 668265263u ^
        salt * 2246822519u;

    hash = (hash ^ (hash >> 13u)) * 1274126177u;
    hash ^= hash >> 16u;

    return static_cast<float>(hash & 0x00FFFFFFu) /
           static_cast<float>(0x01000000u);
}

struct TreeSpawnSettings
{
    float spawnRate;
    float minHeight;
    float maxHeight;
    float minRadius;
    float maxRadius;
    sf::Color color;
};

TreeSpawnSettings treeSpawnSettings(BiomeType biome)
{
    switch (biome)
    {
        case BiomeType::Forest:
            return {0.86f, 0.34f, 0.72f, 8.f, 13.f, sf::Color(35, 120, 45)};
        case BiomeType::Swamp:
            return {0.58f, 0.32f, 0.70f, 8.f, 12.f, sf::Color(45, 95, 45)};
        case BiomeType::Grassland:
            return {0.20f, 0.32f, 0.72f, 6.f, 9.f, sf::Color(75, 145, 65)};
        case BiomeType::Tundra:
            return {0.07f, 0.32f, 0.68f, 5.f, 8.f, sf::Color(120, 155, 135)};
        case BiomeType::Desert:
            return {0.025f, 0.32f, 0.68f, 4.f, 6.f, sf::Color(145, 135, 80)};
        default:
            return {0.f, 0.f, 0.f, 0.f, 0.f, sf::Color::Transparent};
    }
}

float smallWaterThreshold(BiomeType biome)
{
    switch (biome)
    {
        case BiomeType::Swamp:
            return 0.68f;
        case BiomeType::Forest:
            return 0.78f;
        case BiomeType::Grassland:
            return 0.84f;
        case BiomeType::Tundra:
            return 0.90f;
        case BiomeType::Desert:
            return 0.96f;
        default:
            return 1.1f;
    }
}

bool hasAdjacentWater(const Chunk& chunk, int localX, int localY)
{
    for (int offsetY = -1; offsetY <= 1; ++offsetY)
    {
        for (int offsetX = -1; offsetX <= 1; ++offsetX)
        {
            if (offsetX == 0 && offsetY == 0)
            {
                continue;
            }

            int sampleX = localX + offsetX;
            int sampleY = localY + offsetY;

            if (sampleX < 0 ||
                sampleX >= Chunk_Size ||
                sampleY < 0 ||
                sampleY >= Chunk_Size)
            {
                continue;
            }

            if (chunk.tiles[sampleY][sampleX].biome == BiomeType::WaterBody)
            {
                return true;
            }
        }
    }

    return false;
}

void addBeachBiomes(Chunk& chunk)
{
    for (int localY = 0; localY < Chunk_Size; ++localY)
    {
        for (int localX = 0; localX < Chunk_Size; ++localX)
        {
            Tile& tile = chunk.tiles[localY][localX];

            if (tile.biome == BiomeType::WaterBody ||
                tile.biome == BiomeType::Mountain ||
                tile.biome == BiomeType::Swamp)
            {
                continue;
            }

            if ((tile.height >= 0.30f && tile.height < 0.34f) ||
                (tile.height < 0.72f && hasAdjacentWater(chunk, localX, localY)))
            {
                tile.biome = BiomeType::Beach;
            }
        }
    }
}

void generateTreesForChunk(Chunk& chunk,
                           int chunkX,
                           int chunkY,
                           FastNoiseLite& treeGenerator)
{
    chunk.trees.clear();

    int chunkStartX = chunkX * Chunk_Size;
    int chunkStartY = chunkY * Chunk_Size;
    int chunkEndX = chunkStartX + Chunk_Size - 1;
    int chunkEndY = chunkStartY + Chunk_Size - 1;

    int startCellX = floorDiv(chunkStartX, Tree_Cell);
    int endCellX = floorDiv(chunkEndX, Tree_Cell);
    int startCellY = floorDiv(chunkStartY, Tree_Cell);
    int endCellY = floorDiv(chunkEndY, Tree_Cell);

    for (int cellY = startCellY; cellY <= endCellY; ++cellY)
    {
        for (int cellX = startCellX; cellX <= endCellX; ++cellX)
        {
            int worldTileX = cellX * Tree_Cell + Tree_Cell / 2;
            int worldTileY = cellY * Tree_Cell + Tree_Cell / 2;

            if (worldTileX < chunkStartX ||
                worldTileX > chunkEndX ||
                worldTileY < chunkStartY ||
                worldTileY > chunkEndY)
            {
                continue;
            }

            int localX = worldTileX - chunkStartX;
            int localY = worldTileY - chunkStartY;
            const Tile& tile = chunk.tiles[localY][localX];
            TreeSpawnSettings settings = treeSpawnSettings(tile.biome);

            if (settings.spawnRate <= 0.f ||
                tile.height < settings.minHeight ||
                tile.height > settings.maxHeight)
            {
                continue;
            }

            float broadDensity =
                (treeGenerator.GetNoise(worldTileX * 0.015f,
                                        worldTileY * 0.015f) +
                 1.f) *
                0.5f;

            float localDensity =
                (treeGenerator.GetNoise(worldTileX * 0.18f,
                                        worldTileY * 0.18f) +
                 1.f) *
                0.5f;

            float spawnChance =
                settings.spawnRate * (0.50f + broadDensity * 0.50f);

            spawnChance *= (0.75f + localDensity * 0.25f);

            if (hashFloat(cellX, cellY, 17u) > spawnChance)
            {
                continue;
            }

            float jitter = static_cast<float>(Tree_Cell * Tile_Size) * 0.34f;
            float offsetX = (hashFloat(cellX, cellY, 29u) - 0.5f) * jitter;
            float offsetY = (hashFloat(cellX, cellY, 31u) - 0.5f) * jitter;
            float radiusMix = hashFloat(cellX, cellY, 43u);
            int shade =
                static_cast<int>((hashFloat(cellX, cellY, 47u) - 0.5f) * 28.f);

            Tree tree;
            tree.position = {
                (static_cast<float>(worldTileX) + 0.5f) *
                        static_cast<float>(Tile_Size) +
                    offsetX,
                (static_cast<float>(worldTileY) + 0.5f) *
                        static_cast<float>(Tile_Size) +
                    offsetY};

            tree.radius =
                settings.minRadius +
                (settings.maxRadius - settings.minRadius) * radiusMix;

            tree.color = sf::Color(
                static_cast<uint8_t>(
                    std::clamp(static_cast<int>(settings.color.r) + shade,
                               0,
                               255)),
                static_cast<uint8_t>(
                    std::clamp(static_cast<int>(settings.color.g) + shade,
                               0,
                               255)),
                static_cast<uint8_t>(
                    std::clamp(static_cast<int>(settings.color.b) + shade,
                               0,
                               255)));

            chunk.trees.push_back(tree);
        }
    }

    chunk.treesGenerated = true;
}

void buildTreeMesh(Chunk &chunk)
{
    chunk.treeMesh.clear();

    for (const Tree &tree : chunk.trees)
    {
        float r = tree.radius;

        float left = tree.position.x - r;
        float right = tree.position.x + r;
        float top = tree.position.y - r;
        float bottom = tree.position.y + r;
        sf::Vertex vertices[] = {
            // First Triangle
            {{left, top}, tree.color, {0.0f, 0.0f}},
            {{right, top}, tree.color, {0.0f, 0.0f}},
            {{right, bottom}, tree.color, {0.0f, 0.0f}},

            // Second Triangle
            {{left, top}, tree.color, {0.0f, 0.0f}},
            {{right, bottom}, tree.color, {0.0f, 0.0f}},
            {{left, bottom}, tree.color, {0.0f, 0.0f}}};

        for (const auto &vertex : vertices)
        {
            chunk.treeMesh.append(vertex);
        }
    }
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

            if (isLandBiome(tile.biome) &&
                tile.height > 0.36f &&
                tile.height < 0.70f)
            {
                float pond =
                    (biomeNoise.GetNoise(
                         worldTileX * 0.12f + 2000.f,
                         worldTileY * 0.12f - 2000.f) +
                     1.f) *
                    0.5f;


                float threshold = smallWaterThreshold(tile.biome);

                if (tile.moisture > 0.75f)
                {
                    threshold -= 0.04f;
                }
                else if (tile.moisture < 0.25f)
                {
                    threshold += 0.03f;
                }

                if (pond > threshold )
                {
                    tile.biome = BiomeType::WaterBody;
                }
            }

            chunk.tiles[localY][localX] = tile;
        }
    }

    addBeachBiomes(chunk);
    cacheChunkTerrainColors(chunk);
    rebuildChunkTerrainMesh(chunk, chunkX, chunkY);
    generateTreesForChunk(chunk, chunkX, chunkY, treeGenerator);
    buildTreeMesh(chunk);

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

int main()
{
    sf::RenderWindow window(sf::VideoMode({800, 600}), "AI Simulation");
    window.setFramerateLimit(1000);

    sf::Font font;
    if (!font.openFromFile("/System/Library/Fonts/Supplemental/Arial Black.ttf"))
    {
        std::cerr << "Failed to load font\n";
    }

    sf::Text text(font);
    text.setCharacterSize(64);
    text.setPosition({100, 100});
    text.setFillColor(sf::Color::White);

    sf::Clock clock;
    float fps = 0.0f;

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

 

    while (window.isOpen())
    {

        float deltaTime = clock.restart().asSeconds();
        fps = 1.0f / deltaTime;

        text.setString("FPS: " + std::to_string(static_cast<int>(fps)));

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
        text.setPosition({pos.x - 400, pos.y - 300});
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
                Chunk& chunk = getChunk(chunks,
                                        chunkX,
                                        chunkY,
                                        noise,
                                        moistureNoise,
                                        temperatureNoise,
                                        continentNoise,
                                        treeGenerator,
                                        biomeNoise);

                window.draw(chunk.terrain);
            }
        }

        for (int chunkY = startChunkY; chunkY <= endChunkY; ++chunkY)
        {
            for (int chunkX = startChunkX; chunkX <= endChunkX; ++chunkX)
            {
                auto it = chunks.find(std::make_pair(chunkX, chunkY));

                if (it == chunks.end())
                {
                    continue;
                }

                window.draw(it->second.treeMesh);
                int totalTrees = 0;

                for (auto &pair : chunks)
                {
                    totalTrees += pair.second.trees.size();
                }

               // std::cout << "Trees: " << totalTrees << "\n";
            }
        }

        window.draw(player.shape);
        window.draw(text);
        window.display();
    }

    return 0;
}
