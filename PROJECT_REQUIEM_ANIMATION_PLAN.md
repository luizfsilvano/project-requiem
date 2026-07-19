# Project Requiem — Plano de Animações

## Objetivo

Registrar o comportamento esperado do personagem e o mapeamento inicial das animações disponíveis na Universal Animation Library 1 e 2.

Os pacotes analisados foram:

- `Universal Animation Library[Pro].zip`;
- `Universal Animation Library 2[Source].zip`.

As versões `_RM` dos arquivos possuem root motion. As versões normais não possuem root motion.

## Locomoção inicial

### Estado parado

- Estado padrão: `Idle_Loop`.
- Variação ocasional: `Idle_LookAround_Loop`.
- A variação pode ser disparada após um intervalo ou por um tempo aleatório controlado. O custo de um timer simples é irrelevante para o desempenho neste contexto.

### Corrida em Jog

Fluxo desejado:

```text
Idle ↔ Jog direcional
```

- Usar as variações `Jog_*_Loop` em todas as oito direções, incluindo `Jog_Fwd_Loop` para frente.
- Não usar `Sprint_Enter`, `Sprint_Loop` ou `Sprint_Exit` neste fluxo.
- A transição visual entre `Idle` e `Jog` é direta.
- A velocidade inicial de referência é `500 uu/s` e deve permanecer ajustável no `CharacterMovement`.
- A aceleração e a desaceleração devem ser curtas, mas não instantâneas, para preservar resposta sem criar uma troca física brusca.
- Durante a desaceleração, o Jog permanece até a velocidade planar chegar perto de zero; só então entra em Idle.
- A animação não deve controlar a velocidade real do personagem.
- As animações sem root motion devem ser usadas para locomoção.

### Direções

O sistema deve permitir movimento em oito direções, incluindo diagonais. A direção real deve ser controlada pelo input e pelo `CharacterMovement`, enquanto a animação deve acompanhar a velocidade e a direção do personagem.

### Pulo

Fluxo desejado:

```text
Jump_Start → Jump_Loop → Jump_Land → Idle  (aterrissagem parada)
                      └→ Jog               (aterrissagem em movimento)
```

Não há, neste momento, uma animação específica de queda. `Jump_Loop` cobre o estado aéreo até a aterrissagem.
`Jump_Land` só deve tocar se o personagem aterrissar parado. Se houver movimento na aterrissagem, ou se o jogador voltar a se mover durante `Jump_Land`, a transição deve ir diretamente para o Jog direcional.

### Esquiva

- Animação: `Roll`.
- Usar a versão root motion de `UAL1_RM`, isolada no asset
  `/Game/ProjectRequiem/Characters/Player/Animations/Locomotion/UAL1_RM/Roll`.
- Tecla: `Shift`, disparada somente no início do input.
- A direção é capturada uma única vez quando a esquiva é aceita. O input direcional
  posterior não altera a trajetória; sem direção, usa-se a frente do personagem.
- O personagem gira para a direção capturada antes do início do root motion e essa
  orientação permanece comprometida até o fim da animação.
- A esquiva não pode ser interrompida por input. Ataque, pulo, entrada em agachamento
  e uma nova esquiva ficam bloqueados durante todo o clipe. Dano letal recebido fora
  dos i-frames é a exceção explícita e inicia a morte imediatamente.
- A janela central entre `0.25` e `0.65` normalizado concede i-frames. O contrato
  `URequiemDodgeComponent::ShouldIgnoreIncomingDamage` deve ser consultado por
  futuros ataques; o fallback de dano genérico da Unreal também retorna zero nessa janela.
- O asset conclui o deslocamento root motion em aproximadamente `0.59` normalizado.
  O root motion permanece comprometido até `0.62`, com uma margem após a última key;
  então o `CharacterMovement` volta a aceitar input preservando a velocidade corrente
  e usando sua própria aceleração/frenagem. Havendo input de movimento nesse recovery,
  a apresentação cruza em `0.05s` diretamente do `Roll` para o `Jog` direcional e o
  mantém através do fim da ação, evitando deslizamento e reinício do loop. Sem input,
  o `Roll` conclui normalmente. Ataque, pulo, agachamento, outra esquiva e a orientação
  capturada continuam bloqueados até o fim do relógio da ação, independentemente do
  clipe que estiver ocupando o slot visual.
