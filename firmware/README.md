# firmware/

## bin/

Images binaires exécutées par la puce émulée de Tresses. Elles sont extraites des
fichiers `.wav` de mise à jour officiels, publiés par leurs auteurs : ce sont les
mêmes programmes que dans un vrai module mis à jour.

- `braids-1.9.bin` : Braids v1.9 (Mutable Instruments), intégré au plugin ;
- `braids-1.9.factory.bin` : sa zone de réglages après la calibration d'usine
  (procédure CAL du firmware, 1 V puis 3 V) ;
- `bees-in-the-trees-4.1.bin` : Bees-in-the-Trees v4.1 (Tim Churches), version
  « factory encoder », à charger depuis le bouton Firmware du plugin.

Pour extraire l'image d'un autre fichier de mise à jour :
`tresses_machine_test fichier.wav extract image.bin`

## eurorack/

Sources du firmware officiel **Braids v1.9** d'Emilie Gillet (licence MIT), copiées
depuis https://github.com/pichenettes/eurorack (version exacte dans `SOURCE.txt`) :

- `braids/` : le firmware complet du module, sans les fichiers de circuit imprimé ;
- `stmlib/` : les parties de stmlib dont il a besoin.

Elles servent à deux choses :

- **la référence des tests** : le même firmware, recompilé pour l'ordinateur
  (`cmake/TressesFirmwareSlots.cmake`, drivers virtuels de `Source/hw/shim/`), doit
  donner exactement le même son que le firmware officiel émulé ;
- **la base d'un firmware maison** : on part de ce code qui marche, on le modifie,
  on l'essaie dans Tresses, puis sur le vrai module.

Pour compiler un firmware pour le **module** (le `.bin`, puis le `.wav` à jouer dans
Braids avec `tools/firmware_wav.py`), il faut la chaîne de compilation ARM
(`arm-none-eabi-gcc`) et le dépôt complet avec ses sous-modules (`stmlib`,
`stm_audio_bootloader`).
