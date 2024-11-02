#pragma once
#include "drawable.h"
#include "smartpointerhelp.h"
#include "glm_includes.h"
#include <array>
#include <unordered_map>
#include <cstddef>


//using namespace std;

// C++ 11 allows us to define the size of an enum. This lets us use only one byte
// of memory to store our different block types. By default, the size of a C++ enum
// is that of an int (so, usually four bytes). This *does* limit us to only 256 different
// block types, but in the scope of this project we'll never get anywhere near that many.
enum BlockType : unsigned char
{
    EMPTY, GRASS, DIRT, STONE, WATER
};

// The six cardinal directions in 3D space
enum Direction : unsigned char
{
    XPOS, XNEG, YPOS, YNEG, ZPOS, ZNEG
};

// Lets us use any enum class as the key of a
// std::unordered_map
struct EnumHash {
    template <typename T>
    size_t operator()(T t) const {
        return static_cast<size_t>(t);
    }
};

struct VertData {
    glm::vec4 pos;

    VertData(glm::vec4 p)
        : pos(p)
    {}
};

struct BlockFaceData {
    Direction dir;
    glm::vec3 dirVec;
    std::array<VertData, 4> verts;

    BlockFaceData(Direction de, glm::vec3 dv, const VertData &a, const VertData &b, const VertData &c, const VertData &d)
        : dir(de), dirVec(dv), verts{a, b, c, d}
    {}
};

const static std::array<BlockFaceData, 6> adjacentF {
    BlockFaceData(XPOS, glm::vec3(1, 0, 0), VertData(glm::vec4(1, 0, 1, 1)),
              VertData(glm::vec4(1, 0, 0, 1)),
              VertData(glm::vec4(1, 1, 0, 1)),
              VertData(glm::vec4(1, 1, 1, 1))),

    BlockFaceData(XNEG, glm::vec3(-1, 0, 0), VertData(glm::vec4(0, 0, 0, 1)),
              VertData(glm::vec4(0, 0, 1, 1)),
              VertData(glm::vec4(0, 1, 1, 1)),
              VertData(glm::vec4(0, 1, 0, 1))),

    BlockFaceData(YPOS, glm::vec3(0, 1, 0), VertData(glm::vec4(0, 1, 1, 1)),
              VertData(glm::vec4(1, 1, 1, 1)),
              VertData(glm::vec4(1, 1, 0, 1)),
              VertData(glm::vec4(0, 1, 0, 1))),

    BlockFaceData(YNEG, glm::vec3(0, -1, 0), VertData(glm::vec4(0, 0, 0, 1)),
              VertData(glm::vec4(1, 0, 0, 1)),
              VertData(glm::vec4(1, 0, 1, 1)),
              VertData(glm::vec4(0, 0, 1, 1))),

    BlockFaceData(ZPOS, glm::vec3(0, 0, 1), VertData(glm::vec4(0, 1, 1, 1)),
              VertData(glm::vec4(0, 0, 1, 1)),
              VertData(glm::vec4(1, 0, 1, 1)),
              VertData(glm::vec4(1, 1, 1, 1))),

    BlockFaceData(ZNEG, glm::vec3(0, 0, -1), VertData(glm::vec4(1, 1, 0, 1)),
              VertData(glm::vec4(1, 0, 0, 1)),
              VertData(glm::vec4(0, 0, 0, 1)),
              VertData(glm::vec4(0, 1, 0, 1)))
};

const static std::unordered_map<BlockType, glm::vec4, EnumHash> block2Color = {
    {GRASS, glm::vec4(glm::vec3(95.f, 159.f, 53.f) / 255.f, 1.f)},
    {DIRT, glm::vec4(glm::vec3(121.f, 85.f, 58.f) / 255.f, 1.f)},
    {STONE, glm::vec4(0.5f, 0.5f, 0.5f, 1.f)},
    {WATER, glm::vec4(0.f, 0.f, 0.75f, 1.f)}
};

// One Chunk is a 16 x 256 x 16 section of the world,
// containing all the Minecraft blocks in that area.
// We divide the world into Chunks in order to make
// recomputing its VBO data faster by not having to
// render all the world at once, while also not having
// to render the world block by block.

// TODO have Chunk inherit from Drawable
class Chunk : public Drawable{
private:
    // All of the blocks contained within this Chunk
    std::array<BlockType, 65536> m_blocks;
    // The coordinates of the chunk's lower-left corner in world space
    int minX, minZ;
    // This Chunk's four neighbors to the north, south, east, and west
    // The third input to this map just lets us use a Direction as
    // a key for this map.
    // These allow us to properly determine
    std::unordered_map<Direction, Chunk*, EnumHash> m_neighbors;

    int idxCount = 0;
    std::vector<GLuint> idx;
    std::vector<glm::vec4> vboInter;

public:
    Chunk(int x, int z, OpenGLContext* context);
    BlockType getLocalBlockAt(unsigned int x, unsigned int y, unsigned int z) const;
    BlockType getLocalBlockAt(int x, int y, int z) const;
    void setLocalBlockAt(unsigned int x, unsigned int y, unsigned int z, BlockType t);
    void linkNeighbor(uPtr<Chunk>& neighbor, Direction dir);

    void createChunkVBOdata(int x, int z);
    void createVBOdata() override;
    void destroyVBOdata() override;
    void create(int x, int z);

    void createChunkFaceVBOdata(const BlockFaceData &f, int i, int j, int k, int xChunk, int zChunk, BlockType currBlock, Chunk* chunk, std::vector<glm::vec4> &vboInter, std::vector<GLuint> &idx, int &idxCount);
    BlockType getAdjacentBlock(int i, int j, int k, const glm::vec3 &dirVec, Chunk *chunk);
    void addFaceVertices(const BlockFaceData &f, const glm::vec4 &blockPos, const glm::vec4 &vertCol, std::vector<glm::vec4> &vboInter, std::vector<GLuint> &idx, int &idxCount);
};
