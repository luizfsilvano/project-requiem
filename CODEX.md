# CODEX.md

Memória permanente do projeto Unity Combat Sandbox.

## Estado do Projeto

Estamos criando um protótipo separado em Unity para testar se a engine é uma opção melhor para a visão 3D do jogo.

O projeto anterior em Godot 4 continua existindo como referência de aprendizado e de combate 2D. Este projeto Unity é uma investigação controlada, não uma migração definitiva ainda.

## Contexto

O protótipo 2D em Godot chegou a um bom feeling de combate, com player, boss, padrões de ataque, UI e mapa. O problema percebido foi escala visual: para um jogo com muitas armas e estilos de combate, sprites 2D top-down/isométricos exigem muito trabalho de asset e animação.

A hipótese atual é que um pipeline 3D pode facilitar:

- reaproveitamento de animações humanoides;
- uso de Mixamo ou assets equivalentes;
- troca de armas por attachment em bones;
- criação de variações de armas sem redesenhar sprites;
- acesso maior a assets gratuitos/pagos de qualidade.

## Visão de Longo Prazo

O jogo desejado é um ARPG online, não massivo como MMO, mas com ambição de combate rico, troca de armas, evolução de personagem e cooperação online.

Essa visão é grande demais para implementar agora. O projeto atual deve validar apenas a base de combate 3D.

## Escopo Atual

Fazer um sandbox mínimo:

- uma cena de teste;
- um player 3D placeholder;
- uma câmera isométrica;
- movimentação WASD;
- um dummy inimigo;
- um ataque básico;
- uma hitbox simples;
- uma estrutura de pastas limpa;
- documentação suficiente para manter decisões e próximos passos.

## Fora de Escopo Por Enquanto

- mundo aberto;
- multiplayer;
- inventário;
- crafting;
- classes;
- magia completa;
- árvore de skills;
- economia;
- quests;
- save/load complexo;
- assets finais;
- animações definitivas.

Esses sistemas podem existir no jogo final, mas não entram antes do pipeline básico de combate 3D estar validado.

## Filosofia

Preferimos uma solução simples funcionando a uma arquitetura perfeita que nunca sai do papel.

Toda decisão técnica deve responder:

1. Isso ajuda a validar o combate 3D?
2. Isso reduz atrito para testar personagem, animação e armas?
3. Isso evita bagunça sem criar burocracia?

## Decisões Atuais

- Engine: Unity 6.3 LTS.
- Linguagem: C#.
- Render/Template inicial: projeto 3D/URP criado pelo Unity Hub.
- Integração com IA: Unity MCP oficial via pacote `com.unity.ai.assistant`.
- MCP open source anterior foi descartado para evitar instabilidade e conflito.
- Este projeto deve ter documentação própria, separada do Godot.
- Estrutura interna principal fica em `Assets/_Project`.

## Próximo Passo

Criar a primeira cena jogável `CombatSandbox` com câmera isométrica, chão, player placeholder, movimento e dummy.

Não importar Mixamo ainda. Primeiro validar que o Codex consegue editar a Unity via MCP e que a base de cena funciona.

## Atualizacao 2026-07-08 - Primeiro Sandbox Criado

A cena `CombatSandbox` foi criada em `Assets/_Project/Scenes/CombatSandbox.unity`.

Conteudo atual:

- chao simples;
- iluminacao basica;
- camera ortografica isometrica com follow;
- player placeholder com movimento WASD relativo a camera;
- dummy de treino com vida;
- ataque basico no Space com hitbox simples;
- prefabs iniciais para `PlayerPlaceholder`, `TrainingDummy` e `MainCameraRig`.

Decisoes praticas deste passo:

- o primeiro sandbox usa placeholders primitivos da Unity, sem Mixamo;
- movimento e ataque usam o Input System diretamente via `Keyboard.current`;
- a cena e os prefabs iniciais podem ser reconstruidos pelo menu `Tools/Combat Sandbox/Rebuild CombatSandbox Scene`.

Proximo passo: testar manualmente o feeling do movimento e do ataque no Play Mode. Se a base estiver confortavel, o proximo teste sera importar um humanoide com animacoes simples.

## Atualizacao 2026-07-08 - Direcao Third-Person

A direcao do sandbox mudou de isometrico para terceira pessoa.

Motivo: a visao atual depende mais de personagem 3D, armas visiveis, Mixamo, troca de equipamento e combate fisico. Terceira pessoa testa essa dor diretamente melhor do que uma camera isometrica.

