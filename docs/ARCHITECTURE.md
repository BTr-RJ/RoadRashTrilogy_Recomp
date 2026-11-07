# Arquitetura inicial

## Fluxo e fronteiras

```text
ROM local original -> validacao/hash -> copia privada + perfil da revisao
                                       |
                        evidencias de codigo e saltos indiretos
                                       |
                        GenesisRecomp fixado -> C gerado
                                       |
                       compilador C + runtime + adaptador do jogo
                                       |
                           executavel nativo Windows x64
```

O fluxo até o executável está implementado nos três jogos, com adaptadores por
ROM, launcher, áudio externo e apresentação CRT. Menus e trajetórias de corrida
foram verificados; campanhas completas ainda não. Consulte o ROADMAP e os
relatórios AUDIO-FIX/VIDEO-FIX para o estado atual das melhorias.

O objetivo e executar a logica 68000 traduzida antes da execucao. Os dispositivos
VDP, audio, entrada e Z80 ainda precisam de implementacoes compativeis. O engine
candidato inclui fallback de interpretacao para destinos nao resolvidos em seu
runner de cartucho. Precisamos medir e eliminar esses destinos nas rotas
qualificadas antes de declarar uma recompilacao estatica completa. O Z80 e uma
decisao separada: pode permanecer emulado inicialmente, devidamente identificado.

## Perfis por revisao

Nunca transportar enderecos entre Road Rash 1, 2 e 3 ou entre revisoes sem
evidencia. O hash da entrada acompanha cada resultado. A lista de hashes
qualificados comeca vazia. Descobertas compartilhadas nao vazias exigem uma
entrada qualificada; evidencias futuras de revisoes distintas devem ganhar
perfis separados. Enderecos especificos e adaptadores pertencem a este projeto,
sem alterar o engine compartilhado com suposicoes dos jogos.

## Melhoria visual e tempo

Primeiro preservar a simulacao e a apresentacao originais como referencia.
A frequencia de apresentacao do console nao determina quantas vezes a logica
de corrida efetivamente atualiza. Medir essas duas frequencias por jogo.

Na versão 0.3.0, o VDP interpola posição/escala dos atores correspondidos e
scroll da pista entre estados confirmados. A simulação mantém seu ritmo nativo;
o histórico visual fica fora do estado convidado. [SPRITE-ZOOM.md](SPRITE-ZOOM.md)
descreve os passes, os resets, o atraso visual e os resultados medidos.

Alta resolucao tem dois niveis: ampliar a imagem original e reconstruir a
renderizacao da estrada/atores em uma superficie maior. Escalar pixels e util,
mas nao acrescenta detalhe aos assets. O segundo nivel depende de recuperar
o estado e a projecao do renderizador de cada jogo.

Widescreen precisa ampliar o campo visivel e revisar projecao, recorte, sprites,
espelhos e HUD. Menus podem preservar 4:3 centralizado. Proporcao final 16:9 como
primeiro alvo; ultrawide somente depois de validar cenarios e limites.

## Shaders RetroArch

A meta e aceitar presets e passes do ecossistema Libretro. Avaliar um host
compativel como [librashader](https://github.com/SnowflakePowered/librashader),
incluindo licenca, API grafica e requisitos de compilacao, antes de incorpora-lo.
Compatibilidade inicial pretendida: presets `.slangp` e passes `.slang`, com
parametros, escalas, texturas e historico necessarios. Nao presumir suporte a
todo preset RetroArch. Consulte a [especificacao Slang](https://docs.libretro.com/development/shader/slang-shaders/).
O runtime integra librashader/D3D11 e três presets koko-aio, além dos filtros
leves do runner. Falhas de shader conservam a apresentação normal.

## LAN e online

Comecar identificando o multiplayer que cada titulo oferece. Sincronizar as
entradas do modo existente e um problema distinto de criar novos pilotos,
camera individual ou corrida multiplayer em um jogo originalmente individual.

Primeiro implementar replay deterministico e snapshots completos: RAM 68000,
Z80, VDP, audio, bancos, perifericos, RNG, agendador e estado dos adaptadores.
Comparar hashes por tick e provar restaure/reexecute. Depois fazer dois processos
locais, LAN direta e, por ultimo, internet com negociacao de conexao.

Protocolo deve negociar jogo, hash ROM, versao, mods e regras de simulacao.
Rollback e candidato, condicionado a snapshots e ressimulacao corretos. Rede
nao deve sincronizar estado de shader nem permitir que interpolacao de imagem
altere a logica. Nenhum modo de rede esta implementado nesta fase.
