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

## Atualizacao 2026-07-11 - Tela de Personagem, Inventario e Equipamento

A segunda etapa do sistema de itens adicionou uma tela funcional de personagem sobre o dominio de instancias, sem mover regras de inventario para os componentes visuais.

Arquitetura atual:

- `EquipmentSlotType` centraliza os 12 slots estruturais: `MainHand`, `OffHand`, `Head`, `Chest`, `Hands`, `Legs`, `Feet`, `Accessory1`, `Accessory2`, `AxeTool`, `PickaxeTool` e `KnifeTool`;
- `EquipmentSlotMask` permite que cada `ItemDefinition` declare exatamente os slots aceitos;
- `ItemDefinition` agora tambem possui descricao, icone opcional e `ItemCategory` para filtros;
- Bronze Sword e Bronze Axe sao categoria `Weapon` e aceitam apenas `MainHand | OffHand`;
- `WeaponData` continua sendo a unica fonte de prefab equipado, dano, combo, stamina, poise, lunge e hitbox; esses dados nao foram duplicados na UI;
- `PlayerEquipment` guarda uma lista serializavel de `EquipmentSlotState`, e cada slot referencia apenas o ID de uma `ItemInstance` que continua pertencendo ao `PlayerInventory`;
- o mesmo ID nao pode ocupar dois slots; equipar, substituir, mover, trocar e desequipar validam ownership e compatibilidade antes da mutacao;
- operacoes envolvendo arma continuam bloqueadas durante `BasicMeleeAttack.IsAttacking` ou `PlayerCameraRelativeMovement.IsDodging`;
- `OffHand` representa nesta fase a antiga arma secundaria alternavel: somente a arma ativa e instanciada no socket direito e participa do combate; dual wield, shield e uma mao esquerda simultanea continuam futuros;
- `PlayerEquipment.EquipmentChanged` e `ActiveWeaponChanged` notificam a tela e o preview;
- `ItemInstance.Changed` notifica mudancas de quantidade/durabilidade, e `PlayerInventory.InventoryChanged` retransmite as mudancas das instancias possuidas;
- a UI nunca escreve na lista interna do container nem nos IDs dos slots; ela chama somente as APIs publicas do inventario/equipamento.

Tela e interacoes:

- `I` abre e fecha a tela; `Tab` continua exclusivo do lock-on e `F2` continua abrindo o dev console;
- inventario e dev console sao modais exclusivos para nao disputar o cursor;
- ao abrir, a tela captura o estado anterior do cursor, usa `None/visible` e ativa `GameplayInputGate.InventoryOpen`;
- enquanto aberta, movimento, corrida, ataque, dodge, lock-on, mouse da camera, `1`, `2`, `Q` e coleta por `E` ficam bloqueados, mas `Time.timeScale` nao muda e o mundo continua rodando;
- ao fechar, o cursor anterior e restaurado e a entrada de gameplay fica suprimida ate o frame seguinte, evitando ataque/relock pelo clique de fechamento;
- a coluna esquerda possui grid e filtros `All`, `Weapons`, `Armor`, `Consumables`, `Materials`, `Tools` e `Other`;
- os cards mostram monograma quando nao ha icone, quantidade, raridade, qualidade, durabilidade e badge de equipado;
- a coluna direita agrupa corpo, armas, acessorios e ferramentas usando `EquipmentSlotType`, sem strings como identidade de slot;
- apenas `MainHand` e `OffHand` possuem gameplay completo agora; os demais aparecem vazios e preparados para definicoes futuras compativeis;
- selecionar atualiza nome, descricao, tipo, raridade/qualidade, quantidade, durabilidade, resumo de dano/combo e metadados opcionais quando preenchidos;
- existem acoes de equipar, ativar, desequipar e descartar;
- duplo clique executa a acao padrao;
- drag-and-drop funciona de inventario para equipamento, entre slots e de equipamento de volta para o inventario;
- durante o drag, todos os slots recebem highlight valido/invalido; uma tentativa incompativel falha sem perder ou duplicar a instancia;
- `PlayerStaminaHud` voltou a criar/usar seu proprio Canvas, evitando que o HUD antigo seja anexado ao Canvas da nova tela.

Preview e assets:

- `Assets/_Project/Prefabs/UI/CharacterInventoryScreen.prefab` e a tela reutilizavel em uGUI;
- `Assets/_Project/Prefabs/UI/InventoryItemSlot.prefab` e o card reutilizavel do grid;
- `Assets/_Project/Prefabs/UI/CharacterPreviewRig.prefab` contem apenas Y Bot, Animator, tres luzes e uma camera dedicada, sem scripts de gameplay e sem `AudioListener`;
- `Assets/_Project/UI/CharacterPreview.renderTexture` recebe a camera dedicada e e exibida por `RawImage`;
- a layer `CharacterPreview` isola modelo, arma, camera e luzes; a Main Camera e as luzes do mundo excluem essa layer;
- o preview instancia somente o `equippedPrefab` da arma ativa e reaproveita a pose real exposta por `PlayerEquipment.TryGetEquippedWeaponPose`;
- arrastar o preview rotaciona o personagem e a roda do mouse aplica zoom limitado;
- `CombatSandbox` possui exatamente um `UIEventSystem` com `InputSystemUIInputModule`;
- `Tools/Combat Sandbox/Build Character Inventory UI` reconstrói somente os assets desta tela e sua integracao, sem executar o `CombatSandboxSceneBuilder` legado.

Validacao realizada:

- a tecla `I` foi exercitada pelo Input System para abrir/fechar, com cursor `Locked/hidden -> None/visible -> Locked/hidden`;
- fechar e tentar atacar no mesmo frame retornou `false` com o gate de supressao ativo;
- `W`, `Shift`, `Tab`, `1`, `Q`, `E` e clique esquerdo mantidos durante a tela aberta nao moveram o player, nao atacaram, nao iniciaram dodge, nao criaram lock-on e nao trocaram a arma;
- os pickups reais adicionaram duas instancias distintas, quantidade `1` e durabilidade `100/100`;
- os 12 slots foram confirmados; espada no `Head` falhou com `IncompatibleSlot`;
- swap, move, desequipar e reequipar preservaram duas instancias possuidas e exatamente um slot por ID;
- uma mudanca de durabilidade para `73/100` gerou exatamente um evento de inventario e atualizou automaticamente os detalhes;
- equipar durante ataque e durante dodge falhou com `ActionLocked`;
- `1`, `2` e `Q` continuaram ativando `MainHand`, `OffHand` e toggle com a tela fechada;
- `F2` continuou funcionando, e `I` foi ignorado enquanto o dev console estava aberto;
- preview rotacionou `37` graus, alterou o zoom de `4.35` para `3.85` e refletiu a arma ativa;
- filtros mostraram `2` armas e `0` armaduras sem criar itens falsos;
- `Full HD (1920x1080)` e `WXGA (1366x768)` mantiveram as tres colunas sem sobreposicao e o personagem centralizado;
- Skeleton manteve controller e hitbox validos; Golem chegou a `Recovery` com controller e duas hitboxes validos;
- 20 prefabs e 725 GameObjects de prefab foram varridos com `0` scripts ausentes e `0` referencias quebradas; a cena tambem terminou com `0` scripts ausentes;
- foi confirmado exatamente `1` AudioListener, `1` EventSystem e a Main Camera excluindo `CharacterPreview`;
- Play Mode terminou com `0 errors` e `0 warnings`; o Editor ficou fora de Play Mode, cena limpa e Console vazio;
- `Assets/SazenGames` continua presente e nao foi alterada.

Continuam fora de escopo: armaduras/acessorios/ferramentas reais, stats e bonus desses slots, dual wield, consumo/reparo de durabilidade, afinidade funcional, roubo, crafting, save, banco de dados, multiplayer, peso, lojas, bancos/bau, loot de cadaver, gamepad completo e arte/audio/VFX finais.

## Atualizacao 2026-07-11 - Interacao de Mundo, Containers e Dialogo

A terceira etapa da fundacao de itens conectou as `ItemInstance` ao mundo por uma camada generica de interacao, sem transferir regras de ownership para pickups, baus ou componentes visuais.

Arquitetura atual:

