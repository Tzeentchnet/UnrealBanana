# Changelog

## v0.2.0 — Multi-vendor support (UE 5.7)

Major rewrite. Adds Google Gemini, FAL.ai, and Replicate as first-class vendors;
adds Nano Banana 2 / Nano Banana Pro model tiers; introduces an editor toolbar
window; reorganizes the bridge module into provider/HTTP folders.

### Plugin manifest / build
- `UnrealBanana.uplugin` — bumped to **v0.2.0**, `EngineVersion: 5.7.0`,
  registered new `UnrealBananaEditor` module.
- `Source/NanoBananaBridge/NanoBananaBridge.Build.cs` — added `DeveloperSettings`
  public dependency and `PrivateIncludePaths` so nested `Providers/`, `Http/`,
  and `Tests/` folders resolve cleanly.
- `Source/UIProgress/UIProgress.Build.cs` — added `DeveloperSettings` private
  dependency.

### New: types & settings
- `Source/NanoBananaBridge/Public/NanoBananaTypes.h` + `Private/NanoBananaTypes.cpp`
  — `ENanoBananaVendor`, `ENanoBananaModel`, aspect / resolution / output-format
  enums, `FNanoBananaRequest`, `FNanoBananaReferenceImage`,
  `FNanoBananaImageResult`, plus `FNanoBananaTypeUtils` helpers.
- `Source/NanoBananaBridge/Public/NanoBananaSettings.h` + `Private/NanoBananaSettings.cpp`
  — fully rewritten `UNanoBananaSettings`. Per-vendor structs
  (`FGoogleVendorConfig`, `FFalVendorConfig`, `FReplicateVendorConfig`),
  defaults, `GetEffectiveApiKey()` with env-var fallback
  (`GEMINI_API_KEY` / `GOOGLE_API_KEY`, `FAL_KEY`, `REPLICATE_API_TOKEN`).

### New: provider abstraction
- `Source/NanoBananaBridge/Private/Providers/IImageGenProvider.h` — interface
  `IImageGenProvider` plus `FProviderCallbacks` (lambda callbacks for progress,
  success, failure, request-built).
- `Source/NanoBananaBridge/Private/Providers/ProviderFactory.h` / `.cpp` —
  `FProviderFactory::Make(ENanoBananaVendor)` factory.
- `Source/NanoBananaBridge/Private/Providers/Google/GoogleGeminiProvider.h` / `.cpp`
  — Gemini `:generateContent` endpoint, `inlineData` reference images, static
  helpers `ResolveModelId` / `BuildEndpointUrl` / `BuildRequestJson`.
- `Source/NanoBananaBridge/Private/Providers/Fal/FalAiProvider.h` / `.cpp` — sync
  `fal.run/{slug}` first, falls back to `queue.fal.run/{slug}` on timeout / 5xx
  (skips fallback on 401 / 403 / 422), polls `/requests/{id}/status`, fetches
  result JSON, then parallel image-URL downloads.
- `Source/NanoBananaBridge/Private/Providers/Replicate/ReplicateProvider.h` / `.cpp`
  — POST `/v1/predictions` with `Prefer: wait`, polls `urls.get` if non-terminal.
  Optional version-hash override per model.

### New: shared HTTP utilities
- `Source/NanoBananaBridge/Private/Http/Base64Image.h` / `.cpp` — texture /
  render-target / file-path → PNG, data-URI builder, MIME sniffer (PNG / JPEG /
  WebP).
- `Source/NanoBananaBridge/Private/Http/JsonResponseScanner.h` / `.cpp` —
  recursive scanner for `inline_data` / `inlineData` / `b64_json` / `bytesBase64`
  fields in arbitrary JSON responses.
- `Source/NanoBananaBridge/Private/Http/PollLoop.h` / `.cpp` — `FPollLoop`
  (`TSharedFromThis`) with caller-supplied request factory and decision
  function; `FTSTicker`-driven exponential backoff.

### Rewritten: async action
- `Source/NanoBananaBridge/Public/NanoBananaBridgeAsyncAction.h` +
  `Private/NanoBananaBridgeAsyncAction.cpp` — new factories `GenerateImage(Request)`
  and `CaptureViewportAndGenerate(prompt, vendor, model, ...)`. Multi-image
  `OnCompleted(TArray<FNanoBananaImageResult>, CompositePath)`. `Cancel()` API.
  When `bSaveDebugRequestResponse` is on, dumps `Saved/NanoBanana/Debug/*_request.json`
  and `*_response.json`.

### Rewritten: UMG widget base
- `Source/UIProgress/Public/NanoBananaWidgetBase.h` +
  `Private/NanoBananaWidgetBase.cpp` — added `UComboBoxString* VendorCombo` and
  `ModelCombo`, `UButton* CancelButton`, `CancelGeneration()`, multi-image
  `OnCompleted` BlueprintImplementableEvent. Combos auto-populate in
  `NativeConstruct`.

### New: editor module
- `Source/UnrealBananaEditor/UnrealBananaEditor.Build.cs`
- `Source/UnrealBananaEditor/Public/UnrealBananaEditorModule.h` +
  `Private/UnrealBananaEditorModule.cpp` — registers a Tools menu entry
  (`LevelEditor.MainMenu.Tools` → "Generate from Viewport (Nano Banana)").
- `Source/UnrealBananaEditor/Private/SGenerateFromViewportWindow.h` / `.cpp` —
  Slate window with prompt text box, vendor / model `SComboBox`, Generate /
  Close. Uses `CaptureViewportAndGenerate` when a `GameViewport` exists,
  otherwise falls back to text-only `GenerateImage`. Reports completion via
  `FNotificationInfo`.

### Tests
- `Source/NanoBananaBridge/Private/Tests/Providers/Providers_RequestBuilder_Tests.cpp`
  — 9 automation tests covering the static request builders for all three
  providers (`UnrealBanana.Providers.*`).
- `Source/NanoBananaBridge/Private/Tests/Settings_EnvFallback_Test.cpp` —
  env-var fallback test (`UnrealBanana.Settings.EnvVarFallback`) using an
  `FScopedEnv` RAII helper around `FPlatformMisc::SetEnvironmentVar`.

### Notes
- `IHttpRequest::OnRequestProgress` is deprecated in 5.7; upload-progress
  callback removed in favor of stage-based progress.
- Provider lifetime during in-flight HTTP is preserved via
  `TSharedFromThis<IImageGenProvider, ESPMode::ThreadSafe>` + `TWeakPtr`
  lambda captures.
- Reference images use `FNanoBananaReferenceImage` (`Texture` / `RenderTarget`
  / `FilePath`) so they remain Blueprint-friendly; bytes are resolved
  internally.

## v0.1.0 — Initial release

- Single-vendor (Google Gemini 2.5 Flash via `:generateContent`).
- Modules: `ViewportCapture`, `NanoBananaBridge`, `ImageComposer`, `UIProgress`.
- Async Blueprint node `CaptureAndGenerate` + advanced variant with mask /
  size / negative prompt.
- Side-by-side composite output.
