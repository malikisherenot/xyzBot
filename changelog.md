# Changelog

## v1.17.0
- Auto updater: checks GitHub once a day for a new release, shows its changelog in the xyzBot menu (Bot > Updates), and downloads it in-game after you confirm. The download is checked against a SHA-256 file before it replaces the mod, and the old file is kept as a backup. Restart the game to finish
- The build workflow now also produces the .sha256 file; upload it to the release next to the .geode

## v1.16.1
- Profiler: the batching estimate now uses tick time instead of the whole frame (which was dominated by vsync idle), shows tick capacity in TPS, and has three estimates; collisions, effects and enter effects are timed separately

## v1.16.0
- Profiler (TPS settings) with three labels: frame time split between ticks and drawing, cost per tick by part, and an estimate of how much batching ticks could gain
- Render Speed and Render Time labels; the render and profiler labels are not drawn into the video

## v1.15.2
- Trajectory Autoclicker now follows Echo's rules: it keeps the current input state until that would die, only changes it when an alternative survives longer (checked over twice the lookahead), taps instead when that is better, and defaults to a lookahead of 1

## v1.15.1
- Render: frames are encoded on a separate thread with a small queue, so the game no longer waits for the encoder
- Render: on Windows the GPU readback uses a ring of pixel buffers, so the game no longer stalls waiting for the screenshot

## v1.15.0
- New Trajectory Autoclicker (Bot > Survival): simulates ahead every tick and clicks, releases or taps to survive; clicks are recorded when recording

## v1.14.2
- Render: hardware encoders (NVENC, AMF, QuickSync) are tried first when available, falling back to software
- Render: vsync is switched off while rendering on Windows, so the render is not capped by the monitor refresh rate
- Render: the frame budget no longer drops ticks while rendering

## v1.14.1
- Import: Silicate v2 and v3 .slc (restarts keep only the last attempt), TCBot .tcm, old Echo .echo, Echo json and Echo plain text

## v1.14.0
- Accuracy modes like xdBot: Vanilla, Input Fixes (position stored at every click), Frame Fixes (every tick). Macros play with whatever fixes they contain
- Much wider macro import: xdBot .gdr / .gdr.json (with their frame fixes and old frame offsets), legacy .xd, Silicate v1 .slc, GDR1 from other bots, .txt/.csv
- Macro search in the Macros tab; Recording and Playback options moved to the Bot tab
- TPS optimization: ticks of a frame that are never drawn skip the progress bar update (Fast Sub-Ticks); optional visibility skip; less work per tick

## v1.13.0
- New Wave Drag Fix (Visuals > Fixes): correct wave trail on D blocks, slopes, portals, spider orbs/pads and at high TPS (port of NyteLyte's Wave Trail Drag Fix, GPL-3.0)

## v1.12.1
- Render: FFmpeg API support is now enabled on Android builds (same setup as Echo), dependency declared as optional for Android

## v1.12.0
- Rendering: Start Render / Stop Render in the new Render tab, using the eclipse.ffmpeg-api mod
- Fixed frame time while rendering, optional macro to render, resolution, FPS, bitrate, codec, end tail, song audio

## v1.11.0
- Tap the number in any value row (TPS, speed, lengths, colors, ...) to type an exact value

## v1.10.0
- Take Over: switch from playing a macro to recording it (button, F8, or automatically on your first tap)
- Continue After Finish: a macro keeps recording at its end
- Continued macros are saved back under their own name

## v1.9.0
- Ported from Echo (GPL-3.0): Fix Rotation Bug (visual tilt of ship, UFO, wave and swing)
- Macro tools: Record Players and Play Players (Player 1 or 2 only), Auto Name Replay, No Overwrite with rename formats
- Macro Merge, Sequence (ordered or random playlist), Swap Player Input, Inverse Input

## v1.8.0
- Ported from Scarlet Utils (own implementation): Click Green Dash / Black Orbs, Easy Black Orb UFO, Click Jump / Gravity Pads
- Auto Straight Fly, Auto Straight UFO, Auto Swift, Extra Clicks, Flip Input On Death
- No Death Effect, No Effects, Hide Endscreen, Hide New Best, Stacked Orb Optimization, Restart First Frame
- Level and audio fade in/out, layout mode background/ground colors
- Noclip per player, swift autoclicker

## v1.7.0
- Prevent Death and trajectory ignore the hidden spike at the start of every level
- On-screen frame stepper controls (Step, +N, Hold, Run) for touch devices, with a settings page

## v1.6.2
- Macros are saved under the level name
- Smaller file size text under macro names
- Menu font switch (GD old / Arial new) in the Theme tab

## v1.6.1
- Fixed build error (ARIAL_TAG declared too late in Draw.hpp)
- Arial text is now sized to match GD's font automatically (fixes tiny text)

## v1.6.0
- Fixed build error in Labels.cpp (labels now take the active layer)
- Arial Italic now compiled by Geode from the TTF (fixes the font not loading)
- (?) tooltips next to options and labels
- Option to hide the floating button while playing a level
- Removed the pause-menu button; gear buttons moved left

## v1.5.0
- Editor playtest support: record, replay, TPS, labels, hitboxes, trajectory
- StartPos support: macros store their start position and can be played from a later StartPos
- Start Position label
- .xyz format v3 (stores start position; v2 and v1 still load)

## v1.4.0
- Fixed stray lines, white panels and square artifacts (premultiplied alpha, duplicate polygon points)
- Theme tab: 7 presets, custom accent and panel tint
- Label colors (per label and all labels)
- Arial Italic font for all menus and labels
- Animations: menu open/close, row slide-in, sliding tab highlight, switch knob, floating button

## v1.3.0
- SSB Factor and SSB Event labels
- SSB Fix (song speed correction during the bug)
- Legacy Ship Weight (upside-down ship), tunable %

## v1.2.1
- Fixed invalid dependencies in mod.json
- Darker, clearer OFF switches
- Gear buttons for option settings
- Floating button no longer disappears after startup

## v1.2.0
- Glass UI with scrolling, blurred panel
- New draggable XYZ circle button (all layers)
- Hypercompressed .xyz v2 format (old .xyz still loads)
- Fixed circular hazard hitboxes, player circle

## v1.1.0
- Labels, layout mode
- Prevent Death, Maintain Gravity, Mirror, No Mirror Portals
- Autoclicker, CPS, autosave, auto exit, speedhack audio

## v1.0.0
- First release
