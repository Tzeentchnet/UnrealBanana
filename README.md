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
   - `ApiBaseUrl` (choose one):
     - Gemini 2.5 Flash image-to-image (generateContent + image_generation tool):
       `https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent`
     - Legacy Image Generation API (single-shot):
       `https://generativelanguage.googleapis.com/v1beta/models/imagegeneration:generate`
   - `ApiKey`: Your Gemini API key (or your custom provider key)
   - `bUseGenerateContent`: true to use 2.5 Flash image-to-image via `:generateContent` with the image_generation tool
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
- `NanoBananaBridgeAsyncAction::CaptureAndGenerateAdvanced(WorldContext, Prompt, OptionalMaskRT, NumImages, Width, Height, NegativePrompt, bShowUI)`
  - Adds optional mask (render target), output size, candidate count, and negative prompt.

## Notes
- Screenshot capture uses `UGameViewportClient::OnScreenshotCaptured` + `FScreenshotRequest::RequestScreenshot`.
- With Gemini 2.5 Flash (`:generateContent`), the request includes `tools: [{ image_generation: {} }]` and uses the `contents/parts` schema with prompt text plus the captured image as `inline_data`. The API key is sent as `?key=...`.
- With the Image Generation API (`models/imagegeneration:generate`), the request also uses `contents/parts` with prompt text and optional `inline_data` for img2img; no `tools` are required. The response returns images in the standard `candidates[].content.parts[].inline_data.data` shape (or a provider-specific shape).
- Response parser supports two modes:
  - Google: scans the response JSON for any `inline_data.data` image parts (not just the first candidate) and decodes to PNG. Also supports common fields like `b64_json` or `bytesBase64`.
  - Custom: accepts raw image bytes (`image/*`) or JSON `{ image: base64 }`
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
