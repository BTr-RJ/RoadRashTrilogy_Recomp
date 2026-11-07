# Aproximação e estabilidade — 0.4.2

Registro da revisão 0.4.2. A correção adicional das emendas de pista e das
marcações duplicadas está em [ROAD-SEAMS.md](ROAD-SEAMS.md), versão 0.4.3.

Abra `Jogar.cmd`. As mudanças fazem parte da opção existente **Zoom contínuo e
movimento suave**, ligada por padrão, nos três jogos. A revisão trata as
piscadas dos adversários durante atualizações incompletas, o limite inferior
esquerdo do bitmap da pista de RR1, a cadência das marcações centrais e a
aproximação do tráfego.

## Tráfego e composição

Os descritores de desenho são compactados e reaproveitados. Usá-los como
identidade de uma moto permitia misturar a história de atores diferentes.
Hooks no início e no fim da montagem nativa agora identificam cada ator físico
e seus componentes. A composição usa somente conjuntos completos e confirmados
de peças da SAT, com suas cores, transparência, espelhamento e prioridade.
Uma transformação comum mantém tronco, braços e moto juntos.

Se uma transferência de textura ou SAT ficar incompleta, a última composição
completa pode cobrir esse intervalo por até 12 frames. Isso exige que o plano
nativo ainda indique um ator visível. Remoção explícita, pausa e saída da
corrida cancelam a retenção. Uma previsão inválida perto da câmera retorna à
pose confirmada, em vez de produzir um retângulo vazio. Coordenadas horizontais
da SAT são desembrulhadas em torno do ator, evitando peças na borda oposta.

O tráfego ganha uma ampliação progressiva de até **18% nas dimensões** perto
da câmera, além da perspectiva contínua. A posição dos pneus na pista é a
âncora; a física e as distâncias da simulação não mudam. O jogador conserva
sua pose atual e não recebe essa ampliação. A previsão dos atores identificados
é limitada a seis frames; atores sem essa identidade mantêm o limite de dois.
O buffer de cinco frames removido na 0.4.1 permanece removido.

## Pista e marcações

RR1 usa duas metades de um bitmap finito para formar a estrada. Ao mostrar
mais da pista, o scroll esquerdo chega ao limite nativo de 320 e deixa uma
borda vertical nas linhas inferiores. A apresentação prolonga a última
tangente válida do raster atual, sem alterar o scroll vertical. Trechos sem
esse limite ou com descontinuidades conservam os valores nativos. O centro
usado para reconhecer e projetar as marcações acompanha a borda corrigida.

A amostragem também podia ultrapassar a cunha válida e voltar a dados
de outra região. Cada metade agora prolonga somente seu terreno externo e
descarta a região interna que pertence à outra metade. A proteção exige uma
linha confirmada de terreno, preservando céu e montanhas.

As faixas centrais são sprites nativos, portanto escapavam da projeção já
aplicada ao chão. A revisão reconhece os sprites monocromáticos de marcação
perto do centro da pista, calibra seu espaçamento e largura e os desenha como
material do chão usando profundidade e fase da câmera atuais. A pintura fica
sob os veículos. Retas sem marcação, linhas sólidas ou amostras insuficientes
mantêm o desenho nativo. Após pausa, apenas o espaçamento previamente
confirmado é conservado como referência de calibração; fase e geometria são
obtidas novamente do estado atual.

## Verificação

As capturas ficam em `build/validation/approach042/`. A comparação animada local
é gerada por `tools/make_approach_preview.py` em `comparacao.html`, mostrando
0.4.1 e 0.4.2 com os mesmos comandos. Os binários anteriores estão preservados
em `before/bin/`; as capturas nativas de combate e touring vêm da validação
0.4.1, com RAM/VRAM iguais aos replays atuais.

```powershell
$env:RR_VISUAL_VALIDATION='build/validation/approach042'
.tools/python/python.exe tools/probe_visual_regressions.py rr1 --scenario combat
.tools/python/python.exe tools/check_visual_regressions.py rr1 --scenario combat --native-root build/validation/regressions04
.tools/python/python.exe tools/probe_visual_regressions.py rr1 --scenario touring
.tools/python/python.exe tools/check_visual_regressions.py rr1 --scenario touring --native-root build/validation/regressions04
.tools/python/python.exe tools/probe_visual_regressions.py rr1 --scenario corner
.tools/python/python.exe tools/check_visual_regressions.py rr1 --scenario corner
```

Repita combate e touring para RR2/RR3. Para recriar a referência da curva,
execute o probe com `--scenario corner --label native --native`. O cenário
captura 240 frames consecutivos com mudanças de direção. Combate inclui golpes,
chute e direção; touring usa 48 checkpoints separados por 150 frames.

Os testes da engine incluem peças de um ator em trabalhos separados,
transferência de textura interrompida, perda parcial da SAT, remoção explícita,
prioridade entre atores, previsão inválida, extensão de terreno e céu preservado.
A curva verifica também que a antiga faixa vertical de grama no canto inferior
esquerdo foi substituída pelo asfalto que continua além da tela.
Os testes de marcação verificam fase, largura, repetição, coordenadas negativas,
retorno ao desenho nativo e rejeição de um espaçamento falso após a pausa.

As auditorias verificam cor e dono do pixel no instante do raster. Atores
maiores podem encobrir mais pixels de outro veículo; essas oclusões ficam
separadas da corrupção causada pelo fundo. Esses critérios não detectam todo
defeito visual possível. A cobertura inclui trajetórias reproduzidas nas pistas
iniciais, sem homologar todas as campanhas. Tela dividida continua nativa.

## Resultados

Passaram os oito grupos CTest em `build/release02` e `build/native`, os 17
testes Python, oito replays auditados no raster, nove comparações de vídeo,
três comparações de PCM e os três lançamentos reais com CRT. Os executáveis
locais das duas pastas foram recompilados para 0.4.2. O relatório agregado e
os hashes dos executáveis usados ficam em `build/validation/approach042/result.json`.

As auditorias cobriram **29.072.556 pixels do jogador**, sem corrupção pelo
fundo, sem perda dos pixels nativos de atores verificados e sem candidatos a
faixa azul no asfalto. Oclusões por outro ator projetado são registradas
separadamente. A curva corrigiu os 320 pixels da faixa de grama usada como
regressão no canto inferior esquerdo. A captura adicional de tráfego contém
480 frames consecutivos, incluindo a aproximação e passagem de um carro.

Na sequência de combate de RR1, a máscara da pintura amarela teve 57 estados
distintos na 0.4.1 e 120 na 0.4.2, em 120 capturas sincronizadas. No replay de
aceleração, o conjunto da cena teve 120 imagens distintas em 120 frames nos
três jogos, em 4:3 e 16:9. Isso mede estas sequências, sem garantir uma imagem
nova em todo frame de qualquer situação.

RAM, VRAM, HUD, título, pausa e estado após retomada permaneceram iguais nas
comparações correspondentes. Os WAVs completos de geometria desligada e
ligada ficaram idênticos nos três jogos. A calibração das faixas conservou o
período confirmado após pausa e retomada. As preferências do launcher também
permaneceram intactas.

O probe de 3.600 frames com apresentação oculta, CRT Monitor Balanced a
1280×720 e VSync desligado mediu 357 / 313 / 306 fps em RR1 / RR2 / RR3 nesta
máquina. Esse teste mede folga de processamento sem limite de velocidade;
não mede latência de controle, cadência de um monitor ou saída física de áudio.
