#include "Mapper.h"
#include <map>
#include <set>

namespace k2m
{

std::vector<Range> Mapper::calculateKeyRanges (const std::vector<int>& sortedRoots,
                                               int minKeyBound,
                                               int maxKeyBound)
{
    std::vector<Range> ranges;
    const size_t n = sortedRoots.size();
    if (n == 0)
        return ranges;

    ranges.resize (n);

    if (n == 1)
    {
        ranges[0] = { minKeyBound, maxKeyBound };
        return ranges;
    }

    std::vector<int> boundaries (n - 1);
    for (size_t i = 0; i < n - 1; ++i)
    {
        boundaries[i] = (sortedRoots[i] + sortedRoots[i + 1]) / 2;
    }

    // Primeira região
    ranges[0] = { minKeyBound, boundaries[0] };

    // Regiões intermediárias
    for (size_t i = 1; i < n - 1; ++i)
    {
        ranges[i] = { boundaries[i - 1] + 1, boundaries[i] };
    }

    // Última região
    ranges[n - 1] = { boundaries[n - 2] + 1, maxKeyBound };

    return ranges;
}

std::vector<Range> Mapper::calculateVelocityRanges (const std::vector<int>& sortedVelocities,
                                                    int minVelBound,
                                                    int maxVelBound)
{
    std::vector<Range> ranges;
    const size_t n = sortedVelocities.size();
    if (n == 0)
        return ranges;

    ranges.resize (n);

    if (n == 1)
    {
        ranges[0] = { minVelBound, maxVelBound };
        return ranges;
    }

    std::vector<int> boundaries (n - 1);
    for (size_t i = 0; i < n - 1; ++i)
    {
        boundaries[i] = (sortedVelocities[i] + sortedVelocities[i + 1]) / 2;
    }

    // Primeira camada de velocity
    ranges[0] = { minVelBound, boundaries[0] };

    // Camadas intermediárias
    for (size_t i = 1; i < n - 1; ++i)
    {
        ranges[i] = { boundaries[i - 1] + 1, boundaries[i] };
    }

    // Última camada
    ranges[n - 1] = { boundaries[n - 2] + 1, maxVelBound };

    return ranges;
}

std::vector<Region> Mapper::buildRegions (const std::vector<SampleAsset>& assets,
                                          int minKeyBound,
                                          int maxKeyBound)
{
    std::vector<Region> regions;
    if (assets.empty())
        return regions;

    // Coletar raízes de notas únicas ordenadas
    std::set<int> uniqueRootsSet;
    std::set<int> uniqueVelsSet;

    for (const auto& a : assets)
    {
        uniqueRootsSet.insert (a.rootKey);
        uniqueVelsSet.insert (a.velocity);
    }

    std::vector<int> sortedRoots (uniqueRootsSet.begin(), uniqueRootsSet.end());
    std::vector<int> sortedVels  (uniqueVelsSet.begin(), uniqueVelsSet.end());

    auto keyRanges = calculateKeyRanges (sortedRoots, minKeyBound, maxKeyBound);
    auto velRanges = calculateVelocityRanges (sortedVels, 1, 127);

    // Mapear cada raiz para seu Range
    std::map<int, Range> rootToKeyRange;
    for (size_t i = 0; i < sortedRoots.size(); ++i)
        rootToKeyRange[sortedRoots[i]] = keyRanges[i];

    // Mapear cada velocity para seu Range
    std::map<int, Range> velToVelRange;
    for (size_t i = 0; i < sortedVels.size(); ++i)
        velToVelRange[sortedVels[i]] = velRanges[i];

    for (const auto& a : assets)
    {
        Region reg;
        reg.assetId = a.id;
        reg.samplePath = a.processedPath.isNotEmpty() ? a.processedPath : a.rawPath;
        reg.rootKey = a.rootKey;

        const auto kRange = rootToKeyRange[a.rootKey];
        reg.keyLow = kRange.low;
        reg.keyHigh = kRange.high;

        const auto vRange = velToVelRange[a.velocity];
        reg.velocityLow = vRange.low;
        reg.velocityHigh = vRange.high;

        reg.loopMode = "no_loop";
        reg.loopStart = 0;
        reg.loopEnd = 0;

        regions.push_back (reg);
    }

    return regions;
}

} // namespace k2m
