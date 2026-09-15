#include "PluginScanWorker.h"

#include <condition_variable>
#include <mutex>
#include <queue>

namespace k2m
{

namespace
{

// Prefere um executável worker dedicado (console simples, sem JUCEApplication) ao lado do
// executável atual; se não existir, cai de volta para relançar o próprio executável atual.
// Ver comentário no topo de ScanWorkerMain.cpp para o porquê.
juce::File resolveScanWorkerExecutable()
{
    const auto currentExe = juce::File::getSpecialLocation (juce::File::currentExecutableFile);

   #if JUCE_WINDOWS
    const auto dedicated = currentExe.getSiblingFile ("K2M_ScanWorker.exe");
   #else
    const auto dedicated = currentExe.getSiblingFile ("K2M_ScanWorker");
   #endif

    return dedicated.existsAsFile() ? dedicated : currentExe;
}

//==============================================================================
// Lado worker: roda dentro do processo filho descartável. Recebe (formatName, fileOrIdentifier),
// consulta a fábrica do plugin e devolve a lista de PluginDescription encontrada como XML.
// Se o plugin travar o processo, o coordinator simplesmente perde a conexão (ver ScanCoordinator).
class ScanWorkerProcess final : private juce::ChildProcessWorker,
                                private juce::AsyncUpdater
{
public:
    ScanWorkerProcess()
    {
        juce::addDefaultFormatsToManager (formatManager);
    }

    using juce::ChildProcessWorker::initialiseFromCommandLine;

private:
    void handleMessageFromCoordinator (const juce::MemoryBlock& mb) override
    {
        if (mb.isEmpty())
            return;

        const std::lock_guard<std::mutex> lock (mutex);
        pendingBlocks.emplace (mb);
        triggerAsyncUpdate();
    }

    void handleConnectionLost() override
    {
        juce::MessageManager::getInstance()->stopDispatchLoop();
    }

    void handleAsyncUpdate() override
    {
        for (;;)
        {
            juce::MemoryBlock block;
            {
                const std::lock_guard<std::mutex> lock (mutex);
                if (pendingBlocks.empty())
                    return;

                block = pendingBlocks.front();
                pendingBlocks.pop();
            }

            sendResults (doScan (block));
        }
    }

    juce::OwnedArray<juce::PluginDescription> doScan (const juce::MemoryBlock& block)
    {
        juce::MemoryInputStream stream (block, false);
        const auto formatName = stream.readString();
        const auto identifier = stream.readString();

        juce::OwnedArray<juce::PluginDescription> results;

        for (auto* format : formatManager.getFormats())
        {
            if (format->getName() == formatName)
            {
                format->findAllTypesForFile (results, identifier);
                break;
            }
        }

        return results;
    }

    void sendResults (const juce::OwnedArray<juce::PluginDescription>& results)
    {
        juce::XmlElement xml ("LIST");
        for (const auto* desc : results)
            xml.addChildElement (desc->createXml().release());

        const auto str = xml.toString();
        sendMessageToCoordinator ({ str.toRawUTF8(), str.getNumBytesAsUTF8() });
    }

    juce::AudioPluginFormatManager formatManager;
    std::mutex mutex;
    std::queue<juce::MemoryBlock> pendingBlocks;
};

//==============================================================================
// Lado coordinator: vive no processo principal (K2M.exe ou um dos executáveis de teste).
// Lança uma cópia do próprio executável como worker e troca uma mensagem por arquivo escaneado.
class ScanCoordinator final : private juce::ChildProcessCoordinator
{
public:
    explicit ScanCoordinator (const juce::File& executableToRelaunch)
    {
        launched = launchWorkerProcess (executableToRelaunch, kPluginScanWorkerUID, 0, 0);
    }

    bool isLaunched() const noexcept { return launched; }

    enum class State { timeout, gotResult, connectionLost };

    struct Response
    {
        State state;
        std::unique_ptr<juce::XmlElement> xml;
    };

