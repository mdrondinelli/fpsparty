# fpsparty — agent guide

C++ multiplayer FPS. Client/server over ENet, Vulkan rendering, fixed-tick
simulation with client-side snapshot interpolation.

## Documentation

Project docs live in [`docs/`](docs/):

- [`docs/testing.md`](docs/testing.md) — testing policy. **Read before adding or
  generating tests.** Covers the test-size budget, the unit-vs-integration
  pyramid, density rules, and the AI test-generation workflow.

## Build & test

- Build: `cmake --build build`
- Test: `ctest --test-dir build` (must be green before merge)
