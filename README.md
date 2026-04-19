# Unreal Banana

An Unreal Engine 5.7 plugin that turns your viewport (or any prompt) into
AI-generated images using Google's **Nano Banana** family of models. Pick
your vendor — **Google Gemini**, **FAL.ai**, or **Replicate** — drop in an
API key, and generate from Blueprint, UMG, or the editor toolbar.

> Looking for a list of what changed recently? See [Changelog.md](Changelog.md).

---

## What you can do with it

- **Generate from the editor.** `Tools → Generate from Viewport (Nano Banana)`
  opens a window with a prompt box and vendor/model dropdowns.
- **Generate from Blueprint.** Async node `Capture Viewport And Generate` (or
  `Generate Image` for text-only) returns one or more images plus a saved
  side-by-side composite.
- **Use it from a UMG widget.** Inherit from `NanoBananaWidgetBase` and bind
  the optional `PromptTextBox`, `ProgressBar`, `ResultImage`, `VendorCombo`,
  `ModelCombo`, and `CancelButton` widgets — everything is auto-wired.

Supported models:

| Tier | Google | FAL.ai | Replicate |
|---|---|---|---|
| **Nano Banana**   | `gemini-2.5-flash-image`         | `fal-ai/nano-banana`     | `google/nano-banana` |
| **Nano Banana 2** | `gemini-3.1-flash-image-preview` | `fal-ai/nano-banana-2`   | `google/nano-banana` * |
| **Nano Banana Pro** | `gemini-3-pro-image-preview`   | `fal-ai/nano-banana-pro` | `google/nano-banana-pro` |

`*` Replicate hadn't published a dedicated `-2` slug at time of writing; pick
**Custom** with your own model id (and optional version hash) if it appears
later.

---

## Install

1. Copy this folder into your project as `Plugins/UnrealBanana/`.
2. Open the project — UnrealBuildTool compiles the modules on launch.
3. Confirm the plugin is enabled in **Edit → Plugins → Project → Other**.

---

## Get your API keys

You only need the key for the vendor(s) you plan to use. They all have free
tiers.

### Google Gemini (recommended for first-time users)

1. Go to **<https://aistudio.google.com/apikey>**.
2. Sign in with a Google account, click **Create API key**, and copy the
   value (starts with `AIza...`).
3. The Gemini free tier covers casual use; image generation is metered — see
   <https://ai.google.dev/pricing>.

### FAL.ai

1. Sign up at **<https://fal.ai/dashboard>**.
2. Open **Dashboard → Keys** and click **Add key**. Copy the value (starts
   with `fal-...` or contains a `:` separator depending on the key style).
3. Pricing is per-image — see <https://fal.ai/pricing>.

### Replicate

1. Sign up at **<https://replicate.com>**.
2. Open **<https://replicate.com/account/api-tokens>** and create a token
   (starts with `r8_...`).
3. Replicate bills per second of GPU time. Pricing for the model is on its
   page, e.g. <https://replicate.com/google/nano-banana>.

---

## Configure the plugin

Open **Edit → Project Settings → Plugins → Nano Banana / Gemini Images** and
fill in the keys for the vendors you want to use:

- **Defaults**
  - `Default Vendor` — which vendor a Blueprint call uses if it doesn't
    specify one. (Default: `Fal`.)
  - `Default Model` — `NanoBanana` / `NanoBanana 2` / `NanoBanana Pro` /
    `Custom`. (Default: `NanoBanana 2`.)
  - `Default Aspect`, `Default Resolution`, `Default Output Format`,
    `Default Num Images`, `Default Negative Prompt`.
- **Vendors → Google** — paste your Gemini key into `Api Key`.
- **Vendors → Fal** — paste your FAL key into `Api Key`. Tick
  `Always Use Queue` if you only want the async/queue endpoint.
