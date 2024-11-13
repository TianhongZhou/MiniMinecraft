#include "terrain.h"
#include "cube.h"
#include <stdexcept>
#include <iostream>

#include <glm/gtc/random.hpp>

Terrain::Terrain(OpenGLContext *context)
    : m_chunks(), m_generatedTerrain(),
      m_chunkVBOsNeedUpdating(true), mp_context(context)
{}

Terrain::~Terrain() {

}

// Combine two 32-bit ints into one 64-bit int
// where the upper 32 bits are X and the lower 32 bits are Z
int64_t toKey(int x, int z) {
    int64_t xz = 0xffffffffffffffff;
    int64_t x64 = x;
    int64_t z64 = z;

    // Set all lower 32 bits to 1 so we can & with Z later
    xz = (xz & (x64 << 32)) | 0x00000000ffffffff;

    // Set all upper 32 bits to 1 so we can & with XZ
    z64 = z64 | 0xffffffff00000000;

    // Combine
    xz = xz & z64;
    return xz;
}

glm::ivec2 toCoords(int64_t k) {
    // Z is lower 32 bits
    int64_t z = k & 0x00000000ffffffff;
    // If the most significant bit of Z is 1, then it's a negative number
    // so we have to set all the upper 32 bits to 1.
    // Note the 8    V
    if(z & 0x0000000080000000) {
        z = z | 0xffffffff00000000;
    }
    int64_t x = (k >> 32);

    return glm::ivec2(x, z);
}

// Surround calls to this with try-catch if you don't know whether
// the coordinates at x, y, z have a corresponding Chunk
BlockType Terrain::getGlobalBlockAt(int x, int y, int z) const
{
    if(hasChunkAt(x, z)) {
        // Just disallow action below or above min/max height,
        // but don't crash the game over it.
        if(y < 0 || y >= 256) {
            return EMPTY;
        }
        const uPtr<Chunk> &c = getChunkAt(x, z);
        glm::vec2 chunkOrigin = glm::vec2(floor(x / 16.f) * 16, floor(z / 16.f) * 16);
        return c->getLocalBlockAt(static_cast<unsigned int>(x - chunkOrigin.x),
                                  static_cast<unsigned int>(y),
                                  static_cast<unsigned int>(z - chunkOrigin.y));
    }
    else {
        throw std::out_of_range("Coordinates " + std::to_string(x) +
                                " " + std::to_string(y) + " " +
                                std::to_string(z) + " have no Chunk!");
    }
}

BlockType Terrain::getGlobalBlockAt(glm::vec3 p) const {
    return getGlobalBlockAt(p.x, p.y, p.z);
}

bool Terrain::hasChunkAt(int x, int z) const {
    // Map x and z to their nearest Chunk corner
    // By flooring x and z, then multiplying by 16,
    // we clamp (x, z) to its nearest Chunk-space corner,
    // then scale back to a world-space location.
    // Note that floor() lets us handle negative numbers
    // correctly, as floor(-1 / 16.f) gives us -1, as
    // opposed to (int)(-1 / 16.f) giving us 0 (incorrect!).
    int xFloor = static_cast<int>(glm::floor(x / 16.f));
    int zFloor = static_cast<int>(glm::floor(z / 16.f));
    return m_chunks.find(toKey(16 * xFloor, 16 * zFloor)) != m_chunks.end();
}


uPtr<Chunk>& Terrain::getChunkAt(int x, int z) {
    int xFloor = static_cast<int>(glm::floor(x / 16.f));
    int zFloor = static_cast<int>(glm::floor(z / 16.f));
    return m_chunks[toKey(16 * xFloor, 16 * zFloor)];
}


const uPtr<Chunk>& Terrain::getChunkAt(int x, int z) const {
    int xFloor = static_cast<int>(glm::floor(x / 16.f));
    int zFloor = static_cast<int>(glm::floor(z / 16.f));
    return m_chunks.at(toKey(16 * xFloor, 16 * zFloor));
}

