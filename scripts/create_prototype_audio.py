"""Generate four original, quiet mono PCM game cues; no downloaded media."""
import math
import random
import struct
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'art' / 'audio'
OUT.mkdir(parents=True, exist_ok=True)
RATE = 22050
rng = random.Random(41009)

def write(name, duration, sample):
    count = int(duration * RATE)
    data = bytearray()
    for i in range(count):
        t = i / RATE
        value = max(-1, min(1, sample(t, i / count)))
        data.extend(struct.pack('<h', int(value * 32767)))
    with wave.open(str(OUT / (name + '.wav')), 'wb') as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(RATE)
        output.writeframes(data)

write('S_PrototypeShot', .11, lambda t, p: .23 * math.exp(-p * 8) * (.6 * math.sin(2 * math.pi * (180*t-350*t*t)) + .4 * rng.uniform(-1, 1)))
write('S_PrototypeHit', .14, lambda t, p: .20 * math.exp(-p * 7) * (math.sin(2 * math.pi * 90*t) + .25 * rng.uniform(-1, 1)))
write('S_PrototypePickup', .22, lambda t, p: .15 * math.sin(math.pi*p) * math.sin(2 * math.pi * (720*t+420*t*t)))
write('S_PrototypeExtract', .55, lambda t, p: .14 * math.sin(math.pi*p) * (math.sin(2*math.pi*440*t) + .5*math.sin(2*math.pi*660*t)))
print('Generated four original PCM cues in', OUT)
