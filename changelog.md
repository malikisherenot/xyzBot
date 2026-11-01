# Changelog

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
