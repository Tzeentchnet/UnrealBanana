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
     - For Google Gemini Images API: `https://generativelanguage.googleapis.com/v1beta/models/imagegeneration:generate`
     - Or set your custom endpoint URL if not using Google
   - `ApiKey`: Your Gemini API key (or your custom provider key)
   - `Model`: Optional (kept for reference)
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

## Notes
- Screenshot capture uses `UGameViewportClient::OnScreenshotCaptured` + `FScreenshotRequest::RequestScreenshot`.
- With the Gemini Images API base URL (above), the request uses the `contents/parts` schema with prompt text plus the captured image as `inline_data`. The API key is sent as `?key=...` on the URL.
- Response parser supports two modes:
  - Google: finds `candidates[0].content.parts[].inline_data.data` and decodes base64 to PNG
  - Custom: accepts raw image bytes (`image/*`) or JSON `{ image: base64 }`
- Output directory defaults to `Saved/NanoBanana/` (configurable in settings).

## License
Proprietary / Internal use only (adjust as needed).