    // Espera até 50ms pela resposta do worker; timeout permite ao chamador checar shouldExit() em loop.
    Response getResponse()
    {
        std::unique_lock<std::mutex> lock (mutex);

        if (! condvar.wait_for (lock, std::chrono::milliseconds (50), [&] { return gotResult || connectionLost; }))
            return { State::timeout, nullptr };

        const auto state = connectionLost ? State::connectionLost : State::gotResult;
        connectionLost = false;
        gotResult = false;
        return { state, std::move (resultXml) };
    }

    using juce::ChildProcessCoordinator::sendMessageToWorker;

private:
    void handleMessageFromWorker (const juce::MemoryBlock& mb) override
    {
        const std::lock_guard<std::mutex> lock (mutex);
        resultXml = juce::parseXML (mb.toString());
        gotResult = true;
        condvar.notify_one();
    }

    void handleConnectionLost() override
    {
        const std::lock_guard<std::mutex> lock (mutex);
        connectionLost = true;
        condvar.notify_one();
    }

    bool launched = false;
    std::mutex mutex;
    std::condition_variable condvar;
    std::unique_ptr<juce::XmlElement> resultXml;
    bool connectionLost = false;
    bool gotResult = false;
};

//==============================================================================
class OutOfProcessScanner final : public juce::KnownPluginList::CustomScanner
{
public:
    bool findPluginTypesFor (juce::AudioPluginFormat& format,
                             juce::OwnedArray<juce::PluginDescription>& result,
                             const juce::String& fileOrIdentifier) override
    {
        if (coordinator == nullptr || ! coordinator->isLaunched())
        {
            coordinator = std::make_unique<ScanCoordinator> (resolveScanWorkerExecutable());

            if (! coordinator->isLaunched())
            {
                coordinator = nullptr;
                return false;
            }
        }

        juce::MemoryBlock block;
        juce::MemoryOutputStream stream (block, true);
        stream.writeString (format.getName());
        stream.writeString (fileOrIdentifier);

        if (! coordinator->sendMessageToWorker (block))
        {
            coordinator = nullptr;
            return false;
        }

        for (;;)
        {
            if (shouldExit())
                return true;

            const auto response = coordinator->getResponse();

            if (response.state == ScanCoordinator::State::timeout)
                continue;

            if (response.xml != nullptr)
            {
                for (const auto* item : response.xml->getChildIterator())
                {
                    auto desc = std::make_unique<juce::PluginDescription>();
                    if (desc->loadFromXml (*item))
                        result.add (desc.release());
                }
            }

            // Conexão perdida = o worker crashou ao consultar este arquivo. Devolver false aqui faz
            // scanAndAddFile() colocar o arquivo na blacklist do KnownPluginList (não tenta de novo
            // nesta sessão). O próximo arquivo relança um coordinator novo.
            if (response.state == ScanCoordinator::State::connectionLost)
                coordinator = nullptr;

            return response.state == ScanCoordinator::State::gotResult;
        }
    }

    void scanFinished() override
    {
        coordinator = nullptr;
    }

private:
    std::unique_ptr<ScanCoordinator> coordinator;
};

} // anonymous namespace

//==============================================================================
bool commandLineIsScanWorker (const juce::String& commandLine)
{
    return commandLine.trim().startsWith ("--" + juce::String (kPluginScanWorkerUID) + ":");
}

bool runPluginScanWorkerIfRequested (const juce::String& commandLine)
{
    auto worker = std::make_unique<ScanWorkerProcess>();

    if (! worker->initialiseFromCommandLine (commandLine, kPluginScanWorkerUID))
        return false;

    juce::MessageManager::getInstance()->runDispatchLoop();
    return true;
}

void installOutOfProcessScanner (juce::KnownPluginList& listToConfigure)
{
    listToConfigure.setCustomScanner (std::make_unique<OutOfProcessScanner>());
}

} // namespace k2m