void Terrain::setGlobalBlockAt(int x, int y, int z, BlockType t)
{
    if(hasChunkAt(x, z)) {
        uPtr<Chunk> &c = getChunkAt(x, z);
        glm::vec2 chunkOrigin = glm::vec2(floor(x / 16.f) * 16, floor(z / 16.f) * 16);
        c->setLocalBlockAt(static_cast<unsigned int>(x - chunkOrigin.x),
                           static_cast<unsigned int>(y),
                           static_cast<unsigned int>(z - chunkOrigin.y),
                           t);
    }
    else {
        throw std::out_of_range("Coordinates " + std::to_string(x) +
                                " " + std::to_string(y) + " " +
                                std::to_string(z) + " have no Chunk!");
    }
}

Chunk* Terrain::instantiateChunkAt(int x, int z) {
    uPtr<Chunk> chunk = mkU<Chunk>(x, z, mp_context);
    Chunk *cPtr = chunk.get();
    m_chunks[toKey(x, z)] = std::move(chunk);
    // Set the neighbor pointers of itself and its neighbors
    if(hasChunkAt(x, z + 16)) {
        auto &chunkNorth = m_chunks[toKey(x, z + 16)];
        cPtr->linkNeighbor(chunkNorth, ZPOS);
    }
    if(hasChunkAt(x, z - 16)) {
        auto &chunkSouth = m_chunks[toKey(x, z - 16)];
        cPtr->linkNeighbor(chunkSouth, ZNEG);
    }
    if(hasChunkAt(x + 16, z)) {
        auto &chunkEast = m_chunks[toKey(x + 16, z)];
        cPtr->linkNeighbor(chunkEast, XPOS);
    }
    if(hasChunkAt(x - 16, z)) {
        auto &chunkWest = m_chunks[toKey(x - 16, z)];
        cPtr->linkNeighbor(chunkWest, XNEG);
    }

    return cPtr;
}

// TODO: When you make Chunk inherit from Drawable, change this code so
// it draws each Chunk with the given ShaderProgram
void Terrain::draw(int minX, int maxX, int minZ, int maxZ, ShaderProgram *shaderProgram) {
    for (int x = minX; x <= maxX; x += 16) {
        for (int z = minZ; z <= maxZ; z += 16) {
            const uPtr<Chunk> &chunk = getChunkAt(x, z);
            if (!chunk) {
                continue;
            }
            chunk->create(x, z);
            shaderProgram->drawInterleaved(*chunk);
        }
    }
}

glm::vec2 random2(glm::vec2 p) {
    return glm::fract(glm::sin(glm::vec2(glm::dot(p, glm::vec2(549.1, 874.2)), glm::dot(p, glm::vec2(764.1, 126.8)))) * 79871.465f);
}

float surflet(glm::vec2 P, glm::vec2 gridPoint) {
    float distX = abs(P.x - gridPoint.x);
    float distY = abs(P.y - gridPoint.y);
    float tX = 1.f - 6.f * pow(distX, 5.f) + 15.f * pow(distX, 4.f) - 10.f * pow(distX, 3.f);
    float tY = 1.f - 6.f * pow(distY, 5.f) + 15.f * pow(distY, 4.f) - 10.f * pow(distY, 3.f);

    glm::vec2 gradient = 2.f * random2(gridPoint) - glm::vec2(1.f);
    glm::vec2 diff = P - gridPoint;
    float height = glm::dot(diff, gradient);
    return height * tX * tY;
}

float PerlinNoise(glm::vec2 uv) {
    float surfletSum = 0.f;
    for(int dx = 0; dx <= 1; dx++) {
        for(int dy = 0; dy <= 1; dy++) {
            surfletSum += surflet(uv, glm::floor(uv) + glm::vec2(dx, dy));
        }
    }
    return surfletSum;
}

int calculateMountainHeight(int x, int z) {
    return (int) (std::pow((PerlinNoise(glm::vec2(x, z) / 50.f) + 1.f) / 2.f, 3.f) * 250.f);
}

int calculateGrasslandHeight(int x, int z) {
    return (int) ((0.5f - std::abs(PerlinNoise(glm::vec2(x, z) / 70.f))) * 30.f);
}

