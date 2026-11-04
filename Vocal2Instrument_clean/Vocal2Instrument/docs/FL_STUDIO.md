# Instalar e testar no FL Studio (Windows 10/11 x64)

> Este roteiro **ainda não foi executado** por mim no FL Studio. Siga na ordem; os pontos marcados com ⚠ são os mais prováveis de precisar de ajuste.

## 1. Instalar o VST3

1. Compile (README) ou baixe o artefato do GitHub Actions.
2. Copie a **pasta** `Vocal2Instrument.vst3` para `C:\Program Files\Common Files\VST3\` (precisa de permissão de administrador).
3. FL Studio → **Options ▸ Manage plugins** → *Find installed plugins* (ou *Verify plugins* se já estiver listado).
4. Procure "Vocal2Instrument" em *Plugin database ▸ Installed ▸ Generators* (ele é marcado como instrumento).

## 2. Teste rápido sem a DAW (recomendado primeiro)

Abra `Vocal2Instrument.exe` (Standalone).

1. Arraste um WAV de voz solo para a janela (ou **LOAD**).
2. Opcional: arraste sobre a forma de onda para analisar só um trecho.
3. **ANALYZE** → as notas aparecem no piano roll com a curva de pitch bruto em azul.
4. Ajuste **SENSITIVITY** e rode ANALYZE de novo se faltarem ou sobrarem notas. Ajuste **QUANTIZE / OCTAVE / BPM / INSTRUMENT** e use **CONVERT TO MIDI** (isso refaz as notas a partir da análise e descarta edições manuais).
5. **EXPORT MIDI** e abra o `.mid` em qualquer programa para conferir.

## 3. Uso dentro do FL Studio

### Caminho A – arrastar o MIDI (principal, mais previsível)
1. Adicione o Vocal2Instrument num canal (*Channels ▸ Add one ▸ Vocal2Instrument*).
2. Carregue o áudio, ANALYZE, edite.
3. **Segure o botão `⇩ DRAG MIDI` e arraste** para o *Playlist* ou para o *Piano roll* de outro canal (com o instrumento que quiser: piano, FLEX, Sytrus, VST de cordas…).
   ⚠ Se o arrasto não for aceito, use **EXPORT MIDI** e arraste o `.mid` do Explorer para o FL Studio.

### Caminho B – MIDI em tempo real
O plugin emite as notas do piano roll pelo transporte do FL (posição 0 = início do projeto).
1. Na janela do plugin (*Plugin settings ⚙ ▸ VST wrapper settings*), defina a **MIDI output port** (ex.: 1).
2. Em outro canal com um instrumento, abra o *Input* do canal e escolha a mesma porta em *Input ▸ MIDI input port* (FL: *Settings ▸ MIDI ▸ Input/Output ports*, porta virtual de saída ligada à entrada do instrumento).
3. Dê play. ⚠ O encaminhamento de MIDI de saída de VST3 varia entre versões do FL; se não funcionar, use o Caminho A.

## 4. Checklist de teste

- [ ] O plugin aparece na lista e abre sem travar.
- [ ] WAV carrega, forma de onda aparece.
- [ ] ANALYZE termina e mostra notas (voz solo clara).
- [ ] Arrastar/redimensionar/apagar/adicionar nota funciona.
- [ ] EXPORT MIDI abre corretamente em outro programa; andamento e notas conferem.
- [ ] DRAG MIDI funciona no Playlist/Piano roll.
- [ ] Salvar e reabrir o projeto do FL restaura parâmetros e notas.
- [ ] MP3 carrega.

Anote qualquer falha com o arquivo de áudio usado para que seja possível reproduzir.
