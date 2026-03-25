# Deux joueurs MCTS pour un moteur de jeu fourni en `jeu.o`

## 1. Presentation du projet

Ce projet contient deux joueurs bases sur Monte Carlo Tree Search (MCTS) pour un moteur de jeu fourni sous forme binaire, via `jeu.o`.

L'objectif n'est pas de coder une strategie parfaite pour un jeu connu d'avance, mais de proposer des joueurs capables de fonctionner dans un cadre partiellement opaque, avec un moteur fourni deja compile.

Le depot contient deux versions principales :

- `joueurs/joueur_MCTS_basique.cpp` : une version simple, proche d'un MCTS standard
- `joueurs/joueur_MCTS.cpp` : une version MCTS amelioree, qui ajoute des filtres tactiques avant la recherche et un rollout un peu plus oriente sur la base des tests établis sur le jeu mystère. Cette version peut être ignoré si on veut un joueur MCTS polyvalent (règles ou jeux différents)

Les deux versions partagent les memes contraintes de base : elles doivent jouer sous une limite de temps courte et composer avec un moteur `jeu.o` dont le comportement n'est pas celui d'un type `Jeu` simplement copiable.

## 2. Idee generale de Monte Carlo Tree Search

Monte Carlo Tree Search repose sur quatre etapes principales.

### Selection
On descend dans l'arbre a partir de la racine en choisissant a chaque noeud l'enfant qui semble le plus prometteur.

Le choix se fait avec une formule de type UCB, qui combine :

- l'exploitation : favoriser les coups qui ont deja de bons resultats
- l'exploration : continuer a essayer des branches encore peu visitees

Le but est de ne pas se bloquer trop vite sur une seule branche, tout en concentrant progressivement le temps de calcul sur les lignes interessantes.

### Expansion
Quand on atteint un noeud non terminal qui possede encore des coups non explores, on en ouvre un nouveau dans l'arbre.

### Simulation / Rollout
A partir de cet etat, on joue une suite de coups simules jusqu'a une certaine profondeur ou jusqu'a une fin de partie.

### Retropropagation
Le resultat de la simulation remonte ensuite dans tous les noeuds traverses. Chaque noeud met a jour :

- son nombre de visites
- son score cumule

Au fil des iterations, l'arbre accumule donc une estimation statistique de la qualite des coups.

### Role de UCB
Le critere UCB sert a equilibrer exploration et exploitation.

Concretement :

- si l'exploration est trop faible, le joueur se fixe trop vite sur quelques branches
- si elle est trop forte, le joueur disperse son temps de calcul et n'apprend pas assez sur les meilleurs coups

Dans ce projet, cet equilibre est principalement regle par `C_EXPLORATION`.

## 3. Presentation des deux joueurs

## `joueur_MCTS_basique`

### Philosophie
Cette version cherche a rester un MCTS simple et lisible :

- pas de filtre tactique avant la recherche
- pas de detection explicite d'un gain immediat avant l'arbre
- rollout purement aleatoire

### Avantages
- structure plus simple a comprendre
- comportement proche d'un MCTS classique
- bonne base de comparaison pour mesurer l'effet des heuristiques ajoutees ensuite

### Limites
- les simulations sont plus bruitees
- le joueur peut perdre du temps sur des coups tactiquement mauvais a tres courte profondeur
- les parties peuvent etre moins bien orientees quand il existe des reponses immediates evidentes

## `joueur_MCTS`

### Ameliorations par rapport au basique
Cette version reste un MCTS, mais elle ajoute des decisions pragmatiques avant la recherche statistique.

### Filtres tactiques
Avant de lancer l'arbre, le joueur :

- cherche un coup gagnant immediat
- essaie d'eliminer les coups qui donnent une victoire immediate a l'adversaire

Le but est d'eviter de depenser des iterations MCTS sur des cas simples a detecter localement.

