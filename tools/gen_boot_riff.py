import math, pathlib
sr = 22050
notes = [
    (523.25, 0.35),
    (659.25, 0.35),
    (783.99, 0.40),
    (1046.50, 0.55),
    (783.99, 0.30),
    (659.25, 0.45),
]
samples = []
for freq, dur in notes:
    n = int(sr * dur)
    for i in range(n):
        t = i / sr
        env = 1.0
        a, r = 0.02, 0.05
        if t < a:
            env = t / a
        elif t > dur - r:
            env = max(0.0, (dur - t) / r)
        s = math.sin(2 * math.pi * freq * t)
        s = max(-1.0, min(1.0, s * 1.4)) * env * 0.55
        samples.append(int(128 + s * 100))
samples.extend([128] * int(sr * 0.15))
path = pathlib.Path("assets/boot_riff.pcm")
path.write_bytes(bytes(samples))
print(f"wrote {path} bytes={len(samples)} duration={len(samples)/sr:.2f}s")
