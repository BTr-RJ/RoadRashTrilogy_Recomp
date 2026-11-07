# Correção de vídeo 0.2.1 — 05/11/2026

Complemento 0.2.2: em widescreen, os presets Slang também desativam moldura e
curvatura para exibir a abertura inteira. As oito opções de filtro do launcher
terminaram com captura no RR1 em 1280×720; relatório em
`build/validation/bugs03/rr1/filters-wide-1.json`. As capturas dos três Slang
foram examinadas e mostram as laterais. A [correção de áudio](AUDIO-FIX.md)
registra os testes de pause e dos motores com remakes nesta compilação.

O teste `tools/probe_launcher.py` confirmou o executável 0.2.2, fonte de
426×224, shader ativo e preferências do usuário preservadas. A captura final
está em `build/validation/bugs03/launcher/present.png`; o relatório aponta o
log da sessão. Esse teste usa janela oculta de 1280×720, não tela cheia 4K.
Nos testes automatizados o problema de enquadramento relatado inicialmente
não se reproduziu em todas as opções; a mudança concreta elimina a moldura
e a curvatura dos presets quando a cena é ampla. Em 4:3 elas são restauradas.

O efeito de fluxo de imagem foi retirado do caminho de apresentação.
`RR_SMOOTH=1` continua sem efeito. Na versão 0.3.0, a opção de movimento do
launcher controla `RR_GEOMETRY`: o substituto opera sobre objetos e scroll no
VDP. Consulte [a implementação atual e seus limites](SPRITE-ZOOM.md).

## Widescreen e CRT

A apresentação normal e o koko-aio calculam o mesmo aspecto a partir da largura
nativa do VDP. H32 (256 pixels) e H40 (320 pixels) mantêm o centro em 4:3; colunas
adicionais ampliam a cena mantendo a escala desse centro. A janela não determina
uma escala horizontal independente. Menus e painel continuam centrados.

A avaliação de colisão/overflow de sprites vem de um passe na largura original.
Com geometria desligada, outro passe desenha as margens e conserva o centro
original. Com geometria ligada, o passe visual também interpola os objetos e
o scroll centrais. Seus resultados não alteram o estado de sprites usado pelo
jogo. Replays sem saída também avaliam a largura original.

Em RR2/3, simplesmente repetir o mapa do VDP revela outra faixa de asfalto numa
curva forte. O perfil de pista usa as coordenadas assinadas da faixa finita, em
vez de aceitar seu wrap de 1024 pixels. O trecho de terreno continua com tiles
originais e sua fase de oito pixels. A correção atua somente nas margens e no
intervalo de scroll do desenho de pista; céu, HUD e centro não são reamostrados.
Endereços verificados no C gerado e nos bytes das ROMs locais: RR2 `$14822` e
RR3 `$1A028` (geradores que usam deslocamento de 672 pixels).

O koko-aio recebe o aspecto corrigido a cada frame. Renderiza num alvo separado;
só um frame de shader bem-sucedido substitui a apresentação normal. Uma falha
não apaga o jogo com uma tela preta.

O Ambilight imersivo sobrepunha uma fotografia com abertura fixa em 4:3, cobrindo
as margens de uma imagem larga. Em widescreen, o runtime desativa essa fotografia,
seu zoom global e a mistura que depende dela, mantendo a moldura/iluminação
geradas pelo shader. Em 4:3, restaura os valores originais do preset.

## Referências consultadas

- [Sonic 1: renderizador de cena e captura de sprites](https://github.com/mstan/SonicTheHedgehogRecomp/blob/master/docs/EXPERIMENTAL_WIDESCREEN.md).
- [Rocket Knight: layouts, parallax e dados de objetos](https://github.com/mstan/RocketKnightAdventuresRecomp/blob/master/game/CUSTOM-VIDEO.md).
- [Guia do mantenedor do framework](https://github.com/RetroPortingToolKit/RetroPortingToolkit.com/blob/main/data/docs/04_guides/05_add-widescreen/index.md).
- [Aspecto no próprio koko-aio](../assets/shaders/koko-aio/shaders-ng/includes/functions.include.slang).
- [Implementação dos dispositivos de sprites Sega, projeto MAME](https://github.com/mamedev/mame/blob/master/src/mame/sega/sega16sp.cpp): referência conceitual de zoom de sprites, sem copiar seu código para o runtime.

Sonic/RKA reconstroem cena e visibilidade específicas de seus jogos. Não existe
um patch genérico que possa ser transplantado diretamente para Road Rash.

## Validação reproduzível

`tools/probe_video.py` executa jogos ocultos independentes, sem controlar a
instância do usuário. Percorre título, seleção, largada, aceleração, curvas,
saída da pista/queda conforme a trajetória original e pausa. Captura seis
estados de corrida por jogo. `tools/compare_video.py` verifica o centro exato
dos PNGs, a RAM completa e a VRAM completa, com as mesmas entradas.

As capturas/logs privados ficam em `build/validation/video-fix/`. O relatório
`comparison.json` registra 63 comparações, com zero diferenças no centro e no
estado amostrado. Isso não equivale a homologar todas as pistas ou uma campanha.
Os testes CTest cobrem também colisão restrita à margem, a pista repetida numa
curva sintética, H32/H40 e a impossibilidade de reativar o fluxo de imagem.

Os cinco testes CTest passaram na versão 0.2.2. Capturas GPU de 1280×720 foram examinadas nos
dois aspectos: RR1/Ambilight, RR2/Monitor-Balanced e RR3/Base. Essa matriz verifica
os três presets oferecidos, sem representar todas as combinações de jogo,
monitor, resolução e preset. As imagens `crt.png` e os logs de carregamento ficam
nos subdiretórios `crt-*-wide` e `crt-*-native` de cada jogo.
Os 17 testes Python da ferramenta de importação/geração também passaram.

Para executar esta correção local, use `Jogar.cmd` na raiz. Ele inicia o launcher
de `build/release02/bin`, que agora procura os três executáveis irmãos dessa
mesma compilação. `build/native/bin` também foi recompilado. As opções de janela
menores agora são 960×540 e 1280×720; os demais tamanhos são 1080p, 1440p e 4K.
A resolução da janela não altera a proporção interna do jogo.

## Zoom e fluidez na engine 0.3.0

Implementados e habilitados no launcher. Consulte [SPRITE-ZOOM.md](SPRITE-ZOOM.md)
para arquitetura, resultados reproduzidos e limites da cobertura.

## Cenário contínuo na engine 0.4.0

Artes originais completas, identidade dos objetos no mundo, saída prolongada
pelas bordas e terreno por profundidade substituem o caminho padrão anterior.
Consulte [ORGANIC-SCENE.md](ORGANIC-SCENE.md) para referências, testes e limites.

## Composição e resposta na engine 0.4.1

Corrige origem das artes sem âncora, composição de pilotos/tráfego e mistura
de scroll antigo com o bitmap atual. Remove o buffer de cinco frames e mantém
as peças atuais dos atores, incluindo golpes. A validação passa a inspecionar
cores e donos dos pixels no raster. Detalhes em [VISUAL-REGRESSIONS.md](VISUAL-REGRESSIONS.md).