Estado atualizado:

- `CombatSandbox` agora usa camera em perspectiva atras/acima do player;
- mouse controla a camera;
- WASD continua relativo a camera;
- `Left Shift` faz dodge simples;
- `Space` mantem ataque basico;
- ainda nao ha lock-on, multiplayer, inventario ou sistema completo de armas.

## Atualizacao 2026-07-08 - Primeiro Humanoide Mixamo

O pack `Action Adventure Pack.zip` foi importado em `Assets/_Project/Art/Mixamo/ActionAdventurePack`.

O `Y Bot.fbx` foi configurado como Humanoid e passou a ser o visual do `PlayerPlaceholder`, substituindo a capsula visual. O controle, hitbox, camera e dummy continuam os mesmos.

As animacoes do pack tambem foram configuradas como Humanoid para uso futuro, mas ainda nao foram ligadas ao controller do player.

## Atualizacao 2026-07-08 - Animacao Inicial do Player

O player agora tem `PlayerThirdPerson.controller` em `Assets/_Project/Animations`.

O controller liga:

- `Idle`;
- `Run`;
- `Dodge`;
- `AttackPlaceholder`.

`PlayerAnimationDriver` alimenta o Animator usando velocidade do `CharacterController` e eventos de ataque/dodge. O ataque ainda usa um placeholder porque o pack importado nao trouxe uma animacao de ataque dedicada.

O visual do Y Bot foi ajustado para `scale 1.12` e `localPosition.y -0.08` para reduzir a sensacao de estar flutuando.

## Atualizacao 2026-07-08 - UAL como Animacao Principal

Foram importados `UAL1_Standard.fbx` e `UAL2_Standard.fbx` em `Assets/_Project/Art/AnimationLibraries`.

O `PlayerThirdPerson.controller` agora usa clips da Universal Animation Library:

- `Idle`: `Armature|Idle_Loop`;
- `Run`: `Armature|Jog_Fwd_Loop`;
- `Dodge`: `Armature|Roll`;
- `AttackPlaceholder`: `Armature|Sword_Regular_A`.

`PlayerAnimationDriver` passou a controlar os estados por `CrossFade`, porque depender apenas das transitions estava deixando o Animator preso em idle durante debug.

## Atualizacao 2026-07-08 - Locks de Acao Basicos

O ataque basico agora fica no botao esquerdo do mouse e usa `Punch_Cross` como placeholder sem arma.

Regras atuais:

- durante o roll, ataque nao entra;
- segurando `Left Shift`, clique esquerdo nao inicia ataque no mesmo frame;
- durante ataque, o player nao inicia roll nem locomocao;
- o roll mantem a direcao inicial, entao o input pode continuar pressionado, mas nao vira 180 graus no meio do roll.

## Atualizacao 2026-07-08 - Primeira Espada Pickup

A `Bronze Sword` foi importada do `Fantasy Props MegaKit[Standard].zip` para `Assets/_Project/Art/Props/FantasyPropsMegaKit/Sword_Bronze.fbx`.

Estado atual:

- existe um pickup `BronzeSwordPickup` na cena `CombatSandbox`;
- o player recebeu `PlayerEquipment`;
- apertar `E` perto da espada equipa a arma na mao direita;
- desarmado, o ataque continua usando socos;
- armado, o ataque troca para `SwordAttackA` e `SwordAttackB` com clips da UAL2;
- a hitbox simples continua a mesma, mas golpes armados usam dano, poise e knockback maiores.

Ainda nao ha inventario, slots de equipamento, UI de pickup ou sistema completo de armas.

## Atualizacao 2026-07-08 - Feedback de Hit no Dummy

O `TrainingDummyHealth` agora concentra o primeiro feedback de combate do alvo.

Estado atual:

- barra flutuante runtime de vida;
- barra flutuante runtime de poise;
- flash de hit e flash diferente para stagger;
- knockback simples;
- scale punch curto no impacto;
- sparks no ponto aproximado de contato;
- reset com pequeno atraso ao zerar a vida;
- perfil simples de impacto para diferenciar mob leve, mob pesado e boss.

A regra de impacto ainda e propositalmente pequena: cada hit tem peso `Light`, `Medium` ou `Heavy`, e o alvo resolve a reacao como `Hit`, `Stagger` ou `Death`. Mob leve pode tomar hitstop/stagger, mob pesado reduz poise/knockback e so usa hitstop em reacao forte, boss nao usa hitstop e nao toma stagger por esse sistema inicial.