### Rollout plus oriente
Le rollout n'est pas purement uniforme. Les coups sont tries selon une regle simple, puis le joueur tire parmi un petit sous-ensemble. L'idee est de reduire un peu le bruit des simulations sans introduire une heuristique trop specialisee au jeu.

### Objectif
Cette version vise a etre plus solide que la version basique contre des adversaires un peu meilleurs qu'un joueur purement aleatoire, tout en restant compatible avec les contraintes du moteur fourni.

## 4. Particularite importante du moteur `jeu.o`

C'est le point technique central du projet.

En pratique, le moteur fourni sous forme de `jeu.o` semble utiliser un etat global partage, ou au minimum un comportement qui rend les objets `Jeu` peu fiables comme copies totalement independantes.

Autrement dit, il n'est pas prudent de supposer que :

```cpp
Jeu copie = jeu;
```

donne toujours un etat autonome, isole, manipulable librement dans une simulation sans effet de bord. (Risque d'erreur de segmentation par exemple)

### Consequence directe
Les deux joueurs utilisent donc un historique interne des coups : `_historique_partie`.

Avant certaines operations importantes, ils reconstruisent un etat coherent par la sequence suivante :

1. `reset()`
2. rejeu des coups de `_historique_partie`

### Pourquoi cette reconstruction
Cette reconstruction sert a repartir d'un etat aussi coherent que possible avant :

- les tests tactiques
- les simulations
- la verification finale du coup choisi

### Pourquoi l'etat est restaure avant de rendre le coup
Comme la recherche MCTS manipule plusieurs etats temporaires, le code restaure explicitement l'etat courant avant de verifier puis de renvoyer le coup final. C'est un choix pragmatique, motive par les limites du moteur fourni.

## 5. Parametres importants et comment les regler

## `MAX_TIME_MS`

### Ce que ce parametre controle
`MAX_TIME_MS` fixe le budget temps interne du joueur pour choisir un coup.

### Si on augmente `MAX_TIME_MS`
- le joueur peut faire plus d'iterations
- l'estimation statistique peut devenir meilleure
- mais on se rapproche de la limite reelle imposee par l'arbitre

### Si on le diminue
- le joueur joue plus vite
- il y a moins de risque de depassement de temps
- mais l'arbre accumule moins d'information

### Point important avec l'arbitre
Si l'arbitre affiche `mutex non rendu`, il faut reduire `MAX_TIME_MS`.

Meme si des tests locaux ne montrent pas d'erreur a `9 ms`, il peut rester un risque a cause :

- des approximations de mesure
- de la machine utilisee
- de l'ordonnancement des threads
- du comportement exact de l'arbitre

Garder une petite marge de securite est prudent.

### Recommandation pratique
- commencer avec une valeur conservative
- augmenter seulement si les tests sont stables
- ne pas considerer qu'une valeur est sure simplement parce qu'elle passe sur quelques executions locales, sauf si on accepte de jouer de manière risquée.

## `C_EXPLORATION`

### Ce que ce parametre controle
`C_EXPLORATION` regle le poids du terme d'exploration dans UCB.

### Si on l'augmente
- le joueur essaie plus de branches peu visitees
- il explore plus large
- mais il peut perdre du temps sur des branches peu prometteuses

### Si on le diminue
- le joueur se concentre plus vite sur les coups deja juges bons
- il exploite davantage
- mais il risque de passer a cote d'une branche meilleure encore peu testee

### Recommandation pratique
- avec tres peu de temps, une valeur trop haute disperse vite la recherche
- contre un adversaire faible, on peut souvent baisser un peu `C_EXPLORATION`
- contre un adversaire plus fort, il faut garder assez d'exploration pour ne pas se figer trop tot

## `MAX_ROLLOUT_DEPTH`

### Ce que ce parametre controle
`MAX_ROLLOUT_DEPTH` limite la longueur des simulations apres l'expansion.

### Si on l'augmente
- chaque rollout va plus loin
- certains resultats peuvent etre plus informatifs
- mais chaque iteration coute plus cher
- on fait donc moins d'iterations au total

### Si on le diminue
- on peut faire plus de simulations
- mais les rollouts deviennent plus courts et parfois moins utiles

### Recommandation pratique
- si les rollouts sont peu informatifs, augmenter fortement la profondeur n'est pas toujours rentable
- si le moteur est lent, une profondeur trop grande coute cher
- il faut trouver un compromis entre quantite et qualite de simulations

## Reglage selon l'adversaire

### Contre un joueur aleatoire ou faible
On peut souvent :

- baisser un peu `C_EXPLORATION`
- garder `MAX_ROLLOUT_DEPTH` modere
- garder `MAX_TIME_MS` prudent plutot que maximal

L'idee est qu'il vaut souvent mieux exploiter vite les lignes deja bonnes.

### Contre un joueur heuristique
On peut essayer :

- un `C_EXPLORATION` intermediaire
- une profondeur de rollout un peu mieux travaillee si elle reste rentable
- un `MAX_TIME_MS` assez stable pour laisser l'arbre s'installer

Dans ce cas, les filtres tactiques de `joueur_MCTS` peuvent devenir plus utiles.

### Contre un autre joueur MCTS
On peut souvent :

- baisser un peu `C_EXPLORATION` pour exploiter davantage
- tester si augmenter `MAX_ROLLOUT_DEPTH` apporte vraiment un meilleur signal
- augmenter `MAX_TIME_MS` seulement si l'environnement reste stable

Mais attention :

- si les rollouts restent tres bruités, augmenter `MAX_ROLLOUT_DEPTH` ne sert pas forcement
- si le temps est instable, augmenter `MAX_TIME_MS` peut surtout rapprocher des limites de l'arbitre

## 6. Limites et pistes d'amelioration

### Cout de reconstruction d'etat
Rejouer l'historique avant les simulations a un cout important. Sur un budget temps court, ce cout devient vite sensible.

### Dependance au moteur fourni
Le comportement de `jeu.o` conditionne fortement les choix d'implementation. Certaines techniques plus propres en theorie deviennent ici fragiles ou peu fiables.

### Sensibilite aux parametres
Les performances changent vite quand on modifie :

- le temps disponible
- l'exploration
- la profondeur des rollouts

Ces parametres doivent donc etre ajustes de maniere "empirique" selon nous.

### Amelioration des rollouts
La version amelioree guide deja un peu les rollouts, mais on pourrait encore tester :

- des politiques de rollout moins bruitees
- des coupures mieux choisies
- des heuristiques locales plus informatives

### Politique de selection finale
Le choix final pourrait encore etre affiné selon les tests :

- meilleur score moyen
- meilleur nombre de visites
- combinaison des deux selon le type d'adversaire

### Optimisation memoire
L'arbre alloue beaucoup de petits objets et reconstruit souvent des vecteurs de coups. Des optimisations memoire peuvent faire gagner du temps reel.

## 7. Conclusion

Le projet propose deux versions d'un meme principe de recherche :

- `joueur_MCTS_basique` reste un MCTS simple, lisible et proche d'une version standard
- `joueur_MCTS` reste lui aussi un MCTS, mais ajoute des heuristiques pragmatiques pour mieux utiliser le budget temps disponible

La version amelioree ne remplace pas le MCTS par autre chose. Elle conserve les quatre etapes classiques, tout en ajoutant :

- un filtre de gain immediat
- un filtre de coups non perdants
- un rollout un peu moins naif

Ces choix ne rendent pas le joueur optimal en toute situation, mais ils correspondent a une approche pragmatique pour un projet pratique dans une arene de jeu avec un moteur binaire fourni et des contraintes de temps fortes.

Utilisation de Claude.ai pour la reformulation du README, des commentaires, et la relecture du code.


Projet réalisé par Walid SMIHI & Mahamadou Dembele dans le cadre du cours d'initiation aux systemes intelligents dispensés par M. Igor Stephan.


