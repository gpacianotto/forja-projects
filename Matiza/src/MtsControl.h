#pragma once

#include <functional>

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"

// Coluna "MTS-ESP" da aba Tuning (RF18): o botão Follow MTS-ESP (parâmetro mtsFollow) e o
// indicador do estado embaixo dele:
//   ● Connected + nome da escala — seguindo o master;
//   ○ No master                  — Follow ligado, mas nenhum master na sessão;
//   ○ Not following              — há um master, mas o Follow está desligado.
//
// Lê o estado publicado pelo áudio (PluginProcessor::getMasterDisplay) num Timer de 10 Hz, na
// thread da interface, e só redesenha quando algo mudou.
class MtsControl final : public juce::Component, private juce::Timer
{
public:
    explicit MtsControl (PluginProcessor& processor);
    ~MtsControl() override { stopTimer(); }

    // Chamado quando o Matiza passa a seguir o master ou deixa de seguir (para o editor
    // desabilitar os knobs da afinação interna). Também chamado logo que é definido.
    void setOnFollowingChanged (std::function<void (bool following)> callback);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    PluginProcessor& pluginProcessor;

    juce::Label label;
    juce::ToggleButton followButton { "Follow" };
    juce::AudioProcessorValueTreeState::ButtonAttachment attachment;

    bool hasMaster = false;
    bool following = false;
    juce::String scaleName;
    juce::Rectangle<int> statusArea;

    std::function<void (bool)> onFollowingChanged;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MtsControl)
};
