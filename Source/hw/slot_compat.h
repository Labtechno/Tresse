// Tresses — compatibilité 32 bits, inclus au début de chaque fichier du firmware,
// à l'intérieur de l'espace de noms du slot.
//
// Sur le Cortex-M3 (arm-none-eabi), uint32_t est un "unsigned long" ; sur un Mac
// ou un PC 64 bits, c'est un "unsigned int". Certains firmwares s'appuient sur
// cette équivalence, par exemple pour déduire le type d'un modèle de fonction.
// Les surcharges ci-dessous rétablissent le comportement du compilateur ARM sans
// toucher au code du firmware.

#if TRESSES_FIRMWARE_BEES
// Bees-in-the-Trees : ParamClip(uint32_t, 0ul, 127ul).
template <typename ParamType, typename LimitType>
inline ParamType ParamClip(ParamType param, LimitType min_param, LimitType max_param) {
  if (param < static_cast<ParamType>(min_param)) {
    return static_cast<ParamType>(min_param);
  } else if (param > static_cast<ParamType>(max_param)) {
    return static_cast<ParamType>(max_param);
  }
  return param;
}
#endif
