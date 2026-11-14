# Tresses — un Braids virtuel pour Reason

Tresses est un plugin instrument (VST3, AU et application autonome) qui contient un
**module Braids virtuel**. Le firmware complet du module tourne dedans **sans
modification** : boucle audio, menu, afficheur, encodeur, réglages en flash,
calibration, testeur de CV, texte défilant… Seuls les composants électroniques
(convertisseurs, afficheur, encodeur, mémoire flash) sont remplacés par des
équivalents virtuels qui se comportent comme ceux de la carte.

Deux versions se compilent depuis ce dossier et peuvent être installées côte à côte :

| Plugin | Firmware |
|---|---|
| **Tresses** | Braids v1.9, le firmware officiel de Mutable Instruments |
| **Tresses Bees** | Bees-in-the-Trees v4.1, de Tim Churches |

---

## Installation sur Mac

### Option A : compiler sur ton Mac (recommandé)

1. Ouvre le **Terminal**, tape `cd ` (avec un espace), glisse le dossier `Tresses`
   dans la fenêtre, puis appuie sur Entrée.
2. Tape une de ces commandes :
   ```
   bash build_mac.sh          # Tresses (Braids v1.9)
   bash build_mac.sh bees     # Tresses Bees
   bash build_mac.sh tout     # les deux
   ```
3. La première fois, le script peut te demander d'installer les outils Apple ou
   CMake : suis les instructions affichées, puis relance la commande.
4. À la fin, ton mot de passe de session est demandé pour copier le plugin dans
   `/Library/Audio/Plug-Ins/VST3`, le dossier que Reason scanne.

### Option B : faire compiler par GitHub

GitHub compile les deux versions en VST3 à chaque envoi sur le dépôt, ou à la
demande : onglet **Actions** → **Build macOS** → **Run workflow**.

1. Ouvre le dernier « run » réussi (coche verte) de l'onglet **Actions**.
2. En bas de la page, rubrique **Artifacts**, télécharge
   **Tresses-braids-macOS** (Braids v1.9) et/ou **Tresses-bees-macOS** (Bees).
3. Dézippe le fichier téléchargé, puis le `Tresses-…-macOS.zip` qu'il contient.
4. Dans le Terminal, tape `cd ` (avec un espace), glisse le dossier `Tresses`
   obtenu dans la fenêtre, appuie sur Entrée, puis tape : `bash install_mac.sh`

### Dans Reason

Relance Reason. Tresses apparaît dans le navigateur, rubrique **Plugins**, fabricant
**Labtechno**. S'il n'apparaît pas, relance le scan des plugins dans les préférences de
Reason (section Plugins).

---

## Utilisation

### La façade

Elle a les mêmes commandes que le module : l'afficheur à 4 caractères, l'encodeur,
les six potards (FINE, COARSE, FM, TIMBRE, MODULATION, COLOR) et les six jacks (TRIG,
V/OCT, FM, TIMBRE, COLOR, OUT).

- **Encodeur** : glisser ou molette pour tourner, clic pour appuyer, maintenir
  environ 1 seconde pour un appui long (CAL., édition du texte défilant).
- **Menu** : c'est celui du firmware, à l'identique. Comme sur le module, les réglages
  sont écrits dans la flash quand tu reviens sur WAVE.
