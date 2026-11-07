# Zoom e movimento por objeto — 0.3.0

> Registro da implementação anterior. A versão 0.4.0 substitui o caminho padrão
> por artes completas, projeção do cenário e terreno em coordenadas de mundo.
> Consulte [ORGANIC-SCENE.md](ORGANIC-SCENE.md) para o estado atual e seus limites.

Use `Jogar.cmd`. Em **Imagem e CRT**, a opção **Zoom contínuo e movimento suave**
vem ligada e é salva com a chave `geometry` no INI do launcher. Ela funciona na
corrida individual dos três jogos, em 4:3 e 16:9, com pixels, linear ou CRT.
Pausa, menus, painel de conquistas e tela dividida usam o desenho original.

## Implementação na engine

`src/presentation/sprite_geometry.cpp` recebe os trabalhos do escalador original.
Cada trabalho registra descritor, escala, posição, imagem e peças da SAT produzidas.
O renderizador só aceita a correspondência quando os atributos apresentados na
VRAM e os hashes de todos os tiles da peça conferem. Uma fila ainda em construção
não se torna uma imagem apresentada por antecipação.

| Jogo | Entrada do escalador | Retornos observados |
|---|---|---|
| RR1 | `$9484` | `$9476` |
| RR2 | `$A5C8` | `$A5B8`, `$26EDA` |
| RR3 | `$1C24` | `$29160`, `$29B16` |

Os hooks são vinculados aos hashes locais das ROMs. O preparo do build verifica
também os opcodes de entrada e retorno. Nenhum hook dessa geometria grava RAM,
VRAM ou registradores do processador, nem consome ciclos do barramento convidado.

A interpolação usa profundidade inversa e coordenadas normalizadas pela escala.
As peças de um objeto compartilham a mesma transformação fracionária, incluindo
seu ponto de apoio. O VDP faz a amostragem inversa de seus tiles durante o desenho,
preservando transparência, paleta, espelhamento e ordem dos sprites. A textura
corresponde à pose atual; poses anteriores não são misturadas.

`tools/patch_sprite_geometry.py` integra os callbacks no runtime fixado, e
`patches/runtime-integration.patch` registra o resultado reproduzível. Um passe
autêntico determina colisão e overflow do hardware. O passe visual transforma os
retângulos e as coordenadas dos planos, sem substituir esses flags. O HUD, a partir
da linha 152, continua autêntico. O antigo `RR_SMOOTH` não reativa fluxo de imagem.

A pista interpola seus valores de scroll antes da consulta aos tiles. Todas as
linhas compartilham um relógio de atualização, evitando degraus causados por
relógios independentes. A assinatura considera SAT, tabela de scroll e VSRAM,
para que uma atualização de pista não dependa de um piloto mudar de posição.

Troca de imagem, reaparecimento, salto grande de posição/escala, pausa ou saída
da corrida reiniciam o histórico. A apresentação percorre um intervalo confirmado;
isso acrescenta atraso visual de até uma atualização da corrida durante a
interpolação. Física, entrada, cronômetro e frequência do áudio seguem nativos.

## Verificação

`tools/probe_geometry_video.py rr1` compara processos independentes com a opção
desligada/ligada. Também aceita `rr2`, `rr3`, `--wide` e `--scenario swerve`.
Cada sequência captura 120 frames consecutivos, compara RAM/VRAM antes e depois
de pausar/retomar, verifica título e pausa e exige igualdade de todos os pixels
do HUD. O cenário swerve aplica direção para os dois lados. O relatório também
exige amostras efetivas de escala intermediária, além de movimento.

Resultados da sequência de aceleração em 16:9, em 06/11/2026:

| Jogo | Imagens nativas / 120 | Com geometria / 120 | Frames adicionais de movimento |
|---|---:|---:|---:|
| RR1 | 28 | 113 | 85 |
| RR2 | 51 | 107 | 56 |
| RR3 | 47 | 106 | 59 |

RAM, VRAM, título, pausa e HUD ficaram idênticos nas comparações correspondentes.
As capturas e os CSVs privados ficam em `build/validation/geometry/`.
`tools/make_geometry_preview.py` empacota as sequências em
`build/validation/geometry/comparacao.html`, com original à esquerda e geometria
à direita, sincronizados no mesmo PNG animado. O HTML é independente da rede.
`tools/probe_launcher.py --game rr1` (também rr2/rr3) percorre o caminho real do
launcher, exige geometria intermediária, fonte 426×224 e CRT ativo, e preserva
as preferências do usuário. Os seis testes CTest incluem propriedade de textura,
escala intermediária, resets, relógio da pista e flags autênticos de colisão.
Os 17 testes Python da importação/geração também passaram.

`tools/probe_geometry_audio.py rr1` (também rr2/rr3) comparou o WAV nativo completo
com a geometria desligada/ligada, incluindo duas pausas e retomadas. Os três WAVs
ficaram idênticos byte por byte. Resultados em `build/validation/geometry/audio/`.

O pipeline completo com geometria e CRT a 1280×720 completou 3.600 frames por
jogo no ensaio sem limite: 474,8 / 448,1 / 449,4 fps médios, incluindo abertura
e corrida. Isso mede margem de processamento, sem prometer essa taxa no jogo.
Uma execução isolada de RR2, limitada ao alvo de 59,94 Hz, completou 2.700 frames
a 59,48 fps médios. Os logs em `build/validation/geometry/runtime/` conservam
também o contador de descarte da fila de áudio do dispositivo SDL dummy; esse
ensaio não homologa a entrega de áudio a um dispositivo físico. A comparação
de WAVs acima verifica separadamente que a geometria não mudou o PCM produzido.

## Limites

O raster e as texturas vêm da resolução original, inclusive dos bitmaps já
produzidos pelo escalador do jogo. O zoom aceita tamanhos fracionários e produz
estados intermediários; não acrescenta detalhes HD aos assets. A pista mantém
seus tiles e conteúdo originais, com scroll interpolado, sem reconstrução 3D.
Não há garantia de 60 imagens diferentes em cada segundo: poses discretas,
recortes nativos e resets continuam visíveis. Objetos ainda não correspondidos
usam sua geometria original. Culling lateral, todas as pistas e campanhas
completas continuam fora da cobertura desta validação. Tela dividida conserva
o renderizador original; multiplayer em rede permanece adiado.
