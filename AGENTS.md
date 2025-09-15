# Repository Guidelines

This repository contains an Unreal Engine plugin composed of multiple C++ modules. Use this guide to navigate the codebase, build locally, and contribute effectively.

## Project Structure & Module Organization
- Root: `UnrealBanana.uplugin` (plugin metadata), `README.md`.
- Source: `Source/<Module>/{Public,Private,*.Build.cs}`
  - Modules: `ImageComposer`, `NanoBananaBridge`, `UIProgress`, `ViewportCapture`.
- Public headers are exported from `Public/`; implementation and tests live in `Private/`.

## Build, Test, and Development Commands
- Build distributable plugin (Windows, from an Engine checkout):
  - `Engine/Build/BatchFiles/RunUAT.bat BuildPlugin -Plugin="%CD%\UnrealBanana.uplugin" -Package="%CD%\Dist\UnrealBanana" -Rocket`
- Use in a game project: copy this repo to `<Project>/Plugins/UnrealBanana`, open the project; UnrealBuildTool compiles modules on launch.
- Regenerate project files (MSVC): `GenerateProjectFiles.bat` in your Engine root.

## Coding Style & Naming Conventions
- Language: C++ (UE style). Indent with 4 spaces; no tabs.
- Class prefixes: `U` UObject, `A` Actor, `S` Slate, `F` struct/class, `I` interface.
- Functions/Types: PascalCase; variables: camelCase; booleans: `bIsEnabled`.
- File names match primary type (e.g., `ViewportCaptureLibrary.h/.cpp`).
- Public API goes in `Public/`; minimize leaks from `Private/`.
- Prefer `TArray`, `TSharedPtr`, `UE_LOG` with a module log category.
- If available, run clang-format before committing: `clang-format -i <files>`.

## Testing Guidelines
- Framework: Unreal Automation Tests (spec- or simple-style).
- Location: `Source/<Module>/Private/Tests/` (e.g., `ImageComposerTests.cpp`).
- Naming: `F<Module>_<Feature>_Spec` or `F<Module>_<Feature>_Test`.
- Run in editor: `UnrealEditor.exe <Project>.uproject -ExecCmds="Automation RunTests All; Quit"`.
- Aim for coverage of public `Library` functions and module startup/shutdown paths.

## Commit & Pull Request Guidelines
- Commits: concise, imperative subject; scope prefix when helpful (e.g., `ViewportCapture: fix capture size clamp`).
- PRs: include description, rationale, test plan, and screenshots/video for UI or editor changes.
- Link related issues; keep PRs focused to one module when possible.
- Bump version in `UnrealBanana.uplugin` when adding features.

## Security & Configuration Tips
- Do not commit secrets. Store runtime settings in project config or `UNanoBananaSettings` (Project Settings > Plugins if applicable).
- Validate external I/O and guard async work in `NanoBananaBridge`.

## Agent-Specific Notes
- Scope: entire repo. Preserve module boundaries and file layout.
- Avoid broad refactors; keep patches surgical and consistent with existing patterns.
