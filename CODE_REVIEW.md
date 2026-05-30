# FPS Party Critical Code Review

Date reviewed: 2026-05-30

Scope: first-party code under `src/`, top-level `CMakeLists.txt`, shader assets, and local build/test behavior. Vendored third-party code under `extern/` was treated as dependency input rather than reviewed source.

## Executive Summary

`fpsparty` is a small C++23 multiplayer FPS prototype. It has a clear split between game simulation, network transport, rendering, and executable entry points. The project has several strong foundations: RAII wrappers around native resources, deterministic fixed-tick simulation intent, explicit serialization code, server-authoritative state, and a relatively compact module graph.

The current risk profile is still high. The repository has no first-party automated tests, no top-level README or runbook, release builds rely on `assert` for several real preconditions, the game simulation has a likely NaN movement bug when no movement keys are pressed, and the renderer owns enough low-level Vulkan synchronization to need stronger invariants than are currently encoded. Network packet parsing is compact, but packet sizing and protocol evolution are brittle.

The highest priority fixes are:

1. Add focused tests for serialization, movement simulation, entity loading/removal, and client prediction.
2. Fix zero-input humanoid movement before it can create NaN positions.
3. Replace release-build `assert` preconditions around ENet/client/game access with explicit state checks or exceptions.
4. Audit swapchain presentation synchronization and encode ownership by frame or by image with clear fences/semaphores.
5. Make build modes, dependencies, shader compilation, and run instructions reproducible.

Verification performed:

- `cmake --build build` succeeds.
- `ctest --test-dir build --output-on-failure` reports no tests.

`log.txt` is intentionally not used as evidence because it is stale.

## What The Project Does

The project implements a local networked FPS prototype:

- `server` starts an ENet server on port `1109`, creates a grid arena, owns authoritative entity state, accepts player join/leave/input messages, and broadcasts snapshots.
- `client` opens a GLFW/Vulkan window, connects to `127.0.0.1:1109`, sends input, predicts local state, receives grid/entity snapshots, and renders a grid, humanoid cubes, and projectiles.
- `game-core` contains shared entity, grid, movement, and projectile simulation.
- `game-server` contains authoritative players, humanoids, projectiles, and snapshot dumping.
- `game-client` contains replicated state loading and client-side prediction.
- `net` wraps ENet packet/event handling and message serialization.
- `graphics` wraps Vulkan resources, swapchains, pipelines, command recording, and work/fence recycling.

## What It Does Right

### The Module Boundaries Are Sensible

The libraries in `CMakeLists.txt` separate transport, core simulation, server game logic, client game logic, graphics, and executables. That is the right shape for this project. `game-core` is shared by client and server, while server-only and client-only replicated types sit in separate libraries.

This gives the project a path toward deterministic simulation tests and headless server tests without pulling Vulkan into every target.

### Resource Ownership Is Mostly Explicit

The code uses RAII wrappers for ENet, Vulkan, VMA, GLFW, and internal reference-counted resources. This is much better than scattering raw handle destruction across the codebase.

Examples:

- `enet::Unique_packet` and `enet::Unique_host` own ENet packet/host lifetimes.
- `graphics::Work` holds references to buffers/images/pipelines touched by in-flight GPU work.
- `rc::Factory` centralizes allocation and lifetime for custom reference-counted objects.
- `graphics::Graphics::deinit_swapchain()` waits for idle before destroying swapchain resources.

The intent is good: resource lifetimes are usually attached to objects, not comments.

### The Network Protocol Is Small And Inspectable

The protocol is currently simple enough to reason about:

- message type first
- typed fields serialized in network byte order
- separate channels for initialization, input/snapshots, and grid snapshots
- reliable delivery for initial join/grid messages

The small `serial::Serializer<T>` pattern makes supported wire types obvious.

### Client Prediction Is Present Early

The client keeps `_in_flight_input_states`, applies authoritative snapshots, removes acknowledged inputs, and replays remaining inputs. That is the standard architecture for a responsive server-authoritative action game. It is rough, but the core idea is already there.

