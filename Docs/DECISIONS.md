# Registro de Decisões

## 2026-07-08 - Criar sandbox Unity separado

Decisão: criar um projeto Unity separado em `C:\Users\zacck\unity-combat-sandbox`.

Motivo: testar se Unity 3D resolve melhor os problemas de asset, animação e armas que apareceram no protótipo 2D em Godot.

Consequência: o projeto Godot não será apagado nem abandonado automaticamente. Ele continua como referência e histórico.

## 2026-07-08 - Usar Unity 6.3 LTS

Decisão: usar Unity 6.3 LTS.

Motivo: versão LTS tende a ser mais estável para um projeto que pode crescer.

## 2026-07-08 - Testar MCP oficial da Unity

Decisão: usar o MCP oficial via pacote `com.unity.ai.assistant`.

Motivo: o MCP open source testado antes criou conflito de configuração e atrito. Para avaliar Unity de forma justa, vamos testar o caminho suportado oficialmente.

## 2026-07-08 - Começar sem Mixamo

Decisão: o primeiro sandbox não usará animações Mixamo.

Motivo: antes de validar pipeline de animação, precisamos validar se Codex consegue controlar a Unity via MCP e criar uma cena jogável mínima.

## 2026-07-08 - Usar `Assets/_Project`

Decisão: concentrar arquivos do jogo em `Assets/_Project`.

Motivo: separar código/assets próprios de arquivos padrão da Unity, samples e pacotes externos.

## 2026-07-08 - Primeiro sandbox com placeholders primitivos

Decisao: criar `CombatSandbox` usando capsulas, cubos e materiais simples, sem importar Mixamo ou assets externos.

Motivo: validar primeiro o fluxo Unity 3D + MCP, movimento, camera e hitbox basica antes de adicionar pipeline de animacao.

## 2026-07-08 - Movimento via Input System direto

Decisao: ler WASD e Space com `UnityEngine.InputSystem.Keyboard.current`.

Motivo: o projeto ja usa o pacote Input System e este passo nao precisa de um asset de input/actions mais elaborado.

## 2026-07-08 - Builder editor-only para a cena inicial

Decisao: criar `CombatSandboxSceneBuilder` com menu `Tools/Combat Sandbox/Rebuild CombatSandbox Scene`.

Motivo: permitir reconstruir cena, materiais e prefabs iniciais de forma repetivel enquanto o sandbox ainda esta nascendo.

## 2026-07-08 - Mudar o sandbox para terceira pessoa

Decisao: a partir daqui, o sandbox principal sera third-person em perspectiva, nao isometrico.

Motivo: a visao do jogo puxa mais para personagem 3D, armas, Mixamo, troca de equipamento e combate fisico. Terceira pessoa valida essa dor real melhor do que uma camera isometrica.

Consequencia: manter o escopo simples por enquanto: camera atras/acima do player, mouse controla camera, WASD relativo a camera, ataque basico, dodge e dummy. Sem lock-on no primeiro passo.

## 2026-07-08 - Usar Y Bot como primeiro placeholder humanoide

Decisao: substituir a capsula visual do player pelo `Y Bot.fbx` do Mixamo Action Adventure Pack.

Motivo: validar o fluxo real de personagem humanoide 3D antes de criar sistema de animacao, armas ou lock-on.

Consequencia: o player ainda usa `CharacterController`, hitbox simples e scripts atuais. Animacoes foram importadas/configuradas como Humanoid, mas ainda nao foram conectadas ao gameplay.

## 2026-07-08 - Animator simples antes de moveset real

Decisao: criar `PlayerThirdPerson.controller` com idle, run, dodge e um estado temporario de ataque.

Motivo: validar rapidamente o pipeline de animacao Humanoid no player real sem ainda criar uma arquitetura grande de combate.

Consequencia: o ataque usa `AttackPlaceholder` ate importarmos uma animacao de ataque dedicada. O ajuste de pe no chao foi feito no offset/escala do visual, nao na hitbox da mao.

## 2026-07-08 - Usar Universal Animation Library para locomocao