Ainda nao ha IA inimiga, lock-on, stamina final, UI final de combate ou sistema generico de atributos.

## Atualizacao 2026-07-08 - Janela de Dano por Contato da Animacao

O ataque nao liga mais a hitbox imediatamente ao iniciar a animacao.

`BasicMeleeAttack` agora calcula uma janela de dano atrasada por golpe, usando um momento de contato normalizado da animacao. Isso deixa o dano acontecer mais perto do fim visual do swing, em vez de acertar o dummy durante a preparacao do golpe.

Os timings ainda sao valores de sandbox e podem ser ajustados no Inspector.

## Atualizacao 2026-07-08 - Hitbox Mais Larga no Terceiro Golpe

O terceiro golpe armado agora usa uma variacao maior da hitbox base durante a janela ativa.

`MeleeHitbox` guarda a posicao/escala original para os golpes normais e, no `SwordAttackC`, aumenta a regiao lateral e um pouco a profundidade frontal. A/B continuam usando a caixa frontal normal.

## Atualizacao 2026-07-08 - Mirar o Terceiro Golpe no Windup

Durante o recovery do segundo golpe armado e o windup do `SwordAttackC`, o player pode rotacionar usando input direcional relativo a camera, mas continua sem locomocao livre.

Quando a janela de dano do terceiro golpe abre, a rotacao volta a travar. Isso deixa o jogador corrigir a direcao do golpe pesado sem transformar o ataque em movimento livre.

## Atualizacao 2026-07-08 - Inimigo Placeholder e Dev Console

Foi adicionado um inimigo placeholder criado em runtime pelo dev console.

Estado atual:

- `F2` abre/fecha o dev console;
- o console consegue spawnar, iniciar, parar, resetar e remover o inimigo placeholder;
- o inimigo anda diretamente ate o player e usa um ataque simples com hitbox frontal;
- o player recebeu `PlayerHealth` runtime para poder tomar dano;
- o console tem toggles de hitboxes, vida infinita do player, inimigo invencivel, flag reservada de stamina infinita, AI do inimigo e sliders de velocidade do player/inimigo;
- as hitboxes do player e do inimigo ficam escondidas por padrao e aparecem via console.

Ainda nao ha pathfinding, animacao de inimigo, asset final de mob, stamina real ou UI final de debug. O objetivo deste passo e permitir testar combate contra um alvo ativo sem depender de asset externo.

## Atualizacao 2026-07-08 - Creep Horror Creature como Primeiro Mob Visual

O asset `Creep Horror Creature` importado da Unity Store passou a ser usado pelo spawn do dev console.

Estado atual:

- `F2 > Spawn creep enemy` cria o inimigo usando `Assets/Creep Horror Creature/Prefabs/Creep1.prefab` como visual;
- o root de gameplay continua sendo `EnemyPlaceholder`, com `CapsuleCollider`, `TrainingDummyHealth`, `EnemyPlaceholderAI` e `EnemyAttackHitbox`;
- o visual do Creep fica como filho do root e e ajustado automaticamente para encostar no chao;
- o AI placeholder toca animacoes simples do controller do asset: idle, walk, bite e death;
- se o asset nao existir, o spawn cai para uma capsula fallback para o sandbox continuar abrindo.
- o inimigo nao revive mais automaticamente ao morrer; ele fica morto ate `Reset enemy` ou `Despawn enemy` pelo console.

Ainda nao ha moveset final do inimigo, navmesh, hit reactions animadas ou balanceamento definitivo.

## Atualizacao 2026-07-09 - Inimigo com State Machine Inicial

`EnemyPlaceholderAI` deixou de ser apenas uma rotina direta de perseguir e atacar.

Estado atual:

- a AI agora usa estados explicitos: `Idle`, `Chase`, `Windup`, `Attack`, `Recovery`, `Stagger` e `Death`;
- o ataque tem telegraph visual no chao durante o windup antes da hitbox ativar;
- stagger interrompe windup/ataque/recovery, desliga a hitbox e segura o inimigo parado ate o lock acabar;
- morte tambem cancela ataque e congela a animacao de death no final;
- o dev console mostra o estado atual do inimigo para facilitar debug;
- o feedback visual de dano do alvo passou a usar `MaterialPropertyBlock`, preservando os materiais/texturas do Creep em vez de alterar diretamente o material do renderer.

Ainda nao ha pathfinding/NavMesh, hit reaction animada dedicada ou moveset variado.

