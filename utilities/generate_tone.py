# generate_chromatic_tone.py
# Creates a short (~1.5–2 s) chromatic scale C4 → C5 as PROGMEM C header
# 16-bit signed stereo PCM @ 44100 Hz
# Suitable for ESP8266 + ESP8266Audio library

import math
import struct

# ────────────────────────────────────────────────
# CONFIGURATION
# ────────────────────────────────────────────────

OUTPUT_FILENAME     = "short_audiosample.h"
SAMPLE_RATE         = 44100
DURATION            = 1.6               # seconds – adjust if needed (1.0–2.0 safe)
AMPLITUDE           = 0.80              # 0.0–1.0 (avoid clipping)
BITS_PER_SAMPLE     = 16
CHANNELS            = 2                 # stereo

# C4 = MIDI 60 → C5 = MIDI 72 (13 notes)
START_MIDI_NOTE     = 60
END_MIDI_NOTE       = 72
NUM_NOTES           = END_MIDI_NOTE - START_MIDI_NOTE + 1

TIME_PER_NOTE       = DURATION / NUM_NOTES

# ────────────────────────────────────────────────

def midi_note_to_freq(note):
    """Convert MIDI note number to frequency (A4 = 440 Hz)"""
    return 440.0 * (2.0 ** ((note - 69) / 12.0))

# Calculate total samples
total_samples = int(SAMPLE_RATE * DURATION + 0.5)
print(f"Generating {total_samples:,} samples × {CHANNELS} channels")
print(f"Estimated size: ~{total_samples * CHANNELS * 2 + 100:,} bytes")

# Generate interleaved stereo samples (left = right = same value)
samples = []  # list of int16 values

for i in range(total_samples):
    t = i / SAMPLE_RATE
    note_index = min(int(t / TIME_PER_NOTE), NUM_NOTES - 1)
    freq = midi_note_to_freq(START_MIDI_NOTE + note_index)
    
    phase = 2.0 * math.pi * freq * t
    value = math.sin(phase) * AMPLITUDE
    
    # Scale to 16-bit signed integer
    s16 = int(value * 32767.0)
    s16 = max(-32768, min(32767, s16))
    
    # Stereo: same sample on left and right
    samples.append(s16)  # left
    samples.append(s16)  # right

# ────────────────────────────────────────────────
# Build minimal PCM WAV header
# ────────────────────────────────────────────────

data_size_bytes = len(samples) * 2  # 16-bit = 2 bytes per sample

header = bytearray()

# RIFF header
header.extend(b'RIFF')
header.extend((36 + data_size_bytes).to_bytes(4, 'little'))
header.extend(b'WAVE')

# fmt chunk
header.extend(b'fmt ')
header.extend((16).to_bytes(4, 'little'))                      # subchunk1 size
header.extend((1).to_bytes(2, 'little'))                       # AudioFormat: PCM = 1
header.extend(CHANNELS.to_bytes(2, 'little'))                  # NumChannels
header.extend(SAMPLE_RATE.to_bytes(4, 'little'))               # SampleRate
byte_rate = SAMPLE_RATE * CHANNELS * (BITS_PER_SAMPLE // 8)
header.extend(byte_rate.to_bytes(4, 'little'))                 # ByteRate
block_align = CHANNELS * (BITS_PER_SAMPLE // 8)
header.extend(block_align.to_bytes(2, 'little'))               # BlockAlign
header.extend((BITS_PER_SAMPLE).to_bytes(2, 'little'))         # BitsPerSample

# data chunk
header.extend(b'data')
header.extend(data_size_bytes.to_bytes(4, 'little'))

# ────────────────────────────────────────────────
# Write to .h file
# ────────────────────────────────────────────────

with open(OUTPUT_FILENAME, "w", encoding="utf-8") as f:
    f.write(f"// {OUTPUT_FILENAME}\n")
    f.write(f"// Auto-generated {DURATION:.2f}s chromatic scale C4→C5\n")
    f.write(f"// {SAMPLE_RATE} Hz, {BITS_PER_SAMPLE}-bit signed {CHANNELS}-channel PCM\n")
    f.write(f"// Use with ESP8266Audio library + AudioFileSourcePROGMEM\n\n")
    
    f.write("#ifndef SHORT_AUDIO_SAMPLE_H\n")
    f.write("#define SHORT_AUDIO_SAMPLE_H\n\n")
    f.write("#include <Arduino.h>\n\n")
    
    f.write("const uint8_t sample_wav[] PROGMEM = {\n")
    
    # Write header bytes (12 bytes per line)
    for i, b in enumerate(header):
        if i % 12 == 0:
            f.write("  ")
        f.write(f"0x{b:02x},")
        if (i + 1) % 12 == 0:
            f.write("\n")
    
    f.write("\n  // PCM audio data (little-endian signed 16-bit stereo)\n")
    
    # Write samples – 8 samples per line (16 bytes)
    for i, s in enumerate(samples):
        if i % 8 == 0:
            f.write("  ")
        packed = struct.pack("<h", s)  # little-endian signed 16-bit
        f.write(f"0x{packed[0]:02x},0x{packed[1]:02x},")
        if (i + 1) % 8 == 0:
            f.write("\n")
    
    f.write("\n};\n\n")
    f.write(f"const size_t sample_wav_len = sizeof(sample_wav);\n\n")
    f.write("#endif // SHORT_AUDIO_SAMPLE_H\n")

print(f"\nFile created: {OUTPUT_FILENAME}")
print(f"Total bytes: {len(header) + len(samples)*2:,}")
print(f"Header size: {len(header)} bytes")
print(f"Data size: {len(samples)*2:,} bytes")
print("Done. Include this file in your Arduino sketch.")
