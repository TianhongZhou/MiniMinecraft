#include "terrain.h"
#include "cube.h"
#include <stdexcept>
#include <iostream>

#include <glm/gtc/random.hpp>
#include "blocktypeworker.h"
#include "vboworker.h"

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
            const uPtr<Chunk>& chunk = getChunkAt(x, z);
            if (!chunk) continue;

            chunk->createChunkVBOdata(x, z);

            if (chunk->idxCountOpaque > 0) {
                chunk->bufferOpaqueData();
                shaderProgram->drawInterleaved(*chunk, OPAQUE_INDEX);
            }
        }
    }

    for (int x = minX; x <= maxX; x += 16) {
        for (int z = minZ; z <= maxZ; z += 16) {
            const uPtr<Chunk>& chunk = getChunkAt(x, z);
            if (!chunk) continue;

            if (chunk->idxCountTransparent > 0) {
                chunk->bufferTransparentData();
                shaderProgram->drawInterleaved(*chunk, TRANSPARENT_INDEX);
            }
        }
    }
}

float random1(glm::vec2 p) {
    return glm::fract(glm::sin(glm::dot(p, glm::vec2(463.1, 587.8))) * 16543.786f);
}

glm::vec2 random2(glm::vec2 p) {
    return glm::fract(glm::sin(glm::vec2(glm::dot(p, glm::vec2(549.1, 874.2)), glm::dot(p, glm::vec2(764.1, 126.8)))) * 79871.465f);
}

glm::vec3 random3(glm::vec3 p) {
    return glm::fract(glm::sin(glm::vec3(glm::dot(p, glm::vec3(465.1, 635.4, 767.3)), glm::dot(p, glm::vec3(165.4, 535.6, 418.7)), glm::dot(p, glm::vec3(912.1, 327.9, 298.1)))) * 18973.49f);
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

float surflet3D(glm::vec3 P, glm::vec3 gridPoint) {
    float distX = abs(P.x - gridPoint.x);
    float distY = abs(P.y - gridPoint.y);
    float distZ = abs(P.z - gridPoint.z);

    float tX = 1.f - 6.f * pow(distX, 5.f) + 15.f * pow(distX, 4.f) - 10.f * pow(distX, 3.f);
    float tY = 1.f - 6.f * pow(distY, 5.f) + 15.f * pow(distY, 4.f) - 10.f * pow(distY, 3.f);
    float tZ = 1.f - 6.f * pow(distZ, 5.f) + 15.f * pow(distZ, 4.f) - 10.f * pow(distZ, 3.f);

    glm::vec3 gradient = 2.f * random3(gridPoint) - glm::vec3(1.f);
    glm::vec3 diff = P - gridPoint;
    float height = glm::dot(diff, gradient);
    return height * tX * tY * tZ;
}

float PerlinNoise3D(glm::vec3 uvw) {
    float surfletSum = 0.f;

    for (int dx = 0; dx <= 1; dx++) {
        for (int dy = 0; dy <= 1; dy++) {
            for (int dz = 0; dz <= 1; dz++) {
                surfletSum += surflet3D(uvw, glm::floor(uvw) + glm::vec3(dx, dy, dz));
            }
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

    int step = 4;
    float noiseCache[17][17][33];

    for (int x = 0; x <= 16; x += step) {
        for (int z = 0; z <= 16; z += step) {
            for (int y = 0; y <= baseHeight; y += step) {
                noiseCache[x][z][y / step] = PerlinNoise3D(glm::vec3(xMin + x, y, zMin + z) / 30.f);
            }
        }
    }

    for(int x = xMin; x < xMin + 16; x++) {
        for(int z = zMin; z < zMin + 16; z++) {
            setGlobalBlockAt(x, 0, z, BEDROCK);

            // Commented out for other parts to run smoothly
            // Need to be uncommented for showing cave sys
            // for (int y = 1; y <= baseHeight; y++) {
            //     int x0 = (x - xMin) / step * step;
            //     int x1 = x0 + step;
            //     int z0 = (z - zMin) / step * step;
            //     int z1 = z0 + step;
            //     int y0 = y / step * step;
            //     int y1 = y0 + step;

            //     float wx = float(x - xMin - x0) / step;
            //     float wz = float(z - zMin - z0) / step;
            //     float wy = float(y - y0) / step;

            //     float c00 = glm::mix(noiseCache[x0][z0][y0 / step], noiseCache[x1][z0][y0 / step], wx);
            //     float c01 = glm::mix(noiseCache[x0][z0][y1 / step], noiseCache[x1][z0][y1 / step], wx);
            //     float c10 = glm::mix(noiseCache[x0][z1][y0 / step], noiseCache[x1][z1][y0 / step], wx);
            //     float c11 = glm::mix(noiseCache[x0][z1][y1 / step], noiseCache[x1][z1][y1 / step], wx);

            //     float c0 = glm::mix(c00, c10, wz);
            //     float c1 = glm::mix(c01, c11, wz);
            //     float cave = glm::mix(c0, c1, wy);

            //     if (cave < 0) {
            //         setGlobalBlockAt(x, y, z, (24 <= y && y < 25) ? LAVA : EMPTY);
            //     } else {
            //         setGlobalBlockAt(x, y, z, STONE);
            //     }
            // }

            setGlobalBlockAt(x, baseHeight - 1, z, STONE);
            setGlobalBlockAt(x, baseHeight, z, STONE);

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

            if (x - xMin > 2 && xMin + 16 - x > 2 && z - zMin > 2 && zMin + 16 - z > 2) {
                float rand = random1(glm::vec2(x, z));
                if (rand > 0.992f && isGrassland && interpolatedHeight + baseHeight >= waterLevel) {
                    float rand2 = random1(glm::vec2(z, x));
                    int height = floor(rand2 * 3.f) + 4;
                    for (int k = floor(rand2 * 2.f) + 1; k >= 0; k--) {
                        for (int i = -k; i <= k; i++) {
                            for (int j = -k; j <= k; j++) {
                                if (getGlobalBlockAt(x + i, height + 1 - k + interpolatedHeight + baseHeight, z + j) == EMPTY) {
                                    setGlobalBlockAt(x + i, height + 1 - k + interpolatedHeight + baseHeight, z + j, LEAF);
                                }
                            }
                        }
                    }

                    for (int y = 1; y < height; y++) {
                        setGlobalBlockAt(x, y + interpolatedHeight + baseHeight, z, WOOD);
                    }
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
