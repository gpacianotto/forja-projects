#pragma once

#include <array>
#include <atomic>

// Entrega de dados grandes (tabelas) entre duas threads, sem lock e sem alocação: da interface
// para o áudio (formas desenhadas, RF15; afinação desenhada, RF17) ou do áudio para a
// interface (tabela do master do MTS-ESP, RF18).
//
// Três cópias do dado: uma publicada (a mais recente pronta), uma em uso pelo leitor e uma
// livre para o escritor. O escritor nunca escreve na publicada nem na que o leitor está
// usando, então o leitor sempre lê um dado inteiro e estável.
//
// Regras: um único escritor e um único leitor (cada um sempre na mesma thread). O leitor
// chama read() e pode usar a referência até o próximo read(). Escritor e leitor nunca
// esperam um pelo outro, então os dois lados podem ser a thread de áudio.
//
// Em Java, faríamos isso com um AtomicReference para um objeto imutável novo a cada mudança;
// aqui reaproveitamos três objetos fixos para o áudio nunca precisar liberar memória.
template <typename T>
class TripleBuffer
{
public:
    // --- Escritor ---

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

    // --- Leitor ---

    const T& read() noexcept
    {
        // Marca a publicada como "em uso" e confere que ela continua sendo a publicada. Se o
        // escritor publicou outra no meio, tenta de novo. Assim o escritor, que evita a
        // publicada e a em uso, nunca escreve na cópia que o leitor vai ler.
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