## Atualizacao 2026-07-09 - Skeleton Low Poly como Mob de Teste

O spawn do dev console foi trocado do `Creep Horror Creature` para o asset `SazenGames/Skeleton`.

Estado atual:

- `F2 > Spawn skeleton enemy` instancia `Assets/SazenGames/Skeleton/Prefabs/Skeleton_110.prefab`;
- o esqueleto usa os override controllers do pacote para idle, walk, slash, take damage e death;
- `EnemyPlaceholderAI` agora aceita assets que usam um controller de estado unico chamado `anim`, trocando o controller por acao;
- o Creep ainda nao foi deletado do projeto, apenas deixou de ser usado pelo spawn runtime.

Os warnings de importacao vistos em `Skeleton_take_damage`, `Skeleton_revive`, `Skeleton_death` e `Skeleton_throw_projectiles` sao avisos de retargeting/translation DOF em ossos especificos. Para o sandbox atual, usando o proprio esqueleto do pacote, eles nao bloqueiam o teste.

## Atualizacao 2026-07-09 - Timing do Ataque do Skeleton

O ataque do skeleton foi ajustado para ficar mais justo de desviar.

Estado atual:

- a janela de dano do inimigo liga mais tarde no slash, perto do fim visual da animacao;
- o `attackWindup` padrao subiu para `0.78s`;
- a hitbox fica ativa por `0.12s`;
- o stagger lock padrao subiu para `0.85s`, deixando a animacao de tomar hit respirar melhor;
- o quadrado vermelho de telegraph continua disponivel para debug, mas fica desligado por padrao;
- o dev console F2 expoe sliders para `Attack windup/contact delay`, `Hitbox active time`, `Attack recovery`, `Attack cooldown`, `Stagger lock` e o toggle `Show red attack telegraph`.

## Atualizacao 2026-07-09 - Enemy Tuning Persistente e Abas no Dev Console

Os timings do skeleton deixaram de depender de sliders temporarios do dev console.

Estado atual:

- existe um asset persistente em `Assets/_Project/Settings/EnemyCombatTuning.asset`;
- o asset expoe no Inspector movimento, alcance, windup, janela ativa, recovery, cooldown, telegraph e stagger lock;
- o `DevConsole` aplica esse asset quando spawna o inimigo;
- o F2 foi dividido em abas: `Enemy`, `Player` e `Debug`;
- a aba `Enemy` mostra um resumo dos valores salvos no asset, mas o ajuste persistente deve ser feito no Inspector.

## Atualizacao 2026-07-09 - Skeleton Animator Controller Proprio

O skeleton deixou de trocar `overrideController` por acao durante runtime.

Estado atual:

- existe `Assets/_Project/Animations/SkeletonEnemy.controller`;
- o controller tem estados reais `Idle`, `Walk`, `Attack`, `Stagger` e `Death`;
- o `DevConsole` sobrescreve o controller original do prefab pelo controller do sandbox ao spawnar;
- `EnemyPlaceholderAI` continua usando `CrossFadeInFixedTime`, agora entre estados do mesmo controller, reduzindo snaps visuais como a mao teleportando de `Attack` para `Idle`;
- `EnemyCombatTuning.asset` foi atualizado para os valores testados: `moveSpeed 2.5` e `attackWindup 0.5`.

## Atualizacao 2026-07-09 - SkeletonEnemy como Prefab Reutilizavel

O inimigo skeleton deixou de ser montado apenas em runtime pelo dev console.

Estado atual:

- existe `Assets/_Project/Prefabs/Enemies/SkeletonEnemy.prefab`;
- o prefab tem root de gameplay com `CapsuleCollider`, `Rigidbody`, `TrainingDummyHealth` e `EnemyPlaceholderAI`;
- o visual `SkeletonVisual` usa o controller `SkeletonEnemy.controller`;
- a hitbox fica como filho `EnemyAttackHitbox`;
- `TrainingDummyHealth` e `EnemyPlaceholderAI` recebem `EnemyCombatTuning.asset` direto no prefab;
- o dev console F2 agora tenta spawnar esse prefab primeiro e só usa montagem runtime como fallback.

Esse prefab pode ser arrastado para outras cenas e reutilizado sem depender do dev console.

## Atualizacao 2026-07-09 - Cleanup de Morte do Skeleton

O `SkeletonEnemy.prefab` agora limpa o corpo morto sem bloquear o mapa.

Estado atual:

