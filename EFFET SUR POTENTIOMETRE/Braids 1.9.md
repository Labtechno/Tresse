# Braids 1.9 — effet des potards TIMBRE et COLOR

Mesure faite avec Tresses sur le firmware officiel Braids v1.9 (Mutable Instruments), modèle par modèle.

## Comment lire ce tableau

- **Effet** : à quel point le son change quand on tourne le potard de 0 à 100 %, l'autre potard restant au milieu.
  Échelle : 🔴 presque rien · 🟠 faible · 🟡 moyen · 🟢 fort · 🔵 très fort.
- **Zone utile** : la partie de la course où se fait 80 % du changement. « début / milieu / fin de course » signale un effet concentré sur un bout du tour : une automation de 0 à 100 % n'agira vraiment que là.
- **Force** : le changement cumulé, en dB (spectre en tiers d'octave et volume) : moins de 5 presque rien, 5 à 12 faible, 12 à 25 moyen, 25 à 50 fort, plus de 50 très fort.
- **Conditions** : notes à 65 Hz et 262 Hz, un trigger à chaque mesure, 250 ms de son analysées.

Limite : la mesure regarde le son moyen sur un quart de seconde. Elle sous-estime les effets qui jouent sur le mouvement du son (le phasing de CSAW, le hasard de CLOU) : pour ceux-là, l'oreille reste juge.

Pour refaire la mesure : `tresses_knob_map` (voir `tools/knob_map.cpp`). Données brutes : `docs/carte-potards-brut.csv`.

## Tableau

| N° | Modèle | TIMBRE : rôle | TIMBRE : effet | TIMBRE : zone utile | COLOR : rôle | COLOR : effet | COLOR : zone utile | Remarque |
|---:|---|---|---|---|---|---|---|---|
| 1 | `CSAW` | Largeur de l'encoche | 🔴 presque rien (3.8 dB) | – | Profondeur et sens de l'encoche | 🔴 presque rien (4.4 dB) | – | TIMBRE n'a presque pas d'effet ; COLOR n'a presque pas d'effet |
| 2 | `MORPH (/\-_)` | Forme : triangle → scie → carré → impulsion | 🟢 fort (42.7 dB) | 3 à 96 % | Caractère : filtre + saturation | 🟡 moyen (16.9 dB) | 45 à 98 % (fin de course) | COLOR agit sur un bout de course |
| 3 | `SAW/SQUARE (//-_)` | Déphasage de la scie / largeur d'impulsion | 🟢 fort (26.8 dB) | 7 à 93 % | Fondu scie → carré | 🟠 faible (11.4 dB) | 2 à 91 % |  |
| 4 | `FOLD` | Force du repliement | 🔵 très fort (56.4 dB) | 9 à 81 % | Mélange sinus / triangle | 🟡 moyen (15.2 dB) | 5 à 85 % |  |
| 5 | `BUZZ (uuuu)` | Sinus → peigne (formant) | 🟢 fort (37.7 dB) | 45 à 91 % (fin de course) | Désaccord des deux formes | 🟠 faible (9.2 dB) | 11 à 82 % | TIMBRE agit sur un bout de course |
| 6 | `SUB- (carre + sub)` | Largeur d'impulsion | 🟢 fort (28.7 dB) | 4 à 95 % | Niveau du sub (gauche : 2 oct. sous, droite : 1 oct. sous, milieu : aucun) | 🟡 moyen (16.5 dB) | 15 à 82 % |  |
| 7 | `SUB/ (scie + sub)` | Forme de la scie | 🟢 fort (33.0 dB) | 8 à 95 % | Niveau du sub (gauche : 2 oct. sous, droite : 1 oct. sous, milieu : aucun) | 🟡 moyen (21.9 dB) | 20 à 80 % |  |
| 8 | `SYN- (carre sync)` | Intervalle maître / esclave | 🔵 très fort (63.0 dB) | 5 à 87 % | Mélange des deux oscillateurs | 🟡 moyen (12.8 dB) | 3 à 89 % |  |
| 9 | `SYN/ (scie sync)` | Intervalle maître / esclave | 🟢 fort (47.8 dB) | 7 à 86 % | Mélange des deux oscillateurs | 🟠 faible (10.0 dB) | 7 à 88 % |  |
| 10 | `//x3 (3 scies)` | Hauteur du 2e oscillateur (crantée) | 🟢 fort (27.9 dB) | 11 à 83 % | Hauteur du 3e oscillateur (crantée) | 🟢 fort (27.9 dB) | 11 à 83 % |  |
| 11 | `-_x3 (3 carres)` | Hauteur du 2e oscillateur (crantée) | 🟢 fort (37.5 dB) | 12 à 84 % | Hauteur du 3e oscillateur (crantée) | 🟢 fort (37.5 dB) | 12 à 84 % |  |
| 12 | `/\x3 (3 triangles)` | Hauteur du 2e oscillateur (crantée) | 🟢 fort (39.7 dB) | 14 à 90 % | Hauteur du 3e oscillateur (crantée) | 🟢 fort (39.7 dB) | 14 à 90 % |  |
| 13 | `SIx3 (3 sinus)` | Hauteur du 2e oscillateur (crantée) | 🟡 moyen (24.3 dB) | 14 à 91 % | Hauteur du 3e oscillateur (crantée) | 🟡 moyen (24.3 dB) | 14 à 91 % |  |
| 14 | `RING` | Hauteur du 2e sinus | 🔵 très fort (80.4 dB) | 27 à 91 % | Hauteur du 3e sinus | 🔵 très fort (80.4 dB) | 27 à 91 % |  |
| 15 | `//// (essaim de scies)` | Désaccord des 7 scies | 🔴 presque rien (4.8 dB) | – | Filtre passe-haut | 🟡 moyen (16.2 dB) | 13 à 91 % | TIMBRE n'a presque pas d'effet |
| 16 | `//uu (scie + filtre en peigne)` | Accord du filtre en peigne | 🟢 fort (47.7 dB) | 18 à 87 % | Réinjection (milieu : aucune) | 🟡 moyen (16.4 dB) | 3 à 80 % |  |
| 17 | `TOY*` | Vitesse d'horloge du jouet | 🟢 fort (29.2 dB) | 7 à 95 % | Glitchs | 🟠 faible (5.2 dB) | 13 à 81 % |  |
| 18 | `ZLPF` | Fréquence de coupure | 🟢 fort (38.4 dB) | 6 à 66 % | Forme : scie → carré → triangle | 🟡 moyen (16.5 dB) | 18 à 93 % |  |
| 19 | `ZPKF` | Fréquence de coupure | 🟢 fort (29.6 dB) | 19 à 71 % (milieu de course) | Forme : scie → carré → triangle | 🟢 fort (30.5 dB) | 19 à 94 % | TIMBRE agit sur un bout de course |
| 20 | `ZBPF` | Fréquence de coupure | 🟢 fort (42.7 dB) | 7 à 68 % | Forme : scie → carré → triangle | 🟡 moyen (20.5 dB) | 26 à 97 % |  |
| 21 | `ZHPF` | Fréquence de coupure | 🟢 fort (30.3 dB) | 18 à 68 % (milieu de course) | Forme : scie → carré → triangle | 🟡 moyen (19.4 dB) | 24 à 98 % | TIMBRE agit sur un bout de course |
| 22 | `VOSM` | Fréquence du 1er formant | 🟢 fort (37.4 dB) | 41 à 96 % (fin de course) | Fréquence du 2e formant | 🟢 fort (29.5 dB) | 50 à 96 % (fin de course) | TIMBRE agit sur un bout de course ; COLOR agit sur un bout de course |
| 23 | `VOWL` | Voyelle a, e, i, o, u | 🟢 fort (48.9 dB) | 11 à 93 % | Décalage des formants | 🟢 fort (34.9 dB) | 11 à 92 % |  |
| 24 | `VFOF` | Voyelle a, e, i, o, u | 🟡 moyen (21.0 dB) | 10 à 87 % | Décalage des formants | 🟠 faible (10.0 dB) | 13 à 91 % |  |
| 25 | `HARM` | Harmonique centrale | 🟠 faible (7.0 dB) | 13 à 79 % | Répartition des harmoniques | 🟡 moyen (22.3 dB) | 17 à 87 % |  |
| 26 | `FM` | Quantité de modulation | 🟢 fort (32.6 dB) | 3 à 87 % | Rapport modulateur / porteuse | 🔵 très fort (81.8 dB) | 23 à 94 % |  |
| 27 | `FBFM` | Quantité de modulation | 🟢 fort (33.4 dB) | 3 à 72 % | Rapport modulateur / porteuse | 🔵 très fort (64.3 dB) | 20 à 91 % |  |
| 28 | `WTFM` | Quantité de modulation | 🟢 fort (25.4 dB) | 2 à 92 % | Rapport modulateur / porteuse | 🔵 très fort (52.4 dB) | 24 à 93 % |  |
| 29 | `PLUK` | Amortissement | 🟠 faible (11.5 dB) | 1 à 67 % | Position du pincement | 🟠 faible (9.4 dB) | 2 à 82 % |  |
| 30 | `BOWD` | Friction de l'archet | 🔵 très fort (57.0 dB) | 8 à 90 % | Position de l'archet | 🟢 fort (36.7 dB) | 4 à 89 % |  |
| 31 | `BLOW` | Pression d'air | 🟠 faible (6.6 dB) | 33 à 95 % | Géométrie de l'instrument | 🟠 faible (9.5 dB) | 7 à 71 % |  |
| 32 | `FLUT` | Pression d'air | 🟠 faible (9.8 dB) | 11 à 90 % | Géométrie de l'instrument | 🔴 presque rien (2.1 dB) | – | COLOR n'a presque pas d'effet |
| 33 | `BELL` | Amortissement | 🟠 faible (11.9 dB) | 8 à 81 % | Inharmonicité | 🟠 faible (9.0 dB) | 7 à 92 % |  |
| 34 | `DRUM` | Amortissement | 🟠 faible (9.4 dB) | 6 à 77 % | Brillance | 🔴 presque rien (3.1 dB) | – | COLOR n'a presque pas d'effet |
| 35 | `KICK` | Durée (decay) | 🟡 moyen (22.7 dB) | 7 à 63 % (milieu de course) | Brillance (tone) | 🟠 faible (6.2 dB) | 15 à 40 % (début de course) | TIMBRE agit sur un bout de course ; COLOR agit sur un bout de course |
| 36 | `CYMB` | Coupure du filtre passe-bande | 🟡 moyen (20.8 dB) | 28 à 92 % | Mélange carrés / bruit | 🟠 faible (9.5 dB) | 2 à 75 % |  |
| 37 | `SNAR` | Équilibre des deux résonances (tone) | 🟠 faible (5.1 dB) | 1 à 90 % | Quantité de bruit (snappy) | 🟡 moyen (15.1 dB) | 2 à 73 % |  |
| 38 | `WTBL` | Balayage de la table | 🟠 faible (11.8 dB) | 2 à 50 % (début de course) | Choix de la table (20 tables) | 🔵 très fort (160.3 dB) | 9 à 92 % | TIMBRE agit sur un bout de course |
| 39 | `WMAP` | Position horizontale dans la carte | 🔵 très fort (74.2 dB) | 7 à 92 % | Position verticale dans la carte | 🔵 très fort (59.4 dB) | 4 à 83 % |  |
| 40 | `WLIN` | Parcours de toutes les formes | 🔵 très fort (128.7 dB) | 6 à 91 % | Mode d'interpolation | 🟡 moyen (15.7 dB) | 12 à 93 % |  |
| 41 | `WTx4` | Morphing entre 16 formes | 🔵 très fort (90.1 dB) | 5 à 86 % | Accord des 4 voix | 🟡 moyen (17.5 dB) | 6 à 91 % |  |
| 42 | `NOIS` | Résonance du filtre | 🟠 faible (9.6 dB) | 3 à 97 % | Passe-bas → passe-haut | 🟡 moyen (17.8 dB) | 9 à 68 % (milieu de course) | COLOR agit sur un bout de course |
| 43 | `TWNQ` | Résonance des 2 filtres | 🔴 presque rien (2.0 dB) | – | Écart entre les 2 filtres | 🟢 fort (31.2 dB) | 47 à 95 % (fin de course) | TIMBRE n'a presque pas d'effet ; COLOR agit sur un bout de course |
| 44 | `CLKN` | Périodicité | 🟡 moyen (22.0 dB) | 83 à 99 % (fin de course) | Nombre de niveaux (2 à 32) | 🔴 presque rien (2.3 dB) | – | COLOR n'a presque pas d'effet ; TIMBRE agit sur un bout de course |
| 45 | `CLOU` | Densité des grains | 🟡 moyen (20.6 dB) | 12 à 87 % | Hasard sur la hauteur des grains | 🔴 presque rien (1.0 dB) | – | COLOR n'a presque pas d'effet |
| 46 | `PRTC` | Densité des grains | 🟢 fort (44.1 dB) | 3 à 34 % (début de course) | Hasard sur la hauteur des grains | 🟠 faible (8.0 dB) | 9 à 89 % | TIMBRE agit sur un bout de course |
| 47 | `QPSK` | Débit | 🔴 presque rien (0.9 dB) | – | Valeur transmise (8 bits) | 🔴 presque rien (0.0 dB) | – | TIMBRE n'a presque pas d'effet ; COLOR n'a presque pas d'effet |

## En bref

- **Potards sans effet notable** : CSAW (les deux), QPSK (les deux), TIMBRE de TWNQ et de l'essaim de scies, COLOR de CLOU, DRUM et FLUT.
- **Effet concentré sur un bout de course** : TIMBRE de CLKN (83 à 99 %) et de PRTC (3 à 34 %), COLOR de KICK (15 à 40 %).
- **Percussions** : KICK, SNAR, DRUM et BELL ont des potards faibles ou concentrés.
- **Modèles qui réagissent bien sur toute la course** : FM, FBFM, WTFM, RING, SYN-, BOWD, WMAP, WLIN, WTx4.