- A esquiva funciona sem alterar os estados `Normal`, `CombatUnarmed` e `CombatSword`; ao terminar,
  a apresentação retorna ao estado de locomoção/postura compatível com o modo preservado.

### Agachamento

- Entrada: `Crouch_Enter`.
- Saída: `Crouch_Exit`.
- Tecla: `Ctrl` em hold; pressionar entra, manter permanece agachado e soltar sai.
- Parado, `Crouch_Enter` e `Crouch_Exit` podem concluir normalmente. Se surgir intenção de movimento, a apresentação entrega em até dois updates para o loop agachado direcional ou para `Jog`, evitando deslizamento durante os one-shots.
- Usar as variações direcionais disponíveis:
  - `Crouch_Fwd_L_Loop`;
  - `Crouch_Fwd_Loop`;
  - `Crouch_Fwd_R_Loop`;
  - `Crouch_Bwd_L_Loop`;
  - `Crouch_Bwd_Loop`;
  - `Crouch_Bwd_R_Loop`;
  - `Crouch_Left_Loop`;
  - `Crouch_Right_Loop`.

## Combate sem armas — primeira etapa

O jogador começa em postura normal. O passe desarmado pode ser ativado por:

- ataque com o botão esquerdo;
- lock-on pelo botão do meio do mouse;
- recebimento de dano.

O passe desarmado estabeleceu `Normal` e `CombatUnarmed`; o lote de espada acrescenta
`CombatSword`, descrito abaixo. `Z` agora alterna entre o estilo com espada e o desarmado,
sem sair do combate. Sem espada equipada, o botão esquerdo entra automaticamente em
`CombatUnarmed` antes do primeiro golpe; dano aceito e lock-on preservam `CombatSword`
quando ele já está ativo e, nos demais casos, usam a entrada desarmada validada.

Fluxo de entrada:

```text
Normal → PunchKick_Enter → Combat Idle
```

Como UAL1 e UAL2 não possuem um idle desarmado dedicado, `CombatUnarmed_Idle_Loop` reutiliza de forma estática a pose final de `PunchKick_Enter`. Essa pose também coincide com os limites dos golpes e de `PunchKick_Exit`.

`PunchKick_Enter` e `PunchKick_Exit` são transições de postura exclusivamente paradas e opcionais: exigem ausência de intenção de movimento e velocidade planar efetivamente zerada no início, e só concluem se o jogador permanecer parado. Se o estado mudar enquanto o personagem anda ou ainda freia, a locomoção direcional continua visível e a transição é pulada naquele acionamento. Se movimento, pulo, agachamento, esquiva, reação de dano ou um ataque que assuma a apresentação surgir durante um desses clipes, ele é interrompido e descartado, sem reaparecer quando o personagem parar. Um ataque aceito tem prioridade sobre `PunchKick_Enter` e começa diretamente em `Punch_Cross`; um ataque durante `PunchKick_Exit` volta a `CombatUnarmed` e também começa direto.

Combo planejado:

```text
Punch_Cross
→ Punch_Jab
→ Melee_Knee
→ Melee_Knee_Rec
→ Melee_Hook
→ Melee_Hook_Rec
→ Melee_Uppercut
```

O combo recebe no máximo cinco comandos de ataque. `Melee_Knee_Rec` e `Melee_Hook_Rec` são recuperações automáticas dos golpes anteriores, portanto a reprodução mantém a ordem completa dos sete clipes sem exigir cliques exclusivos para recuperação. Cada golpe ou recuperação que aceita continuação abre uma janela normalizada entre `0.30` e `0.85` e possui somente um slot de follow-up. O primeiro clique válido ocupa esse slot; cliques adicionais na mesma janela são descartados. Assim, spam contínuo sustenta o combo somente enquanto novos cliques alcançam novas janelas, sem criar backlog depois que o jogador para.

Para tornar a cadeia mais dinâmica sem editar os assets, golpes usam play rate inicial de `1.25x` e recuperações `1.35x`, ambos ajustáveis. O lock de movimento termina em `0.60` de cada golpe, sem encerrar o ataque ativo, sua janela ou o follow-up já aceito; recuperações permanecem livres para locomoção e o próximo golpe volta a aplicar seu próprio lock e avanço. Quando existe um follow-up confirmado, `Punch_Cross` e `Punch_Jab` podem fazer handoff a partir de `0.72` do clipe; as recuperações podem entregar o próximo golpe a partir de `0.55`. `Melee_Knee` e `Melee_Hook` entram nas respectivas recuperações a partir de `0.90`. Sem follow-up, a sequência termina na postura de combate, e `Melee_Uppercut` nunca reinicia automaticamente o combo.

