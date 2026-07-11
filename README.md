# Unity Combat Sandbox

Protótipo 3D experimental para validar se Unity + MCP oficial é uma boa base para um ARPG online de ação.

Este projeto não substitui automaticamente o protótipo 2D em Godot. Ele existe para responder uma pergunta prática:

> A Unity facilita o fluxo de personagem 3D, animações humanoides, Mixamo, troca de armas e iteração com Codex o suficiente para valer a migração?

## Objetivo Atual

Construir um sandbox mínimo de combate 3D isométrico:

- câmera isométrica/ortográfica;
- movimento WASD relativo à câmera;
- personagem placeholder;
- dummy com vida;
- ataque básico com hitbox;
- validação do Unity MCP oficial no fluxo real de desenvolvimento.

## Regra Principal

Não começar pelo jogo completo.

Primeiro vamos provar o pipeline:

1. Codex consegue controlar a Unity via MCP.
2. A cena 3D básica é confortável.
3. O movimento e ataque respondem bem.
4. Importar animações e assets 3D é menos doloroso do que o fluxo 2D atual.

## Documentação

- [CODEX.md](CODEX.md): memória viva do projeto.
- [Docs/VISION.md](Docs/VISION.md): visão de longo prazo.
- [Docs/CURRENT_STEP.md](Docs/CURRENT_STEP.md): passo atual do protótipo.
- [Docs/ARCHITECTURE.md](Docs/ARCHITECTURE.md): organização técnica inicial.
- [Docs/DECISIONS.md](Docs/DECISIONS.md): decisões registradas.

## Atualizacao 2026-07-08

O sandbox principal agora segue direcao third-person em perspectiva, nao isometrica.

Objetivo imediato:

- camera atras/acima do player;
- mouse controla a camera;
- WASD relativo a camera;
- dodge simples no `Left Shift`;
- ataque basico no `Space`;
- dummy de treino com vida;
- sem lock-on, multiplayer, inventario ou sistema completo de armas neste passo.
