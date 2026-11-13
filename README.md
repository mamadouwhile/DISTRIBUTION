# Joueurs MCTS pour un moteur de jeu fourni en `jeu.o`

## 1. Presentation du projet

Ce projet contient un joueur Monte Carlo Tree Search (MCTS) pour un moteur de jeu fourni sous forme binaire, via `jeu.o`.

L'objectif n'est pas de coder une strategie parfaite pour un jeu connu d'avance, mais un joueur qui bat un joueur aleatoire (`Random`) et un joueur "brutal" (`Brutal`) **sans connaitre les regles du jeu**. Le joueur n'utilise que l'interface de `Jeu` : `nb_coups()`, `coup_licite()`, `joue()`, `terminal()`, `pat()` et `victoire()`.

Pourquoi MCTS et pas Minimax / alpha-beta ? Alpha-beta a besoin d'une fonction d'evaluation pour juger une position quand on s'arrete avant la fin de la partie. Sans connaitre le jeu, on ne peut pas en ecrire une. MCTS n'en a pas besoin : il juge un coup en finissant la partie au hasard un grand nombre de fois, et la seule information utilisee est le resultat final (victoire, defaite, nul).

## 2. Idee generale de Monte Carlo Tree Search

Monte Carlo Tree Search repose sur quatre etapes principales.

### Selection
On descend dans l'arbre a partir de la racine en choisissant a chaque noeud l'enfant qui semble le plus prometteur.

Le choix se fait avec une formule de type UCB, qui combine :

- l'exploitation : favoriser les coups qui ont deja donne de bons resultats
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
- si elle est trop forte, le joueur disperse son temps de calcul et n'apprend pas assez sur les meilleures branches

Dans ce projet, cet equilibre est principalement regle par `C_EXPLORATION`.


## 3. Organisation des versions

Le `CMakeLists.txt` des joueurs ne compile que `joueurs/joueur_MCTS.*`. Les autres versions sont rangees dans des sous-dossiers, avec les memes noms de fichiers et de classe : pour en tester une, il suffit de copier ses deux fichiers dans `joueurs/` a la place de ceux de la version principale, puis de recompiler.

### Version principale (generique) : `joueurs/joueur_MCTS.*`

Un MCTS classique (UCT), sans aucune connaissance du jeu :

1. **Verifications rapides avant la recherche** : si un coup gagne tout de suite, on le joue ; on retire les coups qui laissent l'adversaire gagner au coup suivant (sauf s'ils sont tous dans ce cas).
2. **Recherche MCTS** pendant 5 ms : selection avec la formule UCB, expansion d'un coup au hasard, simulation aleatoire jusqu'a la fin de la partie, retropropagation du resultat.
3. **Choix final** : le coup de la racine le plus visite. C'est plus fiable que la meilleure moyenne, qui peut venir d'un coup teste tres peu de fois.

Choix techniques expliques dans le code :

- Les noeuds de l'arbre sont ranges dans un `std::vector` et designes par leur indice. Pas de `new` / `delete` : le rangement de l'arbre a la fin du coup est rapide, ce qui compte avec une limite de 10 ms.
- Chaque noeud stocke ses gains **du point de vue du joueur qui a joue le coup** de ce noeud. Ainsi, a chaque niveau, la formule UCB choisit le coup qui est bon pour le joueur qui doit jouer, que ce soit nous ou l'adversaire.
- Pendant la simulation, on tire un numero entre 1 et `nb_coups()` et on verifie `coup_licite()`, au lieu de construire la liste des coups a chaque fois. C'est environ 4 fois plus rapide (on passe d'environ 430 a 1700 simulations par coup avec 6 ms).

### Ancienne version : `joueurs/joueur_MCTS_ancien/joueur_MCTS.*`

C'est la version principale precedente. Elle ajoutait les memes verifications tactiques, mais aussi une regle propre au puissance 4 (privilegier les coups "du milieu" dans les simulations), coupait les simulations a 16 coups et utilisait 9 ms. Elle joue bien, mais elle depasse parfois le temps (voir les resultats plus bas), et sa regle du milieu n'est pas generique.

### Version basique : `joueurs/joueur_MCTS_basique/joueur_MCTS.*`

Un MCTS minimal, sans verification tactique et avec des simulations purement aleatoires.

## 4. Particularite importante du moteur `jeu.o`

C'est le point technique central du projet. En lisant le binaire (`nm` et `objdump -d jeu.o`), on voit que :

- le plateau est stocke dans des **variables globales** (`_plateau`, `_tab`, `_nombre`, `_nb_tours`...). Toutes les copies de `Jeu` partagent donc le meme plateau ;
- le constructeur `Jeu()` appelle `reset()` : ecrire `Jeu t;` remet le plateau de la partie a zero ;
- seul l'etat de fin de partie (`_val` : en cours, victoire ou nul) est propre a chaque objet ;
- `victoire()` est vrai quand c'est **le joueur 1** qui a gagne.

