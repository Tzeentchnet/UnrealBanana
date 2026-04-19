#include "SGenerateFromViewportWindow.h"
#include "NanoBananaBridgeAsyncAction.h"
#include "NanoBananaSettings.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "SGenerateFromViewportWindow"

namespace
{
    static TArray<TSharedPtr<FString>> VendorOptions = {
        MakeShared<FString>(TEXT("Google")),
        MakeShared<FString>(TEXT("Fal")),
        MakeShared<FString>(TEXT("Replicate")),
    };
    static TArray<TSharedPtr<FString>> ModelOptions = {
        MakeShared<FString>(TEXT("NanoBanana")),
        MakeShared<FString>(TEXT("NanoBanana2")),
        MakeShared<FString>(TEXT("NanoBananaPro")),
        MakeShared<FString>(TEXT("Custom")),
    };

    static ENanoBananaVendor StringToVendor(const FString& S)
    {
        if (S == TEXT("Google"))    return ENanoBananaVendor::Google;
        if (S == TEXT("Replicate")) return ENanoBananaVendor::Replicate;
        return ENanoBananaVendor::Fal;
    }
    static ENanoBananaModel StringToModel(const FString& S)
    {
        if (S == TEXT("NanoBanana"))    return ENanoBananaModel::NanoBanana;
        if (S == TEXT("NanoBananaPro")) return ENanoBananaModel::NanoBananaPro;
        if (S == TEXT("Custom"))        return ENanoBananaModel::Custom;
        return ENanoBananaModel::NanoBanana2;
    }
}

void SGenerateFromViewportWindow::Construct(const FArguments& InArgs)
{
    ParentWindow = InArgs._ParentWindow;

    const UNanoBananaSettings& S = UNanoBananaSettings::Get();
    SelectedVendor = S.DefaultVendor;
    SelectedModel  = S.DefaultModel;

    const FString DefaultVendorStr = FNanoBananaTypeUtils::VendorToString(SelectedVendor);
    const FString DefaultModelStr  = FNanoBananaTypeUtils::ModelToDisplayString(SelectedModel);

    TSharedPtr<FString> InitialVendor = VendorOptions[1];
    for (const TSharedPtr<FString>& O : VendorOptions) { if (O.IsValid() && *O == DefaultVendorStr) { InitialVendor = O; break; } }
    TSharedPtr<FString> InitialModel = ModelOptions[1];
    for (const TSharedPtr<FString>& O : ModelOptions) { if (O.IsValid() && *O == DefaultModelStr) { InitialModel = O; break; } }

    ChildSlot
    [
        SNew(SBorder).Padding(12)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
            [
                SNew(STextBlock).Text(LOCTEXT("PromptLabel", "Prompt"))
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
            [
                SAssignNew(PromptBox, SEditableTextBox)
                .HintText(LOCTEXT("PromptHint", "Describe the image..."))
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
                [
                    SNew(STextBlock).Text(LOCTEXT("VendorLabel", "Vendor:"))
                ]
                + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 16, 0)
                [
                    SNew(SComboBox<TSharedPtr<FString>>)
                    .OptionsSource(&VendorOptions)
                    .InitiallySelectedItem(InitialVendor)
                    .OnGenerateWidget_Lambda([](TSharedPtr<FString> Opt) { return SNew(STextBlock).Text(FText::FromString(*Opt)); })
                    .OnSelectionChanged_Lambda([this](TSharedPtr<FString> Opt, ESelectInfo::Type) { if (Opt.IsValid()) SelectedVendor = StringToVendor(*Opt); })
                    [
                        SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(FNanoBananaTypeUtils::VendorToString(SelectedVendor)); })
                    ]
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
                [
                    SNew(STextBlock).Text(LOCTEXT("ModelLabel", "Model:"))
                ]
                + SHorizontalBox::Slot().FillWidth(1)
                [
                    SNew(SComboBox<TSharedPtr<FString>>)
                    .OptionsSource(&ModelOptions)
                    .InitiallySelectedItem(InitialModel)
                    .OnGenerateWidget_Lambda([](TSharedPtr<FString> Opt) { return SNew(STextBlock).Text(FText::FromString(*Opt)); })
                    .OnSelectionChanged_Lambda([this](TSharedPtr<FString> Opt, ESelectInfo::Type) { if (Opt.IsValid()) SelectedModel = StringToModel(*Opt); })
                    [
                        SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(FNanoBananaTypeUtils::ModelToDisplayString(SelectedModel)); })
                    ]
                ]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
            [
                SAssignNew(StatusText, STextBlock).Text(LOCTEXT("StatusReady", "Ready."))
            ]
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0, 16, 0, 0)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
                [
                    SNew(SButton).Text(LOCTEXT("Close", "Close"))
                    .OnClicked(this, &SGenerateFromViewportWindow::OnCloseClicked)
                ]
                + SHorizontalBox::Slot().AutoWidth()
                [
                    SNew(SButton).Text(LOCTEXT("Generate", "Generate"))
                    .OnClicked(this, &SGenerateFromViewportWindow::OnGenerateClicked)
                ]
            ]
        ]
    ];
}

