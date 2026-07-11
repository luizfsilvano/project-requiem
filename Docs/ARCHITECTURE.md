# Arquitetura Inicial

## Organização de Pastas

A pasta principal do projeto fica em:

```text
Assets/_Project
```

Essa pasta deve conter tudo que é específico do jogo. Pacotes externos, samples e arquivos padrão da Unity devem ficar fora dela sempre que possível.

Estrutura inicial:

```text
Assets/_Project/
  Art/
  Audio/
  Materials/
  Prefabs/
  Scenes/
  Scripts/
  Settings/
```

## Scripts

Os scripts devem começar simples. Só criar subpastas quando existir necessidade real.

Estrutura sugerida para a primeira fase:

```text
Assets/_Project/Scripts/
  Player/
  Combat/
  Camera/
  Enemies/
  Core/
```

## Cenas

A primeira cena jogável deve ficar em:

```text
Assets/_Project/Scenes/CombatSandbox.unity
```

Evitar espalhar cenas de teste sem nome claro. Se surgirem experimentos, usar nomes explícitos, por exemplo:

```text
AnimationImportTest.unity
WeaponAttachmentTest.unity
```

## Prefabs

Tudo que for reutilizável deve virar prefab cedo, mas sem exagero.

Primeiros prefabs esperados:

- `PlayerPlaceholder`
- `TrainingDummy`
- `MainCameraRig`

## Princípios Técnicos

- Preferir C# simples e legível.
- Separar input/movimento/combate quando isso ajudar a entender o código.
- Evitar frameworks e dependências antes de provar necessidade.
- Usar componentes pequenos, mas não transformar tudo em microcomponentes.
- Manter nomes claros em inglês para classes, arquivos e GameObjects técnicos.
- Documentar decisões importantes em `Docs/DECISIONS.md` e `CODEX.md`.

## Camadas Futuras

Se o protótipo 3D for aprovado, a arquitetura deve crescer nessa ordem:

1. Player controller sólido.
2. Sistema simples de combate.
3. Pipeline de animação humanoide.
4. Sistema mínimo de armas/movesets.
5. Inimigos e boss.
6. UI básica.
7. Teste técnico de multiplayer.

Multiplayer não deve entrar antes do combate single-player estar gostoso.

