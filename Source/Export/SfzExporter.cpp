#include "SfzExporter.h"

namespace k2m
{

juce::String SfzExporter::generateSfzContent (const std::vector<Region>& regions,
                                              const juce::String& instrumentName)
{
    juce::String sfz;

    sfz << "// ========================================================\n";
    sfz << "// Gerado pelo K2M - Kontakt to MODX M Autosampler\n";
    sfz << "// Instrumento: " << instrumentName << "\n";
    sfz << "// Total de Regioes: " << (int) regions.size() << "\n";
    sfz << "// ========================================================\n\n";

    sfz << "<global>\n";
    sfz << "ampeg_release=0.05\n";
    sfz << "amp_veltrack=0\n\n";

    sfz << "<group>\n";

    for (const auto& r : regions)
    {
        sfz << "<region>";
        sfz << " sample=" << r.samplePath.replaceCharacter ('\\', '/');
        sfz << " pitch_keycenter=" << r.rootKey;
        sfz << " lokey=" << r.keyLow;
        sfz << " hikey=" << r.keyHigh;
        sfz << " lovel=" << r.velocityLow;
        sfz << " hivel=" << r.velocityHigh;

        if (r.loopMode != "no_loop" && r.loopEnd > r.loopStart)
        {
            sfz << " loop_mode=" << r.loopMode;
            sfz << " loop_start=" << r.loopStart;
            sfz << " loop_end=" << (r.loopEnd - 1); // SFZ loop_end é inclusivo
        }
        else
        {
            sfz << " loop_mode=no_loop";
        }

        sfz << "\n";
    }

    return sfz;
}

bool SfzExporter::exportToFile (const juce::File& destinationFile,
                               const std::vector<Region>& regions,
                               const juce::String& instrumentName)
{
    destinationFile.getParentDirectory().createDirectory();

    const auto content = generateSfzContent (regions, instrumentName);
    return destinationFile.replaceWithText (content);
}

} // namespace k2m
