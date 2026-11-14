# Tresses — un Braids émulé pour Reason

Tresses est un plugin instrument (VST3, AU et application autonome) qui contient un
**module Braids émulé** : une puce STM32F103 virtuelle exécute le fichier binaire du
firmware, instruction par instruction, entourée des composants de la carte (timer
audio, convertisseurs, afficheur, encodeur, mémoire flash).

Le plugin démarre sur le firmware officiel Braids v1.9, et peut charger n'importe
quel fichier de firmware `.bin` ou `.wav` de mise à jour. Le détail de l'émulation
et de ses vérifications est dans [`Source/emu/README.md`](Source/emu/README.md).

---

## Installation sur Mac

### Par GitHub (le plus simple)

GitHub compile le VST3 à chaque envoi sur le dépôt, ou à la demande : onglet
**Actions** → **Build macOS** → **Run workflow**.

1. Ouvre le dernier « run » réussi (coche verte) de l'onglet **Actions**.
2. En bas de la page, rubrique **Artifacts**, télécharge **Tresses-macOS**.
3. Dézippe le fichier téléchargé, puis le `Tresses-macOS.zip` qu'il contient.
4. Dans le Terminal, tape `cd ` (avec un espace), glisse le dossier `Tresses`
   obtenu dans la fenêtre, appuie sur Entrée, puis tape : `bash install_mac.sh`
5. Relance Reason : Tresses est dans **Plugins**, fabricant **Labtechno**.

### En compilant sur ton Mac

Dans le Terminal, depuis le dossier du projet : `bash build_mac.sh`

---

## Utilisation

### La façade

Elle a les mêmes commandes que le module : l'afficheur à 4 caractères, l'encodeur,
les six potards (FINE, COARSE, FM, TIMBRE, MODULATION, COLOR) et les six jacks.

- **Encodeur** : glisser ou molette pour tourner, clic pour appuyer, maintenir
  environ 1 seconde pour un appui long (CAL., édition du texte défilant).
- **Menu** : c'est celui du firmware. Comme sur le module, les réglages sont écrits
  en flash quand tu reviens sur WAVE.
- **Processeur du module** : en haut à droite, la part du temps pendant laquelle
  le processeur du module travaille. À 100 %, le firmware n'arrive plus à calculer
  le son à temps : il rejoue de vieux morceaux de son (bégaiement, craquements)
  et l'encodeur ne répond presque plus, comme sur le module. Avec le firmware
  d'origine, les modèles vont d'environ 55 % à 90 % (le plus lourd : les trois
  carrés désaccordés). C'est une estimation : voir Limites.

Comme le module, **Tresses sonne en continu**. Pour des notes détachées, active
l'enveloppe interne sur le volume (`|\VCA` dans le menu) ou utilise un VCA de Reason.

### Changer de firmware

Le bouton **Firmware** (au-dessus de l'afficheur) ouvre un menu :

- **Braids v1.9 (fourni avec Tresses)** ;
- **Charger un fichier .bin ou .wav…** : une image d'application `.bin`, ou le
  fichier `.wav` de mise à jour audio, celui qu'on joue dans un vrai Braids.

Ce qui se passe au chargement :

- Le `.wav` est démodulé et chaque paquet est vérifié (CRC). Un fichier abîmé ou
  compressé (MP3…) est refusé avec un message.
- Le module redémarre sur le nouveau firmware, comme après une mise à jour.
- **Calibration automatique** : la première fois, Tresses exécute la procédure CAL
  du firmware (1 V puis 3 V) en actionnant son menu, comme en sortie d'usine. Si le
  firmware n'a pas de menu CAL. reconnaissable, un message le signale.
- **Une mémoire par firmware** : chaque firmware garde ses propres réglages. En
  revenant à un firmware déjà utilisé, on retrouve ses réglages.
- **Le firmware voyage avec le projet** : le fichier chargé est enregistré dans le
  projet Reason, avec les réglages de chaque firmware utilisé.

Le dossier `firmware/bin/` contient deux images, extraites des fichiers `.wav`
officiels publiés par leurs auteurs (ce sont donc les mêmes programmes que dans un
vrai module mis à jour) :

- `braids-1.9.bin` : Braids v1.9 (Mutable Instruments), le firmware intégré à Tresses ;
- `bees-in-the-trees-4.1.bin` : Bees-in-the-Trees v4.1 (Tim Churches), version
  « factory encoder », pour l'encodeur d'origine du Braids.

Les fichiers `.wav` officiels se chargent aussi directement. Essayés : Braids 1.8,
Braids 1.9, Braids « FM range fix », Bees-in-the-Trees v4.1 (encodeur d'origine et
encodeur inversé). Tous démarrent, se calibrent et jouent juste.

L'effet de TIMBRE et COLOR sur chaque modèle est mesuré dans le dossier
[`EFFET SUR POTENTIOMETRE`](EFFET%20SUR%20POTENTIOMETRE/), un fichier par firmware.

Pour fabriquer le `.wav` de mise à jour d'un `.bin` (par exemple pour flasher ton
propre firmware sur un vrai Braids) : `python3 tools/firmware_wav.py firmware.bin`