- Double-clic sur un potard : retour au milieu (ou à la valeur par défaut).
- Les six potards sont automatisables dans Reason. Les réglages du menu ne le sont
  pas, comme sur le module (sauf en passant par META et l'entrée FM).

Comme le module, **Tresses sonne en continu**. Pour jouer des notes détachées,
active l'enveloppe interne sur le volume (`|\VCA` dans le menu), ou utilise un VCA
dans Reason.

### L'interface MIDI → CV (panneau de droite)

Elle joue le rôle d'un convertisseur MIDI/CV branché sur les jacks :

- **TRIG** ← un front à chaque note jouée, notes liées comprises (ou rien).
- **V/OCT** ← la note jouée et le pitch bend, 1 V/octave, 0 V = C1. Avec COARSE au
  milieu, chaque note MIDI joue exactement sa hauteur.
- **FM, TIMBRE, COLOR** ← rien, la molette, la vélocité, l'aftertouch ou **CV AUTO**,
  un potard de tension automatisable dans Reason. Comme sur le module, la CV TIMBRE
  est dosée par MODULATION, et la CV FM par le potard FM.

### Mémoire

Chaque Tresses est un module à part entière : il a sa propre flash, enregistrée dans
ton projet Reason, et son propre identifiant de puce (qui donne sa « signature » au
réglage SIGN). Les réglages en cours sont aussi enregistrés, même si le menu n'a pas
été refermé sur WAVE. À l'ouverture du projet, le module redémarre, avec l'animation
de démarrage.

Au tout premier démarrage, le module virtuel est calibré comme en usine : la
procédure CAL du firmware est exécutée avec 1 V puis 3 V sur V/OCT.

---

## Ce qui est identique au module, et ce qui est modélisé

**Exécuté tel quel (code du firmware)** : interruption audio à 96 kHz par blocs de 24
échantillons, rendu des modèles, enveloppe, quantificateur, BITS/RATE/SIGN/DRFT,
gestion des triggers et de leur retard, META, RANG, menu, afficheur, encodeur,
calibration, sauvegarde en flash.

**Remplacé par des équivalents virtuels de même interface** (`Source/hw/shim/`) :

- **ADC 12 bits (MCP3204)** : un canal toutes les 3 interruptions, balayage complet
  tous les 12 échantillons, comme sur la carte.
- **Potard FINE** : ADC interne du STM32, avec le filtrage du firmware.
- **TRIG** : détection de fronts identique.
- **Encodeur** : vraie séquence quadrature et anti-rebond du firmware à 1 kHz.
- **Afficheur** : police 14 segments du firmware, luminosité (BRIG) comprise.
- **Flash** : même algorithme de stockage.
- **Identifiant de puce**.
- **Timers** : TIM1 à 96 kHz, SysTick à 1 kHz.

**Modélisé à partir du manuel et du firmware** (`Source/hw/AnalogFrontEnd.h`) :

- Fenêtre V/OCT d'environ −4 V à +6 V (déduite du firmware), 408 codes ADC par volt.
- COARSE ±4 V, FINE ±1 demi-ton.
- TIMBRE et COLOR : 0–5 V = course complète du potard.
- FM : ±5 V à pleine échelle avec l'atténuverseur à fond. C'est une hypothèse, la
  sensibilité n'est pas documentée.
- Pas de bruit de conversion.
- Sortie : la pleine échelle du DAC vaut 0 dBFS. Le signal est rééchantillonné à la
  fréquence de Reason, et passe tel quel si Reason tourne à 96 kHz.

---

## Tester un firmware

Le son, le menu et l'afficheur viennent directement des sources :

| Firmware | Dossier |
|---|---|
| Officiel | `firmware/eurorack/braids/` |
| Bees | `firmware/bees-in-the-trees/braids/` |
| Ton fork | `cmake -B build -DBRAIDS_FIRMWARE_DIR=/chemin/vers/ton/fork` |

Tous les fichiers `braids/*.cc` sont compilés sans modification. Un nouveau modèle
ou un nouveau réglage s'ajoute donc exactement comme dans le firmware, et apparaît
tout seul dans le menu du plugin.

Deux cas demandent une retouche côté plugin :

- **Tu modifies `main()` ou ajoutes une variable globale dans `braids.cc`** :
  reporte-le dans `Source/hw/slot_bridge.inc`. C'est là que le contenu de la boucle
  `while (1)` est reproduit, et que la RAM est remise à zéro au démarrage.
- **Tu inclus un nouvel en-tête système** : ajoute-le dans `Source/hw/slot_prefix.h`.

À propos de **Bees-in-the-Trees** : avec ses réglages par défaut, le réglage FTUN
décale la hauteur d'environ −26 cents. C'est le comportement de son code, donc aussi
celui d'un module réel sous Bees. Ajuste FTUN dans le menu.

### Outil de test hors plugin

`tools/module_test.cpp` (option `-DTRESSES_BUILD_TOOLS=ON -DTRESSES_BUILD_PLUGIN=OFF`,
sans JUCE) fait démarrer le module virtuel, navigue dans le menu, vérifie la justesse,
fait tourner plusieurs modules en parallèle et rend des fichiers WAV.

---

## Limites

- **16 Tresses au maximum** ouverts en même temps dans un projet (une copie du
  firmware par instance, réglable avec `-DTRESSES_MAX_INSTANCES`).
- **Réutilisation d'un emplacement** : quand une instance est supprimée puis qu'une
  autre la remplace dans la même session, les variables `static` déclarées à
  l'intérieur des fonctions du firmware ne sont pas remises à zéro. Elles ne le sont
  qu'au chargement du plugin.
- **Prochaine étape** : la jauge de puissance, pour vérifier qu'un firmware tient
  dans le budget du STM32F103 du module.

---

## Structure

```
Source/hw/shim/                drivers virtuels (même interface que braids/drivers)
Source/hw/slot_bridge.inc      pont plugin <-> firmware, boucle principale
Source/hw/AnalogFrontEnd.h     potards + jacks -> codes ADC
Source/hw/FirmwareSlot.h       interface d'un module virtuel
cmake/TressesFirmwareSlots.cmake  compile le firmware une fois par instance possible
Source/engine/Resampler.h      96 kHz -> fréquence du DAW
Source/PluginProcessor.*       MIDI -> CV, paramètres, état
Source/PluginEditor.*          façade et interface MIDI -> CV
firmware/eurorack/             firmware officiel Braids v1.9 + stmlib
firmware/bees-in-the-trees/    Bees-in-the-Trees v4.1 + stmlib
```

## Licences

- **Braids et stmlib** : © Emilie Gillet, licence MIT.
- **Bees-in-the-Trees** : © Tim Churches, licence MIT (voir les en-têtes des fichiers).
- **JUCE 8** : double licence AGPLv3 ou commerciale. Pour un usage personnel, aucune
  démarche n'est nécessaire. Avant de distribuer le plugin compilé, vérifie les
  conditions de JUCE.

Tresses n'est ni un produit ni un clone officiel de Mutable Instruments.
