#pragma once
#include <thread>
#include <mutex>
#include "terrain.h"

class BlockTypeWorker {
public:
    BlockTypeWorker(Terrain* terrain);
    void operator()();

private:
    Terrain* m_terrain;
    std::vector<Chunk*> m_chunksToProcess;
    void generateBlockTypeData(Chunk* chunk);
};