Saída do contrato desarmado, atualmente sem binding manual dedicado:

```text
Combat → PunchKick_Exit → Normal
```

Sem armas, o jogador não pode bloquear. O contrato expõe a elegibilidade futura para encerrar o combate após 30 segundos sem atacar e longe de inimigos, mas nenhuma saída automática é executada nesta etapa. O dummy de validação não fornece percepção nem proximidade para esse contrato.

O lock-on básico procura uma vez o alvo válido mais próximo dentro de um cone à frente e
mantém o Medieval Combat Dummy como alvo enquanto ele estiver válido e dentro do alcance
de manutenção. O botão do meio alterna aquisição e liberação. Ataque e esquiva não
liberam o alvo; a trajetória e a orientação capturadas pelo `Roll` continuam prioritárias
até o fim da esquiva. Derrota, destruição, invalidação ou distância excessiva encerram o
lock sem forçar a saída de `CombatUnarmed`.

A câmera captura o pitch atual quando o lock começa e acompanha o ponto de foco do alvo
somente pelo yaw do controller. O personagem retoma a orientação para esse ponto pelo
`CharacterMovement` assim que nenhuma ação com prioridade própria estiver controlando o
yaw. Enquanto o lock estiver ativo, o look livre do mouse fica suspenso para não disputar
com o acompanhamento. Um decal circular amarelo fino, sem preenchimento, marca a base do
alvo; não há retículo, troca de alvo ou HUD elaborado neste passe.

Todas as animações deste passe usam as fontes UAL1/UAL2 sem root motion. O modo de combate e o combo não alteram os parâmetros globais de velocidade, aceleração ou desaceleração; o deslocamento continua pertencendo ao `CharacterMovement`. Em `CombatUnarmed`, o idle de combate aparece parado e a locomoção direcional existente permanece ativa enquanto nenhum golpe está comprometido. Um LMB aceito entra em `CombatUnarmed` e executa diretamente `Punch_Cross`, mesmo que `PunchKick_Enter` esteja tocando; o lock físico aplicado é o do próprio golpe. Durante cada `Attack`, esse lock termina em `0.60`; o restante do clipe continua comprometido visualmente e para o combo, mas já aceita locomoção. Cada golpe real substitui brevemente a velocidade planar por um avanço frontal de referência de `350 uu/s`, resolvido pelo próprio `CharacterMovement` com colisão, aceleração e frenagem; recuperações não criam um novo avanço nem relock de movimento. Para o alvo simples de validação, cada clipe de ataque consome uma única consulta ofensiva ao cruzar `0.40` do próprio relógio normalizado. Esse ponto não altera a janela de input, o handoff ou o unlock físico.

## Combate com espada — primeiro passe

`CombatSword` é o terceiro estado de gameplay. `IA_ToggleCombat` continua em `Z`: de
`Normal` ou `CombatUnarmed` equipa a espada e entra em `CombatSword`; de `CombatSword`
retorna a `CombatUnarmed`. A seleção é temporária e não representa inventário, slot ou
sistema de equipamento. Dano e lock-on preservam o estilo com espada já ativo, e o
`Roll` mantém esse estado durante e depois da esquiva.

As transições paradas usam `Sword_Enter`, `Sword_Idle` e `Sword_Exit` de
`/Game/ProjectRequiem/Characters/Player/Animations/Combat/Sword/UAL1`. Assim como no
desarmado, entrada e saída são opcionais: movimento ou outra apresentação prioritária
as descarta, e um ataque aceito pula `Sword_Enter` e começa diretamente no golpe.

Fluxo leve, importado de `.../Combat/Sword/UAL2`:

```text
Sword_Regular_A
→ Sword_Regular_A_Rec
→ Sword_Regular_B
→ Sword_Regular_B_Rec
→ Sword_Regular_C
```

LMB inicia a carga no press e resolve no release/cancel. Um hold menor que `0.65s`
solicita ataque leve; A, A_Rec, B e B_Rec abrem uma janela `0.30–0.85` com um único slot
de follow-up. A e B passam automaticamente às recuperações em `0.90`; com follow-up,
as recuperações entregam o próximo golpe a partir de `0.55`. Golpes e recuperações
permanecem full-body em `1.0x`, sem acelerar os assets. O avanço de `350 uu/s` continua
sob o `CharacterMovement`, e a consulta ofensiva única ocorre em `0.40`, com dano `35`,
alcance `180`, raio `55` e altura `70`.

