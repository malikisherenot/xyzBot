<p align="center">
  <img src="logo.png" alt="xyzBot logo" width="160">
</p>

# xyzBot

A macro bot for [Geometry Dash](https://store.steampowered.com/app/322170/Geometry_Dash/) 2.2081, built with [Geode](https://geode-sdk.org). It records and replays tick-accurate inputs and bundles the assist tools used while making macros.

xyzBot is a cheat mod. Do not use it to submit runs to leaderboards or lists that forbid bots.

## Features

**Recording and playback**
- Tick-accurate recording and playback with a macro library, search, merge, a macro editor and autosave
- Three accuracy modes: **Vanilla** (no corrections), **Input Fixes** (position restored at every click) and **Frame Fixes** (restored every tick). A macro plays with the fixes it contains
- Record or play player 1, player 2 or both; take over on input; continue after finish; sequences
- Prevent Death, Maintain Gravity, mirror options, Block Inputs, autoclicker per player, CPS counter, Auto Exit

**Speed**
- TPS bypass built for high TPS, speedhack with audio, frame stepper, noclip
- Fast Sub-Ticks: skips display-only work on ticks that are never drawn (physics is untouched)

**Visuals**
- Hitboxes, hitbox trail and trajectory
- Wave Drag Fix: correct wave trail on D blocks, slopes, portals and spider orbs, also at high TPS
- 21 labels (tick, position, velocity, CPS, orbs and more) and a layout mode

**Rendering**
- Video rendering through the FFmpeg API mod (`eclipse.ffmpeg-api`). Currently compiled in on Android only.

## Controls

| Key | Action |
| --- | --- |
| F6 | Open or close the menu |
| F7 | Advance one frame (frame stepper) |
| F9 | Step one frame back |
| F8 | Take over from a playing macro |

The menu can also be opened with the floating button (toggle it in the mod settings). Keyboard shortcuts are not available on iOS.

## Macro formats

Use **Import** on the Macros tab. Files in the macros folder are converted to the native `.xyz` format, which is the only format xyzBot saves.

| Format | Extension | Notes |
| --- | --- | --- |
| xyzBot | `.xyz` | Native format |
| GDR1 / GDR2 | `.gdr`, `.gdr.json`, `.gdr2`, `.json` | MessagePack or JSON. xdBot macros keep their frame fixes |
| xdBot legacy | `.xd` | Inputs and position fixes |
| Silicate | `.slc` | v1, v2 and v3 |
| TCBot | `.tcm` | v1 and v2 |
| Echo | `.echo`, `.echo.json` | Old binary and JSON. Echo's newer compressed `.echo` is not supported: export as `.gdr`, `.gdr.json` or `.echo.json` instead |
| Text | `.txt`, `.csv` | Lines of `tick button down [p2]`, with an optional `tps=240` line |

Silicate and TCBot macros that change TPS mid-run are imported at their starting TPS.

## Platforms

Windows and Android (32 and 64-bit) are the supported platforms. macOS is untested and iOS is experimental.

## Building

Follow the [Geode setup guide](https://docs.geode-sdk.org/getting-started/), then:

```sh
geode build
```

The **Build Binary** workflow builds every platform on push.

## Reporting bugs

Open an [issue](https://github.com/malikisherenot/xyzBot/issues) with your platform, game version and steps to reproduce.

Known issues:
- Rendering: the video can split into four tiles and move to the bottom left at the end of a render.
- Playback accuracy can drop slightly with each attempt in the editor.

## Credits

xyzBot adapts work from several GPL-3.0 projects. See [about.md](about.md) for the list, and please check each project for its own license.

Bundled third-party files: [JSON for Modern C++](https://github.com/nlohmann/json) (MIT, `src/third_party/json.hpp`) and DejaVu Sans Condensed Oblique (Bitstream Vera license, [licenses/DejaVu-Fonts.txt](licenses/DejaVu-Fonts.txt)).

## License

GPL-3.0, see [LICENSE](LICENSE).
