# Cenário contínuo na engine — 0.4.0

> Registro da versão 0.4.0. A revisão [0.4.1](VISUAL-REGRESSIONS.md) corrige
> composição dos atores, origem de imagens sem âncora e scroll do raster,
> e remove o buffer visual de cinco frames descrito abaixo.

Abra `Jogar.cmd`. A opção **Zoom contínuo e movimento suave**, em **Imagem e
CRT**, agora inclui o cenário. Funciona nas corridas individuais dos três jogos,
em 4:3 e 16:9, inclusive com CRT. A preferência `geometry` existente continua
valendo. Não é necessário ativar outra opção.

## Referências e decisões

O [guia do próprio Lou](https://www.extentofthejam.com/pseudo/) explica a relação
entre profundidade, projeção, curvas, colinas e sprites apoiados na estrada.
`escala = focal / Z` é hiperbólica: o crescimento acelera perto da câmera.
O sprite precisa compartilhar o chão e a perspectiva da pista para parecer
preso ao mundo. O guia também descreve o caráter híbrido de Road Rash.

O [Mode 7 do SNES](https://snes.nesdev.org/wiki/Mode_7_transform) transforma um
plano de tiles; variar os parâmetros por linha produz a perspectiva do chão.
Isso não escala automaticamente árvores e pilotos. Como referências de hardware
arcade, foram consultadas as implementações dos
[sprites Sega no MAME](https://github.com/mamedev/mame/blob/master/src/mame/sega/sega16sp.cpp),
da [estrada de Out Run](https://github.com/mamedev/mame/blob/master/src/mame/sega/segaic16_road.cpp),
do [X Board de Super Monaco GP](https://github.com/mamedev/mame/blob/master/src/mame/sega/segaxbd.cpp)
e do [vídeo System 32 de Rad Mobile](https://github.com/mamedev/mame/blob/master/src/mame/sega/segas32_v.cpp).
O [Cannonball de Chris White](https://github.com/djyt/cannonball/blob/master/src/main/engine/osprites.cpp)
serviu de referência adicional sobre objetos de Out Run. Nenhum código dessas
implementações foi copiado para esta mudança.

A aplicação aqui combina projeção dos sprites com coordenadas de mundo no
terreno, conservando a geometria e os dados originais da pista do Mega Drive.

## O que mudou

- **Arte completa:** os sprites são decodificados da ROM antes do escalador
  original. A engine amostra essa arte diretamente no tamanho projetado, sem
  ampliar um bitmap que já perdeu pixels ou foi recortado. O mesmo caminho
  atende árvores, placas, construções, objetos e veículos correspondidos.
  Artes compostas mantêm os pontos de apoio das duas partes.
- **Identidade no mundo:** o cenário usa segmento da pista, origem e posição
  da instância. O índice do descritor deixa de ser tratado como identidade:
  o jogo original compacta essa lista a cada atualização.
- **Tempo comum:** posição e escala usam profundidade inversa em um histórico
  confirmado. Objetos, scroll dos planos e terreno consultam o mesmo instante,
  cinco frames atrás do frame simulado. Isso evita mudanças de velocidade
  aparentes quando a atualização nativa alterna sua duração.
- **Saída pelas bordas:** objetos de cenário com pelo menos duas poses
  confirmadas continuam sendo projetados após o descarte nativo. A evolução
  acompanha o avanço confirmado da câmera. O recorte usa o retângulo completo
  da arte, incluindo as margens de 16:9. A retenção tem limite de 40 frames;
  pausa ou mudança de cena limpa o histórico imediatamente.
- **Terreno:** a engine inverte os raios de profundidade da pista por linha e
  calcula UV em coordenadas de mundo. A textura de terreno de duas cores é
  extraída dos tiles do jogo, com a paleta original. Amostragem por área e
  convergência para a cor dominante reduzem cintilação na distância. Materiais
  não reconhecidos conservam seus pixels originais.
- **Pista e fundo:** os planos da estrada, montanhas e céu usam o mesmo tempo
  de apresentação. A pista conserva suas curvas, colinas e raster originais;
  os sprites mantêm o recorte por colinas e a transparência de suas artes.

As peças só são substituídas após conferir atributos, pixels e pertencimento
à lista SAT **ativa**. Entradas antigas, que continuam na VRAM depois do fim
da lista encadeada, não viram objetos fantasmas. Paleta e prioridade vêm da
SAT apresentada. O bit `$4000` do descritor não é interpretado como inversão
vertical; a orientação vertical vem do atributo real do VDP.

## Integração e reprodução

`sprite_geometry.cpp` coordena histórico, projeção, retenção e materiais.
`sprite_assets.cpp` decodifica as linhas 4bpp, incluindo o byte de guarda por
linha. `road_surface.cpp` inverte o mapa de raios e respeita sua visibilidade.
Os hooks em `game_spec.c` só observam dados; o preparo verifica hash e opcodes
das três ROMs. `patch_sprite_geometry.py` instala callbacks de apresentação no
VDP, registrados também em `patches/runtime-integration.patch`.

O passe autêntico continua responsável por colisões e overflow de sprites.
O passe visual muda somente a apresentação. O terreno não substitui pixels de
sprites visíveis. HUD a partir da linha 152, menus, pausa, painel e tela dividida
continuam no desenho original. `RR_SMOOTH` permanece sem efeito.

Para diagnóstico, `RR_GEOMETRY=0` seleciona o desenho nativo;
`RR_GEOMETRY=1 RR_ORGANIC=0` seleciona a apresentação anterior da versão 0.3.
O launcher habilita a apresentação nova ao ligar sua opção de geometria.

```powershell
.tools/python/python.exe tools/probe_geometry_video.py rr1 --wide
.tools/python/python.exe tools/probe_geometry_video.py rr1 --wide --scenario swerve
.tools/python/python.exe tools/probe_geometry_video.py rr1 --wide --baseline previous
.tools/python/python.exe tools/probe_geometry_audio.py rr1
.tools/python/python.exe tools/probe_launcher.py --game rr1
```

Troque `rr1` por `rr2` ou `rr3`. `RR_VIDEO_VALIDATION` escolhe a pasta das
capturas de vídeo. Os ensaios atuais ficam em `build/validation/organic-final/`
e `build/validation/organic-previous/`. `make_geometry_preview.py` gera um
`comparacao.html` independente da rede, com os dois lados sincronizados num
único PNG animado. O padrão compara a implementação anterior com a nova.

## Validação e limites

Na aceleração capturada em 16:9, com 120 frames consecutivos por jogo:

| Jogo | Nativo | Versão anterior | Cenário novo |
|---|---:|---:|---:|
| RR1 | 28 | 113 | 120 |
| RR2 | 51 | 107 | 120 |
| RR3 | 47 | 106 | 120 |

Os valores contam imagens distintas, sem medir isoladamente cada objeto.
RAM/VRAM antes e depois de pausar/retomar, título, pausa e todos os pixels do HUD
foram comparados entre processos independentes. Os relatórios registram também
projeções intermediárias, desenho das artes completas, retenção e terreno.

Passaram 12 comparações de vídeo: aceleração em 4:3 e 16:9, direção para os dois
lados em 16:9 e comparação com a implementação anterior, nos três jogos. As
sequências de direção também tiveram 120 imagens distintas por captura. Na
aceleração em 16:9, houve continuação após o descarte nativo de 24 / 60 / 57
instâncias de cenário em RR1 / RR2 / RR3 durante todo o replay. Os três jogos
passaram pelo caminho real do launcher com fonte 426×224 e CRT ativo, sem
alterar as preferências salvas.

Os WAVs completos produzidos com geometria desligada/ligada, incluindo duas
pausas e retomadas, foram idênticos byte por byte nos três jogos. Isso verifica
o PCM produzido, sem homologar um dispositivo físico de áudio.

O pipeline com CRT Monitor-Balanced a 1280×720 completou 3.600 frames por jogo
no ensaio sem limite: 401,5 / 381,5 / 377,5 fps médios em RR1 / RR2 / RR3.
Esses valores medem margem de processamento, incluindo abertura e corrida;
não são uma promessa de cadência de apresentação. Logs e contadores de áudio
do dispositivo SDL dummy ficam em `organic-final/runtime/`. O descarte da fila
nesse modo sem limite é registrado no log e não é usado para validar o PCM.

Os sete testes CTest cobrem decodificação, profundidade, orientação, paleta,
retenção sem fantasmas de uma única observação, limpeza na pausa, material sem
apagar sprites, rejeição de entradas SAT não encadeadas e flags autênticos do
VDP. Os 17 testes Python da ferramenta
de importação e geração também passaram.

A apresentação acrescenta cinco frames de atraso visual, aproximadamente
83 ms a 59,94 Hz. Entrada e simulação continuam nativas. O raster, as poses e
as artes têm a resolução original; não foram criados detalhes HD. Objetos que
nunca chegaram a ser apresentados pelo jogo original não são instanciados
por este caminho. Sem trajetória ou arte confirmada, há retorno ao desenho
nativo. A retenção é limitada e não garante eliminar todo desaparecimento.

As sequências reproduzidas não homologam todas as pistas, efeitos especiais,
campanhas ou configurações de vídeo. Os resultados de 120 imagens diferentes
não garantem 60 imagens diferentes em todo segundo do jogo. Tela dividida
mantém o renderizador original.
