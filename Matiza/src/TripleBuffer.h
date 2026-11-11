#pragma once

#include <array>
#include <atomic>

// Entrega de dados grandes (tabelas) da thread da interface para a thread de áudio, sem lock
// e sem alocação (RF15).
//
// Três cópias do dado: uma publicada (a mais recente pronta), uma em uso pelo áudio e uma
// livre para a interface escrever. A interface nunca escreve na publicada nem na que o áudio
// está usando, então o áudio sempre lê uma tabela inteira e estável.
//
// Regras: um único escritor (a interface) e um único leitor (o áudio). O leitor chama read()
// uma vez por bloco e pode usar a referência até o próximo read().
//
// Em Java, faríamos isso com um AtomicReference para um objeto imutável novo a cada mudança;
// aqui reaproveitamos três objetos fixos para o áudio nunca precisar liberar memória.
template <typename T>
class TripleBuffer
{
public:
    // --- Escritor (thread da interface) ---

    // Devolve uma cópia livre para preencher. Depois de preencher, chame publish().
    T& beginWrite() noexcept
    {
        const int published = publishedIndex.load();
        const int inUse     = readerIndex.load();

        for (int i = 0; i < 3; ++i)
            if (i != published && i != inUse)
                writing = i;

        return slots[static_cast<std::size_t> (writing)];
    }

    void publish() noexcept { publishedIndex.store (writing); }

    // --- Leitor (thread de áudio) ---

    const T& read() noexcept
    {
        // Marca a publicada como "em uso" e confere que ela continua sendo a publicada. Se a
        // interface publicou outra no meio, tenta de novo. Assim o escritor, que evita a
        // publicada e a em uso, nunca escreve na cópia que o áudio vai ler.
        int index = publishedIndex.load();
        for (;;)
        {
            readerIndex.store (index);
            const int now = publishedIndex.load();
            if (now == index)
                break;
            index = now;
        }

        return slots[static_cast<std::size_t> (index)];
    }

    // Só para inicializar, antes de o áudio começar: escreve a mesma coisa nas três cópias.
    template <typename Fill>
    void fillAll (Fill fill)
    {
        for (auto& slot : slots)
            fill (slot);
    }

private:
    std::array<T, 3> slots {};
    std::atomic<int> publishedIndex { 0 };
    std::atomic<int> readerIndex { 0 };
    int writing = 1; // só o escritor usa
};
