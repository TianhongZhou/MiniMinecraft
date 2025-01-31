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
        Direction dir;
        if (dirVec.x > 0) dir = XPOS;
        else if (dirVec.x < 0) dir = XNEG;
        else if (dirVec.y > 0) dir = YPOS;
        else if (dirVec.y < 0) dir = YNEG;
        else if (dirVec.z > 0) dir = ZPOS;
        else if (dirVec.z < 0) dir = ZNEG;

        Chunk* adjChunk = chunk->m_neighbors[dir];
        if (adjChunk != nullptr) {
            int dx = (dir == XPOS) ? 0 : (dir == XNEG) ? 15 : i;
            int dy = (dir == YPOS) ? 0 : (dir == YNEG) ? 255 : j;
            int dz = (dir == ZPOS) ? 0 : (dir == ZNEG) ? 15 : k;

            return adjChunk->getLocalBlockAt(dx, dy, dz);
        }
    } else {
        return chunk->getLocalBlockAt(i + int(dirVec.x), j + int(dirVec.y), k + int(dirVec.z));
    }
    return EMPTY;
}

void Chunk::addFaceVertices(const BlockFaceData &f, const glm::vec4 &blockPos, const glm::vec4 &vertCol, std::vector<glm::vec4> &vboInter, std::vector<GLuint> &idx, int &idxCount, BlockType currBlock) {
    const auto& faceUV = blockUVs.at(currBlock);
    FaceUV uvCoords;

    if (f.dir == YPOS) {
        uvCoords = faceUV.top;
    } else if (f.dir == YNEG) {
        uvCoords = faceUV.bottom;
    } else {
        uvCoords = faceUV.side;
    }

    float isAnimating = (currBlock == LAVA || currBlock == WATER) ? 1.0f : 0.0f;

    for (int i = 0; i < 4; ++i) {
        const VertData &v = f.verts[i];
        glm::vec4 vertPos = v.pos + blockPos;

        glm::vec2 uv;
        if (i == 0) {
            uv = uvCoords.topLeft;
        } else if (i == 1) {
            uv = uvCoords.topRight;
        } else if (i == 2) {
            uv = uvCoords.bottomRight;
        } else if (i == 3) {
            uv = uvCoords.bottomLeft;
        }

        vboInter.push_back(vertPos);
        vboInter.push_back(vertCol);
        vboInter.push_back(glm::vec4(f.dirVec, 0.f));
        vboInter.push_back(glm::vec4(uv, isAnimating, 0.f));
    }

    idx.push_back(0 + idxCount);
    idx.push_back(1 + idxCount);
    idx.push_back(2 + idxCount);
    idx.push_back(0 + idxCount);
    idx.push_back(2 + idxCount);
    idx.push_back(3 + idxCount);
    idxCount += 4;
}

void Chunk::createChunkVBOdata(int xChunk, int zChunk) {
    destroyVBOdata();

    idxCountOpaque = 0;
    idxCountTransparent = 0;
    idxOpaque.clear();
    idxTransparent.clear();
    vboOpaque.clear();
    vboTransparent.clear();

    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 256; j++) {
            for (int k = 0; k < 16; k++) {
                BlockType currBlock = this->getLocalBlockAt(i, j, k);
                if (currBlock != EMPTY) {
                    for (const BlockFaceData &f : adjacentF) {
                        BlockType adjBlock = getAdjacentBlock(i, j, k, f.dirVec, this);
                        if (adjBlock == EMPTY || (adjBlock == WATER && currBlock != WATER)) {
                            glm::vec4 blockPos(i + xChunk, j, k + zChunk, 0.f);
                            glm::vec4 vertCol = block2Color.at(currBlock);
                            if (currBlock == WATER) {
                                addFaceVertices(f, blockPos, vertCol, vboTransparent, idxTransparent, idxCountTransparent, currBlock);
                            } else {
                                addFaceVertices(f, blockPos, vertCol, vboOpaque, idxOpaque, idxCountOpaque, currBlock);
                            }

                        }
                    }
                }
            }
        }
    }
    this->indexCounts[OPAQUE_INDEX] = idxOpaque.size();
    this->indexCounts[TRANSPARENT_INDEX] = idxTransparent.size();
    idxCountOpaque = idxOpaque.size();
    idxCountTransparent = idxTransparent.size();
}

void Chunk::bufferOpaqueData() {
    bufferData(vboOpaque, idxOpaque, OPAQUE_INDEX);
}

void Chunk::bufferTransparentData() {
    bufferData(vboTransparent, idxTransparent, TRANSPARENT_INDEX);
}

void Chunk::bufferData(const std::vector<glm::vec4> &vertexData, const std::vector<GLuint> &indexData, BufferType indexType) {
    if (bufGenerated[indexType]) {
        mp_context->glDeleteBuffers(1, &bufHandles[indexType]);
        bufGenerated[indexType] = false;
    }

    generateBuffer(indexType);
    mp_context->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufHandles[indexType]);
    mp_context->glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexData.size() * sizeof(GLuint), indexData.data(), GL_STATIC_DRAW);

    if (bufGenerated[INTERLEAVED]) {
        mp_context->glDeleteBuffers(1, &bufHandles[INTERLEAVED]);
        bufGenerated[INTERLEAVED] = false;
    }

    generateBuffer(INTERLEAVED);
    mp_context->glBindBuffer(GL_ARRAY_BUFFER, bufHandles[INTERLEAVED]);
    mp_context->glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(glm::vec4), vertexData.data(), GL_STATIC_DRAW);
}

void Chunk::createVBOdata() {
    bufferData(vboInter, idx, INDEX);
}

void Chunk::destroyVBOdata() {
    if (bufGenerated[OPAQUE_INDEX]) {
        mp_context->glDeleteBuffers(1, &bufHandles[OPAQUE_INDEX]);
        bufGenerated[OPAQUE_INDEX] = false;
    }

    if (bufGenerated[TRANSPARENT_INDEX]) {
        mp_context->glDeleteBuffers(1, &bufHandles[TRANSPARENT_INDEX]);
        bufGenerated[TRANSPARENT_INDEX] = false;
    }

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

    vboOpaque.clear();
    vboTransparent.clear();
    idxOpaque.clear();
    idxTransparent.clear();
}

void Chunk::create(int x, int z) {
    createChunkVBOdata(x, z);
    bufferOpaqueData();
    bufferTransparentData();
}

const std::vector<glm::vec4>& Chunk::getVertexData() const {
    return vboInter;
}

const std::vector<GLuint>& Chunk::getIndexData() const {
    return idx;
}
