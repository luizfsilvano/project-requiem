# Project Requiem — Arquitetura e Fundação

## Objetivo deste documento

Este documento registra as decisões iniciais de arquitetura do Project Requiem para manter consistência entre as próximas etapas de desenvolvimento e entre diferentes threads de trabalho.

O projeto deve evoluir a partir de uma primeira experiência jogável que acompanha o personagem desde o nascimento até a escolha da classe inicial. A implementação deve preparar uma base reutilizável para o jogo futuro sem antecipar sistemas que ainda não fazem parte do MVP.

## Estado atual do projeto

O projeto começou como um template Third Person da Unreal Engine, recém-renomeado para Project Requiem. Os conteúdos de `ThirdPerson` e `Variant_Combat` são referências da Epic e não devem ser tratados como a arquitetura definitiva do jogo.

Todo conteúdo próprio deve ser criado dentro de:

```text
/Game/ProjectRequiem
```

O projeto deve preservar temporariamente os assets do template em seus namespaces originais, evitando misturar exemplos da Epic com conteúdo de produção.

## Decisões confirmadas

- A fundação será híbrida, usando C++ para estruturas robustas, contratos, componentes e sistemas-base.
- Blueprints serão usados para composição, conteúdo específico, apresentação, NPCs, fluxos narrativos e iteração rápida.
- O projeto terá inicialmente um único módulo C++: `ProjectRequiem`.
- A estrutura de pastas será criada de forma abrangente desde o início para evitar uma reorganização trabalhosa no futuro.
- O mapa de proficiência separado é válido e fará parte do planejamento do MVP.
- Nenhum sistema de gameplay será implementado durante o passe inicial de fundação.

## Estrutura de conteúdo

```text
Content/
└── ProjectRequiem/
    ├── Core/
    │   ├── Blueprints/
    │   │   ├── GameFramework/
    │   │   ├── Components/
    │   │   └── Interfaces/
    │   ├── Input/
    │   └── Data/
    ├── Characters/
    │   ├── Player/
    │   │   ├── Blueprints/
    │   │   ├── Animations/
    │   │   ├── Meshes/
    │   │   └── Materials/
    │   └── Shared/
    ├── NPCs/
    │   ├── Shared/
    │   ├── Family/
    │   └── Guild/
    ├── World/
    │   ├── Maps/
    │   │   ├── Dev/
    │   │   ├── Intro/
    │   │   └── Tests/
    │   ├── Environment/
    │   │   ├── Architecture/
    │   │   └── Props/
    │   └── LevelInstances/
    ├── UI/
    │   ├── Common/
    │   ├── Screens/
    │   ├── HUD/
    │   └── Styles/
    ├── Dialogue/
    │   ├── Blueprints/
    │   └── Data/
    ├── PlayerData/
    │   ├── Blueprints/
    │   └── Data/
    ├── Interaction/
    │   ├── Blueprints/
    │   ├── Components/
    │   ├── Interfaces/
    │   └── Data/
    ├── Guild/
    │   ├── Blueprints/
    │   └── Data/
    ├── Combat/
    │   ├── Blueprints/
    │   ├── Components/
    │   ├── Data/
    │   └── Styles/
    ├── Classes/
    │   ├── Blueprints/
    │   ├── Data/
    │   └── UI/
    ├── Audio/
    │   ├── Music/
    │   ├── SFX/
    │   └── Voice/
    ├── Cinematics/
    │   ├── Intro/
    │   └── Shared/
    ├── Art/
    │   ├── Materials/
    │   ├── VFX/
    │   └── Textures/
    ├── Tests/
    │   ├── Functional/
    │   └── Fixtures/
    └── Prototypes/
        ├── Maps/
        ├── Blueprints/
        └── Assets/
```

As pastas podem existir desde o início, mas os assets e sistemas só devem ser criados quando houver uma necessidade concreta.

## Arquitetura C++ e Blueprint

O módulo inicial será:

```text
Source/
├── ProjectRequiem.Target.cs
├── ProjectRequiemEditor.Target.cs
└── ProjectRequiem/
    ├── ProjectRequiem.Build.cs
    ├── Public/
    │   ├── Core/
    │   ├── Characters/
    │   ├── Components/
    │   ├── Data/
    │   └── Interaction/
    └── Private/
        ├── Core/
        ├── Characters/
        ├── Components/
        ├── Data/
        └── Interaction/
```

As primeiras classes-base previstas são:

- `URequiemGameInstance`: dados temporários que atravessam mudanças de mapa.
- `ARequiemGameModeBase`: regras gerais, classes padrão e spawn.
- `ARequiemPlayerController`: input, foco, cursor e alternância entre jogo e UI.
- `ARequiemCharacter`: movimento, câmera e pontos de extensão.
- `FRequiemPlayerIdentity`: nome, sexo e idade inicial.
- `ERequiemIntroStage`: etapa atual da introdução.

Essas classes devem começar pequenas. GameInstance, GameMode, Controller e Character não devem se tornar gerenciadores gerais de todos os sistemas.

## Organização dos mapas

O MVP será planejado com quatro mapas:

