#pragma once


#include "joueur.h"


class Joueur_MCTS : public Joueur
{
private :
  unsigned int _etat;
  
public:
  Joueur_MCTS(std::string nom, bool joueur);

  void initialisation() override;

  void init_partie() override;

  char nom_abbrege() const override;

  void recherche_coup(Jeu, int & coup) override;
};
