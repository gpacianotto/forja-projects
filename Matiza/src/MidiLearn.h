#pragma once

#include <array>
#include <atomic>
#include <cstdint>

#include <juce_audio_processors/juce_audio_processors.h>

#include "MidiMap.h"

// MIDI Learn (RF23): um CC do controlador gira um knob do Matiza.
//
// Três lados:
// - Mapeamento GLOBAL (um perfil do controlador): fica num arquivo do usuário e num objeto
//   compartilhado por todas as instâncias abertas no mesmo processo (juce::SharedResourcePointer,
//   parecido com um singleton com contagem de referências). Só a thread da interface mexe nele.
// - Thread de áudio (processMidi): olha os CCs do bloco. Sem lock, sem alocação: só lê a tabela
//   CC → parâmetro (atômicos) e anota o valor recebido para o timer aplicar.
// - Timer de 60 Hz na thread da interface: aplica os valores anotados aos parâmetros (avisando o
//   host, que pode gravar automação), confirma o CC aprendido e refaz a tabela quando o mapeamento
//   muda. O timer é do processador: o MIDI Learn funciona com a janela do plugin fechada.
class MidiLearn final : private juce::Timer
{
public:
    explicit MidiLearn (juce::AudioProcessor& processor);
    ~MidiLearn() override;

    // --- Thread de áudio (no processBlock, antes das vozes) ---
    void processMidi (const juce::MidiBuffer& midi) noexcept;

    // --- Thread da interface ---
    int ccFor (const juce::String& paramID) const;       // -1 = sem CC
    void startLearning (const juce::String& paramID);   // o próximo CC vai para este parâmetro
    void cancelLearning();
    juce::String getLearningParameter() const;          // "" = não está aprendendo
    void remove (const juce::String& paramID);

    // Muda a cada alteração do mapeamento ou do estado de aprendizado: a interface redesenha.
    std::uint64_t getRevision() const noexcept;

private:
    struct Store; // mapeamento global + arquivo (MidiLearn.cpp)

    void timerCallback() override;
    void rebuildTable();
    void applyPendingValues();

    juce::AudioProcessor& processor;
    juce::SharedResourcePointer<Store> store;

    // Índice em processor.getParameters() de cada ID (montado uma vez no construtor).
    juce::StringArray parameterIDs;

    // Tabela da thread de áudio: ccToParameter[cc] = índice do parâmetro, ou -1.
    std::array<std::atomic<int>, MidiMap::numControllers> ccToParameter;
    std::uint64_t tableRevision = 0; // revisão do Store usada na tabela

    // Último valor recebido de cada CC mapeado (0..1) e se ainda falta aplicar.
    std::array<std::atomic<float>, MidiMap::numControllers> pendingValue;
    std::array<std::atomic<bool>, MidiMap::numControllers> pendingDirty;

    // Aprendizado desta instância: índice do parâmetro esperando um CC (-1 = nenhum) e o CC que
    // a thread de áudio viu enquanto esperava (-1 = nenhum ainda).
    std::atomic<int> learnTarget { -1 };
    std::atomic<int> learnedCc { -1 };
    std::uint64_t learnRevision = 0; // conta inícios/fins de aprendizado (só thread da interface)

    JUCE_DECLARE_NON_COPYABLE (MidiLearn)
};
