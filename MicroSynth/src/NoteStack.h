#pragma once

#include <array>
#include <cstddef>

// Pilha das teclas pressionadas no modo Mono (RF7). A última tecla pressionada fica no topo;
// ao soltar a nota que está tocando, o synth volta para o novo topo.
//
// Tamanho fixo (std::array, como um array Java de tamanho fixo): nunca aloca memória, então
// pode ser usada na thread de áudio. Se encher, a tecla mais antiga é descartada.
//
// C++ puro, sem JUCE: testável sozinha.
class NoteStack
{
public:
    struct Entry
    {
        int   note     = 0;
        int   channel  = 1;
        float velocity = 0.0f;
    };

    static constexpr std::size_t capacity = 128;

    bool empty() const noexcept { return count == 0; }
    std::size_t size() const noexcept { return count; }
    void clear() noexcept { count = 0; }

    // Topo da pilha (só chamar se !empty()).
    const Entry& top() const noexcept { return entries[count - 1]; }

    // Põe a tecla no topo. Se ela já estava na pilha, sai da posição antiga primeiro.
    void push (const Entry& entry) noexcept
    {
        remove (entry.note, entry.channel);

        if (count == capacity)
            removeAt (0); // cheia: descarta a mais antiga

        entries[count++] = entry;
    }

    // Tira a tecla da pilha, em qualquer posição. Não faz nada se ela não estiver lá.
    void remove (int note, int channel) noexcept
    {
        for (std::size_t i = 0; i < count; ++i)
        {
            if (entries[i].note == note && entries[i].channel == channel)
            {
                removeAt (i);
                return;
            }
        }
    }

private:
    void removeAt (std::size_t index) noexcept
    {
        for (std::size_t i = index; i + 1 < count; ++i)
            entries[i] = entries[i + 1];

        --count;
    }

    std::array<Entry, capacity> entries {};
    std::size_t count = 0;
};
