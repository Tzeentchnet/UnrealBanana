# Roadmap

Forward-looking plan for the **UnrealBanana** plugin. Non-binding — items may
shift, drop, or merge as the project evolves. For shipped work see
[Changelog.md](Changelog.md); for current architecture see [Design.md](Design.md);
for contributor workflow see [AGENTS.md](AGENTS.md).

Current shipped version: **v0.2.0** (multi-vendor: Google Gemini, FAL.ai,
Replicate). Engine target: **UE 5.7+**.

---

## Vision & guiding principles

1. **Blueprint-first.** Every meaningful capability should be reachable from
   Blueprint without writing C++. C++ remains the implementation surface;
   Blueprint is the authoring surface.
2. **Vendor-agnostic.** Provider differences live behind `IImageGenProvider`.
   Adding a new vendor should not change Blueprint nodes.
3. **Runtime + editor parity.** Anything available at runtime should also be
   available from the editor toolbar window, and vice versa.
4. **Deterministic & testable.** Pure request builders, mockable HTTP, no
   hidden globals. New features arrive with tests under `Source/*/Private/Tests/`.
5. **Honest scope.** Ship features end-to-end (request → response → BP node →
   test) before adding more breadth.

---

## Status snapshot

| Capability                                     | Status today                                        |
|-----------------------------------------------|-----------------------------------------------------|
| Text-to-image (Google / FAL / Replicate)      | Shipped                                             |
| Image-to-image via reference images           | Shipped                                             |
| Async action with progress / cancel           | Shipped                                             |
| Game-viewport capture → PNG                   | Shipped                                             |
| Side-by-side composition                      | Shipped                                             |
| UMG widget base (`UNanoBananaWidgetBase`)     | Shipped (basic bindings)                            |
| Editor toolbar window                         | Shipped (requires PIE for viewport capture)         |
| Per-vendor request-builder unit tests         | Shipped                                             |
| **Mask / inpainting**                         | **Field exists on `FNanoBananaRequest`, ignored by all three providers** |
| Batch / variation helpers                     | Not implemented                                     |
| Retry / backoff on transient errors           | Not implemented                                     |
| Generation history / cache                    | Not implemented                                     |
| Sequencer / Niagara / Material integration    | Not implemented                                     |
| Mock provider + HTTP integration tests        | Not implemented                                     |

Reference points in source:
- Async surface — [Source/NanoBananaBridge/Public/NanoBananaBridgeAsyncAction.h](Source/NanoBananaBridge/Public/NanoBananaBridgeAsyncAction.h)
- Request / result types — [Source/NanoBananaBridge/Public/NanoBananaTypes.h](Source/NanoBananaBridge/Public/NanoBananaTypes.h)
- Settings — [Source/NanoBananaBridge/Public/NanoBananaSettings.h](Source/NanoBananaBridge/Public/NanoBananaSettings.h)
- Widget base — [Source/UIProgress/Public/NanoBananaWidgetBase.h](Source/UIProgress/Public/NanoBananaWidgetBase.h)
- Viewport capture — [Source/ViewportCapture/Public/ViewportCaptureLibrary.h](Source/ViewportCapture/Public/ViewportCaptureLibrary.h)
- Editor window — [Source/UnrealBananaEditor/Private/SGenerateFromViewportWindow.cpp](Source/UnrealBananaEditor/Private/SGenerateFromViewportWindow.cpp)

Effort tags below: **S** ≈ < 1 day, **M** ≈ 1–3 days, **L** ≈ 1+ week.

---

## v0.3 — Blueprint Power-Up *(headline release)*

The single largest user-facing win: dramatically expand what Blueprint authors
can do without dropping to C++. Every item ships with at least one BP-callable
node and a unit test where pure C++ logic exists.

