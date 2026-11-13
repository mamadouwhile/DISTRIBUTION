#include "joueur_MCTS.h"
#include <chrono>
#include <cmath>
#include <vector>

/*
 * ============================================================================
 *  Joueur MCTS generique (Monte Carlo Tree Search, variante UCT)
 * ============================================================================
 *
 * Idee : on ne connait pas le jeu, donc on ne peut pas ecrire une fonction
 * d'evaluation (dire si une position est "bonne"). La seule information fiable
 * est la fin de partie : victoire, defaite ou nul.
 *
 * MCTS se sert uniquement de ca. Pendant le temps disponible, on repete :
 *   1. SELECTION   : on descend dans l'arbre en choisissant les coups
 *                    prometteurs (formule UCB).
 *   2. EXPANSION   : on ajoute un nouveau coup (un nouveau noeud) a l'arbre.
 *   3. SIMULATION  : on finit la partie au hasard.
 *   4. RETROPROPAGATION : on remonte le resultat dans les noeuds traverses.
 * A la fin, on joue le coup de la racine qui a ete le plus visite.
 *
 * Particularite du moteur jeu.o (verifiee en lisant le binaire) :
 * le plateau est stocke dans des variables globales. Toutes les copies de Jeu
 * partagent donc le meme plateau, et creer un Jeu appelle reset().
 * On ne peut donc pas "copier" une position. A la place :
 *   - on garde la liste des coups de la partie (_historique),
 *   - pour simuler, on fait reset() puis on rejoue cette liste,
 *   - avant de rendre notre coup, on remet le plateau dans la vraie position
 *     (sinon l'arbitre verrait un plateau faux).
 */

namespace {

// Temps de reflexion par coup. L'arbitre laisse 10 ms en tout, mais il faut
// aussi compter le lancement du thread par l'arbitre (souvent 0,1 ms, mais
// on a mesure des pics a plusieurs ms) et le rangement de l'arbre. Avec 9 ms, l'ancienne version depassait parfois le
// temps et perdait la partie. 5 ms laisse une marge confortable, et contre
// Random/Brutal le joueur gagne deja toutes ses parties avec ce budget.
const long TEMPS_MAX_MICROSECONDES = 5000; // 5 ms

// Constante d'exploration de UCB.
// Grande valeur : on essaie plus souvent les coups peu testes.
// Petite valeur : on se concentre vite sur les coups qui semblent bons.
const double C_EXPLORATION = 0.5;

// Securite : si un jeu inconnu ne se termine jamais, on arrete la simulation.
const int LONGUEUR_MAX_SIMULATION = 1000;

// Un noeud de l'arbre = une position atteinte apres une suite de coups.
// Les noeuds sont ranges dans un std::vector et se designent par leur indice
// (plus simple et plus rapide que des pointeurs + delete).
struct Noeud {
  int coup;                       // coup joue pour arriver ici (-1 pour la racine)
  int parent;                     // indice du parent (-1 pour la racine)
  bool joue_par_moi;              // true si "coup" a ete joue par notre joueur
  std::vector<int> enfants;       // indices des noeuds enfants
  std::vector<int> coups_restants;// coups legaux pas encore ajoutes a l'arbre
  double gains = 0.0;             // somme des resultats, du point de vue de celui qui a joue "coup"
  int visites = 0;                // nombre de simulations passees par ce noeud
};

} // namespace


Joueur_MCTS::Joueur_MCTS(std::string nom, bool joueur)
  : Joueur(nom, joueur), _rng(std::random_device{}())
{}

void Joueur_MCTS::initialisation()
{
}

void Joueur_MCTS::init_partie()
{
  _historique.clear();
}

char Joueur_MCTS::nom_abbrege() const
{
  return 'M';
}


// Remet le plateau (global) dans la position actuelle de la partie.
void Joueur_MCTS::reconstruire(Jeu & jeu)
{
  jeu.reset();
  for (int c : _historique)
    jeu.joue(c);
}

// Liste des coups legaux : les numeros de 1 a nb_coups() acceptes par coup_licite.
std::vector<int> Joueur_MCTS::coups_legaux(Jeu & jeu)
{
  std::vector<int> coups;
  for (int c = 1; c <= jeu.nb_coups(); c++)
    if (jeu.coup_licite(c))
      coups.push_back(c);
  return coups;
}

// Resultat d'une partie terminee, de notre point de vue :
// 1 = on a gagne, -1 = on a perdu, 0 = match nul.
// Rappel : victoire() est vrai quand le joueur 1 a gagne, et joueur() est
// vrai quand nous sommes le joueur 1.
int Joueur_MCTS::resultat_pour_moi(Jeu & jeu)
{
  if (jeu.pat())
    return 0;
  return (jeu.victoire() == joueur()) ? 1 : -1;
}


