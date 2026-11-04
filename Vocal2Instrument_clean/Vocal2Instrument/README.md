# Vocal2Instrument

Plugin (VST3 + Standalone, Windows 64-bit) que converte uma gravação vocal **monofônica** em notas MIDI editáveis, para tocar com qualquer instrumento do FL Studio.

> Estado: **MVP (v0.1)**. Veja [Estado real do projeto](#estado-real-do-projeto) antes de usar.

## O que o MVP faz

| Etapa | Situação |
|---|---|
| Importar WAV (também AIFF/FLAC/OGG) por botão ou arrastar-e-soltar | implementado |
| Importar MP3 | habilitado (`JUCE_USE_MP3AUDIOFORMAT=1`), **ainda não testado** |
| Forma de onda + seleção de trecho (arraste; duplo clique limpa) | implementado |
| ANALYZE (pitch + notas) em thread separada, com progresso e cancelamento | implementado |
| Piano roll: mover, redimensionar, adicionar (duplo clique), apagar (botão direito / Delete), nome das notas, grade, zoom | implementado |
| Curva de pitch bruto sobre o piano roll | implementado |
| Sensibilidade, quantização (Off, 1/4…1/32), oitava, BPM, velocidade, instrumento | implementado |
| CONVERT TO MIDI / EXPORT MIDI (.mid, 480 PPQ) / DRAG MIDI para o FL Studio | implementado |
| Saída MIDI em tempo real sincronizada com o transporte do host | implementado, **não testado no FL Studio** |
| Gravação pelo microfone | **não implementado** (botão REC desabilitado) |
| Separação de voz / áudio polifônico | **não implementado** |

## Estrutura

```
Vocal2Instrument/
├─ CMakeLists.txt              raiz: núcleo + testes + plugin
├─ core/                       núcleo DSP em C++17 puro (sem JUCE)
│  ├─ include/v2i/*.h
│  └─ src/
│     ├─ PitchTracker.cpp      YIN + candidatos múltiplos + Viterbi (estilo pYIN)
│     ├─ NoteSegmenter.cpp     suavização, vibrato x troca de nota, segmentação ótima, filtros
│     ├─ Converter.cpp         quantização, transposição, presets de instrumento
│     ├─ MidiFile.cpp          escritor/leitor de .mid
│     └─ WavReader.cpp         leitor/escritor WAV (CLI e testes)
├─ cli/main.cpp                v2i_cli: converte WAV -> MID na linha de comando
├─ tests/test_main.cpp         testes com voz sintética (verdade conhecida)
├─ plugin/                     JUCE 8: processor, editor, waveform, piano roll
│  └─ Source/…
├─ docs/ARCHITECTURE.md        arquitetura, algoritmos, limites
├─ docs/FL_STUDIO.md           instalar e testar no FL Studio
└─ .github/workflows/windows.yml   compila o VST3 no Windows via GitHub Actions
```

## Compilar

### Só o núcleo + testes (qualquer sistema)

```
cmake -S . -B build -DV2I_BUILD_PLUGIN=OFF
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
./build/v2i_cli minha_voz.wav saida.mid --bpm 100 --quant 16 --inst piano --verbose
```

### Plugin no Windows 10/11 (x64)

Requisitos: Visual Studio 2022 (carga de trabalho "Desenvolvimento para desktop com C++"), CMake ≥ 3.22, Git.

```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

O CMake baixa o JUCE 8.0.4 automaticamente. Resultado:
`build/plugin/Vocal2Instrument_artefacts/Release/VST3/Vocal2Instrument.vst3` e `.../Standalone/Vocal2Instrument.exe`.

Sem Visual Studio instalado: faça push para o GitHub; o workflow `.github/workflows/windows.yml` compila e publica o VST3 como artefato.

Instalação e teste no FL Studio: [docs/FL_STUDIO.md](docs/FL_STUDIO.md).

## Licença do JUCE

O JUCE é AGPLv3 / comercial. O `CMakeLists` mantém a tela de abertura do JUCE ligada (`JUCE_DISPLAY_SPLASH_SCREEN=1`). Para distribuir comercialmente você precisa de uma licença JUCE adequada.

## Estado real do projeto

**Verificado neste ambiente (Linux):**
- Núcleo DSP compila sem avisos e passa nos testes com voz sintética: vibrato de ±0,5 e ±0,8 semitom, sem vibrato, voz grave com ruído a 48 kHz, notas repetidas sem pausa, notas de 120 ms, gravação baixa a 22 kHz e ruído puro (0 notas). Em todos: 100 % das notas com pitch correto, 0 notas falsas, erro médio de início de 2–14 ms.
- Escrita/leitura de MIDI (tempo, program change, notas) e quantização/transposição.
- Desempenho: ~182 s de áudio analisados em ~4 s.
- Os 4 arquivos-fonte do plugin passam na checagem de sintaxe (`-fsyntax-only -Wall -Wextra`, sem avisos) contra os cabeçalhos do JUCE 8.0.4.

**Não verificado:**
- Link/execução do VST3 e do Standalone (o ambiente não tem as bibliotecas de sistema do JUCE nem Windows).
- Aparência real da interface, comportamento do arrastar-e-soltar de MIDI e da saída MIDI dentro do FL Studio.
- Qualidade com **vozes reais** (os testes usam voz sintetizada; consoantes, respiração real, desafinação e microfones ruins ainda vão exigir ajuste de parâmetros).
- MP3.

## Limitações conhecidas

- Apenas **voz solo monofônica**. Áudio com várias vozes ou instrumentos (música completa) não é suportado; a separação de voz (ex.: Demucs/Spleeter local) é o próximo passo planejado, não algo pronto.
- O ritmo vem do tempo real do áudio; o BPM é informado pelo usuário (não há detecção automática de andamento).
- Notas são arredondadas ao semitom mais próximo (ainda sem ajuste de escala/tonalidade).
