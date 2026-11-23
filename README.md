<p align="center">
  <img src="logo.png" alt="xyzBot logo" width="160">
</p>

# xyzBot v2.0.0

[![License: GPL 3.0](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Geode](https://img.shields.io/badge/Geode-v5.11.2-blue)](https://geode-sdk.org/)
[![Discord](https://img.shields.io/discord/1234567890?color=7289da&label=Discord&logo=discord&logoColor=white)](https://discord.gg/GE9xbJQU8)
(I'm aware that it says it's invalid? 😭)

A macro bot for [Geometry Dash](https://store.steampowered.com/app/322170/Geometry_Dash/) 2.2081, built with [Geode](https://geode-sdk.org). Your average macrobot but with all your needs :3

## Disclaimer

xyzBot is made for showcase purposes, it is not made for illegitimate completions.

## Features

**Recording and playback**
- Accurate recording and playback with a macro library, search, merge, a macro editor and autosave
Three accuracy modes:

- **Vanilla**
- **Input Fixes**
- **Frame Fixes**
- **Delta Fixes**
- a LOT of assist tools <3

**Visuals**
- Labels (21+)
- Hitboxes (hitbox trail included)

**Rendering**

- Basic Video rendering

## Controls

| Key | Action |
| --- | --- |
| F6 | Open or close the menu |
| F7 | Advance one frame (frame stepper) |
| F9 | Step one frame back |


## Macro formats

Use **Import** on the Macros tab.

| Format | Extension |
| --- | --- |
| xyzBot | `.xyz` |
| GDR1 / GDR2 | `.gdr`, `.gdr.json`, `.gdr2`, `.json` |
| xdBot legacy | `.xd` |
| Silicate | `.slc` |
| TCBot | `.tcm` |
| Echo | `.echo`, `.echo.json` |
| Omega | `.omega` |
| Text | `.txt`, `.csv` |

## Platforms

- Windows
- Android (x32 & x64)
- macOS

## Building

Follow the [Geode setup guide](https://docs.geode-sdk.org/getting-started/), then:

```sh
geode build
```

## Reporting bugs

Open an [issue](https://github.com/malikisherenot/xyzBot/issues) with your platform (⁠つ⁠≧⁠▽⁠≦⁠)⁠つ

Known issues:
- Rendering: the video can split into four tiles and move to the bottom left at the end of a render.
- Playback accuracy can drop slightly with each attempt in the editor. (working on it!)

## Credits

xyzBot adapts work from several GPL-3.0 projects. See [about.md](about.md) for the list, and please check each project for its own license.

Bundled third-party files: [JSON for Modern C++](https://github.com/nlohmann/json) (MIT, `src/third_party/json.hpp`) and DejaVu Sans Condensed Oblique (Bitstream Vera license, [licenses/DejaVu-Fonts.txt](licenses/DejaVu-Fonts.txt)).

## License

GPL-3.0, see [LICENSE](LICENSE).
