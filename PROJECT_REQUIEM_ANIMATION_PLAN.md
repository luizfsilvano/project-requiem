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

### Corrida

Fluxo desejado:

```text
Idle → Sprint_Enter → Sprint_Loop → Sprint_Exit → Idle
```

- `Sprint_Enter` inicia a corrida.
- `Sprint_Loop` permanece enquanto o personagem corre.
- `Sprint_Exit` deve ser usado ao parar depois de atingir a velocidade máxima ou ao inverter a direção em 180 graus.
- A desaceleração deve ser gradual e controlada pelo `CharacterMovement`, mesmo que isso cause um deslizamento visual intencional.
- Se o personagem ainda não atingiu a velocidade máxima, a transição para Idle também deve desacelerar gradualmente.
- A animação não deve controlar a velocidade real do personagem.
- As animações sem root motion devem ser usadas para locomoção.

### Direções

O sistema deve permitir movimento em oito direções, incluindo diagonais. A direção real deve ser controlada pelo input e pelo `CharacterMovement`, enquanto a animação deve acompanhar a velocidade e a direção do personagem.

### Pulo

Fluxo desejado:

```text
Jump_Start → Jump_Loop → Jump_Land
```

Não há, neste momento, uma animação específica de queda. `Jump_Loop` cobre o estado aéreo até a aterrissagem.

### Esquiva

- Animação: `Roll`.
- A versão root motion está disponível no `UAL1_RM.fbx`.
- Tecla reservada: `Shift`.
- A esquiva será implementada futuramente.

### Agachamento

- Entrada: `Crouch_Enter`.
- Saída: `Crouch_Exit`.
- Tecla planejada: `Ctrl`.
- Usar as variações direcionais disponíveis:
  - `Crouch_Fwd_L_Loop`;
  - `Crouch_Fwd_Loop`;
  - `Crouch_Fwd_R_Loop`;
  - `Crouch_Bwd_L_Loop`;
  - `Crouch_Bwd_Loop`;
  - `Crouch_Bwd_R_Loop`;
  - `Crouch_Left_Loop`;
  - `Crouch_Right_Loop`.

## Combate sem armas — futuro

O jogador começa em postura normal. O modo de batalha pode ser ativado por:

- ataque com o botão esquerdo;
- lock-on com o botão do meio do mouse;
- tecla `Z`;
- recebimento de dano.

Fluxo de entrada:

```text
Normal → PunchKick_Enter → Combat Idle
```

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

Saída manual:

```text
Combat → PunchKick_Exit → Normal
```

Sem armas, o jogador não poderá bloquear inicialmente. Se ficar 30 segundos sem atacar e longe de inimigos, o estado de batalha poderá ser encerrado automaticamente.

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