void SGenerateFromViewportWindow::SetStatus(const FString& S)
{
    if (StatusText.IsValid()) StatusText->SetText(FText::FromString(S));
}

FReply SGenerateFromViewportWindow::OnGenerateClicked()
{
    const FString Prompt = PromptBox.IsValid() ? PromptBox->GetText().ToString() : FString();
    if (Prompt.IsEmpty())
    {
        SetStatus(TEXT("Prompt is empty."));
        return FReply::Handled();
    }

    UWorld* World = nullptr;
    if (GEditor)
    {
        World = GEditor->PlayWorld ? GEditor->PlayWorld : GEditor->GetEditorWorldContext().World();
    }
    if (!World)
    {
        SetStatus(TEXT("No active editor world."));
        return FReply::Handled();
    }

    // Note: viewport capture path requires a Game Viewport — works while PIE is running.
    // Without PIE, this falls back to a text-only request (no reference image).
    const bool bHasGameViewport = (GEngine && GEngine->GameViewport != nullptr);

    UNanoBananaBridgeAsyncAction* Action = nullptr;
    if (bHasGameViewport)
    {
        Action = UNanoBananaBridgeAsyncAction::CaptureViewportAndGenerate(
            World, Prompt, SelectedVendor, SelectedModel, /*bShowUI*/ false, /*bAlsoSaveComposite*/ true);
    }
    else
    {
        FNanoBananaRequest Req;
        Req.Prompt = Prompt;
        Req.Vendor = SelectedVendor;
        Req.Model = SelectedModel;
        const UNanoBananaSettings& S = UNanoBananaSettings::Get();
        Req.Aspect = S.DefaultAspect;
        Req.Resolution = S.DefaultResolution;
        Req.OutputFormat = S.DefaultOutputFormat;
        Req.NumImages = FMath::Max(1, S.DefaultNumImages);
        Req.NegativePrompt = S.DefaultNegativePrompt;
        Action = UNanoBananaBridgeAsyncAction::GenerateImage(World, Req, /*bAlsoSaveComposite*/ false);
    }

    if (!Action)
    {
        SetStatus(TEXT("Failed to create action."));
        return FReply::Handled();
    }

    ActiveAction = Action;
    TWeakPtr<SGenerateFromViewportWindow> WeakThis = SharedThis(this);

    Action->OnProgress.AddLambda([WeakThis](float Pct, const FString& Stage)
    {
        if (TSharedPtr<SGenerateFromViewportWindow> Pinned = WeakThis.Pin())
        {
            Pinned->SetStatus(FString::Printf(TEXT("[%d%%] %s"), (int32)(Pct * 100), *Stage));
        }
    });
    Action->OnCompleted.AddLambda([WeakThis](const TArray<FNanoBananaImageResult>& Results, const FString& CompositePath)
    {
        if (TSharedPtr<SGenerateFromViewportWindow> Pinned = WeakThis.Pin())
        {
            const FString Path = Results.Num() > 0 ? Results[0].SavedPath : FString();
            Pinned->SetStatus(FString::Printf(TEXT("Done. Saved to: %s"), *Path));
            FNotificationInfo Info(FText::FromString(FString::Printf(TEXT("Nano Banana generated %d image(s)."), Results.Num())));
            Info.ExpireDuration = 4.0f;
            FSlateNotificationManager::Get().AddNotification(Info);
        }
    });
    Action->OnFailed.AddLambda([WeakThis](const FString& Err)
    {
        if (TSharedPtr<SGenerateFromViewportWindow> Pinned = WeakThis.Pin())
        {
            Pinned->SetStatus(FString::Printf(TEXT("Failed: %s"), *Err));
        }
    });

    SetStatus(TEXT("Submitting..."));
    Action->Activate();
    return FReply::Handled();
}

FReply SGenerateFromViewportWindow::OnCloseClicked()
{
    if (TSharedPtr<SWindow> W = ParentWindow.Pin())
    {
        FSlateApplication::Get().RequestDestroyWindow(W.ToSharedRef());
    }
    return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