- quando o inimigo morre, `TrainingDummyHealth` desliga os colliders do root e da hitbox;
- a barra flutuante de vida/poise e desativada imediatamente na morte;
- rigidbodies filhos param de detectar colisao e ficam kinematic;
- depois de `30s`, o corpo faz fadeout por `1.4s` e o root e destruido;
- esses valores ficam expostos no Inspector em `TrainingDummyHealth > Death Cleanup`;
- o prefab foi reconstruido com `disableCollisionOnDeath`, `destroyAfterDeath`, `deathDespawnDelay` e `deathFadeDuration` serializados.

## Atualizacao 2026-07-09 - Lock-On Inicial

O player agora tem um lock-on simples para melhorar leitura e direcao do combate.

Estado atual:

- `Tab` trava/destrava o alvo valido mais proximo na frente da camera;
- `PlayerLockOnController` busca `TrainingDummyHealth` vivos dentro de raio/angulo configuraveis no Inspector;
- a camera suaviza yaw/pitch para manter o alvo enquadrado enquanto o lock-on esta ativo;
- o ataque gira o player para o alvo travado antes da janela de hitbox/lunge;
- existe um indicador runtime simples em volta do alvo travado;
- `PlayerPlaceholder.prefab` e a cena `CombatSandbox` foram atualizados com o componente e as referencias.

## Atualizacao 2026-07-09 - Troca Lateral de Lock-On

O lock-on agora troca de alvo com movimento lateral do mouse.

Estado atual:

- enquanto o lock-on esta ativo, arrastar o mouse para esquerda/direita tenta trocar para o proximo alvo vivo daquele lado;
- a troca respeita o lado visual da camera, entao funciona melhor contra varios inimigos no enquadramento;
- `mouseRetargetThreshold`, `mouseRetargetCooldown` e `sideSwitchDeadZone` ficam expostos no Inspector do `PlayerLockOnController`;
- se nao houver alvo valido daquele lado, o lock-on atual continua ativo.

## Atualizacao 2026-07-09 - Stamina Inicial e HUD Simples

O player agora tem stamina real para acoes de combate.

Estado atual:

- `PlayerStamina` controla pool, regen, delay de regen e custos;
- dodge consome stamina;
- ataques consomem stamina por golpe/combo;
- sem stamina suficiente, dodge e ataque nao entram;
- corrida nao consome stamina;
- `DevSettings.InfiniteStamina` existe como toggle real no dev console;
- `PlayerStaminaHud` cria um texto simples `Stamina: atual/max` no canto superior esquerdo;
- `PlayerPlaceholder.prefab` e `CombatSandbox` foram atualizados com stamina e HUD.

## Atualizacao 2026-07-09 - Weapon Data Inicial

A espada deixou de depender apenas de valores hardcoded no ataque/hitbox.

Estado atual:

- existe `WeaponData` como ScriptableObject para definir nome, prefab equipado, combo e golpes;
- existe `WeaponAttackData` para cada golpe com duracao, hit moment, stamina, dano, poise, knockback, impacto, lunge e hitbox;
- `Assets/_Project/Data/Weapons/BronzeSword.asset` guarda os dados atuais da Bronze Sword;
- `WeaponPickup` prefere equipar via `WeaponData`, mantendo fallback antigo por prefab/nome;
- `PlayerEquipment` guarda a arma equipada atual em `EquippedWeapon`;
- `BasicMeleeAttack` le duracao, hit moment, stamina, lunge e combo recovery da arma equipada quando houver data;
- `MeleeHitbox` le dano, poise, knockback, impacto e forma de hitbox do golpe ativo quando houver data.

## Atualizacao 2026-07-09 - Inventario Minimo de Armas

O ciclo pickup -> inventario -> equipamento agora existe em formato pequeno.

Estado atual:

- `PlayerInventory` guarda uma lista de `WeaponData`;
- `AddWeapon` evita duplicatas por asset;
- pickup de arma adiciona no inventario e equipa automaticamente;
- teclas `1` a `9` equipam armas pelos slots da lista;
- o dev console mostra arma equipada e quantidade de armas;
- o HUD simples mostra `Weapon: nome (quantidade)`;
- `PlayerPlaceholder.prefab` e `CombatSandbox` foram atualizados com `PlayerInventory`.

## Atualizacao 2026-07-09 - Bronze Axe como Segunda Arma

Foi criada uma segunda arma real usando asset do pack `Fantasy Props MegaKit[Standard].zip`.

Estado atual:

