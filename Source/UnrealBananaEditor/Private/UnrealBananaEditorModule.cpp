#include "UnrealBananaEditorModule.h"
#include "SGenerateFromViewportWindow.h"
#include "ToolMenus.h"
#include "LevelEditor.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "UnrealBananaEditor"

void FUnrealBananaEditorModule::StartupModule()
{
    UToolMenus::RegisterStartupCallback(
        FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FUnrealBananaEditorModule::RegisterMenus));
}

void FUnrealBananaEditorModule::ShutdownModule()
{
    UToolMenus::UnRegisterStartupCallback(this);
    UToolMenus::UnregisterOwner(this);
}

void FUnrealBananaEditorModule::RegisterMenus()
{
    FToolMenuOwnerScoped OwnerScoped(this);

    if (UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools"))
    {
        FToolMenuSection& Section = Menu->FindOrAddSection("UnrealBanana");
        Section.AddMenuEntry(
            "GenerateFromViewport",
            LOCTEXT("GenerateFromViewport_Label", "Generate from Viewport (Nano Banana)"),
            LOCTEXT("GenerateFromViewport_Tip", "Open the Nano Banana generation window."),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateRaw(this, &FUnrealBananaEditorModule::OnGenerateFromViewportClicked)));
    }
}

void FUnrealBananaEditorModule::OnGenerateFromViewportClicked()
{
    TSharedRef<SWindow> Window = SNew(SWindow)
        .Title(LOCTEXT("WindowTitle", "Nano Banana — Generate"))
        .ClientSize(FVector2D(520, 280))
        .SupportsMinimize(false).SupportsMaximize(false);

    Window->SetContent(SNew(SGenerateFromViewportWindow).ParentWindow(Window));

    if (TSharedPtr<SWindow> Root = FSlateApplication::Get().GetActiveTopLevelWindow())
    {
        FSlateApplication::Get().AddWindowAsNativeChild(Window, Root.ToSharedRef());
    }
    else
    {
        FSlateApplication::Get().AddWindow(Window);
    }
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUnrealBananaEditorModule, UnrealBananaEditor)