void Terrain::CreateTestScene()
{
    // // TODO: DELETE THIS LINE WHEN YOU DELETE m_geomCube!
    // // m_geomCube.createVBOdata();

    // // Create the Chunks that will
    // // store the blocks for our
    // // initial world space
    // for(int x = 0; x < 64; x += 16) {
    //     for(int z = 0; z < 64; z += 16) {
    //         instantiateChunkAt(x, z);
    //     }
    // }
    // // Tell our existing terrain set that
    // // the "generated terrain zone" at (0,0)
    // // now exists.
    // m_generatedTerrain.insert(toKey(0, 0));

    // // Create the basic terrain glm::floor
    // for(int x = 0; x < 64; ++x) {
    //     for(int z = 0; z < 64; ++z) {
    //         if((x + z) % 2 == 0) {
    //             setGlobalBlockAt(x, 128, z, STONE);
    //         }
    //         else {
    //             setGlobalBlockAt(x, 128, z, DIRT);
    //         }
    //     }
    // }
    // // Add "walls" for collision testing
    // for(int x = 0; x < 64; ++x) {
    //     setGlobalBlockAt(x, 129, 16, GRASS);
    //     setGlobalBlockAt(x, 130, 16, GRASS);
    //     setGlobalBlockAt(x, 129, 48, GRASS);
    //     setGlobalBlockAt(16, 130, x, GRASS);
    // }
    // // Add a central column
    // for(int y = 129; y < 140; ++y) {
    //     setGlobalBlockAt(32, y, 32, GRASS);
    // }
}

void Terrain::CreateInitialScene()
{
    int xMin = -2, xMax = 2;
    int zMin = -2, zMax = 2;

    for (int x = xMin; x <= xMax; x++) {
        for (int z = zMin; z <= zMax; z++) {
            m_generatedTerrain.insert(toKey(x * 64, z * 64));

            for(int m = x * 64; m < (x + 1) * 64; m += 16) {
                for(int n = z * 64; n < (z + 1) * 64; n += 16) {
                    instantiateChunkAt(m, n);
                    generateBiome(m, n);
                }
            }
        }
    }
}

void Terrain::generateBiome(int xMin, int zMin) {
    int baseHeight = 128;
    int waterLevel = 138;
    int snowLevel = 200;

    for(int x = xMin; x < xMin + 16; x++) {
        for(int z = zMin; z < zMin + 16; z++) {
            // This part is commemted out since otherwise
            // the game will be super lagging. I will uncomment
            // this part after the multithread in m2 implemented.
            // for (int y = 0; y <= baseHeight; y++) {
            //     setGlobalBlockAt(x, y, z, STONE);
            // }

            int mountainHeight = calculateMountainHeight(x, z);
            int grasslandHeight = calculateGrasslandHeight(x, z);

            float blendFactor = PerlinNoise(glm::vec2(x, z) / 2000.f);
            blendFactor = glm::smoothstep(0.3f, 0.7f, blendFactor * 0.5f + 0.5f);

            int interpolatedHeight = glm::mix(grasslandHeight, mountainHeight, blendFactor);
            interpolatedHeight = glm::clamp(interpolatedHeight, 0, baseHeight - 1);

            bool isGrassland = interpolatedHeight < 20;

            for (int y = 0; y <= interpolatedHeight; y++) {
                if (y + baseHeight >= snowLevel && y == interpolatedHeight && !isGrassland) {
                    setGlobalBlockAt(x, y + baseHeight, z, SNOW);
                } else if (y == interpolatedHeight) {
                    setGlobalBlockAt(x, y + baseHeight, z, isGrassland ? GRASS : STONE);
                } else {
                    setGlobalBlockAt(x, y + baseHeight, z, isGrassland ? DIRT : STONE);
                }
            }


            if (interpolatedHeight + baseHeight < waterLevel) {
                for (int y = interpolatedHeight + baseHeight; y < waterLevel; y++) {
                    setGlobalBlockAt(x, y, z, WATER);
                }
            }
        }
    }
}

void Terrain::updateScene(glm::vec3 pos) {
    int xFloor = glm::floor(pos.x / 16.f);
    int zFloor = glm::floor(pos.z / 16.f);
    int x = 16 * xFloor;
    int z = 16 * zFloor;

    std::vector<std::pair<int, int>> offsets = {
        {0, 16}, {16, 0}, {0, -16}, {-16, 0},
        {16, -16}, {16, 16}, {-16, -16}, {-16, 16}
    };

    for (const auto& offset : offsets) {
        int neighborX = x + offset.first;
        int neighborZ = z + offset.second;

        if (!hasChunkAt(neighborX, neighborZ) || !m_chunks[toKey(neighborX, neighborZ)]) {
            instantiateChunkAt(neighborX, neighborZ);

            //generateBiome(neighborX, neighborZ);
        }
    }
}

