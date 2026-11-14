// Tresses — lecture des fichiers de firmware (voir FirmwareFile.h). Licence MIT.
//
// Le format du .wav est celui de stm_audio_bootloader/qpsk/encoder.py (Emilie
// Gillet) : chaque symbole de 2 bits dure une période de porteuse et module sa
// phase ; un paquet = préambule (8 x 0x00, 4 x 0x99, 4 x 0xCC), 256 octets de
// données, CRC32. Le décodeur ci-dessous est écrit pour Tresses.

#include "FirmwareFile.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstring>

namespace tresses {
namespace emu {

namespace {

constexpr double kCarrierHz = 6000.0;
constexpr double kSymbolRate = 6000.0;
constexpr double kPi = 3.14159265358979323846;

uint32_t le32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24); }
uint16_t le16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }

// Lit le premier canal d'un fichier WAV (PCM 8/16/24/32 bits ou flottant 32 bits).
bool readWav(const uint8_t* file, size_t size, std::vector<float>& samples, double& sampleRate,
             std::string& error) {
  if (size < 12 || std::memcmp(file, "RIFF", 4) != 0 || std::memcmp(file + 8, "WAVE", 4) != 0) {
    error = "Ce n'est pas un fichier WAV.";
    return false;
  }
  int format = 0, channels = 0, bits = 0;
  size_t pos = 12;
  const uint8_t* data = nullptr;
  size_t dataSize = 0;
  while (pos + 8 <= size) {
    const uint32_t chunkSize = le32(file + pos + 4);
    const uint8_t* body = file + pos + 8;
    const size_t available = std::min<size_t>(chunkSize, size - pos - 8);
    if (std::memcmp(file + pos, "fmt ", 4) == 0 && available >= 16) {
      format = le16(body);
      channels = le16(body + 2);
      sampleRate = le32(body + 4);
      bits = le16(body + 14);
      if (format == 0xfffe && available >= 26) format = le16(body + 24);
    } else if (std::memcmp(file + pos, "data", 4) == 0) {
      data = body;
      dataSize = available;
    }
    pos += 8 + static_cast<size_t>(chunkSize) + (chunkSize & 1);
  }
  if (data == nullptr || channels < 1 || sampleRate < 8000.0 ||
      !((format == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32)) || (format == 3 && bits == 32))) {
    error = "Ce format de fichier WAV n'est pas pris en charge.";
    return false;
  }
  const size_t bytes = static_cast<size_t>(bits / 8);
  const size_t frame = bytes * static_cast<size_t>(channels);
  const size_t count = dataSize / frame;
  samples.resize(count);
  for (size_t i = 0; i < count; ++i) {
    const uint8_t* p = data + i * frame;
    float v;
    if (format == 3) { std::memcpy(&v, p, 4); }
    else if (bits == 8) v = (static_cast<float>(p[0]) - 128.0f) / 128.0f;
    else if (bits == 16) v = static_cast<float>(static_cast<int16_t>(le16(p))) / 32768.0f;
    else if (bits == 24) v = static_cast<float>(static_cast<int32_t>((p[0] << 8) | (p[1] << 16) | (static_cast<uint32_t>(p[2]) << 24)) >> 8) / 8388608.0f;
    else v = static_cast<float>(static_cast<int32_t>(le32(p))) / 2147483648.0f;
    samples[i] = v;
  }
  return true;
}

// Cherche les paquets dans la suite de symboles. Renvoie le nombre de paquets valides.
int extractPackets(const std::vector<uint8_t>& symbols, size_t packetSize, std::vector<uint8_t>& out,
                   bool& gap) {
  static const uint8_t marker[32] = { 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1,
                                      3, 0, 3, 0, 3, 0, 3, 0, 3, 0, 3, 0, 3, 0, 3, 0 };
  const size_t packetSymbols = 4 * (packetSize + 4);
  std::vector<uint8_t> bytes(packetSize + 4);
  int packets = 0;
  size_t i = 0;
  size_t previousEnd = 0;
  gap = false;
  while (i + 32 + packetSymbols <= symbols.size()) {
    if (std::memcmp(&symbols[i], marker, 32) != 0) { ++i; continue; }
    const uint8_t* s = &symbols[i + 32];
    for (size_t b = 0; b < packetSize + 4; ++b) {
      bytes[b] = static_cast<uint8_t>((s[4 * b] << 6) | (s[4 * b + 1] << 4) | (s[4 * b + 2] << 2) | s[4 * b + 3]);
    }
    const uint32_t expected = (static_cast<uint32_t>(bytes[packetSize]) << 24) | (bytes[packetSize + 1] << 16) |
                              (bytes[packetSize + 2] << 8) | bytes[packetSize + 3];
    if (crc32(bytes.data(), packetSize) == expected) {
      // Entre deux paquets il n'y a que le préambule ou une courte pause : un trou
      // de la longueur d'un paquet signifie qu'un paquet abîmé a été sauté.
      if (packets > 0 && i - previousEnd >= 32 + packetSymbols) gap = true;
      previousEnd = i + 32 + packetSymbols;
      out.insert(out.end(), bytes.begin(), bytes.begin() + static_cast<long>(packetSize));
      ++packets;
      i += 32 + packetSymbols;
    } else {
      ++i;
    }
  }
  return packets;
}

