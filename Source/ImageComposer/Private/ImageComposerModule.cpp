#include "Modules/ModuleManager.h"

class FImageComposerModule : public IModuleInterface
{
public:
    virtual void StartupModule() override {}
    virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FImageComposerModule, ImageComposer)

