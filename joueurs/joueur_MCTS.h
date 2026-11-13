#pragma once

#include "joueur.h"
#include <vector>
#include <random>

/**
 * Joueur MCTS (Monte Carlo Tree Search, variante UCT) generique.
 *
 * Il ne connait pas les regles du jeu : il utilise seulement l'interface
 * de Jeu (nb_coups, coup_licite, joue, terminal, pat, victoire).
 * Les explications detaillees sont dans joueur_MCTS.cpp et dans README.md.
 */
class Joueur_MCTS : public Joueur
{
private:
  // Coups joues depuis le debut de la partie (les notres et ceux de l'adversaire).
  // Indispensable car le moteur jeu.o garde le plateau dans des variables
  // globales : pour simuler, on remet le plateau a zero puis on rejoue cette liste.
  std::vector<int> _historique;

  std::mt19937 _rng; // generateur aleatoire pour les simulations

  // Petites fonctions utilitaires (voir le .cpp)
  void reconstruire(Jeu & jeu);
  std::vector<int> coups_legaux(Jeu & jeu);
  int resultat_pour_moi(Jeu & jeu);

public:
  Joueur_MCTS(std::string nom, bool joueur);

  void initialisation() override;

  void init_partie() override;

  char nom_abbrege() const override;

  void recherche_coup(Jeu jeu, int & coup) override;
};
