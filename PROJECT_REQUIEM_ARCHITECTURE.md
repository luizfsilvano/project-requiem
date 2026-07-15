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

O primeiro uso concreto de composição de combate é `URequiemCombatComponent`, anexado a `ARequiemCharacter`. O componente é a fonte de verdade dos estados `Normal` e `CombatUnarmed`, recebe pedidos de entrada e ataque e expõe pontos futuros de entrada por lock-on e dano. Para o combo desarmado, ele aceita somente um pedido inicial e um único follow-up por janela; cliques excedentes são descartados, nunca acumulados como uma fila de golpes futuros. Ele não implementa armas, dano, hitboxes, inimigos ou bloqueio.

`ARequiemCharacter` encaminha `IA_ToggleCombat` e `IA_PrimaryAttack` ao componente e ignora `IA_Move` enquanto um golpe ou recuperação está comprometido. Movimento, colisão, velocidade, aceleração e frenagem continuam pertencendo ao `CharacterMovement`; o pequeno avanço de cada golpe é aplicado pelo componente como uma substituição curta da velocidade planar, sem root motion e sem alterar os parâmetros globais de locomoção.

`URequiemPlayerAnimInstance` observa o componente e cuida da apresentação e do relógio autoral: entrada, postura parada, combo, recuperações, saída, play rate e abertura da janela em tempo normalizado. O componente continua decidindo se o input cabe no slot disponível. Durante movimento sem um golpe ativo, a mesma máquina direcional de locomoção permanece responsável pela animação. Os clipes de combate usam `DefaultSlot` sem root motion.

O componente mantém apenas a consulta de elegibilidade para uma futura saída automática após 30 segundos de inatividade e sem inimigos próximos. Não existe polling, inimigo ou saída automática nesta etapa.

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
