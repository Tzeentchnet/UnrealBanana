// Slate window with a prompt textbox + vendor/model dropdowns + Generate button.
// Calls UNanoBananaBridgeAsyncAction::CaptureViewportAndGenerate against the active viewport.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "NanoBananaTypes.h"

class SWindow;
class SEditableTextBox;
class STextBlock;
class UNanoBananaBridgeAsyncAction;

class SGenerateFromViewportWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SGenerateFromViewportWindow) {}
        SLATE_ARGUMENT(TSharedPtr<SWindow>, ParentWindow)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    FReply OnGenerateClicked();
    FReply OnCloseClicked();

    void SetStatus(const FString& Stage);

    TSharedPtr<SEditableTextBox> PromptBox;
    TSharedPtr<STextBlock> StatusText;
    TWeakPtr<SWindow> ParentWindow;

    ENanoBananaVendor SelectedVendor = ENanoBananaVendor::Fal;
    ENanoBananaModel  SelectedModel  = ENanoBananaModel::NanoBanana2;

    TWeakObjectPtr<UNanoBananaBridgeAsyncAction> ActiveAction;
};