Decisao: trocar idle/run/dodge/ataque placeholder do Mixamo Action Adventure Pack por clips da Universal Animation Library.

Motivo: os clips do Mixamo estavam crus, sem loop confiavel, e o dodge anterior usava uma animacao aerea.

Consequencia: locomocao e dodge ficam em clips mais adequados. O ataque ja usa um swing de espada placeholder, mesmo antes de termos modelo de espada.

## 2026-07-08 - Tratar ataque e dodge como acoes bloqueantes

Decisao: ataque e dodge agora respeitam locks simples de acao. Roll bloqueia ataque, ataque bloqueia roll e locomocao, e o roll preserva sua direcao inicial.

Motivo: aproximar o feeling de jogos third-person de acao, onde atacar tem compromisso e dodge nao pode ser cancelado por clique de ataque.

Consequencia: ainda nao existe sistema completo de combo/cancel window/stamina. O ataque sem arma usa `Punch_Cross` como placeholder ate termos modelo e animacao de arma.

## 2026-07-08 - Espada como pickup antes de inventario

Decisao: a primeira arma do sandbox entra como item no chao, com pickup por interacao e equipamento direto na mao direita do player.

Motivo: validar a sensacao de pegar/equipar uma arma e trocar o moveset antes de criar inventario, slots, raridade ou sistema completo de equipamentos.

Consequencia: o player ainda pode atacar desarmado com socos. Ao equipar a `Bronze Sword`, o ataque passa a usar animacoes de espada e valores de hit um pouco mais fortes.

## 2026-07-08 - Melhorar feedback do alvo antes de criar sistemas grandes

Decisao: evoluir primeiro o `TrainingDummyHealth` com feedback de hit, vida, poise, stagger, knockback e perfil simples de impacto.

Motivo: depois que movimento, dodge e combo ficaram bons, o proximo ganho de feeling vem do alvo responder ao jogador. Isso valida combate melhor do que abrir inventario, lock-on, IA ou atributos completos agora.

Consequencia: o dummy gera barras e efeitos em runtime, sem UI final. O perfil de impacto separa mob leve, mob pesado e boss para evitar que hitstop/stagger vire regra universal do jogo. Os golpes agora tambem carregam peso simples de impacto (`Light`, `Medium`, `Heavy`) e o alvo resolve a reacao (`Hit`, `Stagger`, `Death`).

## 2026-07-08 - Criar inimigo placeholder antes de importar mob final

Decisao: adicionar um inimigo placeholder e um dev console `F2` para controlar testes.

Motivo: validar combate contra alvo ativo antes de depender de modelo, animacao e importacao de assets de inimigo. O console evita que o inimigo atrapalhe testes de movimentacao, weapon timing e hitboxes quando nao estiver ativo.

Consequencia: o inimigo nasce por comando do dev console, fica parado ate ser iniciado e usa IA direta simples sem pathfinding. O console concentra flags temporarias de teste como hitboxes, vida infinita, inimigo invencivel e multiplicadores de velocidade.

## 2026-07-08 - Separar visual do mob e root de gameplay

Decisao: usar `Creep Horror Creature` como primeiro visual de inimigo, mas manter collider, vida, AI e hitbox em um root `EnemyPlaceholder` separado.

Motivo: o asset parece bom para testar leitura de inimigo, mas ainda estamos em sandbox. Separar visual de gameplay deixa facil trocar `Creep1` por outro prefab sem refazer dano, barras, console e IA.

Consequencia: o dev console instancia `Creep1.prefab` como filho visual quando o asset existe, toca animacoes simples do controller de demo e usa uma capsula fallback se o asset nao estiver disponivel.

## 2026-07-08 - Morte do inimigo controlada pelo dev console

Decisao: inimigos de teste nao revivem automaticamente ao zerar a vida.

Motivo: para o sandbox de combate, e mais util ver a animacao de morte ate o fim e manter o corpo morto no mundo. Quando for preciso repetir o teste, o reset fica explicito no dev console.

Consequencia: `Reset enemy` e `Despawn enemy` passam a ser o caminho de respawn/limpeza. As barras flutuantes pertencem ao inimigo e sao removidas junto com ele.
