// Tresses — Resampler
//
// Le moteur Braids tourne toujours à 96 kHz, comme sur le module. Ce
// rééchantillonneur convertit ce flux vers la fréquence du DAW (44.1, 48,
// 88.2 kHz...) avec un filtre sinc fenêtré (Kaiser) qui coupe juste sous le
// Nyquist de sortie : le contenu au-delà de 20 kHz que le module produit (et
// que son filtre de sortie analogique élimine) ne se replie pas dans l'audible.
//
// À 96 kHz côté DAW, le signal passe tel quel (bit à bit).

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace tresses {

class Resampler {
 public:
  static constexpr int kPhases = 256;       // résolution de la table du noyau
  static constexpr double kZeroCrossings = 32.0;
  static constexpr double kKaiserBeta = 8.0;
  static constexpr int kBlock = 24;         // taille des blocs fournis par la source
  static constexpr double kPi = 3.14159265358979323846;

  void prepare(double input_rate, double output_rate) {
    step_ = input_rate / output_rate;
    passthrough_ = std::abs(step_ - 1.0) < 1e-9;

    const double cutoff_hz = 0.5 * std::min(input_rate, output_rate) * 0.93;
    const double wc = cutoff_hz / input_rate;  // cycles / échantillon d'entrée
    half_width_ = passthrough_ ? 0 : static_cast<int>(std::ceil(kZeroCrossings / (2.0 * wc)));

    int size = 1;
    while (size < 2 * half_width_ + 4 * kBlock + 8) size <<= 1;
    buffer_.assign(static_cast<size_t>(size), 0.0f);
    mask_ = size - 1;

    if (!passthrough_) {
      const int taps = 2 * half_width_;
      table_.assign(static_cast<size_t>(taps * kPhases + 2), 0.0f);
      const double i0_beta = besselI0(kKaiserBeta);
      for (int k = 0; k <= taps * kPhases + 1; ++k) {
        // d = distance (en échantillons d'entrée) au point d'interpolation.
        const double d = static_cast<double>(k) / kPhases - half_width_;
        const double x = d / half_width_;
        double w = 0.0;
        if (std::abs(x) < 1.0) {
          w = besselI0(kKaiserBeta * std::sqrt(1.0 - x * x)) / i0_beta;
        }
        const double arg = 2.0 * wc * d;
        const double sinc = std::abs(arg) < 1e-12
            ? 1.0 : std::sin(kPi * arg) / (kPi * arg);
        table_[static_cast<size_t>(k)] = static_cast<float>(2.0 * wc * sinc * w);
      }
    }
    reset();
  }

  void reset() {
    std::fill(buffer_.begin(), buffer_.end(), 0.0f);
    written_ = 0;
    ipos_ = 0;
    frac_ = 0.0;
  }

  // Latence introduite, en échantillons de sortie.
  int latencyInOutputSamples() const {
    return passthrough_ ? 0 : static_cast<int>(std::lround(half_width_ / step_));
  }

  // Produit un échantillon de sortie. `pull(float* dst)` doit écrire kBlock
  // échantillons à la fréquence d'entrée (96 kHz).
  template <typename Source>
  float process(Source&& pull) {
    if (passthrough_) {
      while (written_ <= ipos_) pullBlock(pull);
      const float y = buffer_[static_cast<size_t>(ipos_ & mask_)];
      ++ipos_;
      return y;
    }

    // Position de sortie P = ipos_ + frac_ (en échantillons d'entrée).
    // On somme x[n] * h(n - P) pour n dans [n0, n0 + 2*half_width_).
    const bool has_frac = frac_ > 0.0;
    const int64_t n0 = ipos_ - half_width_ + (has_frac ? 1 : 0);
    const double offset = has_frac ? 1.0 - frac_ : 0.0;  // n0 - P + half_width_
    const int taps = 2 * half_width_;
    while (written_ < n0 + taps) pullBlock(pull);

    const double t = offset * kPhases;
    const int base = static_cast<int>(t);
    const float mu = static_cast<float>(t - base);
    const float* h = table_.data() + base;
    float acc = 0.0f;
    int64_t n = n0;
    for (int j = 0; j < taps; ++j, ++n, h += kPhases) {
      const float coeff = h[0] + mu * (h[1] - h[0]);
      acc += buffer_[static_cast<size_t>(n & mask_)] * coeff;
    }

    frac_ += step_;
    const double whole = std::floor(frac_);
    ipos_ += static_cast<int64_t>(whole);
    frac_ -= whole;
    return acc;
  }

 private:
  template <typename Source>
  void pullBlock(Source& pull) {
    float block[kBlock];
    pull(block);
    for (int i = 0; i < kBlock; ++i) {
      buffer_[static_cast<size_t>((written_ + i) & mask_)] = block[i];
    }
    written_ += kBlock;
  }

  static double besselI0(double x) {
    double sum = 1.0, term = 1.0;
    const double y = x * x / 4.0;
    for (int k = 1; k < 64; ++k) {
      term *= y / (static_cast<double>(k) * k);
      sum += term;
      if (term < sum * 1e-17) break;
    }
    return sum;
  }

  double step_ = 2.0;
  bool passthrough_ = false;
  int half_width_ = 0;
  std::vector<float> table_;
  std::vector<float> buffer_;
  int64_t mask_ = 0;
  int64_t written_ = 0;
  int64_t ipos_ = 0;
  double frac_ = 0.0;
};

}  // namespace tresses
