#pragma once

#include "../Sampling/SamplingPlan.h"
#include "../Mapping/Multisample.h"
#include <juce_core/juce_core.h>
#include <vector>

namespace k2m
{

struct ProjectData
{
    int schemaVersion = 1;
    juce::String projectId;
    juce::String name = "Novo Instrumento";
    juce::String pluginName;
    juce::String pluginIdentifier;
    SamplingPlan plan;
    std::vector<CaptureJob> jobs;
    std::vector<SampleAsset> assets;
    std::vector<Region> regions;
};

class ProjectStore
{
public:
    // Salva o projeto de forma atômica gerando backup
    static bool saveProject (const juce::File& projectFile, const ProjectData& data);

    // Carrega o manifesto project.k2m
    static bool loadProject (const juce::File& projectFile, ProjectData& outData);
};

} // namespace k2m
