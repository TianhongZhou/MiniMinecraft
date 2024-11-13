#pragma once
#include <thread>
#include <mutex>
#include "terrain.h"

class VBOWorker {
public:
    VBOWorker(Terrain* terrain);
    void operator()();

private:
    Terrain* m_terrain;
    std::vector<Chunk*> m_chunksToProcess;
    void generateVBOData(Chunk* chunk, ChunkVBOdata& vboData);
};