### The Codebase Is Compact Enough To Harden

There are about 7k first-party C++ lines. That is small enough that adding tests and invariants now would have an outsized payoff. The project has not yet grown around hidden behavior.

## Critical Findings

### 1. Zero-Input Movement Can Produce NaN Positions

Severity: High

`simulate_humanoid_movement()` normalizes `movement_vector` unconditionally:

```cpp
auto movement_vector = Eigen::Vector3f{0.0f, 0.0f, 0.0f};
...
movement_vector.normalize();
```

Reference: `src/game/core/humanoid_movement.cpp:9` and `src/game/core/humanoid_movement.cpp:22`.

When no movement keys are pressed, or opposing keys cancel each other out, the vector has zero length. Normalizing a zero vector is not a valid movement operation and commonly results in NaN components. Those positions then flow into server authoritative state in `src/game/server/game.cpp:57` to `src/game/server/game.cpp:61` and client prediction in `src/game/client/replicated_game.cpp`.

Impact:

- Idle players can get invalid positions.
- NaNs can serialize over the network.
- Collision checks and rendering matrices can become undefined.
- Client prediction error becomes meaningless once NaNs enter the state.

Recommended fix:

- Only normalize if squared norm is greater than a small epsilon.
- Return unchanged position and zero velocity for no movement.
- Add tests for no input, opposing input, diagonal input, and normal movement speed.

### 2. Release Builds Rely On `assert` For Runtime State Validity

Severity: High

Several code paths guard real runtime preconditions with `assert`, which disappears in `NDEBUG` builds.

Examples:

- `net::Client::connect()` asserts it is not already connecting or connected at `src/net/client.cpp:31`.
- `game::Client::service_game_state()` asserts `_game` exists at `src/game/client/client.cpp:18`.
- `game::Client::get_player()` asserts `_game` exists at `src/game/client/client.cpp:82`.
- `net::Server::handle_event()` asserts a disconnecting peer is in `_peers` at `src/net/server.cpp:123`.
- ENet `Peer` methods dereference `_value` without checking, for example `src/enet.hpp:163`.

Impact:

- A bad call order can become a null dereference in release builds.
- Network edge cases can turn into memory safety bugs.
- Public methods are easy to misuse because their contracts are not enforced by types or runtime errors.

Recommended fix:

- Keep debug assertions, but add release-visible checks for external state transitions.
- Make `Peer` operations fail fast on null handles or avoid default-constructible operational peers.
- Consider explicit state enums for client connection state.
- Add tests around connect/disconnect/idempotent calls using fake or integration ENet hosts.

### 3. Swapchain Presentation Synchronization Needs A Formal Invariant

Severity: High

Current frame submission selects the presentation wait semaphore by acquired swapchain image:

- Acquire image in `Graphics::record_frame_work()` at `src/graphics/graphics.cpp:213` to `src/graphics/graphics.cpp:227`.
- Signal `_swapchain_image_release_semaphores[frame_resource.swapchain_image_index]` in `Graphics::submit_frame_work()` at `src/graphics/graphics.cpp:236` to `src/graphics/graphics.cpp:244`.
- Present waits on that same semaphore at `src/graphics/graphics.cpp:246` to `src/graphics/graphics.cpp:252`.
- Release semaphores are created per swapchain image in `src/graphics/graphics.cpp:293` to `src/graphics/graphics.cpp:297`.

Per-image presentation semaphores can be valid if the code proves a semaphore is not reused until presentation has consumed it. That proof is not obvious here. Waiting for `frame_resource.pending_work` only proves GPU rendering submitted by this process is done; it does not by itself prove the presentation engine is finished waiting on a binary semaphore from a prior present.

This may work on common drivers and frame patterns, but the invariant should be explicit because Vulkan synchronization bugs are often intermittent.

Recommended fix:

