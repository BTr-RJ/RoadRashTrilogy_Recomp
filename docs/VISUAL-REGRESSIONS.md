# Composição e resposta — 0.4.1

Registro da revisão 0.4.1. A versão atual e as mudanças adicionais de tráfego,
pista e marcações estão em [APPROACH-SMOOTHING.md](APPROACH-SMOOTHING.md).

Esta revisão corrige regressões da apresentação 0.4.0 nos três jogos. Abra
`Jogar.cmd`; a opção de zoom existente continua habilitando a melhoria.

## Causas e correções

O caminho de artes completas foi aplicado também a pilotos e tráfego, cujas
animações são compostas, recortadas e recoloridas pelo escalador original.
Redesenhar essas imagens como um billboard único perdeu a composição das
peças e podia desenhar novamente uma parte do tronco. Esses objetos agora usam
seus tiles atuais confirmados na SAT, com a transparência, paleta e prioridade
de cada peça. As peças de cada trabalho compartilham a transformação. Uma
troca de imagem reinicia a previsão dos atores, evitando usar a geometria de
uma pose anterior em um golpe ou em outro veículo.

O cenário continua usando artes completas e retenção nas bordas, mas somente
o hook de posicionamento na pista autoriza esse caminho. A assinatura de
imagem, segunda imagem, flags e profundidade relativa precisa corresponder
ao descritor. Um slot reaproveitado para uma moto não herda a classificação de
uma árvore que ocupava esse endereço anteriormente.

As artes sem âncoras explícitas também estavam posicionadas incorretamente:
o escalador usa **centro/base**, e a implementação usava topo/esquerda. Isso
colocava pessoas e outros objetos abaixo do recorte por colina e os tornava
invisíveis. O decodificador agora respeita a origem nativa.

O passe visual desenha também as peças atuais não correspondidas, preservando
sua geometria nativa, sem reaplicar os limites de sprites por linha. Isso reduz
o pisca-pisca em cenas densas. O passe autêntico continua determinando overflow
e colisões para a simulação.

Na pista, scroll vertical atrasado e bordas A/B atrasadas independentemente
eram usados junto ao bitmap atual. Essa mistura podia expor dados do raster
como faixas azuis, especialmente em mudanças de nível. A apresentação conserva
o scroll vertical e a largura atuais. A previsão horizontal aplica a mesma
translação aos dois lados, limitada a quatro pixels, sem abrir uma fenda entre
as metades da pista. O terreno usa o mapa de raios atual e mantém sua textura
em coordenadas de mundo.

## Atraso visual

O buffer de cinco frames, aproximadamente 83 ms, foi removido. A pose confirmada
mais recente é apresentada imediatamente. Entre atualizações nativas, a
previsão é limitada a dois frames para atores e a seis para a câmera do cenário.
O cenário e o terreno compartilham essa previsão; manter apenas dois frames
para ambos congelava parte dos intervalos mais longos de RR1. O jogador em escala de
câmera conserva sua pose atual, inclusive golpes e direção. A retenção de
cenário após descarte nativo continua limitada a 40 frames.

Isso remove o atraso acrescentado pela melhoria. Não representa uma medição
de latência total de controle, monitor, sincronização ou CRT.

## Verificação reproduzível

```powershell
.tools/python/python.exe tools/probe_visual_regressions.py rr1
.tools/python/python.exe tools/check_visual_regressions.py rr1
.tools/python/python.exe tools/probe_visual_regressions.py rr1 --scenario touring
.tools/python/python.exe tools/check_visual_regressions.py rr1 --scenario touring
```

Repita com `rr2` e `rr3`. Os comparadores precisam das capturas nativas
correspondentes, geradas com `--label native --native`. O cenário `combat`
captura aceleração, golpes, chute e direção com comandos separados para cada
botão. O cenário `touring` registra 48 checkpoints, separados por 150 frames,
para observar tráfego, curvas e mudanças de nível. Os inputs e logs ficam em
`build/validation/regressions04/`.

A auditoria por `RR_GEOMETRY_AUDIT` observa o dono e a cor de cada pixel durante
os dois passes do VDP. Ela verifica se o fundo substituiu uma parte visível do
jogador ou dos atores na pose atual. Alterações causadas por outro veículo
projetado à frente são contadas separadamente como oclusão. A busca por faixas
azuis compara pixels de asfalto fora dos sprites e das áreas dos billboards.
Os contadores são específicos desses critérios, não um detector universal de
defeitos visuais.

Essa checagem é necessária porque um dump de VRAM ao fim do frame pode conter
uma animação que foi transferida depois de suas linhas terem sido desenhadas.
Igualdade de RAM/VRAM e contagem de imagens distintas, usadas na validação
anterior, não eram suficientes para detectar essas regressões de composição.

Os testes da engine incluem âncora padrão, reaproveitamento de descritor,
troca de imagem, apresentação imediata da escala confirmada, scroll vertical e
largura atuais, transparência/prioridade, auditoria no raster e uma linha com
21 sprites visuais mantendo o overflow autêntico. O zoom do cenário e a
projeção do terreno continuam sendo verificados separadamente.

`tools/make_regression_preview.py` gera a comparação animada de RR1 e RR2 em
`build/validation/regressions04/comparacao.html`, usando executáveis e capturas
preservados da 0.4.0 e a revisão corrigida. Os dois lados executam os mesmos
inputs, com RAM, VRAM e HUD comparados. A página é independente da rede.

As verificações cobrem trajetórias reproduzidas nas pistas iniciais; não
homologam todas as pistas e campanhas. Tela dividida conserva o desenho
original. O fluxo de imagem por `RR_SMOOTH` continua desativado.

## Resultados desta revisão

Os sete grupos CTest e os 17 testes Python passaram. Passaram também as seis
auditorias de raster (combate e touring nos três jogos), nove comparações de
apresentação (4:3, 16:9 e direção em 16:9) e três comparações de PCM. RAM, VRAM,
HUD, título, pausa e retomada permaneceram idênticos nas amostras correspondentes.
Os três WAVs completos ficaram idênticos entre geometria desligada/ligada.

| Jogo | Pixels do jogador auditados no replay touring | Corrupção do jogador | Pixels atuais de atores perdidos | Candidatos a faixa azul no asfalto |
|---|---:|---:|---:|---:|
| RR1 | 6.756.465 | 0 | 0 | 0 |
| RR2 | 7.003.341 | 0 | 0 | 0 |
| RR3 | 6.724.075 | 0 | 0 | 0 |

O touring registrou ainda 1.410 / 325 / 222 pixels do jogador cobertos por outro
ator projetado em RR1 / RR2 / RR3. Esses casos têm outro sprite como dono do
pixel, não uma marcação da pista, e permanecem registrados separadamente nos
CSVs e JSONs. No cenário de combate, todos os pixels auditados do jogador
permaneceram idênticos ao passe nativo, sem essas oclusões adicionais.

As capturas de aceleração tiveram 120 imagens distintas em 120 frames por jogo,
tanto em 4:3 quanto em 16:9. Isso confirma que o movimento adicional do cenário
foi mantido após remover o buffer, sem garantir essa cadência em toda situação.
As preferências salvas são preservadas pelo teste do caminho real do launcher
com CRT e fonte 426×224. O relatório agregado fica em
`build/validation/regressions04/result.json`.