void Joueur_MCTS::recherche_coup(Jeu jeu, int & coup)
{
  auto debut = std::chrono::steady_clock::now();
  auto temps_ecoule = [&]() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::steady_clock::now() - debut).count();
  };

  // --- 0. Mise a jour de l'historique -------------------------------------
  // L'arbitre nous donne dans "coup" le dernier coup joue par l'adversaire
  // (ou COUP_INVALIDE = -1000 si c'est le tout premier coup de la partie).
  if (coup > 0)
    _historique.push_back(coup);

  Jeu simu = jeu; // objet de travail pour les simulations
  reconstruire(simu);
  std::vector<int> coups_racine = coups_legaux(simu);

  if (coups_racine.size() == 1) {
    coup = coups_racine[0];
    _historique.push_back(coup);
    return; // le plateau n'a pas ete modifie
  }

  // --- 1. Verifications rapides avant la recherche ------------------------
  // a) Si un coup gagne tout de suite, on le joue.
  for (int c : coups_racine) {
    reconstruire(simu);
    simu.joue(c);
    if (simu.terminal() && resultat_pour_moi(simu) == 1) {
      reconstruire(jeu);      // on remet le vrai plateau avant de repondre
      coup = c;
      _historique.push_back(coup);
      return;
    }
  }

  // b) On enleve les coups qui laissent l'adversaire gagner au coup suivant
  //    (sauf si tous les coups sont dans ce cas).
  std::vector<int> coups_surs;
  for (int c : coups_racine) {
    reconstruire(simu);
    simu.joue(c);
    bool dangereux = false;
    if (!simu.terminal()) {
      std::vector<int> reponses = coups_legaux(simu);
      for (int r : reponses) {
        reconstruire(simu);
        simu.joue(c);
        simu.joue(r);
        if (simu.terminal() && resultat_pour_moi(simu) == -1) {
          dangereux = true;
          break;
        }
      }
    }
    if (!dangereux)
      coups_surs.push_back(c);
  }
  if (!coups_surs.empty())
    coups_racine = coups_surs;

  if (coups_racine.size() == 1) {
    reconstruire(jeu);
    coup = coups_racine[0];
    _historique.push_back(coup);
    return;
  }

  // --- 2. Recherche MCTS ----------------------------------------------------
  std::vector<Noeud> arbre;
  arbre.reserve(20000);

  Noeud racine;
  racine.coup = -1;
  racine.parent = -1;
  racine.joue_par_moi = false; // le coup qui a mene a la racine est celui de l'adversaire
  racine.coups_restants = coups_racine;
  arbre.push_back(racine);

  while (temps_ecoule() < TEMPS_MAX_MICROSECONDES) {
    reconstruire(simu);
    int n = 0; // on part de la racine

    // 2.1 SELECTION : tant que tous les coups du noeud sont deja dans l'arbre,
    // on descend vers l'enfant qui a le meilleur score UCB :
    //     gains/visites  +  C * sqrt( ln(visites du parent) / visites )
    //     (exploitation)     (exploration : bonus aux coups peu testes)
    // "gains" est compte du point de vue du joueur qui choisit a ce noeud,
    // donc chaque camp choisit le coup qui est bon pour lui.
    while (arbre[n].coups_restants.empty() && !arbre[n].enfants.empty()) {
      int meilleur = -1;
      double meilleur_score = -1.0;
      double log_parent = std::log((double)arbre[n].visites);
      for (int e : arbre[n].enfants) {
        double moyenne = arbre[e].gains / arbre[e].visites;
        double ucb = moyenne + C_EXPLORATION * std::sqrt(log_parent / arbre[e].visites);
        if (ucb > meilleur_score) {
          meilleur_score = ucb;
          meilleur = e;
        }
      }
      n = meilleur;
      simu.joue(arbre[n].coup);
    }

    // 2.2 EXPANSION : on ajoute un coup pas encore essaye (choisi au hasard).
    if (!arbre[n].coups_restants.empty()) {
      std::vector<int> & restants = arbre[n].coups_restants;
      int i = (int)(_rng() % restants.size());
      int c = restants[i];
      restants[i] = restants.back();
      restants.pop_back();

      simu.joue(c);

      Noeud enfant;
      enfant.coup = c;
      enfant.parent = n;
      enfant.joue_par_moi = !arbre[n].joue_par_moi; // les joueurs alternent
      if (!simu.terminal())
        enfant.coups_restants = coups_legaux(simu);
      arbre.push_back(enfant);          // attention : peut deplacer le vector,
      int indice_enfant = (int)arbre.size() - 1;
      arbre[n].enfants.push_back(indice_enfant); // donc on n'utilise plus "restants" ici
      n = indice_enfant;
    }

    // 2.3 SIMULATION : on termine la partie avec des coups au hasard.
    //     On tire un numero entre 1 et nb_coups() ; s'il n'est pas licite on
    //     retire (plus rapide que de construire la liste des coups a chaque fois).
    int longueur = 0;
    while (!simu.terminal() && longueur < LONGUEUR_MAX_SIMULATION) {
      int nb = simu.nb_coups();
      if (nb <= 0)
        break;
      int c = 1 + (int)(_rng() % nb);
      if (!simu.coup_licite(c)) {
        std::vector<int> coups = coups_legaux(simu); // cas rare : on prend la liste
        if (coups.empty())
          break;
        c = coups[_rng() % coups.size()];
      }
      simu.joue(c);
      longueur++;
    }

    // Resultat de la simulation pour nous : 1 victoire, 0.5 nul, 0 defaite.
    double resultat = 0.5;
    if (simu.terminal()) {
      int r = resultat_pour_moi(simu);
      resultat = (r == 1) ? 1.0 : (r == -1) ? 0.0 : 0.5;
    }

    // 2.4 RETROPROPAGATION : on remonte jusqu'a la racine.
    // Chaque noeud compte le resultat du point de vue du joueur qui a joue son coup.
    while (n != -1) {
      arbre[n].visites++;
      arbre[n].gains += arbre[n].joue_par_moi ? resultat : 1.0 - resultat;
      n = arbre[n].parent;
    }
  }

  // --- 3. Choix final : le coup le plus visite ------------------------------
  // (plus fiable que la meilleure moyenne, qui peut venir de peu de simulations)
  int meilleur_coup = coups_racine[0];
  int max_visites = -1;
  for (int e : arbre[0].enfants) {
    if (arbre[e].visites > max_visites) {
      max_visites = arbre[e].visites;
      meilleur_coup = arbre[e].coup;
    }
  }

  // On remet le vrai plateau avant de rendre la main a l'arbitre.
  reconstruire(jeu);
  coup = meilleur_coup;
  _historique.push_back(coup);
}
