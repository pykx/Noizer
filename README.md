# Noizer.apk

Minimal Android application generating white noise.

The application is using hardware-accelerated audio using open-source library [OpenSL ES](https://www.khronos.org/opensles), as recommended by Google.

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
Debug build types can be installed by ```adb install [.apk]```.

Release build types must be signed to be installed. 

See Androids [signing procedure](https://developer.android.com/studio/publish/app-signing#signing-manually) using apksigner.