### L'interface MIDI → CV (panneau de droite) et le câblage dans Reason

Un plugin ne reçoit pas de câbles CV : Reason lui envoie des notes et des valeurs de
paramètres. Ce panneau les convertit en tensions sur les jacks du module.

| Jack | Dans Reason | Réglage dans Tresses |
|---|---|---|
| **TRIG** | entrée Gate du rack du plugin | source TRIG : « Notes MIDI » |
| **V/OCT** | entrée Note (et Pitch Bend) | rien, 0 V = C1, 1 V/octave |
| **FM** | une entrée CV du rack, affectée au paramètre « FM CV » | source FM : « Automation », puis l'atténuverseur FM |
| **TIMBRE** | une entrée CV, affectée à « TIMBRE CV » | source TIMBRE : « Automation », puis MODULATION |
| **COLOR** | une entrée CV, affectée à « COLOR CV » | source COLOR : « Automation » |

L'affectation des entrées CV se fait dans le **CV Programmer** du rack Reason.

### Mémoire

Chaque Tresses a sa propre flash, enregistrée dans le projet Reason, et son propre
identifiant de puce (la « signature » du réglage SIGN). À l'ouverture du projet, le
module est remis sous tension, comme un module qu'on rallume : les réglages du menu
sont ceux de la dernière sauvegarde (retour sur WAVE). Un Tresses neuf démarre avec
des réglages d'usine : la calibration CAL du firmware y a été faite à 1 V et 3 V.

---

## Ce qui est émulé, et ce qui est modélisé

**Exécuté tel quel** : tout le firmware, sous forme binaire.

**Émulé d'après la documentation des composants** (`Source/emu/`) :

- le processeur Cortex-M3, ses interruptions et son timer système ;
- le timer audio (TIM1), les deux liaisons SPI, l'ADC externe MCP3204 et le DAC ;
- l'ADC interne et son DMA (potard FINE) ;
- les broches : entrée TRIG, encodeur en quadrature, registre à décalage de
  l'afficheur ;
- la flash : programmation, effacement des pages, et le blocage du processeur
  pendant ces opérations (le son se fige un instant à la sauvegarde, comme sur le
  module).

**Modélisé à partir du manuel et du firmware** (`Source/hw/AnalogFrontEnd.h`) :
les plages des potards et des entrées. V/OCT couvre environ −4 V à +6 V, COARSE
±4 V, TIMBRE et COLOR 0–5 V. La sensibilité de l'entrée FM (±5 V à pleine échelle)
est une hypothèse. Il n'y a pas de bruit de conversion.

---

## Vérifications

```
bash tools/armtests/run_tests.sh                       # le processeur
cmake -B build-tools -DTRESSES_BUILD_PLUGIN=OFF -DTRESSES_BUILD_TOOLS=ON
cmake --build build-tools
build-tools/tresses_machine_test firmware/bin/braids-1.9.bin boot      # démarrage, menu, flash, CAL
build-tools/tresses_machine_test firmware/bin/braids-1.9.bin compare   # audio contre la référence
build-tools/tresses_machine_test un_firmware.wav load                  # lecture d'un fichier + CAL
build-tools/tresses_machine_test un_firmware.wav cpu                   # charge du processeur, modèle par modèle, et surcharge
build-tools/tresses_knob_map docs/carte-potards-brut.csv              # influence de TIMBRE et COLOR, modèle par modèle
```

- **Processeur** : 566 instructions comparées à QEMU, la bibliothèque C et les 47
  modèles de Braids comparés au même code compilé pour l'ordinateur.
