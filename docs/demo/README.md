# Xuss-C metal demo (esprec)

Host-paced ESPREC1 captures of the product UI on a real M5GO-class board.

## Bench (capture rate)

See [`bench_capture_rate.json`](bench_capture_rate.json). Summary @ 115200 baud, 320×240:

| Metric | Value |
|--------|------:|
| Mean full-panel shot | ~18.5 s |
| Max sustainable host-paced FPS | **~0.054** |
| Identity RTT before/after shot | ~50–60 ms |

The limit is **USB serial + base64**, not host CPU. The device UI is blocked for each emit. Raising continuous `--hz` above ~0.05 does not help.

Playback smoothness for demos uses **GIF frame delays** (keyframe style), not capture FPS.

## Artifacts

| File | Description |
|------|-------------|
| `xuss-c-demo.gif` | Narrative demo (captions *above* panel) |
| `xuss-c-demo-product.gif` | Same keyframes, pure product pixels |
| `00_…png` … | Unlabeled stills (banner / Details identity intact) |

## Re-record

```text
pip install -e ../esprec
python tools/bench_capture_rate.py --port COMx -o docs/demo/bench_capture_rate.json
python tools/demo_record.py --port COMx -o docs/demo --captions --living-samples 3
```

Device multi-frame ring-buffer / spool-after-stop is **not** implemented yet — tracked on tig/esprec.