- Use one render-finished semaphore per frame-in-flight and only reuse it when that frame fence proves the submit has completed, or use a robust per-swapchain-image ownership scheme that waits on the exact image/fence lifecycle required by the swapchain.
- Consider timeline semaphores for internal GPU work, but keep swapchain binary semaphore rules clear.
- Add debug labels and a small stress mode that resizes/toggles present mode repeatedly under validation layers.

### 4. Snapshot Packet Sizing Is Brittle And Has An Extra Byte

Severity: Medium

`Server::send_entity_snapshot()` allocates:

```cpp
sizeof(Message_type) + sizeof(net::Sequence_number) +
sizeof(std::uint16_t) + sizeof(std::uint8_t) +
public_state.size() + player_state.size()
```

Reference: `src/net/server.cpp:76` to `src/net/server.cpp:80`.

Only message type, tick number, public state size, public state, and player state are written at `src/net/server.cpp:84` to `src/net/server.cpp:88`. The extra `sizeof(std::uint8_t)` does not correspond to a written field.

Impact:

- The receiver currently treats the extra byte as part of `player_state_reader` because it creates the player-state subreader from the remainder of the packet.
- If the trailing byte is zero-initialized or unused, this may not currently break parsing, but it corrupts the packet contract.
- Future protocol changes become harder because packet lengths cannot be trusted.

Recommended fix:

- Remove the extra byte.
- Add a packet builder helper that returns writer offset and asserts the exact final size.
- Add round-trip tests for every message type.

### 5. Enum Deserialization Fails To Byte-Swap

Severity: Medium

The enum serializer writes a byteswapped underlying value:

```cpp
const auto byteswapped_value = network_byteswap(casted_value);
```

But the reader calls `network_byteswap(buffer);` and discards the result:

```cpp
network_byteswap(buffer);
const auto enum_value = static_cast<T>(buffer);
```

Reference: `src/serial/serialize.hpp:79` to `src/serial/serialize.hpp:91`.

For the current enum types, the underlying type is `std::uint8_t`, so byte-swapping is a no-op and the bug is latent. If a wider enum is added, cross-endian decoding breaks.

Recommended fix:

- Assign the return value: `buffer = network_byteswap(buffer);`
- Add serialization tests for a `std::uint16_t`-backed enum.

### 6. Client Snapshot Handling Assumes Player Always Has A Humanoid

Severity: Medium

`Client::on_entity_snapshot()` dereferences `player->get_humanoid()` without checking:

- Initial position capture: `src/game/client/client.cpp:121` to `src/game/client/client.cpp:122`.
- Final position capture: `src/game/client/client.cpp:147` to `src/game/client/client.cpp:148`.

The server can represent a player with no humanoid. `Player_dumper` writes only the humanoid id, and if it is zero no input state follows. The server also removes humanoids on projectile collision while players remain.

Impact:

- A local player death or spawn transition can crash the client.
- Any snapshot ordering where player data arrives before humanoid data can crash.

Recommended fix:

- Check `player->get_humanoid()` before reading position.
- Treat no-humanoid snapshots as a normal state, clear prediction state if appropriate, and wait for respawn.
- Add a test for a player snapshot with no humanoid.

### 7. Simulation Has No Collision With The Grid

Severity: Medium

The grid is rendered and serialized, but movement does not query it. Humanoid movement just moves in a straight line, and projectile movement only falls under gravity. The server collision pass only checks projectile/humanoid AABBs and projectile `y < 0`.

References:

- Humanoid movement: `src/game/core/humanoid_movement.cpp`.
- Projectile movement: `src/game/core/projectile_movement.cpp`.
- Server tick collision: `src/game/server/game.cpp:73` to `src/game/server/game.cpp:110`.

Impact:

- Walls and floors are visual, not gameplay constraints.
- Players can move through arena walls.
- Projectiles ignore walls.

Recommended fix:

- Decide whether the grid is terrain, collision, or decoration.
- If terrain, add grid-aware sweeps or at least axis-separated collision for humanoids and projectiles.
- Add deterministic tests for wall stop, floor support, and projectile impact.

### 8. The Server Tick Loop Can Spiral Or Drift Under Load

