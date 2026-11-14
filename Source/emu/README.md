# Source/emu — la puce virtuelle (chantier en cours)

Objectif : charger un fichier de firmware Braids (`.bin` ou `.wav`) directement dans
Tresses, en émulant le STM32F103 du module au lieu de recompiler le code source.

| Étape | Contenu | État |
|---|---|---|
| 1 | Processeur Cortex-M3 (`CortexM3.*`) : instructions Thumb/Thumb-2, exceptions, NVIC, SysTick, comptage des cycles | fait |
| 2 | Périphériques du STM32F103 et carte Braids (`BraidsMachine.*`), firmware complet, plugin branché dessus | fait |
| 3 | Chargement de fichiers `.bin` et `.wav` depuis l'interface, calibration automatique, réglages par firmware, firmware enregistré dans le projet | fait |
| 4 | Temps d'accès à la flash, jauge de charge, craquements réalistes | à faire |
| 5 | Autres firmwares, optimisation, compilation sur GitHub | à faire |

## Vérifier le module complet

Voir la section « Vérifications » du README principal (`tresses_machine_test`).

L'émulateur saute les boucles d'attente du firmware (quand le programme tourne en
rond sans rien modifier en attendant une interruption). Le résultat est strictement
identique, et la part de temps ainsi sautée donne la charge du processeur.

## Vérifier le processeur

```
bash tools/armtests/run_tests.sh
```

Il faut `arm-none-eabi-gcc` (avec newlib) et, pour les comparaisons instruction par
instruction, `qemu-system-arm`. QEMU ne sert que de référence pendant les tests : il
n'y a aucun code de QEMU dans Tresses.

- **arith** : arithmétique 32/64 bits, bibliothèque C (printf, malloc, qsort…),
  à cinq niveaux d'optimisation ; référence : le même code compilé pour l'ordinateur.
- **flags** : 566 instructions, chacune avec environ 5 000 jeux d'opérandes et de
  drapeaux ; référence : QEMU.
- **irq** : SysTick, interruptions imbriquées, PendSV, SVC, PRIMASK, BASEPRI, WFI ;
  référence : QEMU.
- **braids_dsp** : les 47 modèles de Braids compilés avec les options du firmware ;
  références : l'ordinateur et QEMU.

Les durées d'instructions suivent le manuel du Cortex-M3. Les temps d'attente de la
flash ne sont pas encore pris en compte (étape 4) : les mesures de cycles sont donc
sous-estimées pour l'instant.
