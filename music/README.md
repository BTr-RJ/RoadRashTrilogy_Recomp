# Selectable soundtrack — 0.50

The Windows standard/offline release installers now include 18 full-length
Dean Harris Composer remake tracks, covering 20 mapped music cues across the
three games. The owner requested bundled music on 6 November 2026. This is a
local release candidate pending publication approval; no new license grant is
implied. Credits, official source URLs, durations and SHA-256 values travel in
`music/dean-harris/sources.json`. Source ZIPs exclude the recordings.

The installer copies `music/dean-harris/` with the rest of the application.
Playback resolves this path relative to the installed root, with no developer
drive path or separate music download. Existing private per-game maps under
`.local/music/dean-harris/` take precedence. Original music remains selectable;
unmapped, missing or undecodable files fall back to original audio.

Release preparation uses `tools/package_release.py --music-pack
.local/music/dean-harris --offline` with a fresh output directory. Only mapped
tracks, three TSV maps and sanitized source credits enter the installers;
unmapped audio and downloader `.info.json` files stay private. Track hashes are
verified before packaging and again by `tools/audit_release.py`.

## Earlier local pack and playback details

O launcher oferece a trilha original do Mega Drive e remakes de Dean Harris
Composer. A cópia local contém 18 faixas completas obtidas das publicações
públicas oficiais do artista; isso não representa os três álbuns completos.
Os arquivos e metadados com os links de origem ficam em
`.local/music/dean-harris/source/`, e os mapas de comandos em
`.local/music/dean-harris/rr1.tsv`, `rr2.tsv` e `rr3.tsv`.

Cada linha TSV tem três campos separados por tabulação: comando numérico de
música da ROM, nome do arquivo em `source/` e título. Esses comandos foram
mapeados nos drivers das revisões locais. O antigo JSON de exemplo não é
consumido pelo player atual.

O player usa Media Foundation do Windows para decodificar os M4A/AAC locais
em PCM estéreo. WAV e MP3 também dependem dos codecs disponíveis no Windows;
não há suporte próprio a Vorbis/FLAC. Cada faixa repete do começo ao fim; pontos
específicos de loop e fades não estão implementados. Arquivo ausente ou falha
de decodificação devolve aquele comando à trilha original.

O pause conserva a posição e a retomada continua desse ponto. O volume geral,
a música externa e os efeitos têm controles separados no launcher. Os valores
iniciais são 20% para remakes e 100% para efeitos; efeitos admitem até 200%.
O ganho dos efeitos só se aplica com remake ativo; a opção de trilha original
mantém a mixagem do Mega Drive. Os adaptadores preservam o caminho dos motores
ao substituir o BGM, sem silenciar o chip inteiro.

A [verificação de áudio](../docs/AUDIO-FIX.md) registra capturas PCM dos três
jogos, efeitos durante a corrida e duas pausas/retomadas. Outras pistas e
eventos ainda precisam de validação. A reprodução usa `RR_MUSIC=dean-harris`,
`RR_ROOT` apontando para o projeto e, opcionalmente, `RR_MUSIC_VOLUME` (0–100)
e `RR_EFFECTS_VOLUME` (0–200); o launcher configura essas variáveis.

Créditos: Dean Harris Composer. Fontes e ofertas do artista:
[Patreon oficial](https://www.patreon.com/DeanHarrisComposer) e
[publicação do álbum de Road Rash 3](https://www.patreon.com/DeanHarrisComposer/posts/road-rash-3-full-53364429).
As gravações mantêm os direitos dos respectivos titulares; a inclusão no
instalador não concede uma nova licença sobre as músicas.