| Mapa | Responsabilidade |
|---|---|
| `L_Dev_Foundation` | Validar a fundação, input, possessão, câmera, PIE e packaging. |
| `L_Intro_Prologue` | Nascimento, criação simples do personagem e passagem de tempo. |
| `L_Intro_Village` | Casa, preparação, caminho, placas, NPCs, guilda, registro e transição. |
| `L_Intro_Proficiency` | Teste de proficiência, manequim e demonstração dos quatro estilos. |

Fluxo planejado:

```text
L_Intro_Prologue
    → L_Intro_Village
    → L_Intro_Proficiency
    → L_Intro_Village / Guilda
    → Escolha da classe
```

Nascimento e passagem de tempo serão representados futuramente por Level Sequences, como `LS_Intro_Birth` e `LS_Intro_TimePassage`. A criação do personagem e a escolha da classe serão estados de interface, não mapas separados.

## Princípios de sistemas

- Usar composição preferencialmente em vez de criar subclasses para cada classe ou estilo.
- Manter estilo de combate e classe como conceitos separados.
- Usar Data Assets para definições estáticas de estilos e classes.
- Manter estado de sessão separado de dados persistentes.
- Criar abstrações somente quando houver um segundo uso concreto.
- Não transformar o template da Epic em dependência do Core.
- Não implementar GAS, multiplayer, inventário, quests completas ou SaveGame durante a fundação.

Componentes como interação e combate serão criados somente nas etapas em que forem necessários. A fundação deve preparar os pontos de extensão, não antecipar o gameplay.

### Primeiro passe de combate desarmado

O primeiro uso concreto de composição de combate é `URequiemCombatComponent`, anexado a `ARequiemCharacter`. O componente é a fonte de verdade dos estados `Normal` e `CombatUnarmed`, recebe pedidos de entrada e ataque, usa `ReceivedDamage` como razão de entrada no lote atual e mantém somente o lock-on como ponto futuro. Para o combo desarmado, ele aceita somente um pedido inicial e um único follow-up por janela; cliques excedentes são descartados, nunca acumulados como uma fila de golpes futuros. Ele não implementa armas, vida, cálculo de dano, hitboxes, inimigos ou bloqueio.

`ARequiemCharacter` encaminha `IA_ToggleCombat` e `IA_PrimaryAttack` ao componente e ignora `IA_Move` durante os `60%` iniciais de cada golpe. O componente separa esse lock físico do ataque ativo e da janela de combo: o movimento pode retornar durante o follow-through sem cancelar a animação nem o follow-up, e cada golpe seguinte reaplica seu próprio lock. Em uma autoentrada ainda em movimento, o lock inicial permite que o `CharacterMovement` freie a velocidade herdada antes de `PunchKick_Enter`, sem acumular novo input. Movimento, colisão, velocidade, aceleração e frenagem continuam pertencendo ao `CharacterMovement`; o pequeno avanço de cada golpe é aplicado pelo componente como uma substituição curta da velocidade planar, sem root motion e sem alterar os parâmetros globais de locomoção.

`URequiemPlayerAnimInstance` observa o componente e cuida da apresentação e do relógio autoral: entrada, postura parada, combo, recuperações, saída, play rate e abertura da janela em tempo normalizado. O componente continua decidindo se o input cabe no slot disponível. Durante movimento sem um golpe ativo, a mesma máquina direcional de locomoção permanece responsável pela animação. `PunchKick_Enter` e `PunchKick_Exit` só assumem o `DefaultSlot` sem intenção e com velocidade planar efetivamente zerada; movimento durante esses one-shots devolve imediatamente a apresentação à locomoção e deixa a transição pendente para a próxima parada. Os demais clipes de combate também usam `DefaultSlot` sem root motion.

O componente mantém apenas a consulta de elegibilidade para uma futura saída automática após 30 segundos de inatividade e sem inimigos próximos. Não existe polling, inimigo ou saída automática nesta etapa.

### Primeiro passe de esquiva

`URequiemDodgeComponent`, anexado a `ARequiemCharacter`, é a fonte de verdade da
esquiva e permanece ortogonal a `Normal` e `CombatUnarmed`. Ele aceita apenas uma
esquiva aterrissada e não agachada, captura uma direção mundial imutável, mantém o
relógio normalizado da ação e expõe os locks, recovery e i-frames. O componente não
implementa stamina, hitboxes, inimigos nem a vida do personagem.

`ARequiemCharacter` encaminha `IA_Roll` no `Shift`, usa o input direcional atual
relativo à câmera ou a própria frente como fallback e bloqueia ataque, pulo,
entrada em agachamento e novas esquivas até o fim da ação. O asset encerra seu
deslocamento de raiz em aproximadamente `0.59`; root motion e movimento ficam
comprometidos até `0.62`, quando o root motion deixa de ser aplicado sem zerar a
velocidade corrente. O `CharacterMovement` volta a responder ao input com sua aceleração
e frenagem; se houver intenção de movimento, a apresentação cruza do `Roll` para o `Jog`
direcional em `0.05s` e preserva esse loop através do fim da ação, sem reiniciá-lo em
`1.0`. Sem input, o `Roll` conclui normalmente. A orientação capturada e os demais locks
permanecem até o fim em ambos os casos. `TakeDamage` retorna zero
durante os i-frames como proteção mínima, e futuros resolvedores de hit devem
consultar `ShouldIgnoreIncomingDamage` antes de aplicar impacto.

