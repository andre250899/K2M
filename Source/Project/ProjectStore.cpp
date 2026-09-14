#include "ProjectStore.h"

namespace k2m
{

bool ProjectStore::saveProject (const juce::File& projectFile, const ProjectData& data)
{
    projectFile.getParentDirectory().createDirectory();

    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty ("schemaVersion", data.schemaVersion);
    root->setProperty ("projectId", data.projectId);
    root->setProperty ("name", data.name);
    root->setProperty ("pluginName", data.pluginName);
    root->setProperty ("pluginIdentifier", data.pluginIdentifier);

    // Plano
    auto planObj = std::make_unique<juce::DynamicObject>();
    planObj->setProperty ("startNote", data.plan.startNote);
    planObj->setProperty ("endNote", data.plan.endNote);
    planObj->setProperty ("noteStep", data.plan.noteStep);
    planObj->setProperty ("includeEndNote", data.plan.includeEndNote);
    planObj->setProperty ("noteDurationSeconds", data.plan.noteDurationSeconds);
    planObj->setProperty ("preRollSeconds", data.plan.preRollSeconds);
    planObj->setProperty ("releaseMinSeconds", data.plan.releaseMinSeconds);
    planObj->setProperty ("releaseMaxSeconds", data.plan.releaseMaxSeconds);
    planObj->setProperty ("sampleRate", data.plan.sampleRate);

    juce::Array<juce::var> vels;
    for (int v : data.plan.velocities)
        vels.add (v);
    planObj->setProperty ("velocities", vels);

    root->setProperty ("plan", juce::var (planObj.release()));

    // Jobs
    juce::Array<juce::var> jobsArray;
    for (const auto& j : data.jobs)
    {
        auto jobObj = std::make_unique<juce::DynamicObject>();
        jobObj->setProperty ("id", j.id);
        jobObj->setProperty ("ordinal", j.ordinal);
        jobObj->setProperty ("note", j.note);
        jobObj->setProperty ("velocity", j.velocity);
        jobObj->setProperty ("status", j.status);
        jobObj->setProperty ("attempt", j.attempt);
        jobObj->setProperty ("rawPath", j.rawPath);
        jobObj->setProperty ("error", j.error);
        jobsArray.add (juce::var (jobObj.release()));
    }
    root->setProperty ("jobs", jobsArray);

    // Assets
    juce::Array<juce::var> assetsArray;
    for (const auto& a : data.assets)
    {
        auto assetObj = std::make_unique<juce::DynamicObject>();
        assetObj->setProperty ("id", a.id);
        assetObj->setProperty ("rawPath", a.rawPath);
        assetObj->setProperty ("processedPath", a.processedPath);
        assetObj->setProperty ("rootKey", a.rootKey);
        assetObj->setProperty ("velocity", a.velocity);
        assetObj->setProperty ("sampleRate", a.sampleRate);
        assetObj->setProperty ("numChannels", a.numChannels);
        assetObj->setProperty ("totalFrames", (juce::int64) a.totalFrames);
        assetObj->setProperty ("peakDb", a.peakDb);
        assetObj->setProperty ("rmsDb", a.rmsDb);
        assetsArray.add (juce::var (assetObj.release()));
    }
    root->setProperty ("assets", assetsArray);

    // Regiões
    juce::Array<juce::var> regionsArray;
    for (const auto& r : data.regions)
    {
        auto regObj = std::make_unique<juce::DynamicObject>();
        regObj->setProperty ("assetId", r.assetId);
        regObj->setProperty ("samplePath", r.samplePath);
        regObj->setProperty ("rootKey", r.rootKey);
        regObj->setProperty ("keyLow", r.keyLow);
        regObj->setProperty ("keyHigh", r.keyHigh);
        regObj->setProperty ("velocityLow", r.velocityLow);
        regObj->setProperty ("velocityHigh", r.velocityHigh);
        regObj->setProperty ("loopMode", r.loopMode);
        regObj->setProperty ("loopStart", (juce::int64) r.loopStart);
        regObj->setProperty ("loopEnd", (juce::int64) r.loopEnd);
        regionsArray.add (juce::var (regObj.release()));
    }
    root->setProperty ("regions", regionsArray);

    const auto jsonString = juce::JSON::toString (juce::var (root.release()), true);

    // Escrita atômica com backup
    auto tempFile = projectFile.getSiblingFile (projectFile.getFileName() + ".tmp");
    auto bakFile = projectFile.getSiblingFile (projectFile.getFileName() + ".bak");

    if (! tempFile.replaceWithText (jsonString))
        return false;

    if (projectFile.existsAsFile())
    {
        if (bakFile.existsAsFile())
            bakFile.deleteFile();
        projectFile.copyFileTo (bakFile);
        projectFile.deleteFile();
    }

    return tempFile.moveFileTo (projectFile);
}

bool ProjectStore::loadProject (const juce::File& projectFile, ProjectData& outData)
{
    if (! projectFile.existsAsFile())
        return false;

    const auto content = projectFile.loadFileAsString();
    juce::var parsed = juce::JSON::parse (content);

    if (! parsed.isObject())
        return false;

    outData.schemaVersion = parsed.getProperty ("schemaVersion", 1);
    outData.projectId = parsed.getProperty ("projectId", "").toString();
    outData.name = parsed.getProperty ("name", "Instrumento").toString();
    outData.pluginName = parsed.getProperty ("pluginName", "").toString();
    outData.pluginIdentifier = parsed.getProperty ("pluginIdentifier", "").toString();

    // Ler plano
    if (parsed.hasProperty ("plan"))
    {
        const auto& p = parsed["plan"];
        outData.plan.startNote = p.getProperty ("startNote", 21);
        outData.plan.endNote = p.getProperty ("endNote", 108);
        outData.plan.noteStep = p.getProperty ("noteStep", 3);
        outData.plan.includeEndNote = p.getProperty ("includeEndNote", true);
        outData.plan.noteDurationSeconds = p.getProperty ("noteDurationSeconds", 5.0);
        outData.plan.preRollSeconds = p.getProperty ("preRollSeconds", 0.25);
        outData.plan.releaseMinSeconds = p.getProperty ("releaseMinSeconds", 2.0);
        outData.plan.releaseMaxSeconds = p.getProperty ("releaseMaxSeconds", 8.0);
        outData.plan.sampleRate = p.getProperty ("sampleRate", 44100);

        if (p.hasProperty ("velocities") && p["velocities"].isArray())
        {
            outData.plan.velocities.clear();
            const auto* arr = p["velocities"].getArray();
            for (const auto& v : *arr)
                outData.plan.velocities.push_back ((int) v);
        }
    }

    // Ler Jobs
    outData.jobs.clear();
    if (parsed.hasProperty ("jobs") && parsed["jobs"].isArray())
    {
        const auto* arr = parsed["jobs"].getArray();
        for (const auto& item : *arr)
        {
            CaptureJob j;
            j.id = item.getProperty ("id", "").toString();
            j.ordinal = item.getProperty ("ordinal", 0);
            j.note = item.getProperty ("note", 60);
            j.velocity = item.getProperty ("velocity", 100);
            j.status = item.getProperty ("status", "pending").toString();
            j.attempt = item.getProperty ("attempt", 0);
            j.rawPath = item.getProperty ("rawPath", "").toString();
            j.error = item.getProperty ("error", "").toString();
            outData.jobs.push_back (j);
        }
    }

    // Ler Assets
    outData.assets.clear();
    if (parsed.hasProperty ("assets") && parsed["assets"].isArray())
    {
        const auto* arr = parsed["assets"].getArray();
        for (const auto& item : *arr)
        {
            SampleAsset a;
            a.id = item.getProperty ("id", "").toString();
            a.rawPath = item.getProperty ("rawPath", "").toString();
            a.processedPath = item.getProperty ("processedPath", "").toString();
            a.rootKey = item.getProperty ("rootKey", 60);
            a.velocity = item.getProperty ("velocity", 100);
            a.sampleRate = item.getProperty ("sampleRate", 44100.0);
            a.numChannels = item.getProperty ("numChannels", 2);
            a.totalFrames = (int64_t) item.getProperty ("totalFrames", 0);
            a.peakDb = item.getProperty ("peakDb", -120.0f);
            a.rmsDb = item.getProperty ("rmsDb", -120.0f);
            outData.assets.push_back (a);
        }
    }

    // Ler Regiões
    outData.regions.clear();
    if (parsed.hasProperty ("regions") && parsed["regions"].isArray())
    {
        const auto* arr = parsed["regions"].getArray();
        for (const auto& item : *arr)
        {
            Region r;
            r.assetId = item.getProperty ("assetId", "").toString();
            r.samplePath = item.getProperty ("samplePath", "").toString();
            r.rootKey = item.getProperty ("rootKey", 60);
            r.keyLow = item.getProperty ("keyLow", 0);
            r.keyHigh = item.getProperty ("keyHigh", 127);
            r.velocityLow = item.getProperty ("velocityLow", 1);
            r.velocityHigh = item.getProperty ("velocityHigh", 127);
            r.loopMode = item.getProperty ("loopMode", "no_loop").toString();
            r.loopStart = (int64_t) item.getProperty ("loopStart", 0);
            r.loopEnd = (int64_t) item.getProperty ("loopEnd", 0);
            outData.regions.push_back (r);
        }
    }

    return true;
}

} // namespace k2m
