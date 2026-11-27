# xyzBot on Linux

There is no native Linux version of Geometry Dash, so Geode, and with it xyzBot, runs the Windows build through Wine or Proton.
The normal Windows `.geode` file is what you use. xyzBot has no Windows-only code outside the game's own APIs, so nothing in the
mod itself needs a separate Linux build.

## Playing with xyzBot

1. Install Geometry Dash (Steam) and force the Windows version: Properties > Compatibility > "Force the use of a specific Steam Play
   compatibility tool" (Proton Experimental or a recent Proton).
2. Install Geode with the Windows installer inside the same prefix, or copy Geode's files into the game folder by hand.
3. Add this to the game's Steam launch options, Geode does not load without it:

       WINEDLLOVERRIDES="xinput1_4=n,b" %command%

4. Put `itzmalikhere.xyzbot.geode` in the `geode/mods` folder of the game.

Things that behave differently under Wine:

- **Open Folder buttons** can fail because Wine has no Windows file manager. xyzBot then shows the folder's path in a notification.
  A path like `Z:\home\you\...` is the Linux path `/home/you/...`.
- **Video render:** the hardware encoders (nvenc, amf, qsv) are normally not available in Wine, xyzBot falls back to the next encoder
  automatically. If rendering fails, set Codec to a software one.
- **Clickbot sounds** go through the game's own audio, so they follow the game's output device.

## Building the mod from Linux

Geode cross-compiles the Windows mod on Linux:

1. Install `git`, `cmake` (3.29 or newer), `clang`, `lld` and `llvm`.
2. Install the Geode CLI, then run `geode sdk install-linux` once. It fetches the Windows SDK and the CMake toolchain.
3. Put the Windows FFmpeg libraries (from the "Build FFmpeg" workflow) in `ffmpeg/windows-x64/`, then build for Windows as usual with
   the CLI (`geode build`).