- `IInteractable`, `InteractionContext`, `InteractionPromptData` e `InteractionKind` formam o contrato reutilizavel para pickups, containers, portas, dialogos e interacoes futuras;
- `InteractableBehaviour` concentra nome, descricao, prioridade, alcance, ponto de interacao, disponibilidade e notificacao de mudanca de estado;
- `PlayerInteractor` e o unico leitor da tecla `E` para interacoes de mundo;
- a selecao usa `OverlapCapsuleNonAlloc` na layer `Interactable` e desempate deterministico por prioridade, alinhamento com a camera, distancia e instance ID;
- alvos invalidos, destruidos, desativados, fora de alcance, durante ataque/dodge ou durante qualquer modal limpam alvo e prompt;
- `InteractionPromptView` apresenta apenas o prompt do alvo atual e nao executa regra de gameplay;
- `GameplayInputGate` centraliza os modos exclusivos `Inventory`, `Container`, `Dialogue` e `DevConsole`, com ownership do modal e captura/restauracao unica do cursor;
- abrir UI modal nao altera `Time.timeScale`; movimento, camera, ataque, dodge, lock-on, troca de arma e novas interacoes ficam bloqueados pelo mesmo gate;
- uma UI nao pode fechar ou roubar o modal pertencente a outra UI.

Pickups e ownership:

- `WeaponPickup` implementa o contrato generico e nao consulta mais teclado nem guarda player por trigger;
- Bronze Sword e Bronze Axe continuam usando seus `WeaponData`, quantidade e prefabs existentes;
- coletar chama somente `PlayerInventory.AddWeapon`, cria uma unica `ItemInstance` e desativa o pickup apenas depois da transferencia aceita;
- repetir a interacao no mesmo pickup nao cria outra instancia;
- `PlayerInventory.TryAdd`, como operacao generica de container, nao equipa nem ocupa slot automaticamente; o comportamento historico de pickup permanece no wrapper explicito `AddWeapon`;
- `ItemTransferService` move a mesma referencia e o mesmo ID entre dois `IItemContainer`, valida ownership antes/depois e faz rollback em falha;
- transferencias em lote sao atomicas do ponto de vista da UI: uma falha tenta devolver todas as instancias ja movidas;
- uma instancia equipada nao pode ser armazenada em outro container; primeiro deve ser desequipada;
- `PlayerEquipment` continua sendo a unica autoridade para slots e visual equipado.

Container, porta e NPC:

- `WorldItemContainer` possui um `ItemContainer` proprio, capacidade e seeds de definicao/quantidade;
- os seeds criam IDs somente no `Awake`; nenhum ID runtime fica serializado no prefab ou na cena;
- reiniciar Play Mode recria o conteudo inicial porque persistencia ainda nao existe;
- `ContainerTransferScreen` mostra inventario do player e container lado a lado, detalhes, feedback, selecao, `Take`, `Store`, `Take All`, duplo clique e drag-and-drop;
- fechar, sair do alcance, morrer ou desativar/destruir o container encerra a tela e libera seu modal sem perder ownership;
- `SimpleDoorInteractable` alterna abrir/fechar por uma coroutine curta, bloqueando reentrada durante a animacao;
- `SimpleNpcInteractable` abre um `DialoguePanel` simples com nome e uma fala; branching, quest e AI social nao foram adicionados;
- visuais de bau, porta e NPC sao primitives organizadas sob roots controlados por scripts de `Assets/_Project`.

Assets e integracao:

- `Assets/_Project/Prefabs/UI/InteractionPrompt.prefab` e o prompt reutilizavel;
- `Assets/_Project/Prefabs/UI/ContainerTransferScreen.prefab` e a tela reutilizavel de transferencia;
- `Assets/_Project/Prefabs/UI/DialoguePanel.prefab` e o painel reutilizavel de dialogo;
- `Assets/_Project/Prefabs/World/WorldChest.prefab`, `SimpleDoor.prefab` e `NpcPlaceholder.prefab` sao os placeholders de mundo;
- materiais placeholder ficam em `Assets/_Project/Materials/Interaction` e usam shader URP Lit;
- `CombatSandbox` possui as seis novas roots `InteractionPrompt`, `ContainerTransferScreen`, `DialoguePanel`, `WorldChest`, `SimpleDoor` e `NpcPlaceholder`;
- o `UIEventSystem` existente foi reaproveitado; a cena continua com exatamente um EventSystem e um AudioListener;
- nenhum ZIP de `Downloads` foi necessario ou importado nesta etapa, portanto nao houve nova dependencia externa nem arquivo de licenca adicional;
- `Tools/Combat Sandbox/Build World Interaction Foundation` reconstrui apenas esta feature, recusa Play Mode ou cena ativa dirty e procura somente roots exatas;
- o builder legado `CombatSandboxSceneBuilder` nao foi executado nem alterado.

