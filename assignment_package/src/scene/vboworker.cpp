#include "VBOWorker.h"

VBOWorker::VBOWorker(Terrain* terrain)
    : m_terrain(terrain) {}

void VBOWorker::operator()() {
    while (true) {
        Chunk* chunkToProcess = nullptr;

        std::lock_guard<std::mutex> lock(m_terrain->dataToVBOWorkerThreadsMutex);
        if (!m_terrain->dataToVBOWorkerThreads.empty()) {
            chunkToProcess = m_terrain->dataToVBOWorkerThreads.back();
            m_terrain->dataToVBOWorkerThreads.pop_back();
        }

        if (chunkToProcess) {
            ChunkVBOdata vboData;
            generateVBOData(chunkToProcess, vboData);

            std::lock_guard<std::mutex> lock(m_terrain->dataFromVBOWorkerThreads.lockForResource);
            m_terrain->dataFromVBOWorkerThreads.sharedResource.push_back(vboData);
        }
    }
}

void VBOWorker::generateVBOData(Chunk* chunk, ChunkVBOdata& vboData) {
    chunk->create(chunk->minX, chunk->minZ);
    vboData.vertexData = chunk->getVertexData();
    vboData.indexData = chunk->getIndexData();
}