- **Module complet** : le firmware démarre, son menu fonctionne, il sauve et relit
  ses réglages, et la procédure CAL donne des notes justes.
- **Audio** : pour les 47 modèles, avec des mouvements de potards, de hauteur, de FM
  et des triggers, la sortie est identique échantillon par échantillon à celle de la
  référence (le même firmware recompilé pour l'ordinateur, `Source/hw/`).
  Une exception instructive : à hauteur très basse, le modèle SAW COMB fait un
  décalage de plus de 32 bits, dont le résultat dépend du processeur. La version
  recompilée pour PC se trompe ; l'émulateur donne le résultat d'un vrai ARM.

- **Charge du processeur** : avec le firmware d'origine, aucun des 47 modèles ne
  dépasse 100 % (de 55 % à 90 % selon le modèle, la hauteur et les réglages), ce qui
  correspond au module, qui les joue tous sans décrocher. En ralentissant
  artificiellement la mémoire du processeur, la jauge monte à 100 % et le son
  bégaie exactement comme le prévoit le code du firmware (4 blocs de 24
  échantillons rejoués).

---

## Limites actuelles

- **Seulement des firmwares pour Braids** (image d'application à l'adresse
  0x08004000). Le bootloader du module n'est pas émulé : Tresses décode lui-même
  le `.wav`.
- **Charge du processeur : une estimation, pas une mesure.** Les durées des
  instructions suivent le manuel du Cortex-M3 et les temps d'attente de la mémoire
  flash suivent la documentation du STM32F103, mais les détails internes de la puce
  ne sont pas publiés. La jauge n'a pas encore été comparée à un vrai Braids : il
  faut compter quelques points d'écart. Un firmware qui affiche 70 % passera sur le
  module ; un firmware à 97 % est à vérifier sur le vrai module.
- **Bees-in-the-Trees « reversed encoder »** : cette version est faite pour des
  encodeurs de remplacement qui tournent à l'envers. Dans Tresses (comme sur un
  Braids d'origine), l'encodeur tourne alors dans le mauvais sens : prends la
  version « factory encoder ».
- **Jauge et boucle d'attente** : la jauge compte comme « libre » le temps que le
  firmware passe à tourner en rond en attendant le prochain bloc de son. Si un
  firmware maison attend d'une façon inhabituelle (en modifiant la mémoire à
  chaque tour, par exemple), la jauge le croira occupé et affichera trop haut.
- **Calcul** : l'émulation reste gourmande, car chaque instruction du module est
  rejouée. Compter environ un tiers d'un cœur de serveur de test par instance,
  nettement moins sur un Mac récent.
- **Réouverture d'un projet** : le module redémarre (animation comprise) et les
  réglages de menu non sauvés sur WAVE sont perdus, comme sur le module.

## Structure

```
Source/emu/CortexM3.*          le processeur
Source/emu/BraidsMachine.*     la puce et la carte du module
Source/emu/FirmwareFile.*      lecture des .bin et des .wav de mise à jour
Source/emu/FactorySetup.*      calibration d'usine par le menu du firmware
Source/hw/AnalogFrontEnd.h     potards + jacks -> codes des convertisseurs
Source/hw/ (le reste)          référence pour les tests : firmware recompilé pour PC
Source/engine/Resampler.h      96 kHz -> fréquence du DAW
Source/PluginProcessor.*       MIDI -> CV, paramètres, état
Source/PluginEditor.*          façade et interface MIDI -> CV
firmware/bin/                  image du firmware et réglages d'usine
firmware/eurorack/             sources de Braids v1.9 (+ stmlib) : référence des tests, base d'un firmware maison
EFFET SUR POTENTIOMETRE/       effet de TIMBRE et COLOR, modèle par modèle, un fichier par firmware
tools/firmware_wav.py          .bin -> .wav de mise à jour
tools/                         bancs de test
```

## Licences

- **Braids et stmlib** : © Emilie Gillet, licence MIT.
- **Bees-in-the-Trees** : © Tim Churches, licence MIT.
- **Émulateur (Source/emu)** : écrit pour Tresses, licence MIT, sans code tiers.
- **JUCE 8** : double licence AGPLv3 ou commerciale. Pour un usage personnel, aucune
  démarche n'est nécessaire.

Tresses n'est ni un produit ni un clone officiel de Mutable Instruments.
