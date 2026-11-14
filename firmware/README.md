# firmware/

Sources des firmwares que le module virtuel exécute :

- `eurorack/` : firmware officiel **Braids v1.9** d'Emilie Gillet (licence MIT),
  copié depuis https://github.com/pichenettes/eurorack.
- `bees-in-the-trees/` : **Bees-in-the-Trees v4.1** de Tim Churches (licence MIT),
  copié depuis https://github.com/timchurches/Mutated-Mutables, avec la version de
  stmlib sur laquelle il s'appuie.

Les versions exactes sont dans les fichiers `SOURCE.txt`. Chaque dossier contient :

- `braids/` : le firmware complet du module, sans les fichiers de circuit imprimé ;
- `stmlib/` : les parties de stmlib dont il a besoin (`utils/`, `dsp/`, `system/`, `ui/`).

Le plugin compile tous les fichiers `braids/*.cc`, plus `stmlib/utils/random.cc` et
`stmlib/system/system_clock.cc`. Les drivers (`braids/drivers/`) et
`stmlib/system/storage.h`/`uid.h` sont remplacés par les versions virtuelles de
`Source/hw/shim/`, qui ont la même interface.

Pour compiler le firmware du **module** (le fichier .wav à jouer dans Braids), il faut
le dépôt complet avec ses sous-modules (`stmlib`, `stm_audio_bootloader`) et la chaîne
de compilation ARM.

## bin/

Images binaires prêtes à être exécutées par la puce émulée :

- `braids-1.9.bin` : compilé depuis `eurorack/` (arm-none-eabi-gcc 13) ; intégré au plugin ;
- `braids-1.9.factory.bin` : sa zone de réglages après la calibration d'usine ;
- `bees-in-the-trees-4.1.bin` : compilé depuis `bees-in-the-trees/`, à charger depuis
  le bouton Firmware du plugin.
