// Editor module: registers a "Generate from Viewport" toolbar button under Tools.
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FUnrealBananaEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void RegisterMenus();
    void OnGenerateFromViewportClicked();

    FDelegateHandle ToolMenusHandle;
};
