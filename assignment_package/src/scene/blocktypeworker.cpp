#include "BlockTypeWorker.h"

BlockTypeWorker::BlockTypeWorker(Terrain* terrain)
    : m_terrain(terrain) {}

void BlockTypeWorker::operator()() {
    while (true) {
        Chunk* chunkToProcess = nullptr;

        std::lock_guard<std::mutex> lock(m_terrain->dataToBlockTypeWorkerThreadsMutex);
        if (!m_terrain->dataToBlockTypeWorkerThreads.empty()) {
            chunkToProcess = m_terrain->dataToBlockTypeWorkerThreads.back();
            m_terrain->dataToBlockTypeWorkerThreads.pop_back();
        }

        if (chunkToProcess) {
            generateBlockTypeData(chunkToProcess);

            std::lock_guard<std::mutex> lock(m_terrain->dataFromBlockTypeWorkerThreads.lockForResource);
            m_terrain->dataFromBlockTypeWorkerThreads.sharedResource.push_back(chunkToProcess);
        }
    }
}

void BlockTypeWorker::generateBlockTypeData(Chunk* chunk) {
    m_terrain->generateBiome(chunk->minX, chunk->minZ);
}