- `Axe_Bronze.fbx` foi extraido para `Assets/_Project/Art/Props/FantasyPropsMegaKit`;
- existe `Assets/_Project/Data/Weapons/BronzeAxe.asset`;
- existe `Assets/_Project/Prefabs/Items/BronzeAxe_Equipped.prefab`;
- existe `Assets/_Project/Prefabs/Items/BronzeAxePickup.prefab`;
- `BronzeAxePickup` foi colocado na cena `CombatSandbox` perto do pickup da espada;
- Bronze Axe usa dados mais pesados que a espada: mais dano/poise/stamina, ataques mais lentos e lunge menor;
- o inventario foi validado com Bronze Sword no slot `1` e Bronze Axe no slot `2`.

## Atualizacao 2026-07-09 - Trava de Troca de Arma em Acao

A troca de arma agora respeita o estado atual do player.

Estado atual:

- `PlayerInventory` nao equipa outra arma enquanto `BasicMeleeAttack.IsAttacking` estiver ativo;
- a troca tambem fica bloqueada durante dodge pelo `PlayerCameraRelativeMovement.IsDodging`;
- pickups durante uma acao ainda entram no inventario, mas nao trocam a arma equipada no meio do golpe;
- `PlayerPlaceholder.prefab` foi atualizado com referencias diretas para equipamento, ataque e movimento.

## Atualizacao 2026-07-09 - Feedback Temporario de Combate

Foi adicionado um primeiro passe de feedback audiovisual usando os packs temporarios indicados.

Estado atual:

- sons extraidos de `150+ Free RPG Sounds.zip` para `Assets/_Project/Audio/RPGSounds`;
- texturas VFX extraidas de `brackeys_vfx_bundle.zip` para `Assets/_Project/Art/VFX/Brackeys`;
- existe um objeto `CombatFeedback` na cena com `CombatFeedbackAudio` e `CombatFeedbackVfx`;
- ataques do player tocam swing leve/pesado;
- ataques do inimigo tocam som temporario de ataque;
- hits no inimigo tocam som de impacto leve/pesado e geram bursts simples de sangue/impacto/faisca;
- morte do inimigo toca som temporario de morte;
- dano no player toca som temporario de hurt;
- equipar e pegar arma tocam feedback sonoro;
- o shake do hitbox do player agora escala mais em golpes medios/pesados.

## Atualizacao 2026-07-09 - Ajuste de Audio e VFX Temporarios

O feedback temporario foi ajustado depois do primeiro teste visual.

Estado atual:

- `MainCameraRig.prefab` agora tem `AudioListener`;
- `CombatFeedbackAudio` tambem garante um listener em runtime caso a cena esteja sem um;
- one-shots de combate ficaram 2D e com volume base `1`;
- os VFX temporarios nao usam mais as texturas do bundle diretamente, porque elas estavam aparecendo com retangulo preto;
- os hits agora usam particles additive simples sem textura ate trocarmos por VFX final.

## Atualizacao 2026-07-09 - Ajuste do Som do Terceiro Hit

O terceiro hit da espada estava barulhento demais.

Estado atual:

- hit pesado/stagger nao usa mais o clip critico cheio de sons de lamina;
- hit pesado usa o mesmo som base de impacto com pitch/volume diferente;
- `CombatFeedbackAudio` tem um debounce curto para impedir varios sons de hit empilhando no mesmo instante.

## Atualizacao 2026-07-10 - Golem Boss MVP

Foi criado um primeiro boss placeholder para validar combate contra um inimigo grande.

Estado atual:

