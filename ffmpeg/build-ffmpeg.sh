#!/usr/bin/env bash
# Builds a MINIMAL static FFmpeg for xyzBot. Only what rendering needs is switched on, so each platform
# adds a few MB instead of the ~75 MB of the full libraries. Run it inside the FFmpeg source folder.
#   Android:  NDK=/path/to/ndk ABI=arm64-v8a  ./build-ffmpeg.sh android
#   Windows:  (MSYS2 shell with the Visual Studio tools in PATH)  ./build-ffmpeg.sh windows
# Result: copy "install/include" and "install/lib" into  <project>/ffmpeg/<platform>/
#   platform names: android-arm64-v8a, android-armeabi-v7a, windows-x64
set -e
TARGET="${1:?android or windows}"
PREFIX="$PWD/install"

COMMON=(
  --prefix="$PREFIX"
  --disable-everything --disable-autodetect --disable-programs --disable-doc --disable-debug
  --disable-network --disable-avdevice --disable-avfilter --disable-postproc
  --enable-static --disable-shared --enable-pic --enable-small
  --enable-avcodec --enable-avformat --enable-avutil --enable-swscale --enable-swresample
  # video: MPEG-4 is built in (LGPL). Add libx264 or libopenh264 here if you ship those libraries.
  --enable-encoder=mpeg4
  # audio: AAC encoder, decoders for the song formats Geometry Dash uses
  --enable-encoder=aac
  --enable-decoder=mp3float,mp3,vorbis,flac,pcm_s16le,pcm_s24le,pcm_s32le,pcm_f32le,pcm_u8,aac
  --enable-muxer=mp4
  --enable-demuxer=mov,mp3,ogg,flac,wav,aac
  --enable-parser=mpegaudio,vorbis,flac,aac,mpeg4video,h264
  --enable-protocol=file
)

case "$TARGET" in
  android)
    : "${NDK:?set NDK}"; ABI="${ABI:-arm64-v8a}"; API=24
    TC="$NDK/toolchains/llvm/prebuilt/linux-x86_64"
    if [ "$ABI" = "arm64-v8a" ]; then ARCH=aarch64; CC="$TC/bin/aarch64-linux-android$API-clang"; else ARCH=arm; CC="$TC/bin/armv7a-linux-androideabi$API-clang"; fi
    ./configure "${COMMON[@]}" --enable-cross-compile --target-os=android --arch=$ARCH \
      --enable-lto --cc="$CC" --cxx="${CC}++" --ar="$TC/bin/llvm-ar" --ranlib="$TC/bin/llvm-ranlib" --strip="$TC/bin/llvm-strip" \
      --sysroot="$TC/sysroot" --extra-cflags="-Os -fvisibility=hidden" \
      ${MEDIACODEC:+--enable-jni --enable-mediacodec --enable-encoder=h264_mediacodec}
    ;;
  windows)
    # MSVC toolchain (cl.exe in PATH, run from an MSYS2 shell): gives avcodec.lib etc. that link with the mod.
    # -MT matches Geode's static runtime, change it if your mod build uses another one.
    ./configure "${COMMON[@]}" --toolchain=msvc --target-os=win64 --arch=x86_64 --extra-cflags="-MT -O1"
    ;;
  *) echo "unknown target"; exit 1 ;;
esac
make -j"$(nproc)"
make install
echo "Done: copy $PREFIX/include and $PREFIX/lib into <project>/ffmpeg/<platform>/"