O lock de movimento cobre o início e o impacto do golpe; A e B o preservam ao entregar
a `Sword_Regular_A_Rec` e `Sword_Regular_B_Rec`. Quando existe intenção de movimento
durante A_Rec, B_Rec ou a recuperação terminal de C, o full-body cruza gradualmente para
o `Jog` direcional em vez de ser cortado. `SwordRecoveryBlendTime` usa `0.15s` por padrão,
fica exposto para ajuste e recomenda a faixa de `0.10–0.20s`. Para respeitar as durações
distintas dos assets, o início do blend é calculado pela duração e pelo play rate do
clipe, de modo que sua conclusão e a liberação do controle ocorram em `0.75` normalizado.
Nas recuperações intermediárias, o relógio do combo continua ativo mesmo depois de o
`Jog` ocupar o slot: a janela tardia `0.75–0.85` permanece válida e um follow-up aceito
entra no próximo golpe full-body. C encerra a sequência depois do blend. A troca de estilo
é rejeitada enquanto o lock comprometido permanece ativo. Sem input de movimento, a
animação conclui normalmente, sem cancelamento abrupto.

Um hold de pelo menos `0.65s` dispara
`/Game/ProjectRequiem/Characters/Player/Animations/Combat/Sword/UAL1_RM/Sword_Attack_RM`
em `0.5x`. O asset usa root motion com `Anim First Frame`, sem force root lock e com
escala normalizada. Durante o one-shot comprometido, o AnimInstance usa
`RootMotionFromMontagesOnly`; movimento, novo ataque, pulo, agachamento, esquiva e troca
de estilo permanecem bloqueados. O hit único ocorre em `0.50`, causa dano `60` e não
aceita follow-up leve. Havendo intenção de locomoção na recuperação, o pesado usa o
mesmo blend ajustável calculado para concluir em `0.75`; o root motion permanece ativo e
o ataque continua comprometido durante o blend. Internamente, essa janela troca de
`RootMotionFromMontagesOnly` para `RootMotionFromEverything`, porque a UE remove o montage
em blend-out do ponteiro exclusivo de root motion; como o `Jog` não possui root motion,
somente o deslocamento ponderado do pesado continua sendo extraído. Ao fim, volta a
`IgnoreRootMotion`, encerra a sequência e devolve o controle.

O visual usa `SM_Sword_Bronze` em
`/Game/ProjectRequiem/Combat/Styles/Sword/Weapons`. Fora de `CombatSword`, a espada fica
visível em `Socket_Weapon_Back`, um socket mesh-only de `spine_03`; equipada, usa o socket
ajustado manualmente `Socket_Weapon_Hand_R` de `hand_r`. Os dois clipes de transição têm
`40` keys a `30 fps` e duração de `1.3s`. `Sword_Enter` mantém a espada nas costas até o
frame `21` (`0.700s`, `0.53846` normalizado) e só então a anexa à mão. `Sword_Exit`
mantém a espada na mão até o frame `15` (`0.500s`, `0.38462` normalizado), prende-a às
costas nesse contato e conclui o restante da animação já sem a arma na mão. Esses pontos
ficam expostos em `Combat > Sword > Presentation`. Se uma transição opcional for pulada
ou interrompida por uma reação externa, o attachment coerente com o estado de combate é
aplicado imediatamente.
A malha não possui colisão nem participa das consultas de hit. O mesmo diretório contém
`M_Sword_Bronze` e as três texturas próprias. Lock-on, esquiva e o combo desarmado
preservam seus contratos já validados.

## Reações de dano e morte — primeiro passe

Reações leves ou direcionais:

- `Hit_Head`;
- `Hit_Chest`;
- `Hit_Stomach`;
- `Hit_Shoulder_L`;
- `Hit_Shoulder_R`.

Ataques muito fortes:

- `Hit_Knockback`;
- usa a versão root motion de `UAL2_RM`, isolada em
  `/Game/ProjectRequiem/Characters/Player/Animations/Damage/UAL2_RM/Hit_Knockback`.

Morte:

- `Death01`;
- `Death02`.

