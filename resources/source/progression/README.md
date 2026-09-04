# Progression art sources

`make_assets.py` draws every `paim_prog*.png` under `resources/`: the five tier
medals, the level ring, the three badge plates and the glow/spark overlays.

Regenerate them from the repository root:

```bash
python3 resources/source/progression/make_assets.py
```

The art is intentionally white/grey over a black outline. `setColor()` is a
multiply, so the outline stays black while the body picks up the tier or rarity
colour — one plate covers all six rarities and one medal covers four tiers.

The source directory is not listed in `mod.json`, so only the rendered PNGs are
packaged.
