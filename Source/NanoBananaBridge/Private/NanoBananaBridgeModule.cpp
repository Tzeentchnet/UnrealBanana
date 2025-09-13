#include "Modules/ModuleManager.h"

class FNanoBananaBridgeModule : public IModuleInterface
{
public:
    virtual void StartupModule() override {}
    virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FNanoBananaBridgeModule, NanoBananaBridge)

