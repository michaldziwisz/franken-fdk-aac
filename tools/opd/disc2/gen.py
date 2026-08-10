import wave, struct, math, random, sys
sr = 44100
rnd = random.Random(5)
c = lambda x: max(-32768, min(32767, int(x)))
n = sr * 3
st = 0.0
fr = []
for i in range(n):
    t = i / sr
    v = (math.sin(2 * math.pi * 70 * t) + 0.8 * math.sin(2 * math.pi * 150 * t)
         + 0.6 * math.sin(2 * math.pi * 260 * t)
         + 0.4 * math.sin(2 * math.pi * 480 * t))
    st = 0.97 * st + 0.03 * rnd.uniform(-1, 1)
    s = (v + 5.0 * st) * (0.6 + 0.4 * math.sin(2 * math.pi * 0.3 * t))
    # identyczne poziomy L/P, przeciwna faza -> IID heurystycznie WYLACZONE
    fr.append(struct.pack('<hh', c(4200 * s), c(-4200 * s)))
w = wave.open(sys.argv[1], 'wb')
w.setnchannels(2); w.setsampwidth(2); w.setframerate(sr)
w.writeframes(b''.join(fr)); w.close()
