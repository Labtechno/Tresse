#!/bin/bash
# Compile Tresses sur Mac (Apple Silicon + Intel) puis l'installe pour Reason.
# Usage, dans le Terminal, depuis ce dossier :
#   bash build_mac.sh          -> Tresses (firmware officiel Braids v1.9)
#   bash build_mac.sh bees     -> Tresses Bees (firmware Bees-in-the-Trees v4.1)
#   bash build_mac.sh tout     -> les deux
set -euo pipefail
cd "$(dirname "$0")"

if ! xcode-select -p >/dev/null 2>&1; then
  echo "→ Les outils de développement Apple (Command Line Tools) ne sont pas installés."
  echo "  Une fenêtre va s'ouvrir : clique sur « Installer », attends la fin, puis relance : bash build_mac.sh"
  xcode-select --install || true
  exit 1
fi

if ! command -v cmake >/dev/null 2>&1; then
  if command -v brew >/dev/null 2>&1; then
    echo "→ Installation de CMake avec Homebrew…"
    brew install cmake
  else
    echo "→ CMake est nécessaire et Homebrew n'est pas installé."
    echo "  1. Installe Homebrew : https://brew.sh (une ligne à copier dans le Terminal)"
    echo "  2. Puis : brew install cmake"
    echo "  3. Relance : bash build_mac.sh"
    exit 1
  fi
fi

case "${1:-braids}" in
  bees) variants=(bees) ;;
  tout|all) variants=(braids bees) ;;
  *) variants=(braids) ;;
esac

for variant in "${variants[@]}"; do
  dir="build-$variant"
  echo "→ Configuration ($variant ; le premier lancement télécharge JUCE, compter 1 à 2 minutes)…"
  cmake -B "$dir" -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
        -DTRESSES_FIRMWARE="$variant"
  echo "→ Compilation ($variant)…"
  cmake --build "$dir" --config Release -j "$(sysctl -n hw.ncpu)"
  echo "→ Installation ($variant)…"
  bash install_mac.sh "$dir/Tresses_artefacts/Release"
done