bool decodeWav(const uint8_t* file, size_t size, FirmwareImage& image, std::string& error) {
  std::vector<float> x;
  double sampleRate = 0.0;
  if (!readWav(file, size, x, sampleRate, error)) return false;
  const double samplesPerSymbol = sampleRate / kSymbolRate;
  if (samplesPerSymbol < 4.0 || x.size() < 1000) {
    error = "Fichier WAV trop court, ou de fréquence d'échantillonnage trop basse.";
    return false;
  }

  // Passage en bande de base : sommes cumulées de x[n] * exp(-j 2 pi fc n / fe).
  std::vector<std::complex<double>> prefix(x.size() + 1);
  const double w = 2.0 * kPi * kCarrierHz / sampleRate;
  for (size_t n = 0; n < x.size(); ++n) {
    prefix[n + 1] = prefix[n] + static_cast<double>(x[n]) * std::complex<double>(std::cos(w * static_cast<double>(n)), -std::sin(w * static_cast<double>(n)));
  }
  auto symbolAt = [&](double start) {
    const size_t a = static_cast<size_t>(std::llround(start));
    const size_t b = static_cast<size_t>(std::llround(start + samplesPerSymbol));
    return b <= x.size() ? prefix[b] - prefix[a] : std::complex<double>(0.0, 0.0);
  };

  // Calage sur les symboles : le bon décalage est celui où chaque fenêtre ne
  // contient qu'un symbole, donc où l'énergie récupérée est maximale.
  const size_t symbolCount = static_cast<size_t>(static_cast<double>(x.size()) / samplesPerSymbol) - 1;
  int bestOffset = 0;
  double bestEnergy = -1.0;
  const int offsets = static_cast<int>(std::ceil(samplesPerSymbol));
  for (int o = 0; o < offsets; ++o) {
    double energy = 0.0;
    for (size_t k = 0; k < symbolCount; ++k) energy += std::abs(symbolAt(o + static_cast<double>(k) * samplesPerSymbol));
    if (energy > bestEnergy) { bestEnergy = energy; bestOffset = o; }
  }

  // Phase de la porteuse à 90 degrés près (puissance quatrième), puis essai des
  // quatre rotations : la bonne est celle dont les paquets ont un CRC correct.
  std::vector<std::complex<double>> z(symbolCount);
  std::complex<double> fourth(0.0, 0.0);
  for (size_t k = 0; k < symbolCount; ++k) {
    z[k] = symbolAt(bestOffset + static_cast<double>(k) * samplesPerSymbol);
    const std::complex<double> z2 = z[k] * z[k];
    fourth += z2 * z2;
  }
  const double phase = (std::arg(fourth) - kPi) / 4.0;

  image = FirmwareImage();
  image.format = "wav";
  bool damaged = false;
  for (size_t packetSize : { size_t { 256 }, size_t { 128 }, size_t { 64 }, size_t { 512 }, size_t { 1024 } }) {
    for (int rotation = 0; rotation < 4; ++rotation) {
      const std::complex<double> correction = std::polar(1.0, -(phase + rotation * kPi / 2.0));
      std::vector<uint8_t> symbols(symbolCount);
      for (size_t k = 0; k < symbolCount; ++k) {
        const std::complex<double> v = z[k] * correction;
        symbols[k] = static_cast<uint8_t>((v.real() > 0.0 ? 2 : 0) | (v.imag() < 0.0 ? 1 : 0));
      }
      std::vector<uint8_t> data;
      bool gap = false;
      const int packets = extractPackets(symbols, packetSize, data, gap);
      if (packets > image.packets) {
        image.packets = packets;
        image.data = std::move(data);
        damaged = gap;
      }
    }
    if (image.packets > 0) break;
  }
  if (image.packets == 0) {
    error = "Aucun paquet de firmware valide dans ce fichier WAV : ce n'est pas un fichier de mise à jour, ou il a été compressé.";
    return false;
  }
  if (damaged) {
    error = "Un paquet de ce fichier WAV est illisible (fichier abîmé ou compressé). Utilise le fichier d'origine.";
    return false;
  }
  return true;
}

}  // namespace

uint32_t crc32(const uint8_t* data, size_t size) {
  uint32_t c = 0xffffffffu;
  for (size_t i = 0; i < size; ++i) {
    c ^= data[i];
    for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1)));
  }
  return ~c;
}

std::string firmwareId(const std::vector<uint8_t>& image) {
  // Les octets 0xFF de remplissage en fin d'image ne comptent pas : le même
  // firmware a le même identifiant qu'il vienne d'un .bin ou d'un .wav.
  size_t size = image.size();
  while (size > 0 && image[size - 1] == 0xff) --size;
  char text[16];
  std::snprintf(text, sizeof(text), "%08x", static_cast<unsigned>(crc32(image.data(), size)));
  return text;
}

bool readFirmwareFile(const uint8_t* file, size_t size, FirmwareImage& image, std::string& error) {
  if (size >= 12 && std::memcmp(file, "RIFF", 4) == 0) {
    if (!decodeWav(file, size, image, error)) return false;
  } else {
    image = FirmwareImage();
    image.format = "bin";
    image.data.assign(file, file + size);
  }
  // Une image d'application commence par sa table de vecteurs : pile en RAM,
  // point d'entrée dans la zone de l'application.
  if (image.data.size() < 8 || image.data.size() > 112 * 1024) {
    error = "La taille de ce fichier ne correspond pas à un firmware Braids.";
    return false;
  }
  const uint32_t sp = le32(image.data.data()), pc = le32(image.data.data() + 4);
  if (sp < 0x20000000u || sp > 0x20005000u || pc < 0x08004000u || pc >= 0x08020000u || !(pc & 1)) {
    error = "Ce fichier n'est pas un firmware pour Braids.";
    return false;
  }
  return true;
}

}  // namespace emu
}  // namespace tresses