- os pacotes `Giant Monster Model - Golem` e `Giant Animations FREE`, de Kevin Iglesias, foram organizados em `Assets/_Project/Art/Enemies/Golem/KevinIglesiasHumanoidGiant` com os GUIDs preservados;
- existe `Assets/_Project/Prefabs/Enemies/GolemBoss.prefab`;
- existe `Assets/_Project/Animations/GolemBoss.controller`;
- o prefab usa `GolemBossAI`, separado da AI do skeleton, com estados `Idle`, `Chase`, `Windup`, `Telegraph`, `Attack`, `Recovery`, `Stagger` e `Death`;
- o boss alterna deterministicamente entre dois golpes lentos: `Pisao sismico`, usando `Giant@UnarmedAttack01`, e `Varrida lateral`, usando `Armature|Melee_Hook` retargetado;
- cada golpe tem hitbox propria e apenas a hitbox do golpe atual liga durante a janela ativa;
- o pisao usa windup `1.25s`, telegraph de `0.50s`, janela ativa `0.14s` e recovery `1.10s`;
- a varrida usa windup `0.90s`, janela ativa `0.20s` e recovery `0.72s`;
- o golem tem `650 HP`, `280 poise`, multiplicador proprio de dano de poise e stagger lock de `1.35s`;
- `TrainingDummyHealth` agora permite configurar stagger separadamente do perfil de impacto, preservando o comportamento atual do skeleton;
- a barra flutuante foi desligada no boss e `BossHealthHud` cria uma barra simples no centro inferior da tela;
- o lock-on aceita overrides por alvo; no golem, camera e indicador usam altura/raio compativeis com o corpo maior;
- hitboxes ofensivas do player e do inimigo se ignoram, evitando que a area larga do boss vire uma hurtbox falsa;
- o scale punch foi desligado apenas no boss para nao deformar junto o collider e as hitboxes do root;
- o cooldown de cada ataque comeca depois do recovery, preservando uma pausa legivel antes do proximo windup;
- morte continua reutilizando a regra do skeleton: colisao desligada imediatamente, HUD escondido, espera de `30s`, fade de `1.4s` e destruicao do root;
- o fade configura blend transparente e `ZWrite` corretamente para materiais URP/Standard;
- o dev console `F2`, aba `Enemy`, ganhou `Spawn golem boss`, `Start/Stop boss`, `Reset boss` e `Despawn boss`;
- o menu `Tools/Combat Sandbox/Build Golem Boss MVP` reconstrói controller e prefab sem usar o scene builder antigo.

Validacao realizada:

- prefab carregado sem scripts ausentes e com dois hitboxes;
- Play Mode confirmou chase, alternancia dos dois ataques, janelas exclusivas de hitbox, recovery, cooldown pos-recovery, stagger, HUD com preenchimento proporcional, morte, remocao de colisao e fade/despawn;
- Console terminou sem errors ou warnings do projeto;
- `Assets/SazenGames` continua presente e nao foi alterada.

Continuam fora deste MVP: fase 2, cutscene, drops, arena lock, multiplayer e polimento final de audio/VFX.

## Atualizacao 2026-07-10 - Grounding e Pisao Sismico do Golem

O primeiro passe do boss foi ajustado depois do teste em cena.

Estado atual:

- o root, collider e hitboxes do golem continuam em `Y = 0`; apenas `GolemVisual` foi corrigido de `localY +0.375` para `-0.05`, removendo o vao sob os pes sem mudar a escala do boss;
- a pose Idle, caminhada e slam ficam com as solas encostadas no chao; a varrida retargetada mantem apenas uma pequena penetracao temporaria aceitavel para o placeholder;
- o primeiro ataque passou a se chamar `Pisao sismico` e usa o fluxo `Windup -> Telegraph -> Attack -> Recovery`;
- depois do contato visual do pisao, particulas discretas marcam por `0.5s` a area frontal antes do dano;
- no fim do telegraph, `EnemyAttackHitbox.Pulse` aplica imediatamente `36` de dano por `Physics.OverlapBox`, sem depender do proximo `FixedUpdate`;
- a area do pisao fica rente ao chao, a frente do golem, com centro local `(0, 0.45, 2.65)` e tamanho `(3.8, 0.9, 4.2)`;
- `GolemGroundSlamVfx` posiciona o aviso pelo mesmo `BoxCollider` usado no dano e gera poeira/fragmentos no impacto;
- foram importadas somente `Smoke_04_A.png`, `Dirt_01_A.png` e a licenca do `brackeys_vfx_bundle.zip` para `Assets/_Project/Art/VFX/Brackeys`;
- o material de particulas usa alpha blend URP, mantendo o telegraph sutil e evitando retangulos pretos;
- stagger, morte, `Stop boss` e `OnDisable` cancelam o telegraph sem deixar dano ou impacto atrasado;
- a varrida lateral, vida, poise, HUD, morte/fade e sons placeholder nao foram alterados.

Validacao realizada:

- Play Mode confirmou `0.5s` serializados de telegraph, dano apenas depois do aviso, alvo dentro recebendo `36` e alvo lateral fora da area recebendo `0`;
- o impacto gerou poeira e fragmentos transparentes, e a varrida continuou ativando apenas sua propria hitbox;
- cancelar o boss durante o telegraph deixou a AI em `Idle`, removeu as particulas e nao entrou em `Attack`;
- uma rodada limpa com os ataques rodando terminou com `0 errors` e `0 warnings` no Console.

