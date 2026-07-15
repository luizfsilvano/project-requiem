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
- A versão root motion está disponível no `UAL1_RM.fbx`.
- Tecla reservada: `Shift`.
- A esquiva será implementada futuramente.

### Agachamento

- Entrada: `Crouch_Enter`.
- Saída: `Crouch_Exit`.
- Tecla: `Ctrl` em hold; pressionar entra, manter permanece agachado e soltar sai.
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

O jogador começa em postura normal. O modo de batalha pode ser ativado por:

- ataque com o botão esquerdo;
- lock-on futuro;
- tecla `Z`;
- recebimento de dano futuro.

Os estados de gameplay são `Normal` e `CombatUnarmed`. `Z` alterna entre eles e o botão esquerdo entra automaticamente em `CombatUnarmed` antes do primeiro golpe. Lock-on e recebimento de dano usam futuramente o mesmo ponto de entrada, mas não estão funcionais nesta etapa.

Fluxo de entrada:

```text
Normal → PunchKick_Enter → Combat Idle
```

Como UAL1 e UAL2 não possuem um idle desarmado dedicado, `CombatUnarmed_Idle_Loop` reutiliza de forma estática a pose final de `PunchKick_Enter`. Essa pose também coincide com os limites dos golpes e de `PunchKick_Exit`.

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

O combo recebe cinco comandos de ataque. `Melee_Knee_Rec` e `Melee_Hook_Rec` são recuperações automáticas dos golpes anteriores, portanto a reprodução mantém a ordem completa dos sete clipes sem exigir cliques exclusivos para recuperação.

Saída manual:

```text
Combat → PunchKick_Exit → Normal
```

Sem armas, o jogador não pode bloquear. O contrato expõe a elegibilidade futura para encerrar o combate após 30 segundos sem atacar e longe de inimigos, mas nenhuma saída automática é executada nesta etapa porque ainda não existem inimigos.

Todas as animações deste passe usam as fontes UAL1/UAL2 sem root motion. O modo de combate e o combo não alteram velocidade, aceleração ou desaceleração; o deslocamento continua pertencendo ao `CharacterMovement`. Em `CombatUnarmed`, o idle de combate aparece parado e a locomoção direcional existente permanece ativa durante movimento.

## Reações de dano e morte — futuro

Reações leves ou direcionais:

- `Hit_Head`;
- `Hit_Chest`;
- `Hit_Stomach`;
- `Hit_Shoulder_L`;
- `Hit_Shoulder_R`.

Ataques muito fortes:

- `Hit_Knockback`;
- usar a versão root motion disponível em `UAL2_RM.fbx` quando apropriado.

Morte:

- `Death01`;
- `Death02`.

O sistema futuro deverá escolher a reação com base na região e na direção do impacto.

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
- Reservar root motion para ações pontuais que precisam deslocar o personagem pela própria animação, como esquiva e knockback.
- A velocidade real deve continuar sob responsabilidade do `CharacterMovement`.
- O Animation Blueprint deve reagir ao estado do personagem; não deve definir sozinho a velocidade de movimento.
- Implementar primeiro locomoção e estados gerais.
- Implementar combate, dano e interações em etapas separadas.
- As animações de espada não fazem parte deste plano inicial e devem ser avaliadas quando o sistema de armas for planejado.