`FRequiemDamageRequest` recebe uma região controlada e o `AnimInstance` escolhe o
clipe correspondente. O fallback genérico da Unreal usa `Chest`; futuros hitboxes
podem fornecer a região real sem mudar o contrato de vida. Dano forte orienta o
personagem para que o root motion de `Hit_Knockback` produza a direção mundial de
impacto pedida.

Os i-frames entre `0.25` e `0.65` continuam prioritários: dentro da janela, o dano
é ignorado antes de reduzir vida, entrar em combate ou disparar animação. Fora da
janela, dano não letal durante `Roll` é aplicado, mas sua reação espera o término da
esquiva. Dano letal fora dos i-frames encerra a esquiva e inicia imediatamente uma
única morte.

Durante reação, novos inputs de ação ficam bloqueados até o one-shot terminar. Em
morte, movimento, ataque, pulo, agachamento e esquiva permanecem bloqueados, o
`CharacterMovement` fica em `MOVE_None` e a pose final é mantida. Dano posterior
é ignorado. `ResetForTesting` e os comandos `RequiemTestDamage`,
`RequiemTestKill` e `RequiemTestReset` são recursos temporários para PIE; não
representam respawn final.

## Dummy de combate — apresentação de validação

O asset Medieval Combat Dummy instalado pelo Fab é uma `StaticMesh`: não possui
Skeleton, SkeletalMesh, AnimBP, AnimSequence, Montage ou Blueprint próprio. Ele é usado
somente como visual de `BP_PR_CombatDummy`, em escala `0.5`, com a colisão original
desativada e uma cápsula de gameplay no ator próprio.

Como não existe animação compatível, o primeiro feedback de hit é um tilt curto e
procedural do componente visual. A derrota inclina o mesmo componente e desativa a
cápsula até `ResetForTesting`. Eventos Blueprint separados para hit, derrota, reset e
telegraph permitem substituir essa apresentação quando existir um alvo esquelético,
sem acoplar a base C++ a uma animação inexistente.

O ataque do dummy é estacionário e explicitamente solicitado, com um windup antes da
consulta. Não há loop de ataque, locomotion, navmesh ou máquina de estados de IA. Os
comandos temporários `RequiemTestDummyAttack` e `RequiemTestDummyReset` permitem validar
reação, i-frames, morte e reset no PIE sem adicionar um inimigo completo.

## Interações — futuro

| Situação | Animação |
|---|---|
| Beber poção ou bebida | `Drink` |
| Investigar corpo, pegadas ou sacola | `Fixing_Kneeling` |
| Conversar | `Idle_Talking_Loop` |
| Abrir porta | `Interact` |
| Pegar item no chão | `PickUp_Kneeling` |
| Pegar item de mesa ou balcão | `PickUp_Table` |
| Consumir pedra ou item de buff | `Consume` |
| Abrir baú | `Chest_Open` |
| Aplicar bandagem | `Bandage_Loop` |

`Bandage_Loop` é uma animação contínua e deve permanecer ativa enquanto a ação estiver sendo executada.

## Regras técnicas

- Usar animações sem root motion para locomoção, corrida, agachamento e pulo comum.
- Reservar root motion para ações pontuais que precisam deslocar o personagem pela própria animação, como esquiva, knockback e o ataque pesado de espada.
- Durante o trecho de deslocamento comprometido de `Roll`, até `0.62`, o AnimInstance
  usa `RootMotionFromMontagesOnly`; depois retorna a `IgnoreRootMotion`. Se houver input,
  o slot já apresenta o `Jog` direcional sem root motion enquanto o relógio e os locks
  restantes da esquiva continuam independentes até `1.0`. `Hit_Knockback` também usa
  `RootMotionFromMontagesOnly` somente durante seu one-shot. `Sword_Attack_RM` faz o
  mesmo durante o ataque pesado comprometido; seu blend curto usa temporariamente
  `RootMotionFromEverything` para preservar apenas a contribuição ponderada do pesado.
  Fora dessas três ações, permanece
  preservado o controle do `CharacterMovement`; os combos desarmado e leve de espada
  continuam sem root motion.
- A velocidade real deve continuar sob responsabilidade do `CharacterMovement`.
- O Animation Blueprint deve reagir ao estado do personagem; não deve definir sozinho a velocidade de movimento.
- Implementar primeiro locomoção e estados gerais.
- Implementar combate, dano e interações em etapas separadas.
- O primeiro passe de espada é um estilo fechado de validação; não existe sistema de equipamento ou arquitetura geral de armas.
