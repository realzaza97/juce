# Arquitetura

## Decisão principal

**Plugin JUCE (C++) + núcleo DSP independente de framework, tudo local, sem serviços online.**

- *JUCE 8 / CMake*: gera VST3 e Standalone para Windows x64 a partir do mesmo código, tem UI, leitura de WAV/MP3 e drag-and-drop de arquivos para fora do plugin.
- *Núcleo `v2i_core` sem JUCE*: o que decide a qualidade (pitch → notas → MIDI) é testável em linha de comando, com áudio sintético, sem abrir DAW. A UI só chama `v2i::analyze()` e `v2i::convertToMidiNotes()`.
- *Sem Python no caminho do plugin*: evita instalar interpretador, processos externos e falhas de IPC dentro da DAW. Um serviço externo só entra se a separação de voz por rede neural for adicionada (ver "Próximos passos").

## Fluxo de dados

```
WAV/MP3 ──JUCE AudioFormatReader──► mono float
   │  (thread de UI)
   ▼
[worker thread]  PitchTracker ──► frames (8 ms)  ──► NoteSegmenter ──► Note[] (segundos)
   │ AsyncUpdater
   ▼  (thread de UI)
Converter (BPM, speed, quantização, oitava, instrumento) ──► MidiNote[] (beats)  ◄──► Piano roll (edição)
   │                                            │
   │ snapshot (SpinLock, try-lock no áudio)     ├─► EXPORT / DRAG: MidiFile (SMF 0, 480 PPQ)
   ▼ (thread de áudio)                          
processBlock: lê posição do host → emite noteOn/noteOff no MidiBuffer
```

A thread de áudio **nunca** analisa nada: só lê um snapshot das notas (com `ScopedTryLock`) e gera eventos MIDI.

## Escolha do algoritmo de pitch

| Algoritmo | Precisão | Custo | Dependências | Decisão |
|---|---|---|---|---|
| YIN | boa em voz limpa; erros de oitava | muito baixo | nenhuma | base |
| **pYIN** | melhor: várias hipóteses + suavização temporal | baixo | nenhuma | **adotado (variante simplificada)** |
| CREPE | a melhor em áudio ruidoso | alto (rede neural ~22 M parâmetros; ONNX/TF Lite) | runtime de ML | não no MVP; possível backend opcional |

Implementação (`PitchTracker.cpp`):
1. Reamostra para 16 kHz (sinc janelado), filtro passa-altas 60 Hz.
2. Janelas de 30 ms a cada 8 ms; função de diferença YIN + normalização cumulativa (CMNDF).
3. Para cada quadro, guarda os mínimos locais bons (interpolação parabólica), do menor período para o maior, com **penalidade para sub-harmônicos** (candidato cujo período é ~2×/3×/4× o de outro candidato bom).
4. **Viterbi** sobre {candidatos ∪ não-vozeado}: custo de observação = CMNDF; custo de transição proporcional ao salto em semitons; custo fixo para alternar vozeado/não-vozeado. Isso corrige saltos de oitava e decide pausas.
5. Porta de volume relativa ao trecho mais alto da gravação (`sensibilidade` define quantos dB abaixo ainda contam) — funciona em gravações de celular baixas e reduz respiração/ruído.

Bug real encontrado nos testes: ordenar candidatos só pela profundidade descartava a fundamental em sinais muito periódicos (todos os múltiplos do período têm CMNDF ≈ 0). Por isso a lista é ordenada pelo menor período.

## De pitch contínuo para notas (`NoteSegmenter.cpp`)

1. Fecha lacunas não-vozeadas muito curtas (< 45 ms) entre pitches próximos.
2. Mediana de 40 ms (remove picos/oitavas isoladas).
3. **Vibrato × troca de nota:** mediana de ~120 ms. Mantém degraus nítidos (uma média móvel criaria rampas que virariam "notas de passagem" falsas) e achata vibrato.
4. **Segmentação ótima** por programação dinâmica (mínimos quadrados penalizados por nº de segmentos). Um degrau só vira nota nova se o ganho de erro superar a penalidade, o que depende da sensibilidade.
5. Une segmentos vizinhos que arredondam para a mesma nota; absorve "scoops" curtos (< ~85 ms) no início/fim de frases.
6. **Notas repetidas sem pausa**: divide em vales de energia (≥ 4–9 dB conforme a sensibilidade).
7. Remove notas curtas (40–110 ms) e de baixa confiança; velocity vem do nível RMS.

## Conversão para MIDI (`Converter.cpp`)

segundos → beats com `BPM / speed`; quantização por grade com força 0–1; oitava/instrumento; dobra de oitava para o registro do instrumento; resultado monofônico sem sobreposições. O arquivo leva *program change* GM, que só tem efeito em sintetizadores GM — VSTs do FL Studio o ignoram, então o instrumento real é escolhido no canal do FL.

## Voz monofônica × áudio polifônico

O MVP assume **uma melodia por vez**. Em mistura com várias vozes/instrumentos o rastreador seguirá a componente mais periódica e o resultado será instável. Caminho previsto: separar o vocal antes (modelo local tipo Demucs/MDX rodando fora do processo da DAW, via ONNX Runtime ou executável auxiliar com arquivos temporários, para não travar o plugin) e só então analisar. Polifonia real (acordes) exigiria outro algoritmo (ex.: Basic Pitch) e não está no escopo atual.

## Próximos passos (ordem sugerida)

1. Testar com vozes reais e ajustar parâmetros (limiares do `NoteSegmenter`).
2. Compilar e validar o VST3 no Windows/FL Studio (ver FL_STUDIO.md).
3. Gravação pelo microfone (Standalone primeiro).
4. Ajuste de escala/tonalidade (snap opcional) e detecção de BPM.
5. Separação vocal local; backend CREPE opcional.
6. Ferramentas de edição avançadas (seleção múltipla, undo/redo, pré-escuta interna).
