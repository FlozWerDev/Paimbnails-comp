# Versus art sources

`make_assets.py` draws every `paim_vs*.png` under `resources/`: the CreatorLayer
button, the rank chip and division pip, the VS wordmark, the crossed-sword
emblem and laurel frame, the card plate trio, the 24 card glyphs, the 9 mode
glyphs, the duel bar and the match-found burst.

Regenerate them from the repository root:

```bash
python3 resources/source/versus/make_assets.py
```

Same rule as the progression art: white/grey over a pure black outline, so a
`setColor()` multiply keeps the outline black and turns the body into the rank,
rarity or mode colour. One card plate covers all four rarities, and the glyphs
are drawn in a unit square so the same file works on a card and in the HUD.

The source directory is not listed in `mod.json`, so only the rendered PNGs are
packaged.
