#include "Modules/ModuleManager.h"

class FUIProgressModule : public IModuleInterface
{
public:
    virtual void StartupModule() override {}
    virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FUIProgressModule, UIProgress)

