# Project Requiem — Guia Inicial

> Nome provisório do projeto.

## Objetivo

Criar uma primeira experiência jogável curta que represente o início real do jogo e valide suas principais mecânicas fundamentais, sem exigir ainda um mundo completo, inimigos elaborados, bosses ou animações avançadas.

Todo o protótipo anteriormente desenvolvido na Unity será descartado como implementação. Ele pode servir apenas como referência de ideias, mas não como base técnica ou de escopo.

## Roteiro inicial

### 1. Nascimento

O jogador começa vendo o próprio nascimento. Os pais observam a criança, estabelecendo o contexto familiar e o início da vida do personagem.

### 2. Criação do personagem

Logo após o nascimento, começa uma criação de personagem simples, com três escolhas:

- sexo: homem ou mulher;
- idade: apenas um valor numérico, sem alterar a aparência inicialmente;
- nome do personagem.

A criação visual detalhada ficará para uma etapa futura.

### 3. Passagem de tempo

Depois da criação, ocorre uma passagem de tempo. Isso pode ser apresentado por uma cutscene simples focada nos pais, sem necessidade de mostrar toda a infância do personagem.

### 4. Saída de casa

O jogador reaparece já preparado para iniciar a aventura, dentro da própria casa. A cena deve mostrar o personagem arrumando uma mochila ou se preparando para partir.

O objetivo narrativo é deixar claro que chegou o momento de sair de casa e começar a própria vida como aventureiro.

### 5. Caminho até a guilda

O jogo informa que o personagem finalmente irá até a Guilda dos Aventureiros.

Não haverá indicador de mapa, marcador de objetivo ou GPS. O jogador deverá descobrir o caminho observando o ambiente, lendo placas e conversando com NPCs.

Essa etapa deve validar a exploração baseada em contexto e informação do mundo.

### 6. Chegada e registro na guilda

Ao chegar à guilda, o jogador conversa com a recepcionista, realiza o registro e se torna oficialmente um aventureiro.

A guilda será apresentada como um ponto importante de progressão e como o local onde o personagem inicia sua carreira.

### 7. Teste de proficiência

Após o registro, o jogador participa de um teste de proficiência. O teste funciona simultaneamente como:

- tutorial básico de combate;
- showcase dos principais estilos de combate;
- avaliação inicial para recomendação de classe.

O teste será realizado contra um manequim, mantendo o escopo simples.

Os estilos demonstrados inicialmente serão:

- espada comum;
- arco;
- magia;
- combate desarmado, representando o pugilista.

Não será necessário criar classes completas, inimigos normais ou bosses nesta etapa. O foco é fazer o jogador experimentar os arquétipos básicos.

### 8. Aptidão excepcional e escolha de classe

Depois do teste, a recepcionista informa que o jogador é um caso raro, pois demonstrou aptidão para todas as classes apresentadas.

Em seguida, o jogador recebe a liberdade de escolher sua classe inicial.

## Limites do MVP

Ficam fora deste primeiro recorte:

- personalização visual avançada;
- infância jogável completa;
- animações cinematográficas complexas;
- inimigos comuns elaborados;
- bosses;
- combate avançado;
- mundo aberto completo;
- sistemas finais de quests;
- conteúdo posterior à escolha da classe.

## Sistemas que já devem nascer pensando no jogo final

Mesmo com apresentação simples, a implementação deve preparar uma base reutilizável para:

- identidade e dados do personagem;
- nome, sexo e idade;
- diálogos com NPCs;
- descoberta de objetivos por placas e conversas;
- interação com objetos e personagens;
- registro e progressão na guilda;
- sistema de combate modular;
- estilos de combate e classes;
- seleção da classe inicial;
- transições de tempo e mudanças de estado do personagem.

## Critério de sucesso

O MVP estará cumprido quando for possível jogar do nascimento até a escolha de classe, entendendo:

1. quem é o personagem;
2. que ele está deixando a casa para iniciar uma nova vida;
3. como encontrar a guilda sem GPS;
4. como se registrar como aventureiro;
5. como funcionam os estilos básicos de combate;
6. como escolher a classe inicial.

O resultado deve ser uma pequena fatia jogável do jogo definitivo, e não um protótipo genérico de sistemas isolados.
