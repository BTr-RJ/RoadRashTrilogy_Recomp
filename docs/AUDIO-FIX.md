# Áudio 0.2.2 — pause e efeitos com remakes

Os remakes mantêm a posição durante o pause e voltam a tocar na retomada.
Os sons de motor também permanecem ativos com a trilha externa. O launcher
oferece volumes separados, com música em 20% e efeitos em 100% por padrão.

## Causa e correção

As ROMs usam opções que escolhem música ou atualizações de motor. Apenas
interceptar a reprodução da música deixa o jogo no caminho que ignora o motor.
Durante a simulação da corrida com remake ativo, o adaptador seleciona o caminho
de efeitos e restaura a opção original antes de encerrar cada frame.

| Jogo | Opção em RAM | Caminho de efeitos | Referência no código 68000 |
|---|---|---|---|
| RR1 | `$FF05D6` | `0` | teste em `$1524`, desvio em `$152A` |
| RR2 | `$FFF594` | `0` | teste em `$15E6`, desvio em `$15EC` |
| RR3 | `$FF752C` | `1` | teste em `$21F8`, desvio em `$21FE` |

No RR3, o carregador precisa alocar as vozes de motor antes da primeira imagem
de corrida. O hook em `$10B14` permite executar essa inicialização original.
`prepare_runtime_inputs.py` verifica os bytes `4A7900FF752C` na ROM vinculada
ao perfil antes de registrar esse ponto. Atualizar somente a frequência depois
do carregamento não cria as vozes que foram omitidas.

A seleção de trilha original não aplica essa substituição de opção. O sinal de
pause do RR1 fica em `$FF3696`; `$FF368E` é a trava de leitura do botão Start.
Parar a música por pause conserva a faixa e o cursor do player externo.

## Verificação reproduzível

Execute os jogos individualmente com o Python fixado no projeto:

```powershell
.tools/python/python.exe tools/verify_audio_mix.py rr1
.tools/python/python.exe tools/verify_audio_mix.py rr2
.tools/python/python.exe tools/verify_audio_mix.py rr3
```

Cada comando cria uma pasta nova em `build/validation/audio-pcm/<jogo>/` e
executa duas trajetórias iguais, com o remake em 20% e em 0%. O WAV registra
PCM estéreo antes do volume geral e da entrega ao dispositivo. As capturas são
ocultas, com dispositivo de áudio de teste; verificam o áudio gerado pelo jogo.

O teste exige cabeçalho WAV finalizado, formatos e durações iguais, contribuição
da música durante a corrida e nas duas retomadas, ausência dessa contribuição
nos dois pauses e efeitos com variação real nas três janelas de corrida.
Remove a componente contínua de cada canal: um valor constante não demonstra
som audível. Também verifica a restauração da opção original em cinco pontos
de cada execução e rejeita saturação acima de 0,1% nas janelas amostradas.
O relatório registra o SHA256 do executável e rejeita sua troca entre capturas.

Resultados de 05/11/2026, horário de Brasília, com a compilação 0.2.2:

| Jogo | RMS da música em corrida | RMS dos efeitos em corrida | RMS da música nos pauses | Saturação amostrada |
|---|---:|---:|---:|---:|
| RR1 | 732,220 | 1729,420 | 0 | 0% |
| RR2 | 1583,933 | 1448,602 | 0 | 0% |
| RR3 | 535,942 | 957,645 | 0 | 0% |

As três verificações passaram, incluindo efeitos após as duas retomadas.
`tools/probe_audio_pause.py` também passou nos três jogos: RAM de pause
`[0, 1, 0]` e duas retomadas com o mesmo cursor registrado antes de cada pausa.
Relatórios dos executáveis finais conferidos por SHA256:
`rr1/20261106T023947751925Z/result.json`,
`rr2/20261106T023947752570Z/result.json` e
`rr3/20261106T023947752208Z/result.json`, sob a pasta de evidências acima.
Capturas anteriores que falharam permanecem disponíveis com `passed: false`.

Uma execução RR1 com a trilha original produziu PCM idêntico à captura anterior
à mudança: SHA256 `82799ab57e72c78a46f28d082ca7b5e839d83a0d766b1f6b33e873a6ed4bfa85`.
O comparativo está em `rr1/original-20261106T023702Z/comparison.json`, sob a
pasta de evidências. Os cinco testes CTest e os 17 testes Python
também passaram. A trajetória cobre largada, aceleração e duas pausas;
não homologa todas as pistas, eventos ou uma campanha completa.

O executável atualizado é o iniciado por `Jogar.cmd`, em `build/release02/bin`.
O zoom contínuo foi integrado na versão 0.3.0; veja [SPRITE-ZOOM.md](SPRITE-ZOOM.md).
