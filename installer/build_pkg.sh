#!/bin/bash
# Fabrique l'image disque Tresses.dmg : « Installer Tresses.pkg » et
# « Désinstaller Tresses.pkg ».
#
# Usage : bash installer/build_pkg.sh <dossier des artefacts JUCE> [dossier de sortie]
#   par exemple : bash installer/build_pkg.sh build/Tresses_artefacts/Release dist
#
# Dans le dossier de sortie : Tresses.dmg, et les deux .pkg à part.
#
# L'installateur retire d'abord l'ancienne version, puis pose :
#   - le plugin VST3      dans /Library/Audio/Plug-Ins/VST3   (le dossier que Reason lit)
#   - l'application       dans /Applications                  (Tresses sans Reason)
#   - les firmwares       dans /Users/Shared/Tresses/Firmwares
set -euo pipefail
cd "$(dirname "$0")/.."

ARTEFACTS="${1:?dossier des artefacts JUCE (…/Tresses_artefacts/Release)}"
OUTDIR="${2:-dist}"
mkdir -p "$OUTDIR"
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
scripts() {  # nom -> dossier de scripts du composant (ou rien)
  if [ -d "installer/scripts/$1" ]; then
    mkdir -p "$WORK/scripts-$1"
    cp installer/scripts/"$1"/* installer/scripts/common/* "$WORK/scripts-$1/"
    chmod +x "$WORK/scripts-$1"/pre* "$WORK/scripts-$1"/post* 2>/dev/null || true
    echo --scripts "$WORK/scripts-$1"
  fi
}

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
  pkgbuild --root "$WORK/$1" --component-plist "$WORK/$1.plist" $(scripts "$1") \
           --identifier "$2" --version "$VERSION" --install-location / "$WORK/pkgs/$1.pkg"
}
component vst3      com.labtechno.tresses.vst3
component app       com.labtechno.tresses.app
component firmwares com.labtechno.tresses.firmwares

sed "s/@VERSION@/$VERSION/g" installer/distribution.xml > "$WORK/distribution.xml"
productbuild --distribution "$WORK/distribution.xml" --resources installer/resources \
             --package-path "$WORK/pkgs" "$OUTDIR/Installer Tresses.pkg"

# Désinstallateur : un paquet sans fichiers, qui exécute le script de retrait.
mkdir -p "$WORK/uninstall-pkgs"
pkgbuild --nopayload $(scripts uninstall) --identifier com.labtechno.tresses.uninstall \
         --version "$VERSION" "$WORK/uninstall-pkgs/uninstall.pkg"
sed "s/@VERSION@/$VERSION/g" installer/distribution-uninstall.xml > "$WORK/distribution-uninstall.xml"
productbuild --distribution "$WORK/distribution-uninstall.xml" --resources installer/resources-uninstall \
             --package-path "$WORK/uninstall-pkgs" "$OUTDIR/Désinstaller Tresses.pkg"

# Image disque : les deux côte à côte.
mkdir -p "$WORK/dmg"
cp "$OUTDIR/Installer Tresses.pkg" "$OUTDIR/Désinstaller Tresses.pkg" "$WORK/dmg/"
rm -f "$OUTDIR/Tresses.dmg"
hdiutil create -volname "Tresses $VERSION" -srcfolder "$WORK/dmg" -fs HFS+ -format UDZO -ov "$OUTDIR/Tresses.dmg" >/dev/null
echo "Image disque : $OUTDIR/Tresses.dmg (version $VERSION)"
