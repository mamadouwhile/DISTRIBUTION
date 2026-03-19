#include "joueur_MCTS.h"
#include <cstdlib>
#include <cmath>
#include <vector>
#include <limits>

struct NoeudMCTS {
    Jeu etat;
    int coup;
    int nb_victoires;
    int nb_visites;
    int profondeur_absolue;
    NoeudMCTS* parent;
    std::vector<NoeudMCTS*> enfants;
    std::vector<int> coups_non_explores;

    NoeudMCTS(Jeu j, int c, NoeudMCTS* p, int prof_abs) {
        etat = j;
        coup = c;
        nb_victoires = 0;
        nb_visites = 0;
        profondeur_absolue = prof_abs;
        parent = p;
        int n = j.nb_coups();
        for (int i = 1; i <= n; i++) {
            coups_non_explores.push_back(i);
        }
    }

    bool est_pleinement_explore() {
        return coups_non_explores.empty();
    }

    double ucb1() {
        double C = sqrt(2.0);
        if (nb_visites == 0) {
            return 999999.0;
        }
        double exploitation = (double)nb_victoires / nb_visites;
        double exploration = C * sqrt(log((double)parent->nb_visites) / nb_visites);
        return exploitation + exploration;
    }

    NoeudMCTS* meilleur_enfant_ucb() {
        NoeudMCTS* meilleur = NULL;
        double best = -999999.0;
        for (int i = 0; i < (int)enfants.size(); i++) {
            double v = enfants[i]->ucb1();
            if (v > best) {
                best = v;
                meilleur = enfants[i];
            }
        }
        return meilleur;
    }

    NoeudMCTS* meilleur_enfant_final() {
        NoeudMCTS* meilleur = NULL;
        int best_visites = -1;
        for (int i = 0; i < (int)enfants.size(); i++) {
            if (enfants[i]->nb_visites > best_visites) {
                best_visites = enfants[i]->nb_visites;
                meilleur = enfants[i];
            }
        }
        return meilleur;
    }
};

static void liberer_arbre(NoeudMCTS* noeud) {
    for (int i = 0; i < (int)noeud->enfants.size(); i++) {
        liberer_arbre(noeud->enfants[i]);
    }
    delete noeud;
}

static NoeudMCTS* selection(NoeudMCTS* racine) {
    NoeudMCTS* courant = racine;
    while (!courant->etat.terminal() && courant->est_pleinement_explore()) {
        courant = courant->meilleur_enfant_ucb();
    }
    return courant;
}

static NoeudMCTS* expansion(NoeudMCTS* noeud) {
    if (noeud->etat.terminal() || noeud->coups_non_explores.empty()) {
        return noeud;
    }
    int idx = rand() % noeud->coups_non_explores.size();
    int coup_choisi = noeud->coups_non_explores[idx];
    noeud->coups_non_explores.erase(noeud->coups_non_explores.begin() + idx);

    Jeu nouvel_etat = noeud->etat;
    nouvel_etat.joue(coup_choisi);

    NoeudMCTS* enfant = new NoeudMCTS(nouvel_etat, coup_choisi, noeud,
                                      noeud->profondeur_absolue + 1);
    noeud->enfants.push_back(enfant);
    return enfant;
}

static bool simulation(NoeudMCTS* noeud, bool mcts_est_joueur1) {
    Jeu sim = noeud->etat;
    while (!sim.terminal()) {
        int n = sim.nb_coups();
        int coup;
        if (n == 1) {
            coup = 1;
        } else {
            coup = (rand() % n) + 1;
        }
        sim.joue(coup);
    }
    if (sim.pat()) {
        return false;
    }
    return sim.victoire() == mcts_est_joueur1;
}

static void retropropagation(NoeudMCTS* noeud, bool victoire) {
    while (noeud != NULL) {
        noeud->nb_visites++;
        if (victoire) {
            noeud->nb_victoires++;
        }
        noeud = noeud->parent;
    }
}

Joueur_MCTS::Joueur_MCTS(std::string nom, bool joueur)
    : Joueur(nom, joueur), _etat(42)
{}

void Joueur_MCTS::initialisation() {
    srand(_etat++);
}

void Joueur_MCTS::init_partie() {
}

char Joueur_MCTS::nom_abbrege() const {
    return 'M';
}

void Joueur_MCTS::recherche_coup(Jeu jeu, int& coup) {
    if (jeu.nb_coups() == 1) {
        coup = 1;
        return;
    }

    NoeudMCTS* racine = new NoeudMCTS(jeu, -1, NULL, 0);

    for (int iter = 0; iter < 500; iter++) {
        NoeudMCTS* noeud = selection(racine);
        if (!noeud->etat.terminal()) {
            noeud = expansion(noeud);
        }
        bool victoire = simulation(noeud, joueur());
        retropropagation(noeud, victoire);
    }

    NoeudMCTS* meilleur = racine->meilleur_enfant_final();

    if (meilleur != NULL) {
        coup = meilleur->coup;
    } else {
        coup = (rand() % jeu.nb_coups()) + 1;
    }

    liberer_arbre(racine);
}
