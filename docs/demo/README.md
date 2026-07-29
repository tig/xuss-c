# Xuss-C metal demo (esprec)

Host-paced ESPREC1 captures of the product UI on a real M5GO-class board.

## Two capture modes

| Mode | Command | When |
|------|---------|------|
| **Shot** (full 320×240) | `shot` / `esprec snapshot` | Keyframes, docs stills (~18.5 s/frame @ 115200) |
| **Rec → spool** (quarter 80×60) | `esprec rec start hz sec` → `stop` → `spool` | Continuous living UI (~5 Hz sample; transfer later) |

See [`bench_capture_rate.json`](bench_capture_rate.json) for host-paced shot limits.

**Realtime living GIF** (device samples ~5 Hz into flash, then spools):

```text
esprec spool --port COMx --duration 3 --hz 5 -o docs/demo/xuss-c-living-realtime.gif
# or: python tools/demo_record.py --port COMx -o docs/demo --captions
```

Measured on M5GO: **16 frames / 3 s**, mean inter-frame **~210 ms** (~4.8 Hz).

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
