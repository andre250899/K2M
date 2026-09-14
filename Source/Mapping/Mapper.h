#pragma once

#include "Multisample.h"
#include <vector>
#include <algorithm>

namespace k2m
{

struct Range
{
    int low = 0;
    int high = 0;
};

class Mapper
{
public:
    // Calcula as divisões de teclas entre notas consecutivas (midpoint split)
    static std::vector<Range> calculateKeyRanges (const std::vector<int>& sortedRoots,
                                                  int minKeyBound = 0,
                                                  int maxKeyBound = 127);

    // Calcula as divisões de velocities (domínio 1-127)
    static std::vector<Range> calculateVelocityRanges (const std::vector<int>& sortedVelocities,
                                                       int minVelBound = 1,
                                                       int maxVelBound = 127);

    // Constrói todas as regiões a partir de um conjunto de SampleAssets
    static std::vector<Region> buildRegions (const std::vector<SampleAsset>& assets,
                                             int minKeyBound = 0,
                                             int maxKeyBound = 127);
};

} // namespace k2m
