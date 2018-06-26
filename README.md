# Noizer.apk

Minimal Android application generating white noise.

The application is using hardware-accelerated audio using open-source library [OpenSL ES](https://www.khronos.org/opensles).

See the shared library [libnoizer](app/src/main/cpp/libnoizer.cpp) for the noise generator implementation.

## Build
Build with 
```bash
$ ./gradlew build
```

Unsigned release build is found in 
```bash
app/build/outputs/apk/release/app-release-unsigned.apk
```

Debug build is found in 
```bash
app/build/outputs/apk/debug/app-debug.apk
```

## Install
Unsigned application may be installed if third party markets are trusted or by ```adb install [.apk]```.

If application is to be distributed, signing is necessary.