Validacao realizada:

- Play Mode confirmou prompt e alvo generico, coleta unica dos dois pickups e IDs/durabilidade distintos;
- MainHand/OffHand, troca livre, retomada depois da acao e travas durante ataque/dodge continuaram funcionando;
- selecao, botoes, duplo clique, drag-and-drop nos dois sentidos e `Take All` preservaram referencia, ID e ownership sem duplicacao;
- tentativa de armazenar arma equipada falhou com `ItemEquipped` e sem mudar as contagens;
- container abriu com cursor visivel, bloqueou ataque/dodge e impediu um segundo modal; desativar o container fechou a UI e restaurou `Locked/hidden`;
- porta abriu e fechou, atualizou o prompt e rejeitou reentrada durante as duas animacoes;
- NPC abriu dialogo, mostrou speaker/fala, manteve o mundo rodando e bloqueou outro modal;
- Skeleton e Golem mantiveram AI, health e hitboxes e foram instanciados em Play Mode;
- a tela do container e o dialogo foram inspecionados no Game View `WXGA (1366x768)` sem sobreposicao ou corte estrutural;
- 26 prefabs, 791 GameObjects de prefab e a cena foram varridos com `0` scripts ausentes e `0` referencias quebradas;
- Play Mode e a rodada final de Editor terminaram com `0 errors` e `0 warnings` do projeto;
- `Assets/SazenGames` continua presente e nao foi alterada.

Continuam fora de escopo: save/persistencia do container, banco de dados, servidor/multiplayer, respawn ou reposicao de loot, regras de roubo e afinidade, crafting, peso, lojas/bancos, containers compartilhados, loot de cadaver, portas trancadas/chaves, quest branching, AI social, animacoes humanoides do NPC e arte/audio/VFX finais.

## Atualizacao 2026-07-11 - Save System Local v1 (historico)

A quarta etapa adicionou persistencia local deliberadamente pequena para o sandbox. Esta secao registra o contrato original do schema v1; a secao de Quest System abaixo descreve o schema atual e substitui as afirmacoes de versao corrente desta etapa.

Arquitetura e contrato:

- `SaveGameService`, presente como root unica da cena `CombatSandbox`, e a autoridade central para snapshot, validacao, escrita, load, delete, status e diagnostico;
- o formato original desta etapa usava `schemaVersion = 1`; o formato atual e a migracao registrada estao documentados na secao de Quest System;
- `SaveGameDtos` contem somente numeros, booleans, strings, listas e DTOs compostos por esses valores; nao ha `UnityEngine.Object`, `Transform`, `ScriptableObject`, prefab, instance ID da Unity ou referencia runtime no JSON;
- os arquivos ficam em `Application.persistentDataPath` com os nomes `combat-sandbox-save.json`, `combat-sandbox-save.tmp` e `combat-sandbox-save.bak`;
- o save escreve e faz flush do `.tmp`, desserializa e valida esse arquivo, e somente entao usa substituicao atomica do arquivo principal, mantendo o principal anterior como `.bak`;
- o load tenta primeiro o principal e usa o backup somente se leitura, JSON ou validacao semantica do principal falhar; arquivo corrompido nunca e sobrescrito durante load;
- `ItemDefinitionRegistry.asset`, em `Assets/_Project/Data/Items`, registra explicitamente Bronze Sword e Bronze Axe por `definitionId`; IDs vazios, repetidos ou desconhecidos invalidam a operacao;
- `ItemDefinition.DefinitionId` nao usa mais o nome do asset como fallback silencioso;
- `ItemInstance.TryRestore` recria explicitamente o mesmo ID, definicao, quantidade, durabilidade, qualidade, raridade, criador, afinidade, flag de roubado e dono original, sem reflection e sem gerar outro GUID;
- `ItemContainer.TryReplaceContents`, `PlayerInventory.TryRestoreItems`, `PlayerEquipment.TryRestoreState` e `WorldItemContainer.TryRestoreContents` aplicam lotes validados sem passar por pickups, autoequip ou transferencias unitarias;
- equipamento continua guardando apenas IDs de instancias pertencentes ao inventario; o load restaura inventario primeiro, depois slots/arma ativa, e reconstrui o visual equipado uma vez;
- uma mesma `ItemInstance` continua sem poder ocupar dois owners ou dois slots, e a validacao do save tambem exige unicidade global de todos os `instanceId` entre player e containers.

