# Pesquisa e primeira passagem — 5 de novembro de 2026

## Evidencias locais

Os tres ZIPs fornecidos foram lidos sem extrair caminhos do arquivo e sem alterar
os originais. A identidade abaixo combina os bytes locais com referencias de
engenharia reversa cujos SHA1 coincidem. Nao equivale a validar o jogo no runtime.

| Entrada | Tamanho | CRC32 | Serial do cabecalho |
|---|---:|---|---|
| Road Rash USA/Europe | 768 KiB | DEA53D19 | GM T-50116 -00 |
| Road Rash II RR206 USA/Europe | 1 MiB | 0876E992 | GM T-50496 -02 |
| Road Rash 3 USA/Europe | 2 MiB | 15785956 | GM T-50966 -00 |

Os SHA256 completos estao em `games/*/evidence.json` e nos manifestos privados de
importacao. A [lista de cartuchos do MAME](https://github.com/mamedev/mame/blob/master/hash/megadriv.xml)
tambem confirma os SHA1 de RR206 e RR3. O checksum simples do cabecalho diverge
em RR1 e RR2, apesar de os arquivos coincidirem com as entradas de pesquisa;
essa divergencia isolada nao demonstra corrupcao. Nao corrigimos os dumps.

Primeira emissao, usando perfis minimos sem patches:

| Jogo | Resultado | Pendencias detectadas |
|---|---|---|
| RR1 | C emitido, verificacao estrita falhou | EA_FALLBACK em 0x030002; um despacho indireto sem cobertura estatica |
| RR2 | C emitido, verificacao estrita falhou | Entradas em 0x000AA8 e 0x000AAE sobrepoem instrucoes; investigar descoberta |
| RR3 | Emissao estrita passou, 563 funcoes | Nove locais de despacho indireto sem cobertura estatica |

Passar a emissao nao significa iniciar, concluir corrida nem cobrir todos os
destinos. Logs completos permanecem em `build/generated/<jogo>/...`. O comando
`status` apresenta os resultados de geracao correspondentes ao hash importado.
Em RR1, a entrada especulativa 0x030000 produz uma operacao com endereco invalido
em 0x030002. Pode ser dado promovido a codigo, mas isso e uma hipotese: nao foi
adicionada blacklist para simplesmente esconder o diagnostico.

## Referencias que reduzem o trabalho de descoberta

[Road Rash physics, commit ce5af7c](https://github.com/plarionov/road-rash-physics/blob/ce5af7c192539f2573d9251837de097d943a372b/TECHNICAL.md)
descreve o loop de corrida, o passo de pilotos, as duracoes medidas em VBlank e
o estado da estrada para exatamente o SHA1 local. Conferimos nos bytes o JSR
de 0x7E884 para 0xC00C e o JSR de 0xC0AE para 0x10E66, alem dos vetores.
Os enderecos RAM sao candidatos da referencia, ainda sem medicao local em jogo.

[Road Rash II physics, commit 1500da0](https://github.com/plarionov/road-rash-2-physics/blob/1500da065184d745803d9dbe82fd81c55730ecb5/TECHNICAL.md)
identifica a revisao RR206, slots dos dois jogadores e variaveis de camera e
tempo. A fonte relaciona deslocamento lateral a produtos que dependem de dt.
Conferimos os vetores do arquivo local; comportamento e split-screen continuam
sem validacao neste projeto.

[Road Rash 3 physics, commit ab54bcf](https://github.com/plarionov/road-rash-3-physics/blob/ab54bcf59c8067c77347e3321ab6971efa6799a0/TECHNICAL.md)
descreve o contador entre passos, a espera por buffer de imagem e os slots dos
jogadores para o SHA1 local. Ha tambem formulas de deslizamento e curvas que
explicam por que reduzir dt pode alterar o controle. Conferimos os vetores.

Esses projetos sao fontes de hipoteses e pontos de observacao. Nenhum patch de
fisica foi aplicado e nenhum codigo desses repositorios foi incorporado. Medidas
de fps publicadas por seus autores ainda precisam ser reproduzidas aqui.

Outro candidato de pesquisa e [rr3-reverse](https://github.com/deviceargent/rr3-reverse),
que anuncia laboratorio BizHawk/Lua e uma fatia em Godot. Deve ser auditado antes
de reutilizar mapas ou ferramentas; uma recriacao em Godot nao demonstra uma
recompilacao estatica fiel.

## Plano para sprites e pista mais fluidos

O [artigo de escalonamento do Plutiedev](https://plutiedev.com/scaling-sprites)
explica a falta de escalonamento de sprites no hardware e as transicoes entre
tamanhos usadas em jogos. Isso descreve uma limitacao da plataforma; ainda
precisamos identificar o formato de escalonamento de cada Road Rash.

Proposta deste projeto: manter estados de simulacao confirmados e produzir uma
cena de apresentacao separada. Cada piloto, veiculo e objeto de cenario precisa
de identidade estavel, posicao de mundo, dimensoes, orientacao, quadro de
animacao e ponto de apoio. Capturar tambem camera, estrada e paleta no mesmo
limite logico, evitando misturar partes de quadros diferentes.

Interpolar coordenadas de mundo e reprojetar a cada imagem. A escala continua
vem da profundidade interpolada: `escala = focal / profundidade`. Interpolar
apenas a largura final mistura duas projecoes diferentes e pode produzir uma
velocidade aparente incorreta. Manter o apoio do sprite na pista durante o zoom;
escolher filtragem nearest ou linear independentemente da continuidade da escala.

`src/presentation/interpolation.hpp` implementa o primeiro nucleo independente:
interpolacao, escala por profundidade, ancora nos pes, plano proximo e descarte
de historico em reutilizacao de slot, teleporte ou rollback. Foi compilado com
MSVC e passou 13 verificacoes. Ainda nao le os estados dos jogos nem desenha
seus sprites. Nao adiciona quadros de animacao que nao existam nos assets.

Para a pista, recuperar segmentos e sua fracao longitudinal, curvas, alturas,
largura e marcacoes. Avaliar esses dados na camera interpolada e gerar faixas
ou uma malha em resolucao de saida. Preservar a geometria original como primeira
referencia. Usar IDs de segmentos para acompanhar o anel, sem interpolar o
indice bruto quando ele retorna ao inicio. Resolver oclusao em cristas, recorte
de objetos, horizonte, sombras e alinhamento de pilotos com a superficie.

Subpixel e maior precisao nos calculos de apresentacao podem reduzir saltos de
posicao e tamanho. A quantizacao so deve ocorrer ao rasterizar no modo original.
Nao aplicar filtro temporal a colisao, IA ou controle. Uma opcao de camera
suavizada deve ter limites de atraso e reset em queda, corte e retorno a menus.

A interpolacao retrospectiva introduz atraso de ate um passo de simulacao.
Antes de adota-la como padrao, medir esse custo nas taxas reais de atualizacao;
comparar com elevar a simulacao para uma taxa fixa corrigindo suas dependencias
de tempo. Extrapolacao visual pode reduzir atraso, mas precisa de limites e
correcao, sobretudo perto de colisao e salto. Nao prometer ausencia de artefatos.

## Musica e rede

O [album RR3 publicado pelo proprio Dean Harris](https://www.patreon.com/DeanHarrisComposer/posts/road-rash-3-full-53364429)
confirma remixes em MP3 e menciona seu album RR2. Fontes do canal YouTube e o
catalogo RR1 continuam pendentes de confirmacao direta. Configuramos as metas
Original / Dean Harris / Custom e o formato de pacote local em `music/`.
O adaptador devera detectar comandos de BGM, substituir somente a musica e
preservar efeitos, com fallback por faixa. Ainda nao ha reproducao integrada.

Para rede, a pesquisa de RR2/RR3 oferece candidatos para observar dois jogadores,
mas nenhum teste de determinismo foi feito. O primeiro modo seria sincronizar
entradas dos modos existentes. Criar multiplayer simultaneo em RR1 ou cameras
individuais constitui trabalho adicional de regras, estado, camera e HUD.

## Experimentos seguintes

1. Integrar o runtime fixado e produzir um boot instrumentado em modo original.
2. Resolver os diagnosticos de descoberta de RR1/RR2 sem mascarar codigo real.
3. Medir VBlank, passo de pilotos, frame pronto e despacho por cenario/revisao.
4. Capturar uma corrida repetivel e validar cameras, controles, audio e colisao.
5. Recuperar posicoes e escala de um unico piloto; comparar zoom original e
   reprojecao continua com a mesma sequencia de simulacao.
6. Reprojetar uma reta, depois curva, subida/crista e queda, verificando apoio e
   oclusao antes de ampliar para widescreen.
