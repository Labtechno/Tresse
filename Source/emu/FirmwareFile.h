// Tresses — lecture des fichiers de firmware.
//
//   .bin : image brute de l'application (celle que le bootloader écrit à 0x08004000)
//   .wav : fichier de mise à jour audio de Mutable Instruments (modulation QPSK,
//          porteuse 6 kHz, 12 kbit/s, paquets de 256 octets protégés par CRC32),
//          celui qu'on joue dans l'entrée du module pour le mettre à jour.
//
// Le .wav est décodé directement (démodulation + vérification des CRC), ce qui
// donne la même image que celle que le bootloader du module écrirait en flash.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace tresses {
namespace emu {

struct FirmwareImage {
  std::vector<uint8_t> data;   // image de l'application
  std::string format;          // "bin" ou "wav"
  int packets = 0;             // paquets valides lus dans un .wav
};

// Renvoie false et remplit `error` (en français) si le fichier n'est pas utilisable.
bool readFirmwareFile(const uint8_t* file, size_t size, FirmwareImage& image, std::string& error);

// Identifiant court d'une image (CRC32 en hexadécimal), pour retrouver ses réglages.
std::string firmwareId(const std::vector<uint8_t>& image);

uint32_t crc32(const uint8_t* data, size_t size);

}  // namespace emu
}  // namespace tresses
