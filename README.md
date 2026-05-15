# Nova Game Engine

Nova is a community-focused, open-source game engine built in C++20. It is designed to capture the soul and aesthetic of the "classic" era of online building games (2007), while utilizing modern reflection and rendering techniques.

## 🌟 The Vision
Nova is born out of a desire to return to a simpler, community-driven platform, that ROBLOX used to be back in the day. We are here to preserve the aesthetics, but provide modern security, and modern tooling to support modern development.

## 🛠️ Tech Stack
- **Language:** C++20
- **Rendering, input and window management:** [SDL3](https://github.com/libsdl-org/SDL)
- **Scripting** [Luau](https://github.com/luau-lang/luau) and [LuaBridge3](https://github.com/kunitoki/LuaBridge3)
- **License:** GPL v3

## ⚖️ Legal & Asset Disclosures
Nova is an independent project and is not affiliated with, sponsored by, or endorsed by Roblox Corporation.

**Legacy Assets:**
This project utilizes certain legacy textures, meshes, and sounds originally created by Roblox Corporation (c. 2007-2009). These assets are used under the principles of preservation and are the intellectual property of Roblox Corp. Nova is intended as a "clean-room" engine implementation; the source code is entirely original and licensed under the GPL v3.

## 🚀 Getting Started

### Prerequisites

Install [xmake](https://xmake.io):
```bash
# Linux/macOS
curl -fsSL https://xmake.io/shget.text | bash

# Windows (PowerShell)
Invoke-WebRequest -Uri "https://xmake.io/psget.text" -OutFile install.ps1; .\install.ps1
```

You also need a Vulkan-capable GPU and the Vulkan SDK (for `glslc` shader compiler).

### Building

Configure and build in one step:
```bash
xmake f -m debug   # configure for debug mode
xmake              # build
```

Run the engine:
```bash
xmake run
```

Other useful commands:
```bash
xmake f -m release   # switch to release mode
xmake clean          # clean build artifacts
xmake install        # install to system (optional)
```

## Contributing

We welcome all kinds of contributions, as long as you maintain the charm and aesthetics of 2007, and the code is safe and secure.