`URequiemPlayerAnimInstance` dá prioridade de gameplay à esquiva no `DefaultSlot`, usa
temporariamente `RootMotionFromMontagesOnly` e separa o relógio comprometido da
apresentação: após o recovery, um `Jog` visual nunca é usado para ressincronizar o tempo
do `Roll`. Ao fim, o slot permanece na locomoção já ativa ou retorna à postura compatível.
O asset vem de `UAL1_RM`; nenhum parâmetro global de velocidade, aceleração ou frenagem
do `CharacterMovement` é alterado.
O combo desarmado continua com os mesmos clipes, janelas e avanços já validados.

### Primeiro passe de dano e morte

`URequiemHealthComponent`, anexado a `ARequiemCharacter`, é a fonte de verdade de
`MaxHealth`, `CurrentHealth`, reação e morte. `FRequiemDamageRequest` é o contrato
estável para o fallback `TakeDamage`, testes controlados e futuros inimigos ou
hitboxes. O request já carrega quantidade, região (`Head`, `Chest`, `Stomach`,
`ShoulderLeft` ou `ShoulderRight`), força, direção mundial do impacto, tipo de dano
nativo da Unreal e override opcional da animação de morte. Não há GAS, atributos,
resistências, inimigos ou resolução ofensiva nesta etapa.

Os i-frames continuam pertencendo exclusivamente ao `URequiemDodgeComponent` e têm
prioridade sobre qualquer alteração de vida, entrada em combate ou reação. Dano não
letal recebido fora dos i-frames durante `Roll` reduz a vida e fica pendente até a
esquiva terminar, preservando o relógio e a trajetória validados. Dano letal fora da
janela é a única exceção que encerra a esquiva imediatamente. Todo dano aceito cancela
o pedido/step atual do combo por uma API externa explícita e entra em
`CombatUnarmed` com `ReceivedDamage`, sem alterar o fluxo normal do combo.

`URequiemPlayerAnimInstance` observa os serials do componente e escolhe `Hit_Head`,
`Hit_Chest`, `Hit_Stomach`, `Hit_Shoulder_L` ou `Hit_Shoulder_R` conforme a região.
Reações leves e `Death01`/`Death02` são in-place. Dano forte usa
`Hit_Knockback` de `UAL2_RM` com `RootMotionFromMontagesOnly` somente durante o
one-shot; o personagem é orientado para que o deslocamento autoral local resulte na
direção mundial pedida. Ao terminar, o controle volta a `IgnoreRootMotion`.

Morte é idempotente: a primeira transição para `Dead` zera e desabilita o
`CharacterMovement`, interrompe ataque e bloqueia movimento, pulo, agachamento,
ataque e esquiva. Novos danos são ignorados e não reiniciam a montagem. A pose final
da morte permanece pausada até `ResetForTesting`, que existe somente como apoio
temporário de validação PIE e restaura vida, `MOVE_Walking`, modo `Normal` e a
apresentação de locomoção.

## Convenções principais

```text
Mapa:              L_
Blueprint:         BP_
Component Blueprint: BPC_
Blueprint Interface: BPI_
Widget:            WBP_
Data Asset:        DA_
Data Table:        DT_
Struct:            ST_
Enum:              E_
Input Action:      IA_
Input Context:     IMC_
Level Sequence:     LS_
Material:          M_
Material Instance: MI_
Skeletal Mesh:      SKM_
Static Mesh:        SM_
Animation Blueprint: ABP
Sound Effect:        SFX_
Music:               MUS_
Voice:               VO_
```

Identificadores técnicos serão em inglês, sem espaços ou acentos. Textos exibidos ao jogador poderão ser localizados em português.

## Ordem de desenvolvimento

1. Configurar versionamento e proteção do projeto.
2. Criar o módulo C++ e as classes-base.
3. Criar o namespace `/Game/ProjectRequiem` e a estrutura de pastas.
4. Criar `L_Dev_Foundation`.
5. Configurar GameInstance, GameMode, PlayerController, Character e Enhanced Input próprios.
6. Validar PIE, possessão, movimento, câmera e packaging.
7. Criar o esqueleto dos quatro mapas e suas transições.
8. Implementar o perfil, prólogo, exploração, guilda, proficiência e escolha de classe, nessa ordem.

## Primeiro passe autorizado

O primeiro passe deve limitar-se à fundação técnica:

- adicionar C++ ao projeto;
- criar o módulo único `ProjectRequiem`;
- criar as classes-base vazias;
- criar a estrutura abrangente de pastas;
- criar `L_Dev_Foundation`;
- configurar os elementos próprios de GameInstance, GameMode, Controller, Character e Enhanced Input;
- validar compilação, PIE e packaging.

Não implementar ainda nascimento, criação de personagem, diálogos, interação, guilda, combate, inventário, quests ou escolha de classe.
