#!/bin/bash
# Installe Tresses (et Tresses Bees s'il a été compilé) sur Mac :
#   - VST3  -> /Library/Audio/Plug-Ins/VST3        (le dossier que Reason scanne)
#   - AU    -> ~/Library/Audio/Plug-Ins/Components  (Logic, etc.)
#   - appli autonome -> ~/Applications
#
# Usage : bash install_mac.sh [dossier contenant VST3/, AU/ et Standalone/]
# (sans argument : le dossier où se trouve ce script)
set -euo pipefail
cd "$(dirname "$0")"
SRC="${1:-.}"
shopt -s nullglob

vst3s=("$SRC"/VST3/*.vst3)
if [ ${#vst3s[@]} -eq 0 ]; then
  echo "Aucun plugin .vst3 trouvé dans « $SRC/VST3 »."
  exit 1
fi

# Retire l'attribut « téléchargé d'internet » et signe localement (signature
# ad hoc) : sans ça, macOS peut refuser de charger un plugin compilé ailleurs.
prepare() {
  xattr -dr com.apple.quarantine "$1" 2>/dev/null || true
  codesign --force --deep --sign - "$1" >/dev/null 2>&1 || true
}

echo "→ VST3 : /Library/Audio/Plug-Ins/VST3 (ton mot de passe de session va être demandé)"
sudo mkdir -p /Library/Audio/Plug-Ins/VST3
for bundle in "${vst3s[@]}"; do
  name="$(basename "$bundle")"
  echo "   $name"
  sudo rm -rf "/Library/Audio/Plug-Ins/VST3/$name"
  sudo cp -R "$bundle" /Library/Audio/Plug-Ins/VST3/
  sudo xattr -dr com.apple.quarantine "/Library/Audio/Plug-Ins/VST3/$name" 2>/dev/null || true
  sudo codesign --force --deep --sign - "/Library/Audio/Plug-Ins/VST3/$name" >/dev/null 2>&1 || true
done

for bundle in "$SRC"/AU/*.component; do
  name="$(basename "$bundle")"
  echo "→ AU : ~/Library/Audio/Plug-Ins/Components/$name"
  mkdir -p ~/Library/Audio/Plug-Ins/Components
  rm -rf ~/Library/Audio/Plug-Ins/Components/"$name"
  cp -R "$bundle" ~/Library/Audio/Plug-Ins/Components/
  prepare ~/Library/Audio/Plug-Ins/Components/"$name"
done

for bundle in "$SRC"/Standalone/*.app; do
  name="$(basename "$bundle")"
  echo "→ Appli autonome : ~/Applications/$name"
  mkdir -p ~/Applications
  rm -rf ~/Applications/"$name"
  cp -R "$bundle" ~/Applications/
  prepare ~/Applications/"$name"
done

echo ""
echo "✓ Terminé. Relance Reason : Tresses apparaît dans le navigateur, rubrique Plugins (fabricant « Labtechno »)."
