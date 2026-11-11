<p align="center">
  <img src="logo.png" alt="xyzBot" width="160">
</p>

<h1 align="center">xyzBot</h1>

<p align="center">
  A maximalist <a href="https://geode-sdk.org">Geode</a> bot for Geometry Dash <b>2.2081</b>:<br>
  "All-in-one" bot with wide macro compatibility and a lot of assist tools to make botting easier.
</p>

---
[Join the discord server!](https://discord.gg/dMPZGScthA)
---

## Features

**Bot**
- Tick-accurate recording and playback, with a macro library, search, merge and autosave
- Three accuracy modes, like xdBot: **Vanilla** (no corrections), **Input Fixes** (position restored at every click) and **Frame Fixes** (restored every tick). Any macro plays with the fixes it contains
- Record or play player 1, player 2 or both; take over on input; continue after finish; sequences
- Prevent Death, Maintain Gravity, Mirror Inputs, No Mirror Portals, Block Inputs
- Autoclicker (per player) and CPS counter, Auto Exit

**Speed**
- TPS bypass built for high TPS, speedhack with audio, frame stepper, noclip
- Fast Sub-Ticks: skips display-only work on ticks that are never drawn (physics is untouched)

**Visuals**
- Hitboxes, hitbox trail, trajectory
- Wave Drag Fix: correct wave trail on D blocks, slopes, portals and spider orbs, also at high TPS
- 21 labels (tick, position, velocity, CPS, orbs and more)
- Layout mode

## Supported macro formats

Use the **Import** button on the Macros tab. Imported macros are converted to the native `.xyz` format.

| Format | Extension | Notes |
| --- | --- | --- |
| xyzBot | `.xyz` | Native format, the only save format |
| GDR1/GDR2 | `.gdr`, `.gdr.json`, `.json` | msgpack or JSON. xdBot macros keep their frame fixes |
| xdBot legacy | `.xd` | Inputs and position fixes |
| Silicate | `.slc` | v1, v2 and v3 |
| TCBot | `.tcm` | v1 and v2 |
| Echo | `.echo`, `.echo.json`, `.txt` | Old binary, JSON (current and old) and plain text |
| Text | `.txt`, `.csv` | `tick button down [p2]` lines, optional `tps=240` line |

Not supported yet: Echo's new compressed `.echo`. In those programs, export as `.gdr`, `.gdr.json` or `.echo.json` instead.

Silicate and TCBot macros that change TPS mid-run are imported at their starting TPS.

## Platforms

| Platform | Status |
| --- | --- |
| Android 32-bit | 🟢 Supported |
| Android 64-bit | 🟢 Supported |
| Windows | 🟢 Supported |
| macOS | 🟡 Untested |
| iOS | 🔴 Highly experimental |

## Installation

1. Install [Geode](https://geode-sdk.org) for Geometry Dash 2.2081.
2. Download the `.geode` file for your platform from the releases or from a CI build.
3. Put it in the Geode `mods` folder, or open it with Geode, then restart the game.

## Building

xyzBot is built with the Geode SDK (5.11.2). The **Build** workflow in the Actions tab builds every platform, and iOS is optional: **Actions → Build → Run workflow**.

To build locally, follow the [Geode setup guide](https://docs.geode-sdk.org/getting-started/), then:

```sh
geode build
```

## Known bugs

| Status | Meaning |
| --- | --- |
| 🟢 | Solved, next upd |
| ⌛ | Working on it |
| 🟡 | Unknown, mediocre info
| 🔴 | Not going to get fixed |

| Status | Bug |
| --- | --- |
| 🟡 | Render (video result) at the end, the video splits 4 times and moves to the bottom left. |
| ⌛ | Accuracy getting worse each playback attempt in the editor (slightly) |


## Credits

xyzBot builds on the work of others. Thank you to:

- xdBot: macro and accuracy behavior, `.xd` and xdBot `.gdr` handling
- siliconv and slc: Silicate `.slc` layouts
- Echo: TCBot, Echo and plain format layouts
- NyteLyte's [Wave Trail Drag Fix](https://github.com/nytelytee/geode-wave-trail-drag-fix) and syzzi: the wave drag fix

Check each project for its own license.

## License

GPL-3.0, see [LICENSE](LICENSE).
