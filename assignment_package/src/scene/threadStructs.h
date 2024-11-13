#pragma once
#include "glm_includes.h"
#include <GL/gl.h>
#include <mutex>
#include <vector>

template <class T>
struct MutexPair {
    T sharedResource;
    std::mutex lockForResource;
};

struct ChunkVBOdata {
    std::vector<glm::vec4> vertexData;
    std::vector<GLuint> indexData;
};