Severity: Medium

`Server::service_game_state()` subtracts elapsed duration and advances at most one fixed tick:

```cpp
_tick_timer -= duration;
if (_tick_timer <= 0.0f) {
  _tick_timer += _tick_duration;
  _game.tick(_tick_duration);
  return true;
}
```

Reference: `src/game/server/server.cpp:14` to `src/game/server/server.cpp:24`.

If a frame takes multiple tick durations, the server does not catch up. This may be intentional for a prototype, but it means tick rate falls under load and snapshots no longer reflect wall-clock time.

Recommended fix:

- Use an accumulator and process up to a capped number of ticks per loop.
- Broadcast after each tick or after a fixed snapshot cadence.
- Measure and log dropped/capped ticks.

### 9. Build Configuration Is Too Opinionated And Not Reproducible Enough

Severity: Medium

The top-level CMake forces `-O3`, `-stdlib=libc++`, and `-lc++abi` for non-MSVC builds:

Reference: `CMakeLists.txt:199` to `CMakeLists.txt:218`.

It also globally enables Tracy:

Reference: `CMakeLists.txt:5`.

Impact:

- Debug builds still compile with `-O3`, making debugging harder.
- GCC/libstdc++ environments are excluded unless they happen to support libc++.
- Profiling instrumentation is always compiled in.
- Sanitizer and coverage builds are harder than necessary.

Recommended fix:

- Respect `CMAKE_BUILD_TYPE` or presets.
- Make libc++ and Tracy options configurable.
- Add CMake presets for debug, release, ASan/UBSan, and validation.
- Add shader compilation to the build or document how `.spv` files are produced.

### 10. No First-Party Automated Tests

Severity: High for project maturity

`ctest --test-dir build --output-on-failure` reports no tests.

This is the largest maintainability gap because the codebase already has logic that is well suited to tests:

- serialization round-trips
- malformed packet parsing
- grid load/dump
- movement and projectile simulation
- entity world load/remove behavior
- client prediction replay
- sequence number ordering

Recommended fix:

- Add a small test framework target, such as Catch2, doctest, or GoogleTest.
- Start with `game-core` and `serial`; they do not need Vulkan or live networking.
- Make `ctest` part of the default verification workflow.

## Architecture And Maintainability

The strongest architectural choice is the separation between shared game core and client/server-specific entity types. Keep that. It gives the server authority over gameplay while letting the client reuse deterministic movement logic for prediction.

The main architectural weakness is that some subsystems are wrappers, but their contracts are not fully represented in the types. `enet::Peer` can be default-constructed and then dereferenced. `game::Client` exposes methods that require `_game` but only asserts that condition. `Work` can release its resource while callbacks can still be added unless all access is serialized by convention.

This code would benefit from fewer nullable operational handles and more explicit states:

- `Connection_state::disconnected | connecting | connected`
- `Player_state::joined | spawned | dead`
- `Frame_resource` owning all per-frame sync primitives
- packet types with declared payload lengths

The custom `rc` implementation is ambitious. It may be justified for intrusive allocation and GPU lifetime needs, but it should be tested heavily. Reference-counted code is unforgiving. At minimum, add tests for copy/move, weak lock after destruction, downcast behavior, `From_this`, and factory memory cleanup on constructor throw.

## Networking Review

The network layer is concise and readable, but currently trusts too much surrounding state.

Good:

- Message parsing is centralized.
- Malformed packets are dropped rather than blindly applied.
- Reliable messages are used for initial lifecycle events.
- Player input has sequence numbers and server-side monotonic filtering.

Weak:

- No protocol version.
- No packet length validation after parsing.
- No authentication or anti-spoofing within a peer beyond checking the player id is in that peer's player list.
- `Sequence_number` comparisons do not handle wraparound.
- Input and entity snapshots share channel `1`, which may be intentional, but it should be documented because ENet channel ordering can matter.

Recommended direction:

- Define a protocol document or header comment listing message layouts and reliability/channel expectations.
- Add exact-length validation for fixed messages.
- Add fuzz-style tests for short, long, and invalid packets.
- Add sequence comparison helpers before wraparound matters.

## Gameplay Review

The game loop is straightforward and server-authoritative. The current behavior is more prototype than game:

- The server spawns humanoids only while there are fewer than two humanoids.
- Player spawn positions are implicit defaults.
- There is no health, respawn timer, team state, or score state.
- Projectile collision deletes humanoids immediately.
- Grid collision is absent.

That is acceptable for an early prototype, but it should be made explicit in docs or issues. The risk is that networking and rendering will get more complex before core gameplay rules are stable and tested.

Recommended direction:

- Stabilize a tiny gameplay contract: spawn, move, shoot, hit, die, respawn.
- Put it under deterministic tests.
- Only then add richer rendering or network interpolation.

## Graphics Review

The graphics layer shows serious effort: dynamic rendering, explicit work resources, swapchain recreation, resource references for in-flight work, and debug names for semaphores in non-`FPSPARTY_VULKAN_NDEBUG` builds.

The main problem is that Vulkan code requires very tight invariants and this layer currently leaves too much to convention:

- Presentation semaphore reuse should have an explicit proof.
- Layout transition helper always uses color aspect, so it is not safe for depth images even though the same `Image` abstraction can represent depth.
- `begin_rendering()` clears depth to `0.0f` and uses `Compare_op::greater`; that is a valid reversed-depth convention, but it should be documented because it looks wrong at first glance.
- Swapchain recreation waits idle, which is simple and safe, but expensive. Fine for now.

Recommended direction:

- Write a `docs/graphics-sync.md` explaining frame resources and swapchain sync rules.
- Keep validation layers enabled in a debug preset.
- Add a smoke mode that runs a few frames and exits for CI machines with Vulkan support, but keep core tests independent of Vulkan.

## Build, Tooling, And Repository Hygiene

The repository currently lacks a top-level README. That creates avoidable friction:

- How to install dependencies.
- How to configure and build.
- How to compile shaders.
- How to run server and client.
- What platforms are supported.
- What CMake presets are expected.

The repo vendors large dependencies directly. That can be pragmatic, but it should be documented. If vendoring stays, consider trimming generated docs/build outputs from vendored trees if they are not needed.

Recommended minimum docs:

- `README.md`: build/run instructions and project overview.
- `docs/protocol.md`: packet layouts.
- `docs/gameplay.md`: current gameplay rules.
- `docs/graphics-sync.md`: frame synchronization model.

## Suggested Roadmap

### Phase 1: Correctness Baseline

1. Fix zero-input movement.
2. Fix enum byteswap assignment.
3. Fix entity snapshot packet size.
4. Harden client null checks around player/humanoid state.
5. Add unit tests for those fixes.

### Phase 2: Test Harness

1. Add a first-party test target and `enable_testing()`.
2. Test `serial`, `game-core`, and `Entity_world`.
3. Add malformed network packet tests using direct `handle_event` alternatives or a parser split out from ENet events.
4. Add client prediction replay tests without real ENet.

### Phase 3: Runtime Contracts

1. Replace runtime `assert`-only checks with explicit states.
2. Document and test connection lifecycle.
3. Add protocol versioning and length checks.
4. Add logging levels instead of unconditional `std::cout`.

### Phase 4: Graphics Robustness

1. Audit swapchain sync and encode the selected invariant in code.
2. Add validation-layer stress testing locally.
3. Generalize layout transitions for color/depth aspects.
4. Add CMake presets for validation/debug/release.

## Final Assessment

This is a promising prototype with unusually good separation for its size. The code is readable, the library split is sound, and the foundational direction for server-authoritative multiplayer is correct.

The project is not yet robust. The absence of tests is the central problem, and there are several concrete correctness issues that tests would catch immediately. The next engineering push should focus less on adding features and more on locking down deterministic core behavior, protocol correctness, and runtime state invariants. Once those are stable, the existing architecture gives the project a reasonable path forward.
