# Nova Game Engine

Nova is a community-focused, open-source game engine built in C++20. It is designed to capture the soul and aesthetic of the "classic" era of online building games (2007), while utilizing modern reflection and rendering techniques.

## 🌟 The Vision
Nova is born out of a desire to return to a simpler, community-driven platform, that ROBLOX used to be back in the day. We are here to preserve the aesthetics, but provide modern security, and modern tooling to support modern development.

## 🛠️ Tech Stack
- **Language:** C++20
- **Reflection:** [reflect-cpp](https://github.com/getml/reflect-cpp)
- **Rendering, input and window management:** [SDL3](https://github.com/libsdl-org/SDL)
- **Scripting** [Luau](https://github.com/luau-lang/luau) and [LuaBridge3](https://github.com/kunitoki/LuaBridge3)
- **License:** GPL v3

## ⚖️ Legal & Asset Disclosures
Nova is an independent project and is not affiliated with, sponsored by, or endorsed by Roblox Corporation.

**Legacy Assets:**
This project utilizes certain legacy textures, meshes, and sounds originally created by Roblox Corporation (c. 2007-2009). These assets are used under the principles of preservation and are the intellectual property of Roblox Corp. Nova is intended as a "clean-room" engine implementation; the source code is entirely original and licensed under the GPL v3.

## 🚀 Getting Started
### Configuring CMake

vcpkg:
to configure and build, you must install vcpkg:
```bash
git clone https://github.com/microsoft/vcpkg.git

cd vcpkg

.\bootstrap-vcpkg.bat  # On Windows
# or
./bootstrap-vcpkg.sh   # On Linux/macOS   
```

Configure with the standard cmake command:
```bash
cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Debug
```

`-DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake` please point this to the vcpkg/scripts/buildsystems/vcpkg.cmake
`-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` is optional, unless you want full IDE support.
`-G Ninja` is also optional, but highly recommended over make.
`-DCMAKE_BUILD_TYPE=Debug` you can change this to Release, or other CMake build types


### Building with CMake

Build with the standard cmake command:
```bash
cmake --build build
```
You can also use `ninja` or `make` in the directory `build` but this is much better, and it auto-detects!

## Contributing

We welcome all kinds of contributions, as long as you maintain the charm and aesthetics of 2007, and the code is safe and secure.
