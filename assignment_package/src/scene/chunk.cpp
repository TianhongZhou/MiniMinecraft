#include "chunk.h"
#include <iostream>


Chunk::Chunk(int x, int z, OpenGLContext* context)
    : Drawable(context), m_blocks(), minX(x), minZ(z), m_neighbors{{XPOS, nullptr}, {XNEG, nullptr}, {ZPOS, nullptr}, {ZNEG, nullptr}}
{
    std::fill_n(m_blocks.begin(), 65536, EMPTY);
}

// Does bounds checking with at()
BlockType Chunk::getLocalBlockAt(unsigned int x, unsigned int y, unsigned int z) const {
    return m_blocks.at(x + 16 * y + 16 * 256 * z);
}

// Exists to get rid of compiler warnings about int -> unsigned int implicit conversion
BlockType Chunk::getLocalBlockAt(int x, int y, int z) const {
    return getLocalBlockAt(static_cast<unsigned int>(x), static_cast<unsigned int>(y), static_cast<unsigned int>(z));
}

// Does bounds checking with at()
void Chunk::setLocalBlockAt(unsigned int x, unsigned int y, unsigned int z, BlockType t) {
    m_blocks.at(x + 16 * y + 16 * 256 * z) = t;
}

const static std::unordered_map<Direction, Direction, EnumHash> oppositeDirection {
    {XPOS, XNEG},
    {XNEG, XPOS},
    {YPOS, YNEG},
    {YNEG, YPOS},
    {ZPOS, ZNEG},
    {ZNEG, ZPOS}
};

void Chunk::linkNeighbor(uPtr<Chunk> &neighbor, Direction dir) {
    if(neighbor != nullptr) {
        this->m_neighbors[dir] = neighbor.get();
        neighbor->m_neighbors[oppositeDirection.at(dir)] = this;
    }
}

BlockType Chunk::getAdjacentBlock(int i, int j, int k, const glm::vec3 &dirVec, Chunk *chunk) {
    glm::vec3 testBorder = glm::vec3(i, j, k) + dirVec;
    if (testBorder.x >= 16.f || testBorder.y >= 256.f || testBorder.z >= 16.f ||
        testBorder.x < 0.f || testBorder.y < 0.f || testBorder.z < 0.f) {
        Chunk* adjChunk = chunk->m_neighbors[Direction(int(dirVec.x + dirVec.y + dirVec.z))];
        if (adjChunk != nullptr) {
            int dx = (16 + i + int(dirVec.x)) % 16;
            int dy = (256 + j + int(dirVec.y)) % 256;
            int dz = (16 + k + int(dirVec.z)) % 16;
            return adjChunk->getLocalBlockAt(dx, dy, dz);
        }
    } else {
        return chunk->getLocalBlockAt(i + int(dirVec.x), j + int(dirVec.y), k + int(dirVec.z));
    }
    return EMPTY;
}

void Chunk::addFaceVertices(const BlockFaceData &f, const glm::vec4 &blockPos, const glm::vec4 &vertCol, std::vector<glm::vec4> &vboInter, std::vector<GLuint> &idx, int &idxCount) {
    for (const VertData &v: f.verts) {
        glm::vec4 vertPos = v.pos + blockPos;
        vboInter.push_back(vertPos);
        vboInter.push_back(vertCol);
        vboInter.push_back(glm::vec4(f.dirVec, 0.f));
    }
    idx.push_back(0 + idxCount);
    idx.push_back(1 + idxCount);
    idx.push_back(2 + idxCount);
    idx.push_back(0 + idxCount);
    idx.push_back(2 + idxCount);
    idx.push_back(3 + idxCount);
    idxCount += 4;
}

void Chunk::createChunkFaceVBOdata(const BlockFaceData &f, int i, int j, int k, int xChunk, int zChunk, BlockType currBlock, Chunk* chunk, std::vector<glm::vec4> &vboInter, std::vector<GLuint> &idx, int &idxCount) {
    BlockType adjBlock = getAdjacentBlock(i, j, k, f.dirVec, chunk);
    if (adjBlock == EMPTY) {
        glm::vec4 vertCol = block2Color.at(currBlock);
        glm::vec4 blockPos = glm::vec4(i + xChunk, j, k + zChunk, 0.f);
        addFaceVertices(f, blockPos, vertCol, vboInter, idx, idxCount);
    }
}

void Chunk::createChunkVBOdata(int xChunk, int zChunk) {
    idxCount = 0;
    idx.clear();
    vboInter.clear();

    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 256; j++) {
            for (int k = 0; k < 16; k++) {
                BlockType currBlock = this->getLocalBlockAt(i, j, k);

                if (currBlock != EMPTY) {
                    for (const BlockFaceData &f: adjacentF) {
                        createChunkFaceVBOdata(f, i, j, k, xChunk, zChunk, currBlock, this, vboInter, idx, idxCount);
                    }
                }
            }
        }
    }
    this->indexCounts[INDEX] = idx.size();
}


void Chunk::createVBOdata() {
    generateBuffer(INDEX);
    mp_context->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufHandles[INDEX]);
    mp_context->glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(GLuint), idx.data(), GL_STATIC_DRAW);

    generateBuffer(INTERLEAVED);
    mp_context->glBindBuffer(GL_ARRAY_BUFFER, bufHandles[INTERLEAVED]);
    mp_context->glBufferData(GL_ARRAY_BUFFER, vboInter.size() * sizeof(glm::vec4), vboInter.data(), GL_STATIC_DRAW);
}

void Chunk::destroyVBOdata() {
    if (bufGenerated[INTERLEAVED]) {
        mp_context->glDeleteBuffers(1, &bufHandles[INTERLEAVED]);
        bufGenerated[INTERLEAVED] = false;
    }

    if (bufGenerated[INDEX]) {
        mp_context->glDeleteBuffers(1, &bufHandles[INDEX]);
        bufGenerated[INDEX] = false;
    }

    bufHandles.clear();
    bufGenerated.clear();
    indexCounts.clear();
}

void Chunk::create(int x, int z) {
    createChunkVBOdata(x, z);
    createVBOdata();
}