## Atualizacao 2026-07-11 - Fundacao de Itens, Inventario e Dois Slots

O inventario minimo de `WeaponData` foi migrado para uma fundacao baseada em definicao compartilhada e instancia possuida.

Arquitetura atual:

- `ItemDefinition` e a base compartilhada dos tipos de item, com ID estavel, stack maximo e durabilidade padrao;
- `WeaponData` herda de `ItemDefinition`, mas continua sendo a unica fonte dos dados de combo e combate;
- `ItemInstance` e serializavel e guarda ID unico, definicao, quantidade, durabilidade atual/maxima, qualidade e raridade;
- `ItemInstance` tambem reserva metadados opcionais de criador, afinidade, item roubado e dono original, sem implementar suas regras ainda;
- `IItemContainer` e `ItemContainer` concentram consulta, add, remove e move com validacao, rollback e ownership exclusivo da instancia;
- a lista publica do container e read-only, IDs duplicados sao normalizados e a mesma instancia nao pode pertencer a dois containers;
- `PlayerInventory` e o dono das instancias do player e implementa o contrato de container sem expor bypass das regras de equipamento;
- os campos antigos `weapons` e `equippedWeaponIndex` continuam cobertos por migracao via `FormerlySerializedAs`;
- `AddWeapon(WeaponData)` e `EquipWeapon(int)` continuam como wrappers de compatibilidade;
- `PlayerEquipment` possui slots `Primary` e `Secondary`, ambos referenciando instancias que permanecem no inventario;
- apenas o slot ativo instancia o prefab visual na mao, preservando `EquippedWeapon` para `BasicMeleeAttack`, `PlayerAnimationDriver` e `MeleeHitbox`;
- `1` ativa o slot principal, `2` ativa o secundario e `Q` alterna entre os dois;
- a troca e bloqueada tanto no inventario quanto no equipamento durante ataque ou dodge;
- o primeiro pickup de arma ocupa `Primary`; o segundo ocupa `Secondary` e vira o ativo quando a acao permite;
- `WeaponPickup` continua serializando a definicao e a quantidade, criando um novo ID somente no momento da coleta;
- o HUD e o dev console mostram slot ativo, principal, secundario, contagem e durabilidade como feedback temporario de debug.

Assets migrados:

- `BronzeSword.asset` usa `definitionId weapon.bronze_sword`, stack `1` e durabilidade padrao `100`;
- `BronzeAxe.asset` usa `definitionId weapon.bronze_axe`, stack `1` e durabilidade padrao `100`;
- os dois pickup prefabs usam quantidade `1` e preservam as referencias aos mesmos `WeaponData` e prefabs visuais;
- `PlayerPlaceholder.prefab` serializa o novo container vazio, os dois slots vazios e referencias diretas para ataque e movimento;
- a cena `CombatSandbox` herdou a migracao pelos mesmos GUIDs de prefab e nao precisou de overrides novos.

Validacao realizada:

- Play Mode confirmou Sword e Axe como duas `ItemInstance` com IDs distintos, quantidade `1` e durabilidade `100/100`;
- qualidade `Standard`, raridade `Common` e metadados opcionais vazios foram confirmados;
- slots principal/secundario, selecao, toggle, visual equipado e travas de ataque/dodge foram validados;
- add, consulta, remove, move, rollback, ID duplicado e ownership entre containers foram exercitados;
- um golpe real da Bronze Axe aplicou `34` de dano vindo de `WeaponData`, levando um dummy novo de `100` para `66` HP;
- Skeleton chegou ao estado `Attack` e Golem ao estado `Recovery`, ambos vivos e com controllers/hitboxes validos;
- 9 prefabs e 316 GameObjects foram varridos com `0` scripts ausentes;
- a cena terminou limpa, fora de Play Mode, com `0 errors` e `0 warnings` no Console;
- `Assets/SazenGames` continua presente e nao foi alterada.

Continuam fora de escopo: consumo/reparo de durabilidade, regras de qualidade/raridade, afinidade, roubo, crafting, save, banco de dados, multiplayer, loja, peso, loot de cadaver e UI completa.

Observacao de manutencao: `CombatSandboxSceneBuilder` continua legado e nao conhece todos os assets atuais. Nao executar `Tools/Combat Sandbox/Rebuild CombatSandbox Scene` sem modernizar o builder completo; ele nao foi acionado nem teve sua versao alterada nesta migracao.
