# xyzBot

A macro bot for Geometry Dash. It records and replays tick-accurate inputs, and includes assist tools for making macros.

- Record and replay with three accuracy modes: Vanilla, Input Fixes and Frame Fixes
- TPS bypass, speedhack, frame stepper and noclip
- Import macros from xdBot, Silicate, TCBot, Echo and plain text formats
- Hitboxes, hitbox trail, trajectory preview and Wave Drag Fix
- 21 on-screen labels and a layout mode
- Video rendering (Android, requires the FFmpeg API mod)
- Two menu styles: Glass and Paper (experimental)

**This is a cheat mod.** Do not use it to submit runs to leaderboards or lists that forbid bots.

Report bugs and suggestions on the [issue tracker](https://github.com/malikisherenot/xyzBot/issues) or the [Discord server](https://discord.gg/JYYKDxBPDg).

## Credits

xyzBot reuses ideas and code from other projects, all GPL-3.0 licensed like this mod:

- **xdBot**: accuracy modes, `.xd` and xdBot `.gdr` handling
- **Echo**: Echo, TCBot and plain-text macro formats, rotation fix, rendering approach
- **Silicate / slc** (siliconv): `.slc` macro layouts
- **Wave Trail Drag Fix** (NyteLyte, with syzzi's rotation idea): Wave Drag Fix
- **Scarlet Utils**: behaviour of the showcase and click-assist options (reimplemented)
- **GDR** (maxnut): replay format v2
- **JSON for Modern C++** by Niels Lohmann (MIT) and **DejaVu Sans** (Bitstream Vera license)
