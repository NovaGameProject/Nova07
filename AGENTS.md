# AGENTS.md

## Build & Run

```sh
xmake f -m debug && xmake   # configure + build Nova07 (client)
xmake run                    # launch the engine (PlaySolo mode)

xmake build NCCService       # build headless server
xmake run NCCService -- --port 27015

# Multiplayer test:
# Terminal 1: xmake run NCCService -- --port 27015
# Terminal 2: xmake run -- --connect 127.0.0.1 --port 27015
```

Binary output: `build/linux/x86_64/debug/Nova07` (Linux).
`compile_commands.json` auto-generated in `build/`.

Requires: xmake, Vulkan SDK (for `glslc`), Vulkan-capable GPU.

## Project Structure

Single C++20 target under `Nova` namespace, not a monorepo.

- `src/main.cpp` — entrypoint, creates `Nova::Engine`
- `src/ncc_main.cpp` — headless server entrypoint (NCCService target)
- `src/Engine/Engine.hpp` — core engine class (owns Window, TaskScheduler, Renderer, DataModel)
- `src/Engine/Nova.hpp` — umbrella header that includes all Objects and Services
- `src/Engine/Objects/` — Instance hierarchy (Instance → BasePart → Part, Model, Script, Explosion, Humanoid, Player, RemoteEvent, RemoteFunction)
- `src/Engine/Services/` — DataModel, Workspace, ScriptContext, PhysicsService, Lighting, NetworkService
- `src/Engine/Networking/` — NetworkID registry, ReplicationProtocol (binary serialization)
- `src/Engine/Reflection/` — class descriptor system; register new classes in `RegisterClasses.cpp`
- `src/Engine/Rendering/` — Vulkan renderer
- `src/Engine/Physics/` — JoltPhysics integration (assemblies, explosions, contact listener)
- `src/Engine/Common/` — Signal system (`Signal.hpp`), utilities
- `shaders/` — HLSL vertex/fragment shaders
- `resources/` — textures, fonts, skybox, Lua scripts, `.rbxl` level files

## Dependencies

All managed by xmake (see `xmake.lua`): SDL3 (Vulkan), SDL3_image, shaderc, Luau, LuaBridge3, GLM, JoltPhysics, pugixml, **ENet** (networking), Tracy v0.12.2.

Tracy profiler is always enabled (`TRACY_ENABLE` define).

## Shaders

HLSL source in `shaders/` compiled to SPIR-V by the custom `hlsl2spv` xmake rule.

**Filename must contain `vert` or `frag`** — the rule infers the shader stage via string match. A name like `base.vert.hlsl` or `myeffect.frag.hlsl` works; anything else errors.

## Conventions

- All headers use `.hpp` — no `.h` files exist.
- Every source file must have the GPL v3 license header (see `license_header.txt`).
- New game object classes must be registered in `src/Engine/Reflection/RegisterClasses.cpp` using `ClassDescriptorBuilder<T>("ClassName", "ParentClassName")`. Call `.Property()`, `.Method()`, `.Signal()` as needed, then ensure `ClassDescriptor::ResolveInheritance()` is called after all registrations.
- `.rbxl` files are Roblox-format place files loaded by `Engine::LoadLevel()`.
- No test framework, no CI, no linter/formatter configured.

## Key Patterns

- `Engine::Initialize()` → sets up SDL3 window, Vulkan renderer, DataModel, physics, default lighting.
- `Engine::InitializeHeadless()` → headless mode (no window, no renderer) for NCCService.
- `Engine::LoadLevel(path)` → parses `.rbxl` via pugixml, builds instance tree.
- `Engine::Run()` → main loop driven by TaskScheduler.
- Objects use a custom `Signal<T>` for events (e.g., `BasePart::Touched`).
- Luau scripting exposed via `ScriptContext::Execute()` — see `main.cpp` for examples.

## Engine Modes

The engine has three modes (`Engine::Mode`):

| Mode | Physics | Scripts | Render | Network |
|------|---------|---------|--------|---------|
| PlaySolo | ✅ Local | ✅ Local | ✅ | ❌ |
| Client | ❌ | ❌ | ✅ | ✅ |
| Server | ✅ | ✅ | ❌ | ✅ |

- `--connect host` → Client mode (no level file loaded, receives state from server)
- NCCService binary → Server mode (headless, authoritative)
- No args → PlaySolo mode (local simulation)

## Humanoid System

- `Humanoid` class creates R6 body parts (Head, Torso, Arms, Legs) as separate physics bodies.
- Parts connected via `SwingTwistConstraint` joints (JoltPhysics).
- Keep-upright mechanism applies angular impulse to keep character standing.
- Strong impacts can tip character over (2007 Roblox physics feel).
- On death: joints destroyed, parts fall like Legos. No ragdoll posing.
- Humanoid parts bypass the assembly compound-shape system.

## Networking System

- `NetworkService` provides client-server networking using ENet.
- **Threaded architecture**: ENet I/O runs on a background thread; main thread processes queued packets (max 20/frame).
- Server authoritative model: server owns game state, clients receive replicated state.
- FullSync on connect: server sends incremental object stream (5 objects/tick).
- Adaptive replication: CFrame replicates when delta exceeds threshold (0.1 studs / 5°) or at 20Hz max.
- `NetworkID` system maps Instances to network IDs for replication.
- `Player` objects represent connected clients.
- `RemoteEvent` / `RemoteFunction` provide RPC between client and server.
- Properties marked with `.Replicated("PropertyName")` in RegisterClasses.cpp are automatically replicated.
- Services (DataModel, PhysicsService, etc.) are NOT replicated — only game objects.

## Adding a New Object Class

1. Create header/cpp inheriting Instance
2. Add to `InstanceFactory::Register<T>()`
3. Add `ClassDescriptorBuilder<T>()` in RegisterClasses.cpp
4. Add LuaBridge `deriveClass` in ScriptContext::BindAPI()
5. Add to `Nova.hpp` umbrella header
6. For network-replicated properties, call `.Replicated("PropertyName")` in RegisterClasses.cpp
