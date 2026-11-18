#include "MidiLearn.h"

// ----------------------------------------------------------------------------- Store

// O mapeamento global, um só por processo. Criado pela primeira instância do Matiza e apagado
// com a última. Só a thread da interface usa (timers e cliques), então não precisa de lock.
struct MidiLearn::Store
{
    Store()
    {
        const auto text = file().loadFileAsString(); // "" se o arquivo não existe
        map = MidiMap::fromString (text.toStdString());
    }

    // %APPDATA%\Forja Instruments\Matiza\MidiLearn.txt no Windows
    // (~/Library/Application Support/... no Mac). A licença vai ficar na mesma pasta.
    static juce::File file()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
            .getChildFile ("Forja Instruments")
            .getChildFile ("Matiza")
            .getChildFile ("MidiLearn.txt");
    }

    void changed()
    {
        ++revision;
        const auto f = file();
        f.getParentDirectory().createDirectory();
        f.replaceWithText (juce::String (map.toString()));
    }

    MidiMap map;
    std::uint64_t revision = 1;
};

// ----------------------------------------------------------------------------- MidiLearn

MidiLearn::MidiLearn (juce::AudioProcessor& p) : processor (p)
{
    for (auto* param : processor.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param);
        parameterIDs.add (ranged != nullptr ? ranged->getParameterID() : juce::String());
    }

    for (int cc = 0; cc < MidiMap::numControllers; ++cc)
    {
        ccToParameter[static_cast<std::size_t> (cc)].store (-1);
        pendingValue[static_cast<std::size_t> (cc)].store (0.0f);
        pendingDirty[static_cast<std::size_t> (cc)].store (false);
    }

    rebuildTable();
    startTimerHz (60);
}

MidiLearn::~MidiLearn()
{
    stopTimer();
}

void MidiLearn::processMidi (const juce::MidiBuffer& midi) noexcept
{
    for (const auto metadata : midi)
    {
        // getMessage monta a mensagem na pilha (até 8 bytes não alocam memória).
        const auto message = metadata.getMessage();
        if (! message.isController())
            continue;

        const int cc = message.getControllerNumber();
        if (! MidiMap::isLearnable (cc))
            continue;

        // Esperando um CC: anota qual chegou; o timer confirma.
        if (learnTarget.load (std::memory_order_relaxed) >= 0)
        {
            learnedCc.store (cc);
            continue;
        }

        const auto index = static_cast<std::size_t> (cc);
        if (ccToParameter[index].load (std::memory_order_relaxed) >= 0)
        {
            pendingValue[index].store (static_cast<float> (message.getControllerValue()) / 127.0f, std::memory_order_relaxed);
            pendingDirty[index].store (true, std::memory_order_release);
        }
    }
}

void MidiLearn::timerCallback()
{
    // 1. CC aprendido: liga ao parâmetro que estava esperando e grava o arquivo.
    const int cc = learnedCc.exchange (-1);
    const int target = learnTarget.load();
    if (cc >= 0 && target >= 0)
    {
        store->map.assign (cc, parameterIDs[target].toStdString());
        learnTarget.store (-1);
        ++learnRevision;
        store->changed();
    }

    // 2. Mapeamento mudou (aqui ou noutra instância): refaz a tabela do áudio.
    if (tableRevision != store->revision)
        rebuildTable();

    // 3. Valores que chegaram desde o último tique.
    applyPendingValues();
}

void MidiLearn::rebuildTable()
{
    for (int cc = 0; cc < MidiMap::numControllers; ++cc)
    {
        const auto& id = store->map.parameterFor (cc);
        const int index = id.empty() ? -1 : parameterIDs.indexOf (juce::String (id));
        ccToParameter[static_cast<std::size_t> (cc)].store (index); // -1 se o ID não existe mais
    }
    tableRevision = store->revision;
}

void MidiLearn::applyPendingValues()
{
    const auto& parameters = processor.getParameters();

    for (int cc = 0; cc < MidiMap::numControllers; ++cc)
    {
        const auto index = static_cast<std::size_t> (cc);
        if (! pendingDirty[index].exchange (false, std::memory_order_acquire))
            continue;

        const int paramIndex = ccToParameter[index].load();
        if (paramIndex < 0 || paramIndex >= parameters.size())
            continue;

        // "Pula na hora" (RF23): o knob vai direto ao valor do CC. O gesto avisa o host que é
        // uma mudança do usuário (o Live grava automação se estiver gravando).
        auto* param = parameters[paramIndex];
        param->beginChangeGesture();
        param->setValueNotifyingHost (pendingValue[index].load (std::memory_order_relaxed));
        param->endChangeGesture();
    }
}

int MidiLearn::ccFor (const juce::String& paramID) const
{
    return store->map.ccFor (paramID.toStdString());
}

void MidiLearn::startLearning (const juce::String& paramID)
{
    const int index = parameterIDs.indexOf (paramID);
    if (index < 0)
        return;

    learnedCc.store (-1); // um CC que chegou antes do clique não conta
    learnTarget.store (index);
    ++learnRevision;
}

void MidiLearn::cancelLearning()
{
    if (learnTarget.exchange (-1) >= 0)
        ++learnRevision;
}

juce::String MidiLearn::getLearningParameter() const
{
    const int index = learnTarget.load();
    return index >= 0 ? parameterIDs[index] : juce::String();
}

void MidiLearn::remove (const juce::String& paramID)
{
    if (store->map.ccFor (paramID.toStdString()) < 0)
        return;

    store->map.removeParameter (paramID.toStdString());
    store->changed();
    rebuildTable();
}

std::uint64_t MidiLearn::getRevision() const noexcept
{
    return store->revision + learnRevision;
}
