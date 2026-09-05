# musikcube — Noctalia theme template

Generates a musikcube color theme (`~/.config/musikcube/themes/noctalia.json`)
from the current Noctalia wallpaper palette. Registered as
`[theme.templates.user.musikcube]` in `noctalia.toml`.

musikcube auto-discovers any `*.json` file in that directory and lists it in
Settings → "color theme" by its internal `name` field ("noctalia").

## Notes

- Only `hex` values are emitted (no `palette`/256-color index), matching
  every other template in this repo (fzf, tmux, aerc, neovim). musikcube
  treats `palette` as optional and falls back to its own built-in
  approximation per field when absent — see `Colors.cpp`'s `ThemeColor::Set()`.
- **That fallback only matters in 256-color mode.** For the generated colors
  to actually show up, "degrade to 256 color palette" must be OFF in
  musikcube's Settings screen, and the terminal (and any multiplexer, e.g.
  tmux) must support true 24-bit color end-to-end (`can_change_color()`).
  With degrade-to-256 on, or in a terminal that doesn't support dynamic
  color reprogramming, musikcube silently drops to a *hardcoded* 256-color
  approximation of its default dark theme instead — not a bug in this
  template, just how `Colors::Init()` degrades.
- musikcube indexes theme files once at startup (no live-reload, no signal
  handler to hook a `post_hook` into like the neovim template does). A
  wallpaper change updates the file on disk immediately, but musikcube needs
  a restart to pick up the new colors if it's already running.
- Color-role mapping is subjective (same spirit as the Feishin template's own
  disclaimer) — tweak `templates/musikcube-theme.json` directly if a
  particular field reads wrong against your palette.
