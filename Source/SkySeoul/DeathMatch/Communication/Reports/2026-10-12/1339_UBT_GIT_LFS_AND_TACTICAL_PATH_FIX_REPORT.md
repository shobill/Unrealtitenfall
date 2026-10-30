# UBT Git LFS timeout and tactical path fix

Date: 2026-10-12 (Asia/Seoul)

## Outcome

- Restored `SkySeoulEditor Win64 Development` builds after UnrealBuildTool timed out while waiting for `git status`.
- Corrected the tactical-grid Dijkstra priority ordering used by normal path analysis, candidate search, and spawn-distance fields.
- Verified the focused tactical-grid suite: Core, Runtime, and StrategicRoam all pass.

## Build failure diagnosis

The NuGet restore warnings shown by Visual Studio were not the terminating error. UBT successfully launched with its bundled .NET SDK and then waited for this background command:

`git --no-optional-locks status --porcelain`

UBT allows 15 seconds for that process. A performance trace showed that Git read the 8,453-entry index and checked file metadata quickly, then blocked in `git-lfs filter-process`. The working tree had 6,411 tracked files whose timestamps no longer matched the cached index metadata, causing Git LFS to re-evaluate a large number of Unreal assets.

## Local build configuration

Updated `Saved/UnrealBuildTool/BuildConfiguration.xml` with:

- `SourceFileWorkingSet.Provider = None`, so UBT no longer invokes Git to construct its adaptive-unity working set.
- `BuildConfiguration.bUseUnityBuild = false`, so legacy Photon/Windows macros cannot leak from `MyGameInstance.cpp` into later project translation units and corrupt Chaos `GetObject<T>()` declarations.

This file is under `Saved` and is intentionally machine-local. No Git index, Unreal asset, engine source, Photon source, or non-DeathMatch project source was modified.

## Tactical path correction

`TArray::HeapPush/HeapPop` predicates return true when the left element should precede the right element. The three predicates named `MinHeap` used `A.Distance > B.Distance`, which produced a maximum-distance heap. The direct 2,000-unit edge could therefore be finalized before the 600 + 600 route.

Changed all three predicates to `A.Distance < B.Distance` in `DMTacticalGridSubsystem.cpp`:

- Spawn-team distance-field Dijkstra.
- `AnalyzeTacticalPath` Dijkstra.
- Tactical candidate graph search.

This restores weighted shortest-path selection and ensures intermediate contested cells contribute to route risk.

## Verification

- Full editor build after the local configuration change: succeeded, 58 actions, 25.25 seconds.
- Incremental editor build after the Dijkstra correction: succeeded, 4 actions, 12.82 seconds.
- `Automation RunTests SkySeoul.DeathMatch.TacticalGrid`: exit code 0.
  - `SkySeoul.DeathMatch.TacticalGrid.Core`: success.
  - `SkySeoul.DeathMatch.TacticalGrid.Runtime`: success.
  - `SkySeoul.DeathMatch.TacticalGrid.StrategicRoam`: success.

## Broader Combat suite observation

The broader `SkySeoul.DeathMatch.Combat` run completed with 24 successes and 11 failures. Those failures include pre-existing/test-environment-sensitive PIE failures where no RecastNavMesh was available, plus expectation mismatches in Adaptive, Architecture, AttackPosition, Cover, Debug, Parts, and Cadence tests. They are separate from the build timeout and the focused tactical-grid path defect; no unrelated production or test code was changed in this pass.