- **Vendors → Replicate** — paste your Replicate token into `Api Key`. Leave
  `Prefer Sync Wait` on for the lowest-latency path. If Replicate complains
  about an unknown model id, paste a specific version hash into
  `Nano Banana Version Hash` / `Nano Banana Pro Version Hash`.
- **Output**
  - `Output Directory` — where saved images go. Default:
    `Saved/NanoBanana` (project-relative).
  - `Save Debug Request Response` — when on, every request and response JSON
    is dumped to `Saved/NanoBanana/Debug/`. Handy when debugging.
- **Behavior**
  - `Request Timeout Seconds` — soft timeout before sync→queue fallback.
  - `Max Poll Seconds` — total time spent polling a queued job before giving
    up.

### Don't want to commit your keys?

Leave the `Api Key` field empty in Project Settings and set the matching
environment variable. The plugin reads them automatically:

| Vendor | Environment variable(s) |
|---|---|
| Google | `GEMINI_API_KEY`, then `GOOGLE_API_KEY` |
| FAL.ai | `FAL_KEY` |
| Replicate | `REPLICATE_API_TOKEN` |

On Windows (PowerShell, persistent for the user):

```powershell
[Environment]::SetEnvironmentVariable("GEMINI_API_KEY", "AIza...", "User")
```

---

## Try it

### From the editor

`Tools → Generate from Viewport (Nano Banana)` → type a prompt → pick a
vendor and model → **Generate**. If a PIE viewport is active, the current
view is sent as a reference image; otherwise it's a text-only generation.
Outputs land in `Saved/NanoBanana/`.

### From Blueprint

- `Capture Viewport And Generate` — captures the viewport, sends it with
  your prompt, returns `OnCompleted(Results, CompositePath)`.
- `Generate Image` — accepts a full `Nano Banana Request` struct (prompt,
  vendor, model, aspect, resolution, reference images...).
- Both expose `OnProgress(Percent, Stage)`, `OnFailed(Error)`, and a
  `Cancel()` function.

### From UMG

Create a widget that inherits from `NanoBananaWidgetBase`. Add any of these
named widgets and they'll be auto-wired:

- `PromptTextBox` — `EditableTextBox`
- `VendorCombo`, `ModelCombo` — `ComboBoxString` (auto-populated)
- `ProgressBar` — `ProgressBar`
- `ResultImage` — `Image`
- `CancelButton` — `Button`

Call `StartFromViewport` or `StartWithPrompt` from your widget BP.

---

## Modules

- **NanoBananaBridge** — vendor abstraction, settings, async action,
  HTTP/JSON utilities.
- **ViewportCapture** — async viewport-to-PNG capture, render-target
  utilities.
- **ImageComposer** — CPU-side side-by-side PNG composition.
- **UIProgress** — `NanoBananaWidgetBase` UMG glue.
- **UnrealBananaEditor** — Tools menu entry + Slate generation window
  (Editor only).

---

## Build the plugin standalone

From an Engine checkout:

```bat
Engine\Build\BatchFiles\RunUAT.bat BuildPlugin ^
  -Plugin="%CD%\UnrealBanana.uplugin" ^
  -Package="%CD%\Dist\UnrealBanana" -Rocket
```

---

## Troubleshooting

- **"Missing API key for vendor X"** — fill the field in Project Settings
  *or* set the env var listed above and restart the editor.
- **FAL job times out** — increase `Max Poll Seconds`, or tick
  `Always Use Queue` so sync isn't even attempted.
- **Replicate returns 422 "Invalid version"** — paste a known-good version
  hash from the model page into the matching `Version Hash` field.
- **Editor button does nothing in a fresh project** — run a PIE session at
  least once so a `GameViewport` exists; otherwise the window falls back to
  text-only generation (no reference image).
- **Want to inspect what was sent** — turn on `Save Debug Request Response`
  and look in `Saved/NanoBanana/Debug/`.

---

## License