Estado persistido no schema v1 legado:

- posicao e rotacao do player;
- vida jogavel e stamina atuais;
- `lastSanctuaryId` preparado, atualmente com o valor unico `combat_sandbox.sanctuary.default` e sem implementar respawn regional;
- todas as instancias do inventario, incluindo seus metadados preparados para sistemas futuros;
- os 12 registros estruturais de equipamento, `activeWeaponSlot` e `activeItemInstanceId`;
- conteudo completo do `WorldChest`, inclusive lista vazia, e a flag `initializedOrRestored`;
- estado coletado dos pickups de Bronze Sword e Bronze Axe;
- estado aberto/fechado da `SimpleDoor`.

Identidade persistente de mundo:

- `PersistentWorldId` existe somente nas instancias relevantes da cena, nao nos prefabs, evitando que futuras copias de um prefab nascam com o mesmo ID;
- IDs atuais: `combat_sandbox.container.world_chest`, `combat_sandbox.pickup.bronze_sword`, `combat_sandbox.pickup.bronze_axe` e `combat_sandbox.door.simple_door`;
- a validacao detecta participantes sem ID, IDs vazios, duplicados, desconhecidos no save e participantes persistentes ausentes da cena;
- `Tools/Combat Sandbox/Validate Persistence Foundation` executa essa verificacao no Editor; o menu de contexto de `PersistentWorldId` gera um novo ID estavel quando uma instancia de cena e duplicada;
- pickups coletados nao desativam mais o root persistente: visual e collider sao desativados, enquanto o componente permanece localizavel para load e pode ser restaurado;
- `WorldItemContainer.HasRestoredState` distingue uma restauracao runtime dos seeds criados no `Awake`; um container salvo vazio permanece vazio depois do load.

Seguranca do load:

- todo o documento, registry, bindings, world IDs, numeros, definicoes, IDs globais, capacidades, ownership, slots e arma ativa sao validados e materializados fora do mundo antes da primeira mutacao;
- imediatamente antes de aplicar, inventario, container, dialogo e dev console sao fechados; selecao/prompt, ataque, dodge, lock-on e action lock da animacao sao limpos; o input fica suprimido por dois frames;
- antes de mutar o mundo, a pose salva do `CharacterController` e testada contra colliders estaticos e contra a pose salva da porta; coordenadas extremas e penetracao em geometria sao rejeitadas;
- teleport desabilita temporariamente o `CharacterController` e restaura seu estado anterior;
- `PlayerHealth.RestoreHealth` cancela coroutines de hit/morte e `PlayerStamina.RestoreStamina` reinicia o pequeno delay de regeneracao quando necessario;
- o servico captura um snapshot de rollback antes de aplicar; retornos de falha e excecoes inesperadas de visual, fisica ou listeners tentam reconstruir esse estado anterior, e excecoes do proprio rollback tambem viram status controlado;
- vida zero durante a janela de morte nao e uma condicao persistente neste MVP: snapshots sao limitados ao minimo jogavel de `1 HP`.

Ferramentas de desenvolvimento:

- o Dev Console possui a aba `Persistence` com `Save`, `Load`, `Delete Save`, `Show Folder`, `Print Summary`, `Validate World IDs` e `Validate Registry`;
- delete exige um segundo clique de confirmacao em ate cinco segundos;
- a aba mostra schema, existencia de principal/backup, `updatedAt`, contagens atuais de itens do player, containers e objetos persistentes, ultima operacao, fonte do ultimo load, status, erro e caminho;
- autoload existe apenas como campo preparado e permanece desligado na cena; nenhum load automatico acontece por padrao;
- `Tools/Combat Sandbox/Build Persistence Foundation` cria/atualiza somente registry, servico e IDs desta feature, recusa Play Mode ou cena dirty e nao executa nem altera `CombatSandboxSceneBuilder`.

Validacao realizada:

