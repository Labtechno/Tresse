#!/usr/bin/env python3
"""Convertit une image de firmware Braids (.bin) en fichier de mise à jour audio (.wav).

    python3 tools/firmware_wav.py firmware.bin [sortie.wav]

Le .wav obtenu se charge dans Tresses, et se joue aussi dans un vrai Braids pour le
mettre à jour (procédure du manuel : encodeur enfoncé à l'allumage).

Portage pour Python 3 de stm_audio_bootloader/qpsk/encoder.py d'Emilie Gillet
(licence MIT), avec les réglages du makefile de Braids :
48 kHz, 12 kbit/s, porteuse 6 kHz, paquets de 256 octets, pages de 1 Ko.
"""
import sys
import wave
import zlib

import numpy

SAMPLE_RATE, BIT_RATE, CARRIER, PACKET, PAGE, PAUSE = 48000, 12000, 6000, 256, 1024, 0.06


class Encoder:
    def __init__(self):
        self.sample_index = 0

    def encode(self, symbols):
        ratio = SAMPLE_RATE // BIT_RATE * 2
        symbols = numpy.array(symbols, dtype=int)
        up = lambda v: numpy.repeat(v, ratio)
        q = (2 * up(symbols % 2) - 1) / numpy.sqrt(2.0)
        i = (2 * up(symbols // 2) - 1) / numpy.sqrt(2.0)
        t = (numpy.arange(0.0, len(q)) + self.sample_index) / SAMPLE_RATE
        self.sample_index += len(q)
        phase = 2 * numpy.pi * CARRIER * t
        return q * numpy.sin(phase) + i * numpy.cos(phase)

    def blank(self, duration):
        return self.encode([0] * (int(duration * BIT_RATE / 8) * 4))

    def packet(self, data):
        data = data + b"\xff" * (PACKET - len(data))
        crc = zlib.crc32(data) & 0xffffffff
        stream = [0] * 8 + [0x99] * 4 + [0xcc] * 4 + list(data) + [crc >> 24, (crc >> 16) & 0xff, (crc >> 8) & 0xff, crc & 0xff]
        symbols = []
        for byte in stream:
            symbols += [(byte >> 6) & 3, (byte >> 4) & 3, (byte >> 2) & 3, byte & 3]
        return self.encode(symbols)


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    source = sys.argv[1]
    target = sys.argv[2] if len(sys.argv) > 2 else source.rsplit(".", 1)[0] + ".wav"
    data = open(source, "rb").read()
    if len(data) % PAGE:
        data += b"\xff" * (PAGE - len(data) % PAGE)
    encoder = Encoder()
    blocks = [numpy.zeros(SAMPLE_RATE), encoder.blank(1.0)]
    for offset in range(0, len(data), PACKET):
        blocks.append(encoder.packet(data[offset:offset + PACKET]))
        if (offset // PACKET + 1) % (PAGE // PACKET) == 0:
            blocks.append(encoder.blank(PAUSE))
    blocks.append(encoder.blank(1.0))
    signal = numpy.clip(numpy.concatenate(blocks), -1.0, 1.0)
    with wave.open(target, "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(SAMPLE_RATE)
        f.writeframes((signal * 32767.0).astype("<i2").tobytes())
    print(f"{target} : {len(signal) / SAMPLE_RATE:.1f} s pour {len(data)} octets")


if __name__ == "__main__":
    main()