MIT License — Copyright (c) 2026 Kenneth Peters (Tzeentchnet). See [LICENSE](LICENSE).
# Unreal Banana (UE5 Plugin)

Capture the viewport, send it to Nano Banana with a user prompt, display progress and results, and save two images: the raw result and a side-by-side composite of input+result.

## Modules
- ViewportCapture: Async viewport capture to PNG bytes or file; render target utilities.
- NanoBananaBridge: HTTP bridge, async Blueprint node to capture+send+save.
- ImageComposer: CPU-side composition of two PNGs side-by-side.
- UIProgress: Minimal `UUserWidget` base that wires progress/results to UMG.

## Quick Start
1. Copy `UnrealBanana` into your project `Plugins/` folder.
2. Enable the plugin in Project Settings → Plugins.
3. Configure Project Settings → Nano Banana / Gemini Images:
   - `ApiBaseUrl`:
     - `https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent`
   - `ApiKey`: Your Gemini API key
   - `bUseGenerateContent`: true (required)
   - `DefaultWidth`/`DefaultHeight`, `DefaultNumImages`, and `DefaultNegativePrompt` as needed
4. Create a UMG Widget inheriting from `NanoBananaWidgetBase` and add:
   - `EditableTextBox` (named `PromptTextBox`, optional)
   - `ProgressBar` (named `ProgressBar`, optional)
   - `Image` (named `ResultImage`, optional)
5. From Blueprint/UI, call `StartFromViewport` or `StartWithPrompt`.

## Blueprint API
- `NanoBananaBridgeAsyncAction::CaptureAndGenerate(WorldContext, Prompt, bShowUI)`
  - Delegates: `OnProgress(Percent, Stage)`, `OnCompleted(Texture, ResultPath, CompositePath)`, `OnFailed(Error)`.
- `ViewportCaptureLibrary::CaptureCurrentViewportToPNG(WorldContext, bShowUI, OptionalOutputPath, OnCaptured)`
- `ImageComposerLibrary::ComposeSideBySidePNGs(LeftPNG, RightPNG, OutCompositePNG, Padding)`
- `NanoBananaBridgeAsyncAction::CaptureAndGenerateAdvanced(WorldContext, Prompt, OptionalMaskRT, NumImages, Width, Height, NegativePrompt, bShowUI)`
  - Adds optional mask (render target), output size, candidate count, and negative prompt.

## Notes
- Screenshot capture uses `UGameViewportClient::OnScreenshotCaptured` + `FScreenshotRequest::RequestScreenshot`.
 - This plugin targets Gemini 2.5 Flash only via `:generateContent` on the `gemini-2.5-flash` model. The request includes `tools: [{ image_generation: {} }]` and `tool_config.image_generation` options.
 - Response parsing scans for any `inline_data.data` image parts and decodes PNGs. Multiple images are saved with index suffixes; the first is returned to the UI.
 - Output directory defaults to `Saved/NanoBanana/` (configurable in settings).

### Advanced (size, quantity, mask)
- Use `CaptureAndGenerateAdvanced` to set `NumImages`, `Width`, `Height`, and `NegativePrompt` per-call.
- Provide a `UTextureRenderTarget2D` as `OptionalMaskRT` for inpainting/edits; it’s sent as a PNG `mask` under the image_generation tool config.
- Settings defaults are in Project Settings → Nano Banana / Gemini Images.
- For cutting-edge Gemini schema changes, paste a JSON fragment in `AdvancedRequestJSON` (settings). It shallow-merges into the root request (e.g., to override `toolConfig` fields). This is useful to match examples like in the Zenn article.

Notes:
- If `NumImages > 1`, all images are saved to disk with index suffixes, and the first is returned to the UI.
- The request uses snake_case `tool_config.image_generation` fields (per current docs): `number_of_images`, `image_dimensions { width, height }`, optional `negative_prompt`, optional `mask.inline_data`.

## License
Proprietary / Internal use only (adjust as needed).
