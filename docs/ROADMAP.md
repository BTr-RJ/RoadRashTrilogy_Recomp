# Roadmap — revisão de 6 de novembro de 2026

Versão local: 0.4.3, iniciada por `Jogar.cmd`. Zoom e cenário contínuos
estão ligados à engine dos três jogos e habilitados por padrão no launcher.
O cenário usa artes completas e terreno por profundidade; pilotos e tráfego
mantêm a composição da animação atual. O buffer de cinco frames foi removido.
Comparações de RAM/VRAM, HUD, pausa e retomada passaram; os três jogos também
passaram pelo launcher com CRT ativo. O tráfego agora mantém identidade e
composição entre trocas de descritores, com aproximação ampliada perto da câmera.
As duas bordas de RR1/RR2 recebem continuação simétrica e amostragem finita
com as mesmas coordenadas do centro. A substituição das marcações centrais é
conjunta por linha, corrigindo a duplicação. RR3 conserva o caminho anterior.
Detalhes em [ROAD-SEAMS.md](ROAD-SEAMS.md).
Reconstrução da pista em alta resolução e multiplayer em rede permanecem futuros.

| Etapa | Entrega e critério de aceite | Estado verificável |
|---|---|---|
| 1. Base nativa | Três ROMs identificadas por SHA256; C gerado e executáveis próprios; menus e corridas | Implementado, com fallback de interpretação ainda necessário |
| 2. Correções de vídeo | Arte em 4:3 sem preenchimento esticado; SAT de 9 bits; HBlank pendente retido e raster físico | Correções em validação nas três ROMs; falha principal de céu/pista do RR3 reproduzida e corrigida |
| 3. Conquistas no jogo | Painel, notificações e progresso local persistente, sem conta | Seis conquistas locais por jogo; painel F6 e persistência implementados. Não sincroniza desbloqueios oficiais do RetroAchievements |
| 4. Remakes | Faixas completas de Dean Harris; original/remake, pausa, loops e efeitos preservados | 18 faixas completas; pause, duas retomadas e motores verificados em PCM nos três jogos. Volumes independentes e opções originais restauradas por frame; evidências em docs/AUDIO-FIX.md. Demais eventos e pistas em validação |
| 5. CRT Slang | Presets originais koko-aio; aspecto H32/H40 compartilhado; falha mantém a imagem normal | 0.2.2: abertura ampla sem moldura/curvatura em 16:9; parâmetros originais restaurados em 4:3. Oito filtros capturados em RR1 a 1280×720; caminho launcher → RR1 + koko verificado. Tela cheia 4K ainda precisa de validação visual |
| 6. Widescreen real | Abrir laterais mantendo escala e centro; pista sem wrap espúrio; sprites/HUD corretos | Centro, RAM e VRAM idênticos nas 63 comparações da 0.2.2; pista duplicada de RR2 corrigida. A 0.4.0 acrescenta projeção das artes completas e retenção limitada dos objetos confirmados nas bordas. Instanciar objetos nunca apresentados pela ROM e homologar demais pistas continuam pendentes; opção experimental |
| 7. Zoom e movimento a 60 fps | Escalador de sprites por objeto e projeção da pista; cronômetro e áudio preservados | 0.4.3: cenário/terreno por profundidade, atores identificados pelo objeto físico e composição completa confirmada, zoom extra próximo e marcações centrais projetadas. Atualizações incompletas usam retenção curta; remoção explícita e pausa são respeitadas. Sem buffer visual de cinco frames. Auditoria compara pixels e donos no raster, além de RAM/VRAM e HUD; sequências incluem golpes, tráfego, colinas e curvas. Corrida individual; raster e poses continuam nativos. Não garante 60 imagens distintas em toda situação; reconstrução HD e cobertura de todas as pistas pendentes |
| 8. Pacote 0.2 | Launcher, executáveis, dependências e licenças; build reproduzível; relatório de testes e limites | Pendente do fechamento das etapas acima |
| 9. LAN e online | Snapshots e replay determinísticos; depois rollback, quatro pilotos e convites | Adiado pelo usuário; nenhuma sessão é anunciada como disponível |

Para RetroAchievements oficial, login é exigência do serviço para carregar a sessão
e registrar desbloqueios na conta. A opção local não finge sincronização. O cliente
rcheevos já está integrado como recurso opcional, sujeito à validação com conta real.

Validação de corrida deve cobrir largada, aceleração, curvas, colisões, queda,
polícia, chegada, menus e mudanças de resolução. Capturas e logs ficam em
build/validation. Boot ou benchmark curto não homologam uma corrida completa.
