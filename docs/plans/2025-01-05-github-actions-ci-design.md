# GitHub Actions CI/CD Design

## Overview

Automated builds for Windows VST3, macOS AU, and Linux LV2 on every push to main, with artifacts published to a rolling "latest" release.

## Decisions

- **Trigger:** Every push to `main`
- **Versioning:** Date-based (`YYYY.MM.DD-shortsha`), injected into CMake
- **Release strategy:** Rolling "latest" release, overwritten on each push
- **Code signing:** None (macOS users must bypass Gatekeeper)
- **Tests:** Run before builds, fail workflow if tests fail

## Workflow Structure

```
test (Ubuntu)
    ↓ (must pass)
┌───┴───┬───────┐
↓       ↓       ↓
macOS  Windows  Linux
    ↓       ↓       ↓
    └───┬───┴───────┘
        ↓
     release
```

Single workflow file: `.github/workflows/build.yml`

## Platform Builds

### macOS (macos-latest)

- Universal binary (arm64 + x86_64) via existing CMake config
- Output: `~/Library/Audio/Plug-Ins/Components/Room Multi EQ.component`
- Packaged as `RoomMultiEQ-macOS.zip`

### Windows (windows-latest)

- MSVC via Visual Studio (pre-installed)
- Output: `build/RoomMultiEQ_artefacts/Release/VST3/Room Multi EQ.vst3/`
- Packaged as `RoomMultiEQ-Windows.zip`

### Linux (ubuntu-latest)

Dependencies:
```
libasound2-dev libfreetype6-dev libx11-dev libxrandr-dev
libxcursor-dev libxinerama-dev libgl1-mesa-dev
```

- Output: `build/RoomMultiEQ_artefacts/LV2/Room Multi EQ.lv2/`
- Packaged as `RoomMultiEQ-Linux.zip`

## Version Injection

CMakeLists.txt modification to accept version override:

```cmake
# Allow version override from CI
if(NOT DEFINED PROJECT_VERSION)
    set(PROJECT_VERSION "1.0.0")
endif()
project(RoomMultiEQ VERSION ${PROJECT_VERSION})
```

CI passes `-DPROJECT_VERSION=2025.01.05-abc1234` to cmake configure.

## Release Mechanism

Uses `softprops/action-gh-release` action:

- Fixed tag: `latest`
- Display name: `Latest Build (YYYY.MM.DD-sha)`
- Marked as pre-release
- Each push deletes existing tag and recreates with new artifacts

Release notes include:
- Version and commit info
- Download links for each platform
- Warning about macOS Gatekeeper for unsigned builds

Workflow requires `contents: write` permission.

## Artifacts

| Platform | Format | Filename |
|----------|--------|----------|
| macOS | AU (.component) | RoomMultiEQ-macOS.zip |
| Windows | VST3 | RoomMultiEQ-Windows.zip |
| Linux | LV2 | RoomMultiEQ-Linux.zip |
