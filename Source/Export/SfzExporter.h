#pragma once

#include "../Mapping/Multisample.h"
#include <juce_core/juce_core.h>
#include <vector>

namespace k2m
{

class SfzExporter
{
public:
    // Gera o conteúdo textual do arquivo SFZ a partir da lista de regiões
    static juce::String generateSfzContent (const std::vector<Region>& regions,
                                           const juce::String& instrumentName = "K2M Instrument");

    // Salva o arquivo .sfz no destino especificado
    static bool exportToFile (const juce::File& destinationFile,
                             const std::vector<Region>& regions,
                             const juce::String& instrumentName = "K2M Instrument");
};

} // namespace k2m
