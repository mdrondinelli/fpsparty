# Testing Policy & Guide

## 1. Purpose

Testing matters in this repo, and designing **testable APIs** is a valued skill
worth optimizing for. But the test suite must never become a huge blob. AI
agents (and humans) tend to generate sprawling, near-duplicate tests that balloon
to the size of the runtime code and stop being reviewable. This document is the
**contract** for adding tests here. Read it before writing a single `TEST_CASE`.
The overriding goal: keep the suite **maintainable and small relative to runtime
code**, so every test stays reviewable.

## 2. Tests as documentation

A test should read as an **executable spec** of the behavior it covers. Someone
who doesn't know the API should be able to learn how it's meant to be used by
reading its tests.

- **`TEST_CASE` names state the behavior as a sentence,** not the function under
  test. Good: `"Entity world assigns globally unique ids across entity types"`.
  Bad: `"emplace test"`, `"test2"`.
- **Arrange / act / assert is visible at a glance:** set up state, perform the
  one action, assert the observable result. Keep each case short and linear.
- **Drive the real public API** rather than reaching into internals, so the test
  doubles as a usage example.
- **No cleverness that obscures intent.** A reader shouldn't have to decode
  helper indirection to see what's verified. (Shared helpers: yes — see §7.
  Hidden control flow: no.)
- If a case needs a paragraph of comments to explain what it checks, the API or
  the test is probably too complex — fix that instead.

This reinforces the density rules (§7): a test that reads clearly is usually also
small and non-duplicative.

## 3. Layout & conventions

- **Framework:** Catch2 v3. Include `<catch2/catch_test_macros.hpp>` (and
  `<catch2/catch_approx.hpp>` for float compares). Use `TEST_CASE`, `CHECK`,
  `REQUIRE`, `SECTION`, and `GENERATE`.
- **Mirroring:** tests mirror `src/` directories.
  `tests/<area>/<file>.cpp` corresponds to `src/<area>/`.
- **One target per area:** each area is a CMake executable named `<area>-tests`,
  linking the area's library plus `Catch2::Catch2WithMain`, registered with
  `add_test`.
- **Run:** `ctest` from `build/`, or run a target directly, e.g.
  `./build/game-tests`.

### Adding a new test target (copy-paste recipe)

```cmake
add_executable(
  <area>-tests
  tests/<area>/<file>.cpp
)
target_link_libraries(<area>-tests <area> Catch2::Catch2WithMain)
add_test(NAME <area>-tests COMMAND <area>-tests)
```

Then append `<area>-tests` to the shared property/option lists near the bottom of
`CMakeLists.txt`:

- the three `set_property(TARGET ... PROPERTY CXX_STANDARD 23 / CXX_STANDARD_REQUIRED ON / CXX_EXTENSIONS OFF)` lines,
- the per-target `target_compile_options(<area>-tests PRIVATE ${fpsparty_compile_options})` line (and the MSVC `/W4 /WX` branch),
- the per-target `target_link_options(<area>-tests PRIVATE ${fpsparty_link_options})` line.

Tests build with `-Wall -Wextra -Wpedantic -Werror`, so they must be warning-clean.

## 4. The budget (the core rule)

**Test code must NEVER exceed total `src` code.** That is the hard ceiling.

- **Soft target:** test LOC `<= 40%` of the *testable core* =
  `game + scene + serial + net + math + ppm`.
- **Track it:** `cloc tests` vs `cloc src`.
- **In review:** if a PR's test LOC grows faster than the code it covers, **prune
  before merging.**
- As of this writing: **~510 lines of test code vs ~8.5k src** — healthy. Stay
  there.

## 5. Cost-tiered pyramid — push work down

- **Unit (default, ~90%):** pure modules (`scene`, `game`, `serial`, `math`,
  `ppm`). No I/O, no mocks. `tests/game/entity_world.cpp` is the template —
  dense, ~17 lines/case, zero scaffolding.
- **Integration (rare, capped):** network transport. **1-2 SKIP-guarded smoke
  tests TOTAL**, behind a single shared harness — not one mock server per file.
  See `tests/net/transport.cpp`. Skip cleanly when ENet is unavailable:

  ```cpp
  auto enet_guard = enet::Initialization_guard{};
  try {
    enet_guard = enet::Initialization_guard{{}};
    server = std::make_unique<Test_server>(port);
  } catch (enet::Initialization_guard::Construction_error const &) {
    SKIP("ENet initialization is unavailable in this environment.");
  }
  ```

- **None:** graphics/Vulkan, app/render glue, trivial getters.

## 6. Testability is the lever

Separate pure logic from I/O so it unit-tests **without** ENet or Vulkan.

- `scene::Scene` is the model: pure, no I/O, so it is 1:1 testable. Aim for this.
- **Anti-pattern:** the server tick counter in `tests/server/server.cpp` needs a
  live ENet socket just to advance ticks (`SKIP`-guarded for that reason). That
  is an **API smell to fix, not to test around** — the tick logic should be
  extractable and testable without a socket.

## 7. Density rules (anti-bloat)

- **One behavior per `TEST_CASE`.** Cover input variants with `GENERATE` /
  `SECTION`, **never** copy-pasted near-duplicate cases.
- **Shared helpers/harness live in ONE place,** not re-declared per file.
- **Assert behavior / observable contract,** not implementation details.
- **DELETE obsolete tests on refactor — never comment them out.** Real example:
  a 107-line commented-out test plus a stale "ids per type" assertion both rotted
  in `tests/client/local_player.cpp` until they were removed. Don't recreate that.
- **Every assertion must be able to fail for a real reason.** No
  tautological/trivial asserts.

## 8. AI generation workflow (guardrails)

- **Generate module-by-module.** Never "tests for the whole suite" in one shot.
- **Reject any case that only re-exercises a path already covered.**
- Each batch must pass this review checklist before commit:
  - [ ] reads as documentation: behavior-stating name, clear arrange/act/assert
  - [ ] asserts behavior, not implementation
  - [ ] no dead / commented-out tests
  - [ ] no duplicated setup
  - [ ] reuses shared helpers
  - [ ] within the LOC budget
  - [ ] every case can fail for a real reason
  - [ ] obsolete tests deleted, not commented

## 9. What to test vs skip

| Test | Skip / minimize |
| --- | --- |
| Pure logic | Glue code |
| Edge / boundary cases | GPU / Vulkan rendering |
| Bug-prone areas: hash tables, interpolation bracket math, grid raycast | Trivial getters |
| Serialization round-trips (when `serial` changes) | Network transport beyond the one smoke test |

## 10. Coverage priorities / current gaps

- **Highest-value target:** `scene/` interpolation — pure, recently landed, and
  high bug-density. Test it first.
- Backfill `game` edge cases as APIs change.
- **`src/serial` is intentionally left untested for now.** It has not been a bug
  source and is stable. This is a **deliberate owner decision, not an oversight.**
  Revisit only if it starts changing or causing bugs.

## 11. CI

A green `ctest` must **gate merges.** We once shipped a silently-failing `game`
test on `main` after a merge changed entity-id allocation without updating the
test — CI running `ctest` would have caught it.

---

**tl;dr:** tests read as documentation (behavior-stating names, clear
arrange/act/assert); test code stays below `src` (soft target <= 40% of the
testable core); prefer pure unit tests; delete don't comment; generate
module-by-module.