Consequences pour le joueur :

- on ne peut pas copier une position pour la simuler. Le joueur garde la liste des coups de la partie (`_historique`) et, pour chaque simulation, fait `reset()` puis rejoue cette liste ;
- l'arbitre ne transmet pas directement le coup de l'adversaire, mais la variable `coup` qu'il passe au joueur contient le dernier coup joue (ou `-1000` au premier coup). Le joueur s'en sert pour completer son historique ;
- **avant de rendre son coup, le joueur remet le plateau dans la vraie position**, sinon l'arbitre continuerait la partie sur un plateau faux.

## 5. Parametres et comment les regler

Ils sont en haut de `joueurs/joueur_MCTS.cpp`.

### `TEMPS_MAX_MICROSECONDES` (5000 = 5 ms)

Le temps de reflexion par coup. L'arbitre donne 10 ms, mais ce temps commence quand il cree le thread du joueur. On a mesure que le lancement du thread prend en general 0,1 ms, avec des pics a plusieurs ms, et le joueur doit encore ranger son arbre apres la recherche. Si l'arbitre affiche `mutex non rendu`, il faut baisser cette valeur.

- Plus grand : plus de simulations, donc de meilleurs coups, mais plus de risque de perdre au temps.
- Plus petit : plus sur, mais moins de simulations.

Contre `Random` et `Brutal`, 5 ms suffit largement : le joueur gagne toutes les parties qui ne sont pas perdues au temps.

### `C_EXPLORATION` (0.5)

Le poids du terme d'exploration dans UCB. On a essaye 0.5, 1.0 et 1.4 en face-a-face contre l'ancienne version : 0.5 donnait les meilleurs resultats.

- Plus grand : on essaie plus souvent les coups peu testes.
- Plus petit : on se concentre plus vite sur les coups qui semblent bons.

### `LONGUEUR_MAX_SIMULATION` (1000)

Une securite : si un jeu inconnu ne se terminait jamais, la simulation s'arrete et compte comme un nul. Avec le jeu fourni, une partie dure moins de 50 coups, donc cette limite n'est jamais atteinte.

## 6. Resultats mesures

Mesures faites avec l'arbitre fourni (10 ms par coup), le `jeu.o` fourni et une compilation en mode Debug comme dans le `CMakeLists.txt`. Les parties alternent le joueur qui commence.

| Match | Version principale (generique) | Ancienne version |
|---|---|---|
| contre `Random` | **298 / 300** victoires | 189 / 200 |
| contre `Brutal` | **299 / 300** victoires | 190 / 200 |

Toutes les defaites des deux versions contre `Random` et `Brutal` sont des **depassements de temps** (`mutex non rendu`), pas des coups perdants. La nouvelle version en fait beaucoup moins grace a sa marge de temps.

Face-a-face nouvelle contre ancienne version (300 parties) : 122 victoires pour la nouvelle, 95 pour l'ancienne, 83 nuls. Attention, ce resultat vient surtout des depassements de temps de l'ancienne version (43 parties). Sur le plateau seul, l'ancienne gagne un peu plus souvent (92 contre 79), grace a ses 9 ms et a sa regle du puissance 4. La nouvelle version reste generique et ne perd presque jamais au temps.

Les chiffres peuvent varier d'une machine a l'autre, surtout le nombre de depassements de temps.

## 7. Limites et pistes d'amelioration

- **Cout de la reconstruction** : chaque simulation recommence par `reset()` et le rejeu de toute la partie. C'est impose par `jeu.o`, et ca represente environ la moitie du temps de calcul.
- **Reutilisation de l'arbre** : on pourrait garder la partie de l'arbre qui correspond au coup joue et a la reponse de l'adversaire, au lieu de repartir de zero a chaque coup.
- **MCTS-Solver** : marquer les positions dont le resultat est certain (victoire ou defaite forcee) et remonter cette information. On l'a essaye : sur nos tests, il ne gagnait pas plus de parties, donc on l'a laisse de cote pour garder un code simple.
- **Simulations moins aleatoires** : elles sont plus informatives si on connait le jeu (comme la regle du milieu de l'ancienne version), mais le joueur ne serait plus generique.

## 8. Conclusion

Le joueur principal est un MCTS (UCT) generique : il ne connait pas les regles, il respecte les contraintes particulieres de `jeu.o` (plateau global, reconstruction par l'historique) et il garde une marge de temps pour ne pas perdre au temps. C'etait la principale cause de defaite de la version precedente.

Projet realise par Walid SMIHI et Mahamadou Dembele dans le cadre du cours d'initiation aux systemes intelligents dispense par M. Igor Stephan.
