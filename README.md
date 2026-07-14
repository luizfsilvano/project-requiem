# Project Requiem

Project Requiem é um projeto de jogo em Unreal Engine. O primeiro objetivo é construir uma fatia jogável que acompanha o personagem do nascimento até a escolha da classe inicial.

## Estado atual

O repositório está na fundação inicial do projeto Unreal, baseado no template Third Person. Ainda não há sistemas próprios de gameplay do Project Requiem.

Versão da Unreal detectada no `.uproject`: **5.8.0**.

Consulte `PROJECT_REQUIEM_GUIDE.md` para o escopo do MVP e `PROJECT_REQUIEM_ARCHITECTURE.md` para as decisões de arquitetura.

## Clonagem e abertura

Clone o repositório, execute `git lfs pull` e abra `ProjectRequiem.uproject` com a versão compatível da Unreal Engine.

Quando o módulo C++ for adicionado, gere os arquivos de projeto pelo menu de contexto do `.uproject` ou pelo fluxo equivalente da Unreal.

## Histórico Unity

A implementação antiga em Unity foi removida da linha principal, mas continua acessível na branch `archive/unity-prototype` e na tag anotada `unity-prototype-final`.

## Fluxo e commits

O projeto usa branches curtas, sem Gitflow: `main`, `feature/*`, `fix/*`, `refactor/*`, `chore/*` e `docs/*`.

Commits devem seguir Conventional Commits, por exemplo: `chore(repo): configure Unreal repository`.

Git LFS é obrigatório para assets binários da Unreal, incluindo `.uasset` e `.umap`. Use `git lfs install`, `git lfs pull` e `git lfs ls-files`.
