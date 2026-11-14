// Tresses — pont entre le plugin et le firmware d'un slot.
//
// Inclus À L'INTÉRIEUR de l'espace de noms du slot, juste après braids/braids.cc,
// dans la même unité de compilation : il voit donc toutes les variables
// globales du firmware (osc, ui, render_block...) et ses fonctions (Init,
// RenderBlock, interruptions).
//
// La boucle main() du firmware tourne à l'infini et ne peut pas être appelée
// telle quelle : son contenu est reproduit dans MainLoopIteration(). Si ton
// firmware modifie main() ou ajoute des variables globales, reporte-le ici.
//
// Deux familles de firmware sont reconnues (cmake/TressesFirmwareSlots.cmake) :
// le firmware officiel Braids (et ses dérivés) et Bees-in-the-Trees 4.x
// (TRESSES_FIRMWARE_BEES).

namespace tresses_bridge {

using namespace braids;

uint32_t systick_accumulator;
uint32_t sample_rate_hz = 96000;

template <typename T>
void ZeroObject(T& object) {
  std::memset(static_cast<void*>(&object), 0, sizeof(object));
}

// Remise à zéro de la RAM du firmware, comme à la mise sous tension (.bss).
// (Les variables "static" déclarées dans les fonctions du firmware ne sont pas
// accessibles d'ici : elles ne sont remises à zéro qu'au chargement du plugin.)
void ResetFirmwareRam() {
  ZeroObject(osc);
  ZeroObject(envelope);
  ZeroObject(adc);
  ZeroObject(dac);
  ZeroObject(debug_pin);
  ZeroObject(gate_input);
  ZeroObject(internal_adc);
  ZeroObject(sys);
  ZeroObject(jitter_source);
  ZeroObject(ui);
  ZeroObject(braids::settings);
  ZeroObject(stmlib::system_clock);
  ZeroObject(audio_samples);
  ZeroObject(sync_samples);
  current_sample = 0;
  playback_block = 0;
  render_block = 0;
  trigger_detected_flag = false;
  trigger_flag = false;
  trigger_delay = 0;
  stmlib::Random::Seed(0x21);  // valeur initiale dans stmlib/utils/random.cc
#if TRESSES_FIRMWARE_BEES
  ZeroObject(envelope2);
  sh_pitch = 0;
#else
  ZeroObject(quantizer);
  ZeroObject(ws);
  current_scale = 0xff;
#endif
}

void ApplyInputs(const ::tresses::FrameInput& f) {
  for (int i = 0; i < 4; ++i) tresses_hw.adc_code[i] = f.adc[i];
  tresses_hw.internal_adc = f.fine;
  tresses_hw.trig_high = f.trig != 0;
}

// Corps de la boucle while (1) de main() dans braids.cc.
inline void MainLoopIteration() {
#if !TRESSES_FIRMWARE_BEES
  if (current_scale != settings.GetValue(SETTING_QUANTIZER_SCALE)) {
    current_scale = settings.GetValue(SETTING_QUANTIZER_SCALE);
    quantizer.Configure(scales[current_scale]);
  }
#endif
  while (render_block != playback_block) {
    RenderBlock();
  }
  ui.DoEvents();
}

void PowerOn(const uint8_t* flash, size_t flash_size, const uint32_t uid[3],
             const ::tresses::FrameInput& initial, bool factory_calibrate,
             const uint16_t calibration_codes[3]) {
  ResetFirmwareRam();

  // Matériel : flash, identifiant, broches au repos.
  const uint32_t keep_flash_version = tresses_hw.flash_version;
  ZeroObject(tresses_hw);
  std::memset(tresses_hw.flash, 0xff, sizeof(tresses_hw.flash));
  if (flash != nullptr) {
    std::memcpy(tresses_hw.flash, flash, std::min(flash_size, sizeof(tresses_hw.flash)));
  }
  tresses_hw.flash_version = keep_flash_version + 1;
  for (int i = 0; i < 3; ++i) tresses_hw.uid[i] = uid[i];
  tresses_hw.encoder_pin_a = true;
  tresses_hw.encoder_pin_b = true;
  tresses_hw.dac_code = 32768;
  ApplyInputs(initial);
  tresses_tim1.SR = 0;
  systick_accumulator = 0;

  Init();  // braids.cc

  // TIM1 : F_CPU / (période + 1) -> 96 kHz pour Braids.
  sample_rate_hz = static_cast<uint32_t>(F_CPU / (sys.timer_period() + 1));

  if (factory_calibrate) {
    // Procédure CAL du firmware, comme à la sortie d'usine : 1 V puis 3 V sur
    // V/OCT, rien sur FM.
#if TRESSES_FIRMWARE_BEES
    settings.Calibrate(
        calibration_codes[0],
        calibration_codes[1],
        calibration_codes[2]);
#else
    settings.Calibrate(
        calibration_codes[0],
        calibration_codes[1],
        calibration_codes[2],
        0, 4095, 0, 4095);
#endif
  }
}

void Run(const ::tresses::FrameInput* in, float* out, int n) {
  for (int i = 0; i < n; ++i) {
    ApplyInputs(in[i]);

    // Interruption audio (TIM1, priorité maximale).
    tresses_tim1.SR = TIM_IT_Update;
    TIM1_UP_IRQHandler();
    // Le DAC reçoit 32768 - échantillon ; la sortie analogique remet le signe.
    out[i] = (32768.0f - static_cast<float>(tresses_hw.dac_code)) * (1.0f / 32768.0f);

    // SysTick à 1 kHz : interface (encodeur, afficheur, horloge en ms).
    systick_accumulator += 1000;
    if (systick_accumulator >= sample_rate_hz) {
      systick_accumulator -= sample_rate_hz;
      SysTick_Handler();
    }

    MainLoopIteration();
  }
}

void EncoderInput(int steps, bool pressed) {
  tresses_hw.encoder_pending += steps;
  tresses_hw.encoder_switch_low = pressed;
}

void ReadDisplay(::tresses::DisplayState* out) {
  for (int i = 0; i < 4; ++i) {
    out->text[i] = tresses_hw.display_buffer ? tresses_hw.display_buffer[i] : ' ';
  }
  out->brightness = static_cast<uint8_t>(
      tresses_hw.display_brightness ? *tresses_hw.display_brightness : 0);
}

uint32_t FlashVersion() { return tresses_hw.flash_version; }

void ReadFlash(uint8_t* dst) {
  std::memcpy(dst, tresses_hw.flash, sizeof(tresses_hw.flash));
}

size_t SettingsSize() { return sizeof(SettingsData); }

void ReadSettings(uint8_t* dst) {
  std::memcpy(dst, &settings.data(), sizeof(SettingsData));
}

void WriteSettings(const uint8_t* src) {
  std::memcpy(settings.mutable_data(), src, sizeof(SettingsData));
}

int SampleRate() { return static_cast<int>(sample_rate_hz); }

}  // namespace tresses_bridge
