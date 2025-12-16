#pragma once

#include <string>
#include <vector>

struct ChunkInfo {
    std::string chunkId;
    size_t index;
    size_t size;
    std::vector<std::string> nodeIds;
    std::vector<std::string> nodeIps;
    std::vector<int> nodePorts;
};


