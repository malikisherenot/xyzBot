# FFmpeg for xyzBot rendering

xyzBot links FFmpeg itself (src/render/Encoder.cpp), so the eclipse.ffmpeg-api mod is no longer needed.

0. Easiest: run the "Build FFmpeg" workflow on GitHub (Actions tab, .github/workflows/build-ffmpeg.yml). It builds all three
   platforms and gives you one artifact "ffmpeg-libs"; unzip it into the project root and skip to step 3.

1. Or get the FFmpeg source (7.x or 6.x), copy `build-ffmpeg.sh` into it and run it (see the top of the script).
2. Copy the result next to this file:

       ffmpeg/android-arm64-v8a/{include,lib}
       ffmpeg/android-armeabi-v7a/{include,lib}
       ffmpeg/windows-x64/{include,lib}

3. Configure the mod as usual. CMake prints "rendering with FFmpeg from ..." when it finds the libraries; without them
   the mod still builds and rendering is simply off.

Only 5 libraries are needed (avformat, avcodec, swresample, swscale, avutil), not avfilter or avdevice.
They are linked statically and unused code is dropped, so they end up inside the mod binary.

Codecs: the script enables only the built-in `mpeg4` video encoder (LGPL). For small H.264 files add `libx264` (GPL, changes
your license) or `libopenh264`, or build with `MEDIACODEC=1` on Android for the hardware encoder and configure CMake with
`-DXYZ_MEDIACODEC=ON`. The render code tries the encoders in order and uses the first one that opens.
