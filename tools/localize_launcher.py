"""One-time migration of the legacy native launcher copy to release English."""
from pathlib import Path
import re
path=Path(__file__).resolve().parents[1]/'src/launcher/main.cpp'
text=path.read_text(encoding='utf-8')
copy={
'A primeira largada. Cinco estradas da Califórnia,\\nmotos barulhentas e rivais que não correm limpo.':'The original California ride. Five roads, fast bikes,\\nand rivals who never play fair.',
'A rivalidade ganhou outra marcha. Cruze os Estados Unidos\\ne enfrente a estrada com o modo de dois jogadores original.':'Take the rivalry across America. Race solo or challenge\\na friend in the original two-player split screen.',
'Tour de Force. Pilotos, motos e estradas pelo mundo\\nna última viagem da série pelo Mega Drive.':'Tour de Force. Riders, bikes and roads around the world\\nin the trilogy\'s final Mega Drive ride.',
'O executável ou a ROM deste jogo não foi encontrado. Use Importar ROM ou execute a compilação do projeto.':'This game is not installed yet. Choose Install games and supply your own compatible ROM folder.',
'Preparar o jogo':'Install game',
'Não foi possível iniciar o jogo. Confira os arquivos e as permissões da pasta.':'Could not start the game. Check the installation files and folder permissions.',
'Falha ao iniciar':'Launch failed',
'ROM binária':'Cartridge image','Todos os arquivos':'All files',
'Selecione sua ROM original':'Select your original ROM',
'Use um dump binário sem cabeçalho de copiador.':'Use a linear binary dump without a copier header.',
'ROM incompatível':'Unsupported ROM',
'Esta revisão não corresponde ao código recompilado. Use a versão USA/Europe; para Road Rash II, RR206.':'This revision is unsupported. Use the USA/Europe releases listed in the installation guide; Road Rash II must be RR206.',
'Não foi possível salvar a ROM. Confira o espaço e as permissões da pasta.':'Could not save the private ROM copy. Check free space and folder permissions.',
'Falha na importação':'Import failed',
'Volume geral':'Master volume', 'Músicas remake':'Remake music','Efeitos com remakes':'Remake effects',
'Resolução de saída':'Output resolution','Filtro de apresentação':'Display filter',
'Pixels originais':'Original pixels','Equilibrado':'Balanced','CRT clássico':'Classic CRT','CRT leve':'Light CRT','Composto':'Composite',
'Tela cheia':'Fullscreen',
'Expandir as laterais em 16:9 (experimental)':'Expand the sides to 16:9 (experimental)',
'Zoom contínuo e movimento suave':'Continuous zoom and smooth movement',
'A arte preserva a proporção em todos os filtros.\\nEm 16:9, CRT usa abertura inteira, sem moldura.\\nZoom e movimento por objeto na corrida individual.\\nMenus, pausa e modo dividido usam o desenho original.':'Pixel-art proportions are preserved with every filter.\\nWidescreen CRT uses the complete viewport.\\nSmooth zoom applies to single-player races.\\nMenus, pause and split screen keep their original look.',
'Efeitos com a trilha remake':'Effects with remake soundtrack','Trilha sonora':'Audio & music',
'Original do Mega Drive':'Original Mega Drive soundtrack',
'Remakes · Dean Harris Composer':'Dean Harris remakes (your own files)',
'Remakes começam em 20% para destacar os efeitos.\\nO pause conserva a posição da música.\\nCom a trilha original, vale a mixagem do Mega Drive.\\nCréditos e fontes: music/README.md.':'Remake music starts at 20% to keep effects clear.\\nPausing preserves the music position.\\nMissing remake tracks fall back to the original audio.\\nOptional music files are supplied separately.',
'Conquistas dentro do jogo':'Local in-game achievements',
'Pressione F6 durante a partida para abrir seu painel.\\nOs desbloqueios aparecem na tela e ficam salvos no PC.\\nVocê não precisa criar uma conta ou entrar em um serviço.':'Press F6 in game to open your achievement panel.\\nUnlocks appear on screen and are saved on this PC.\\nNo account or service sign-in is needed.',
'Cada jogo tem seu próprio progresso local:\\n\\nPrimeira largada · Motor aquecido · Ritmo de corrida\\nUm minuto na estrada · Cinco minutos na estrada\\nDe volta ao asfalto':'Each game keeps its own local progress:\\n\\nFirst Start / Engine Warm / Race Pace\\nOne Minute on the Road / Five Minutes on the Road\\nBack on the Asphalt',
'Essas conquistas são locais. Registrar desbloqueios\\nna conta do RetroAchievements exige login no serviço.':'These are local achievements. Official RetroAchievements\\nunlocks require a separate service account.',
'Entre na sua conta para carregar as conquistas oficiais.\\nO token fica protegido pelo Windows neste computador.':'Sign in to load official achievements.\\nWindows protects your token on this computer.',
'Usuário':'Username','Senha':'Password',
'Modo casual. Hardcore permanece desativado durante\\na validação do port. A senha não é salva.':'Casual mode. Hardcore is disabled during port validation.\\nYour password is not saved.',
'Entrar':'Sign in','Desconectar':'Sign out','Ver conquistas':'View achievements',
'Fechar':'Close','Salvar':'Save','Imagem e CRT':'Video & CRT','Conquistas':'Achievements',
'ESCOLHA SUA ESTRADA':'CHOOSE YOUR RIDE',
'Três clássicos. Uma garagem. Escolha o jogo e ligue o motor.':'Three classics. Your original games. Choose a ride and start your engine.',
'PREPARAR A MOTO':'SETTINGS',
'Versão ':'Version ',' · execução nativa experimental':' · locally built from your ROM',
'Importe a ROM para preparar este jogo':'Choose Install games to prepare this title with your own ROM.',
'Conquistas locais · F6 durante o jogo\\nSem conta. Seu progresso fica neste PC.':'Local achievements / F6 in game\\nYour progress stays on this PC.',
'Setas: escolher   Enter: jogar   Controle: direcional + A':'Arrow keys / D-pad: choose     Enter / A: play',
'Prévia 0.2 · Mega Drive':'v0.50 / Windows x64',
'LIGAR O MOTOR':'PLAY','Controles e atalhos':'Controls','Importar ROM':'Install games','Sobre o projeto':'About',
'Feche a janela do jogo antes de sair do launcher.':'Close the game window before exiting the launcher.',
'Jogo em execução':'Game running',
'O jogo foi encerrado com erro. Os logs de diagnóstico ficam na pasta do executável.':'The game exited with an error. Diagnostic logs are available in the .local/logs folder.',
'Sessão encerrada':'Game session ended',
'A pasta assets/launcher não foi encontrada. Extraia o pacote completo antes de abrir.':'Launcher artwork is missing. Extract the complete release package before opening the app.',
'Arquivos ausentes':'Missing files',
'Road Rash Trilogy Recomp — Garagem':'Road Rash Trilogy Recomp - 0.50',
}
for original,replacement in copy.items():text=text.replace(original,replacement)
text=re.sub(r'case NETWORK:MessageBoxW\(.*?;break;', 'case NETWORK:rr_controls_show(hwnd);break;',text,flags=re.S)
text=text.replace('case IMPORT:import_rom();break;', 'case IMPORT:rr_install_show(hwnd);break;')
text=re.sub(r'case HELP:MessageBoxW\(.*?;break;', '''case HELP:MessageBoxW(hwnd,L"Road Rash Trilogy Recomp - version " RR_VERSION L"\\\\n\\\\nAn independent Windows fan project using your original Mega Drive ROMs. Native translated 68000 code runs with a fallback for remaining paths.\\\\n\\\\nContinuous sprite zoom, widescreen, CRT filters and local achievements. Network play is not implemented.\\\\n\\\\nClassic covers: libretro-thumbnails. Fonts: Barlow / OFL. CRT: koko-aio. See CREDITS.md and THIRD_PARTY_NOTICES.md.",L"About",MB_OK);break;''',text,flags=re.S)
path.write_text(text,encoding='utf-8')
print('Legacy launcher copy migrated to English')
