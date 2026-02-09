import pygame
import numpy as np
import time

# Init mixer (44100 Hz is standard CD quality)
pygame.mixer.init(frequency=44100, size=-16, channels=2, buffer=512)

def play_tone(freq, duration_sec=0.2, volume=0.4):
    sample_rate = pygame.mixer.get_init()[0]
    max_amp = 32767  # 16-bit

    # Time array for the samples
    t = np.linspace(0, duration_sec, int(sample_rate * duration_sec), endpoint=False)
    wave = np.sin(2 * np.pi * freq * t)

    # Quick fade-out to prevent clicks
    fade_len = int(sample_rate * 0.02)
    envelope = np.ones_like(wave)
    envelope[-fade_len:] = np.linspace(1, 0, fade_len)

    wave *= envelope * volume * max_amp
    wave = wave.astype(np.int16)

    # Make stereo
    stereo_wave = np.column_stack((wave, wave))
    sound = pygame.sndarray.make_sound(stereo_wave)

    channel = sound.play()
    if channel:
        while channel.get_busy():
            pygame.time.delay(10)  # Wait for playback

# Notes: G4 and Eb4 (standard tuning)
G_freq  = 392.00
Eb_freq = 311.13

short_dur = 0.12   # quick "dum"
long_dur  = 1     # "dah"
gap       = 0.03

print("Playing Beethoven's Fifth motif with smooth sine tones...")


play_tone(G_freq, short_dur, volume=0.7)  
time.sleep(gap)
play_tone(G_freq, short_dur, volume=0.7)
time.sleep(gap)
play_tone(G_freq, short_dur, volume=0.7)
time.sleep(gap * 2)
play_tone(Eb_freq, long_dur, volume=0.7 )
time.sleep(2)
print("Playing triumph sound")
# ====================
# Triumphant victory fanfare (short, bright, ascending)
# ====================
print("Playing triumph sound...")

triumph_notes = [523.25, 659.25, 783.99, 1046.50, 783.99]  # C5 E5 G5 C6 G5 (held finish)
triumph_durs  = [0.08, 0.08, 0.08, 0.08, 0.50]             # quick rise + long hold
triumph_gap   = 0.02

for freq, dur in zip(triumph_notes, triumph_durs):
    play_tone(freq, dur, volume=0.7)  # slightly louder for punch
    time.sleep(triumph_gap)

time.sleep(1.0)  # final pause

print("Done!")