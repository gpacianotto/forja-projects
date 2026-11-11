// Testes das peças puras da v0.2: transposição dos osciladores (RF6), glide e pilha de notas (RF7).
// Nomes de TEST_CASE sem acento: o ctest no Windows não os encontra se tiverem.
#include "Glide.h"
#include "NoteStack.h"
#include "TuningEngine.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

// ----------------------------------------------------------------------------- transposeRatio

TEST_CASE ("transposeRatio: sem transposicao = 1")
{
    CHECK_THAT (TuningEngine::transposeRatio (12, 0, 0, 0), WithinRel (1.0, 1e-12));
}

TEST_CASE ("transposeRatio: oitavas dobram e dividem a frequencia")
{
    CHECK_THAT (TuningEngine::transposeRatio (24, 1, 0, 0),  WithinRel (2.0, 1e-12));
    CHECK_THAT (TuningEngine::transposeRatio (24, -2, 0, 0), WithinRel (0.25, 1e-12));
}

TEST_CASE ("transposeRatio: edo passos = uma oitava, em qualquer EDO")
{
    for (int edo : { 5, 12, 19, 24, 31, 52 })
        CHECK_THAT (TuningEngine::transposeRatio (edo, 0, edo, 0), WithinRel (2.0, 1e-12));
}

TEST_CASE ("transposeRatio: 1200 cents = uma oitava, 100 cents = semitom")
{
    CHECK_THAT (TuningEngine::transposeRatio (12, 0, 0, 1200.0), WithinRel (2.0, 1e-12));
    CHECK_THAT (TuningEngine::transposeRatio (24, 0, 0, 100.0),
                WithinRel (TuningEngine::transposeRatio (12, 0, 1, 0), 1e-12));
}

TEST_CASE ("transposeRatio igual a tocar a nota transposta")
{
    TuningEngine engine;
    engine.setSettings ({ 24, 440.0, 69, 2.0 });

    // Osc com +1 oitava e +3 passos em 24 EDO = nota 69 + 24 + 3.
    const double transposed = engine.noteToHz (69) * TuningEngine::transposeRatio (24, 1, 3, 0);
    CHECK_THAT (transposed, WithinRel (engine.noteToHz (69 + 24 + 3), 1e-12));
}

TEST_CASE ("noteToHz aceita altura fracionaria (glide)")
{
    TuningEngine engine;
    engine.setSettings ({ 12, 440.0, 69, 2.0 });

    // Meio caminho entre 69 e 70 = meio semitom = 50 cents acima de 440 Hz.
    CHECK_THAT (engine.noteToHz (69.5), WithinRel (440.0 * std::exp2 (0.5 / 12.0), 1e-12));
}

// ----------------------------------------------------------------------------- Glide

TEST_CASE ("Glide: jumpTo vai direto, sem deslizar")
{
    Glide glide;
    glide.setTime (0.1, 1000.0);
    glide.jumpTo (60.0);

    CHECK_FALSE (glide.isGliding());
    CHECK (glide.next() == 60.0);
}

TEST_CASE ("Glide: tempo zero = salto imediato")
{
    Glide glide;
    glide.setTime (0.0, 48000.0);
    glide.jumpTo (60.0);
    glide.glideTo (72.0);

    CHECK_FALSE (glide.isGliding());
    CHECK (glide.getCurrent() == 72.0);
}

TEST_CASE ("Glide: linear e chega exatamente no alvo no tempo certo")
{
    Glide glide;
    glide.setTime (0.01, 1000.0); // 10 amostras
    glide.jumpTo (60.0);
    glide.glideTo (70.0);

    for (int i = 1; i <= 9; ++i)
        CHECK_THAT (glide.next(), WithinAbs (60.0 + i, 1e-9));

    CHECK (glide.next() == 70.0); // décima amostra: exatamente o alvo
    CHECK_FALSE (glide.isGliding());
    CHECK (glide.next() == 70.0); // e fica lá
}

TEST_CASE ("Glide: novo alvo no meio do glide parte da altura atual")
{
    Glide glide;
    glide.setTime (0.01, 1000.0); // 10 amostras
    glide.jumpTo (60.0);
    glide.glideTo (70.0);

    for (int i = 0; i < 5; ++i)
        glide.next(); // agora em 65

    glide.glideTo (55.0); // volta: de 65 a 55 em 10 amostras
    CHECK_THAT (glide.next(), WithinAbs (64.0, 1e-9));

    for (int i = 0; i < 9; ++i)
        glide.next();

    CHECK (glide.getCurrent() == 55.0);
}

// ----------------------------------------------------------------------------- NoteStack

TEST_CASE ("NoteStack: ultima nota pressionada fica no topo")
{
    NoteStack stack;
    CHECK (stack.empty());

    stack.push ({ 60, 1, 0.5f });
    stack.push ({ 64, 1, 0.6f });
    stack.push ({ 67, 1, 0.7f });

    CHECK (stack.size() == 3);
    CHECK (stack.top().note == 67);
    CHECK (stack.top().velocity == 0.7f);
}

TEST_CASE ("NoteStack: soltar o topo volta para a anterior")
{
    NoteStack stack;
    stack.push ({ 60, 1, 0.5f });
    stack.push ({ 64, 1, 0.5f });

    stack.remove (64, 1);
    CHECK (stack.top().note == 60);

    stack.remove (60, 1);
    CHECK (stack.empty());
}

TEST_CASE ("NoteStack: soltar uma nota do meio mantem o topo")
{
    NoteStack stack;
    stack.push ({ 60, 1, 0.5f });
    stack.push ({ 64, 1, 0.5f });
    stack.push ({ 67, 1, 0.5f });

    stack.remove (64, 1);
    CHECK (stack.size() == 2);
    CHECK (stack.top().note == 67);

    stack.remove (67, 1);
    CHECK (stack.top().note == 60);
}

TEST_CASE ("NoteStack: repetir a nota move para o topo sem duplicar")
{
    NoteStack stack;
    stack.push ({ 60, 1, 0.5f });
    stack.push ({ 64, 1, 0.5f });
    stack.push ({ 60, 1, 0.9f });

    CHECK (stack.size() == 2);
    CHECK (stack.top().note == 60);
    CHECK (stack.top().velocity == 0.9f);
}

TEST_CASE ("NoteStack: mesma nota em canais diferentes sao teclas diferentes")
{
    NoteStack stack;
    stack.push ({ 60, 1, 0.5f });
    stack.push ({ 60, 2, 0.5f });

    CHECK (stack.size() == 2);
    stack.remove (60, 2);
    CHECK (stack.top().channel == 1);
}

TEST_CASE ("NoteStack: remover nota ausente nao faz nada")
{
    NoteStack stack;
    stack.push ({ 60, 1, 0.5f });
    stack.remove (61, 1);
    CHECK (stack.size() == 1);
}

TEST_CASE ("NoteStack: cheia descarta a mais antiga")
{
    NoteStack stack;
    for (int i = 0; i < static_cast<int> (NoteStack::capacity) + 1; ++i)
        stack.push ({ i % 128, 1 + i / 128, 0.5f });

    CHECK (stack.size() == NoteStack::capacity);
    CHECK (stack.top().note == 0);    // a 129ª: nota 0 no canal 2
    CHECK (stack.top().channel == 2);

    // A primeira (nota 0, canal 1) foi descartada.
    stack.remove (0, 2);
    CHECK (stack.top().note == 127);
}