- primeira escrita criou somente o principal; a segunda criou backup e removeu o temporario;
- no mesmo Play Mode, quatro instancias com IDs distintos, durabilidade `73/100`, MainHand/OffHand, arma ativa, vida, stamina, transform, dois pickups, porta aberta e bau vazio foram mutados e restaurados exatamente;
- sair e entrar novamente em Play Mode confirmou que o load recria novas referencias `ItemInstance` preservando os mesmos IDs e nao depende de referencias do processo anterior;
- outro round-trip entre Play Modes preservou ownership exclusivo de dois itens no player e um item real no bau, pickup de Sword coletado, pickup de Axe ainda disponivel e OffHand ativa;
- a instancia do bau preservou o mesmo ID, durabilidade `55/100`, qualidade `Fine`, raridade `Rare`, criador, afinidade, flag de roubado e dono original nao vazios;
- JSON principal propositalmente corrompido caiu para o backup valido e informou `Backup` como fonte;
- definicao desconhecida, ID global duplicado, item em mais de um slot, schema futuro e world ID desconhecido foram rejeitados sem alterar o player ou o mundo;
- IDs de mundo vazios/duplicados e `definitionId` duplicado no registry foram detectados e o estado valido foi confirmado novamente depois do teste;
- load durante ataque, dodge, inventario, container, dialogo e dev console cancelou/fechou o estado transitorio e deixou `GameplayInputGate` em `None`;
- uma posicao dentro do collider do `WorldChest` foi rejeitada nos dois arquivos sem mover o player; uma posicao no vao da porta salva aberta carregou corretamente quando a cena iniciou com a porta fechada;
- uma porta temporariamente inativa foi localizada, restaurada e manteve a pose aberta ao ser ativada novamente;
- na validacao da etapa v1, `Print Summary` confirmou schema v1, quatro itens do player, dois slots ocupados, um container vazio, dois pickups e uma porta;
- a rodada final no schema definitivo confirmou tres itens do player, bau vazio marcado como inicializado/restaurado, quatro objetos persistentes, `updatedAt` valido, exatamente um EventSystem e um AudioListener;
- Skeleton e Golem foram instanciados em Play Mode e mantiveram AI, health e hitboxes esperados;
- 26 prefabs, 791 GameObjects de prefab e 398 GameObjects da cena foram varridos com `0` scripts ausentes e `0` referencias quebradas;
- Play Mode e a verificacao final do Editor terminaram com `0 errors` e `0 warnings`; o Editor ficou fora de Play Mode, com `CombatSandbox` salva e limpa;
- nenhum asset externo dos ZIPs foi necessario ou importado nesta etapa;
- `Assets/SazenGames` continua preservada e nao foi alterada.

Na etapa v1 ficaram fora de escopo: multiplos perfis/slots de save, autosave, checkpoint, save de inimigos ou bosses, NPC/quest, cooldowns e estados transitorios, configuracoes do dev console, criptografia, compressao, cloud, banco de dados, servidor/multiplayer, migracoes de schemas antigos, respawn/reposicao de loot, crafting, peso, lojas/bancos, containers compartilhados, loot de cadaver e regras completas de afinidade/roubo. Quests e a migracao `v1 -> v2` foram adicionadas na etapa seguinte.

## Atualizacao 2026-07-12 - Quest System MVP e Save schema v2

Esta etapa implementa o ciclo jogavel completo de `Problemas com Esqueletos` sobre os sistemas existentes de interacao, combate, inventario, UI modal e persistencia. A implementacao imediata continua pequena e local ao sandbox; reputacao, guildas, quests procedurais, multiplayer e narrativa ramificada continuam futuros.

Arquitetura atual:

