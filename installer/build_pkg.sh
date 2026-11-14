#!/bin/bash
# Fabrique l'installateur Mac de Tresses (Tresses-Installateur.pkg).
#
# Usage : bash installer/build_pkg.sh <dossier des artefacts JUCE> [sortie.pkg]
#   par exemple : bash installer/build_pkg.sh build/Tresses_artefacts/Release
#
# L'installateur pose :
#   - le plugin VST3      dans /Library/Audio/Plug-Ins/VST3   (le dossier que Reason lit)
#   - l'application       dans /Applications                  (Tresses sans Reason)
#   - les firmwares       dans /Users/Shared/Tresses/Firmwares
set -euo pipefail
cd "$(dirname "$0")/.."

ARTEFACTS="${1:?dossier des artefacts JUCE (…/Tresses_artefacts/Release)}"
OUT="${2:-Tresses-Installateur.pkg}"
VERSION="$(sed -n 's/^project(Tresses VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

stage() {  # nom, chemin d'installation, source
  mkdir -p "$WORK/$1/$2"
  cp -R "$3" "$WORK/$1/$2/"
}

stage vst3 "Library/Audio/Plug-Ins/VST3" "$ARTEFACTS/VST3/Tresses.vst3"
stage app  "Applications"                "$ARTEFACTS/Standalone/Tresses.app"
mkdir -p "$WORK/firmwares/Users/Shared/Tresses/Firmwares"
cp firmware/bin/braids-1.9.bin firmware/bin/bees-in-the-trees-4.1.bin "$WORK/firmwares/Users/Shared/Tresses/Firmwares/"
chmod -R a+rX "$WORK/firmwares"

# Signature locale (ad hoc) : sans elle, un Mac Apple Silicon refuse de charger le code.
codesign --force --deep --sign - "$WORK/vst3/Library/Audio/Plug-Ins/VST3/Tresses.vst3"
codesign --force --deep --sign - "$WORK/app/Applications/Tresses.app"

mkdir -p "$WORK/pkgs"
component() {  # nom, identifiant
  # Les bundles ne doivent pas être « déplaçables » : sinon, s'il existe déjà un
  # Tresses ailleurs sur le disque, l'installateur l'écraserait à sa place.
  pkgbuild --analyze --root "$WORK/$1" "$WORK/$1.plist" >/dev/null
  python3 - "$WORK/$1.plist" <<'PY'
import plistlib, sys
path = sys.argv[1]
with open(path, 'rb') as f:
    bundles = plistlib.load(f)
for b in bundles:
    b['BundleIsRelocatable'] = False
with open(path, 'wb') as f:
    plistlib.dump(bundles, f)
PY
  pkgbuild --root "$WORK/$1" --component-plist "$WORK/$1.plist" --identifier "$2" \
           --version "$VERSION" --install-location / "$WORK/pkgs/$1.pkg"
}
component vst3      com.labtechno.tresses.vst3
component app       com.labtechno.tresses.app
component firmwares com.labtechno.tresses.firmwares

sed "s/@VERSION@/$VERSION/g" installer/distribution.xml > "$WORK/distribution.xml"
productbuild --distribution "$WORK/distribution.xml" --resources installer/resources \
             --package-path "$WORK/pkgs" "$OUT"
echo "Installateur : $OUT (version $VERSION)"
