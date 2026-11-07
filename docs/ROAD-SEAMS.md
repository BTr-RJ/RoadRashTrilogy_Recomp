# Bordas e marcações da pista — 0.4.3

Abra `Jogar.cmd`. A revisão corrige quebras no centro e no canto inferior
direito de RR1/RR2 e a duplicação das faixas amarelas durante essas situações.
O zoom de tráfego e a composição de pilotos da revisão anterior continuam ativos.

## Causas e correções

A continuação do raster na 0.4.2 atendia somente ao limite inferior da metade
esquerda de RR1. A outra metade podia continuar presa ao limite nativo,
deslocando o centro inferido da pista. Agora os limites inferior e superior
das duas metades recebem o mesmo tratamento em RR1/RR2. A continuação usa
a última tangente válida do frame atual. O scroll vertical continua atual;
não há buffer de imagem nem atraso adicional. A correção fica abaixo de meia
volta do fetch de 10 bits para preservar seu sinal.

O bitmap de cada plano contém duas bordas de uma faixa finita de terreno.
Cada plano deve contribuir apenas com a metade externa correspondente.
O caminho de RR2 descartava repetições somente nas margens e usava o scroll
nativo ali, mesmo quando o centro já usava uma translação visual. Isso podia
expor a borda oposta do bitmap dentro da pista ou produzir uma emenda ao
entrar na margem. A amostragem de RR1/RR2 agora usa as coordenadas transformadas
em todas as colunas e prolonga o terreno externo de cada metade. O teste de
terreno confirma oito texels da linha atual, preservando céu e montanhas.

As faixas amarelas são compostas por vários sprites nativos. A seleção pela
proximidade do centro podia aceitar alguns pedaços e rejeitar outros quando
as bordas chegavam ao limite. O desenho sintetizado aparecia junto dos
pedaços restantes. A revisão identifica o conjunto monocromático completo,
protegendo os slots correspondidos aos objetos do escalador. Em cada linha,
todos os pedaços identificados são substituídos juntos; se a projeção daquela
linha não for válida, todos continuam no caminho nativo e a pintura sintetizada
não é aplicada. O centro da faixa usa o mesmo par de bordas da amostragem.

RR3 mantém sua seleção e amostragem anteriores em um perfil separado.
As 48 capturas do replay touring, com seus dumps de RAM e VRAM, ficaram
idênticas à 0.4.2: 144 arquivos comparados.

## Verificação reproduzível

As evidências estão em `build/validation/road043/`; os executáveis 0.4.2
foram preservados em `before/bin/`. `corner` e `corner-right` usam direções
opostas e capturam 240 frames consecutivos cada. O replay touring atravessa
curvas e mudanças de nível; combate e tráfego verificam a composição dos atores.

```powershell
$env:RR_VISUAL_VALIDATION='build/validation/road043'
.tools/python/python.exe tools/probe_visual_regressions.py rr2 --scenario corner
.tools/python/python.exe tools/check_visual_regressions.py rr2 --scenario corner
.tools/python/python.exe tools/probe_visual_regressions.py rr2 --scenario corner-right
.tools/python/python.exe tools/check_visual_regressions.py rr2 --scenario corner-right
.tools/python/python.exe tools/check_road_regressions.py
.tools/python/python.exe tools/make_road_preview.py
```

As referências nativas precisam existir. Para recriá-las, execute o probe com
`--label native --native`. A comparação de RR1 `corner` usa também
`--native-root build/validation/approach042`; combate e touring usam
`--native-root build/validation/regressions04`.

O teste de VDP reproduz a borda oposta vazando no limite entre a imagem nativa
e a margem direita após uma translação de quatro pixels. O teste de cena
reproduz dois pedaços de marcação afastados, confirma sua substituição conjunta
e verifica o retorno ao desenho nativo em uma linha sem projeção válida.
Os limites opostos de continuação também são testados. Na captura real de RR2
`corner/080.png`, a faixa antes bifurcava nas três últimas linhas; agora há
um único traço em cada linha.

As verificações cobrem os replays registrados nas pistas iniciais e os casos
sintéticos de regressão. Não homologam todas as campanhas. Os contadores de
raster registram oclusões por outros atores separadamente da corrupção pelo
fundo.

## Resultados

Passaram os oito grupos CTest nas duas pastas de build e os 17 testes Python.
O relatório agregado em `build/validation/road043/result.json` reúne dez
replays auditados no raster, seis pares de vídeo de RR1/RR2 em 4:3 e 16:9,
três comparações de PCM e os três lançamentos reais com CRT. A bifurcação
reproduzida em RR2 passou de duas ramificações para um traço único, com pintura
presente nas 16 linhas verificadas.

Nos replays auditados não houve corrupção do piloto pelo fundo, perda dos
pixels nativos de atores verificados ou candidatos a faixa azul no asfalto.
RAM, VRAM, HUD, título, pausa e estado após retomada permaneceram iguais nos
pares correspondentes. Os WAVs completos ficaram idênticos entre geometria
desligada e ligada. As preferências do launcher permaneceram intactas.

Os binários de `build/release02` e `build/native` foram recompilados para
0.4.3. A comparação animada de RR1/RR2 fica em
`build/validation/road043/comparacao.html`, gerada por
`tools/make_road_preview.py` com os mesmos comandos nos dois lados.