- `QuestDefinition` e um `ScriptableObject` data-driven com `questId` estavel, titulo, descricao, giver/turn-in por ID, lista ordenada de objetivos, textos de dialogo e recompensa;
- definicao e estado possuido pelo jogador ficam separados: `QuestRuntimeState`, `QuestProgressSnapshot` e `QuestObjectiveProgressSnapshot` guardam somente estado runtime e dados serializaveis;
- `PlayerQuestLog` e a autoridade de aceite, progresso, tracking, entrega, reward pendente, snapshots e restore; UI e dialogo chamam suas APIs e nao escrevem no estado diretamente;
- estados suportados: `Unavailable`, `Available`, `Active`, `ReadyToTurnIn` e `Completed`;
- objetivos suportados: `TalkToNpc`, `DefeatTarget`, `OwnItem` e `InteractWithObject`; a quest demonstrativa usa derrota e conversa de retorno;
- `QuestTargetIdentity` usa IDs estaveis de tipo e instancia, sem depender de nome, texto ou hierarquia; morte valida vem do evento real de `TrainingDummyHealth` e exige attacker pertencente ao player;
- cada ID de instancia derrotada e persistido no progresso e conta no maximo uma vez; stagger, fade e uma segunda chamada de morte nao geram credito;
- `PlayerInteractor.InteractionSucceeded` publica progresso somente depois de uma interacao aceita; `TryInteract` concentra a mesma validacao reutilizada pelo caminho da tecla `E`;
- `QuestEncounterController` e event-driven e mantem os tres skeletons da quest inativos antes do aceite, ativos somente no objetivo de derrota e novamente inativos depois dele; a retirada e diferida por um frame para preservar a sequencia visual de morte;
- o hit-stop de combate roda no host persistente `CombatHitStop`, independente do inimigo atingido, para que desativar um encounter durante uma morte nunca deixe `Time.timeScale` reduzido;
- `DialoguePanel` continua sendo o painel existente e agora apresenta aceitar, recusar, estado ativo, concluir, feedback de inventario cheio e texto posterior a conclusao;
- `QuestJournalScreen` e `QuestTrackerHud` consomem eventos do log; nao fazem polling de gameplay nem possuem regra de progressao;
- o Dev Console ganhou a aba `Quests` para listar, iniciar, avancar, resetar e inspecionar tracking, explicitamente como ferramenta de desenvolvimento.

Missao e recompensa:

- asset: `Assets/_Project/Data/Quests/SkeletonTrouble.asset`;
- ID: `quest.problemas_com_esqueletos`;
- NPC giver/turn-in: `npc.combat_sandbox.aldeao`, com instancia `combat_sandbox.npc.quest_giver`;
- objetivo 1: eliminar tres alvos `enemy.skeleton`; as instancias da cena usam IDs `combat_sandbox.enemy.quest_skeleton_01` ate `_03`;
- objetivo 2: retornar e conversar com o mesmo NPC;
- recompensa: `3x Fragmento de Osso`, definida por `Assets/_Project/Data/Items/BoneFragment.asset` com ID `material.bone_fragment` e registrada no `ItemDefinitionRegistry`;
- a recompensa recebe um novo `ItemInstance` com GUID reservado. Inventario cheio mantem a quest em `ReadyToTurnIn` com `PendingDelivery`; um novo dialogo tenta entregar o mesmo ID;
- a quest somente vira `Completed/Granted` depois da transferencia aceita. Item reservado ja presente e reconhecido, enquanto nova conversa ou reload depois de `Granted` nao duplica a recompensa.

UI, teclas e fluxo manual:

- `E`: conversar com o aldeao, aceitar/recusar e voltar para concluir;
- `J`: abrir/fechar o diario; `Esc` e o botao `X` tambem fecham;
- o diario lista ativas e concluidas, mostra descricao, objetivos, progresso e recompensa e permite rastrear/parar de rastrear uma quest ativa;
- o tracker mostra somente a quest rastreada e o objetivo atual;
- diario e dialogo usam `GameplayInputGate` como modais exclusivos. Enquanto abertos, inventario, movimento de gameplay, ataque, dodge, lock-on, troca de arma e outra interacao nao podem disputar input;
- `F2 > Quests`: ferramentas de teste; o fluxo normal nao depende delas.

Persistencia atual:

- `SaveGameService.CurrentSchemaVersion` agora e `2`;
- schema v2 salva quest ID, estado, objetivo atual, progresso por `objectiveId`, IDs de alvos creditados, quest rastreada e reserva/status da recompensa;
- `TryMigrateToCurrentSchema` registra a migracao explicita `v1 -> v2`: todo o restante do save v1 e preservado e o quest log nasce vazio, sendo normalizado para o estado inicial das definicoes atuais;
- quests removidas/desconhecidas sao preservadas como registros orfaos e ignoradas pela UI; tracking desconhecido e limpo sem invalidar o documento inteiro;
- o load prepara e valida o quest log antes de mutar o mundo, aplica quests por ultimo e inclui seu snapshot no rollback;
- load nunca concede recompensa. Entrega pendente continua pendente ate uma chamada explicita de turn-in;
- escrita atomica, `.tmp`, `.bak`, fallback do principal corrompido, IDs de itens, ownership e demais participantes v1 permanecem no mesmo servico.