### Inpainting / mask, end-to-end *(M, Blueprint + Provider)*
- Wire `FNanoBananaRequest::OptionalMask` through the request builders for
  Google, FAL, and Replicate (where the provider's current API supports it).
- Add `MaskStrength` (0–1) and `bInvertMask` to `FNanoBananaRequest`.
- New Blueprint helpers: `BuildMaskFromTexture`, `BuildMaskFromBrushStrokes`,
  `CaptureViewportAndInpaint`.
- Tests: extend `Providers_RequestBuilder_Tests.cpp` with mask-bearing
  assertions per provider.

### Batch & variations *(M, Blueprint)*
- `BatchGenerateImages(TArray<FNanoBananaRequest> Requests, int32 MaxConcurrency)`
  async action — fans out, aggregates results, emits per-item progress.
- `GenerateVariations(FNanoBananaRequest Base, int32 Count, ESeedStrategy Strategy)`
  convenience wrapper.

### Image-to-image ergonomics *(M, Blueprint + Editor)*
- BP-callable wrappers that remove boilerplate around capture:
  - `GenerateFromTexture(UTexture2D*, Prompt, …)`
  - `GenerateFromRenderTarget(UTextureRenderTarget2D*, Prompt, …)`
  - `GenerateFromActorThumbnail(AActor*, Prompt, …)`
  - `GenerateFromCameraActor(ACameraActor*, Prompt, …)` — uses an offscreen
    `USceneCaptureComponent2D`, which also lets the editor toolbar window work
    **without an active PIE viewport** (today's main editor limitation, see
    [SGenerateFromViewportWindow.cpp](Source/UnrealBananaEditor/Private/SGenerateFromViewportWindow.cpp)).

### Result utilities *(S–M, Blueprint)*
- `SaveResultAsAsset(FNanoBananaImageResult, FString PackagePath)` — creates a
  persistent `UTexture2D` `.uasset`.
- `ApplyResultToMaterialInstance(UMaterialInstanceDynamic*, FName Param, Result)`.
- `ApplyResultToMeshComponent(UMeshComponent*, int32 SlotIndex, FName Param, Result)`.
- `BuildSpriteFromResult(Result)` for Paper2D / UMG image use.

### Prompt templating *(S, Blueprint)*
- New `FNanoBananaPromptTemplate` struct + `FormatPrompt(Template, TMap<FString,FString> Params)`
  BP node supporting `{key}` substitution. Useful for parameterized prompts
  driven by gameplay state or data tables.

### Async lifecycle parity *(S, Blueprint)*
- Expose on `UNanoBananaBridgeAsyncAction`: `IsRunning()`, `GetElapsedSeconds()`,
  `GetCurrentStage()`, plus a new `OnRetry(int32 Attempt)` delegate that fires
  when v0.4's retry logic kicks in.

### Settings as Blueprint *(S, Blueprint)*
- Blueprint-readable getters on `UNanoBananaSettings` for default vendor and
  model.
- `SetRuntimeOverride(ENanoBananaVendor, ENanoBananaModel, FString ApiKey)` for
  test scenes / shipping titles that pull credentials from a remote service.

### History subsystem *(M, Blueprint)*
- `UNanoBananaHistorySubsystem` (`UGameInstanceSubsystem`, BP-exposed):
  - `GetRecentResults(int32 N)`
  - `FindByPromptHash(FString Hash)`
  - `ClearHistory()`
- Persists thumbnails + metadata under `Saved/NanoBanana/History/`.

### Widget base improvements *(S, Blueprint)*
- Extend [NanoBananaWidgetBase.h](Source/UIProgress/Public/NanoBananaWidgetBase.h)
  with optional named bindings: `AspectCombo`, `ResolutionCombo`, `SeedSpinBox`,
  `NumImagesSlider`, `NegativePromptBox`, `HistoryListView`. All optional —
  existing widgets continue to work unchanged.

---

## v0.4 — Robustness & pipeline integration

### Structured errors *(S)*
- Replace bare `FString Error` with
  `FNanoBananaError { FString Code; int32 HttpStatus; FString ProviderMessage; bool bRetriable; }`.
- Keep a `FString` accessor for backwards compatibility on `OnFailed`.

### Retry / backoff *(M)*
- Configurable in [NanoBananaSettings.h](Source/NanoBananaBridge/Public/NanoBananaSettings.h):
  `MaxRetries`, `InitialBackoffMs`, `BackoffMultiplier`, `RetryOnStatusCodes`.
- Triggers on 408 / 425 / 429 / 5xx and on provider-specific transient codes.
- Surfaces via the v0.3 `OnRetry` delegate.

### Replicate webhook delivery *(M)*
- Skip polling when the user configures a webhook endpoint; broadcast results
  when the webhook fires. Falls back to polling if not configured.

### Per-vendor rate limiting *(S)*
- Token-bucket limiter keyed by vendor; configurable QPS in settings.

### Telemetry hooks (opt-in) *(S)*
- Local log under `Saved/NanoBanana/Telemetry/` capturing prompt hash, vendor,
  model, latency, byte counts. No network egress.

### Mock provider + HTTP integration tests *(M)*
- New `ENanoBananaVendor::Mock` provider (compiled only with
  `WITH_DEV_AUTOMATION_TESTS`) returning canned responses.
- Integration tests that exercise the full async action against the mock.

---

## v0.5 — Editor & authoring UX

### Decouple the editor window from PIE *(M)*
- Use `USceneCaptureComponent2D` against the active editor viewport's camera so
  the **Generate from Viewport** window works in a fresh project without
  pressing Play.

### Asset-action utility *(S)*
- Right-click a `UTexture2D` in the Content Browser → "Generate Variation with
  Nano Banana…". Pre-fills the reference image and opens the Slate window.

### Drag-and-drop reference images *(S)*
- Accept image drops onto the Slate window; auto-encode to base64.

### Result Gallery dock tab *(M)*
- Dockable Slate panel backed by the v0.3 history subsystem. Filter by vendor /
  model / date / prompt substring; one-click "save as asset".

### Prompt asset type *(M)*
- New `UNanoBananaPromptAsset` (Content Browser asset): saved prompt, params,
  reference images, optional default vendor/model. Drag onto a widget or actor
  to populate a request.

---

## v0.6 — Advanced workflows

### Sequencer track *(L)*
- `UMovieSceneNanoBananaTrack` with keyframed prompts / params, per-frame
  regeneration support, and a "bake to image sequence" action.

### Niagara render-target helpers *(M)*
- Helpers for routing results into Niagara `UNiagaraDataInterfaceTexture`s.

### Material function library *(S)*
- Sample material functions and a small content example pack consuming results.

### Cost estimator *(S)*
- Per-vendor pricing config + a BP node `EstimateCost(FNanoBananaRequest)` so
  tools can warn before expensive runs.

---

## Backlog / exploratory

Promising but unscoped — promote into a milestone when there's a clear use case:

- Streaming / progressive previews if and when providers support it.
- LoRA / custom-model upload helpers (Replicate).
- Depth- or pose-conditioned generation.
- Video-frame conditioning.
- Localization of widget strings.
- Per-project secret management beyond env vars (e.g. OS keychain).

---

## Non-goals

To keep scope honest, the plugin **will not** pursue:

- General-purpose LLM chat / function-calling clients.
- Audio or video generation as a first-class surface (image-only).
- On-device / local model inference (no bundled model weights).
- Replacing the engine's built-in render pipeline or texture streaming.

---

## Contribution notes

- Follow the workflow and code conventions in [AGENTS.md](AGENTS.md).
- New providers: implement `IImageGenProvider`, register in
  `FProviderFactory::Make`, add request-builder tests under
  `Source/NanoBananaBridge/Private/Tests/Providers/`.
- New Blueprint nodes: keep request builders pure (no HTTP) so they can be unit
  tested without the engine's HTTP module.
- Feature requests / discussion: open a GitHub Issue rather than editing this
  file directly.

---

*Last updated: 2026-04-18.*