Arquivos e assets principais:

- dominio: `Assets/_Project/Scripts/Quests/QuestDefinition.cs`, `QuestRuntimeModels.cs`, `QuestDefinitionRegistry.cs`, `PlayerQuestLog.cs`, `QuestSignalHub.cs`, `QuestTargetIdentity.cs`, `QuestEncounterController.cs` e `QuestGiver.cs`;
- UI: `Assets/_Project/Scripts/UI/QuestJournalScreen.cs`, `QuestTrackerHud.cs` e a evolucao de `DialoguePanel.cs`;
- persistencia: `Assets/_Project/Scripts/Persistence/SaveGameDtos.cs` e `SaveGameService.cs`;
- prefabs: `Assets/_Project/Prefabs/UI/QuestJournalScreen.prefab`, `QuestTrackerHud.prefab`, `DialoguePanel.prefab`, `PlayerPlaceholder.prefab`, `NpcPlaceholder.prefab` e `SkeletonEnemy.prefab`;
- registries: `Assets/_Project/Data/Quests/QuestDefinitionRegistry.asset` e `Assets/_Project/Data/Items/ItemDefinitionRegistry.asset`;
- `Tools/Combat Sandbox/Build Quest System MVP` cria/atualiza apenas a feature, preserva outras definicoes nos registries, recusa Play Mode/cena dirty e nao executa `CombatSandboxSceneBuilder`;
- os builders de World Interaction e Persistence foram endurecidos para preservar os componentes/IDs de quest independentemente da ordem de reexecucao.

Validacao realizada:

- Play Mode confirmou recusar, conversar novamente, aceitar, tracking automatico e ativacao dos tres skeletons somente depois do aceite;
- a tecla `J` foi exercitada pelo Input System; diario aberto bloqueou ataque, dodge e inventario e fechou liberando o modal;
- tres mortes reais de `TrainingDummyHealth` produziram `1/3`, `2/3`, `3/3`; repetir dano no primeiro morto nao alterou progresso nem IDs creditados;
- a conversa de retorno pelo `PlayerInteractor` mudou para `ReadyToTurnIn` e atualizou o dialogo para entrega;
- inventario `24/24` manteve recompensa `PendingDelivery` com o mesmo GUID atraves de save/load; liberar um slot entregou exatamente uma instancia de quantidade `3`;
- nova entrega, nova conversa e reload depois de `Completed/Granted` nao criaram outra recompensa;
- save/load foi exercitado em `Available`, `Active 1/3` com ID creditado e tracking, `ReadyToTurnIn`, `PendingDelivery` e `Completed/Granted`;
- um JSON real com `schemaVersion = 1` carregou como v2, iniciou a quest em `Available` e preservou todos os IDs de `ItemInstance`;
- uma quest removida desconhecida foi preservada como orfa sem quebrar a quest conhecida, e tracking desconhecido foi limpo;
- principal corrompido caiu para o backup valido, e o save limpo schema v2 foi restaurado depois do teste;
- Skeleton e Golem foram instanciados com AI, health, roots de fisica e hitboxes esperados;
- os builders Quest, World Interaction e Persistence foram reexecutados em ordens diferentes e os validators continuaram passando, com NPC, registry e encounter preservados;
- 28 prefabs, 831 GameObjects de prefab e 691 GameObjects da cena foram varridos sem scripts ausentes ou referencias quebradas; a cena manteve exatamente um EventSystem e um AudioListener;
- nenhum ZIP externo foi necessario ou importado; `Assets/SazenGames` continua presente e inalterada;
- a rodada final terminou fora de Play Mode, com a cena limpa e sem errors ou warnings do projeto no Console.

Continuam fora de escopo: falha e limite de tempo, branching/editor visual de dialogo, reputacao, guildas, quests procedurais/diarias, compartilhamento, party/multiplayer/backend, marcadores de mapa, cutscenes/voice acting, economia, crafting/profissoes, achievements, save de inimigos mortos, respawn generico, arte/audio/VFX finais e multiplos perfis de save.
