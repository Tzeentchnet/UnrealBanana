// Module boilerplate
#include "Modules/ModuleManager.h"

class FViewportCaptureModule : public IModuleInterface
{
public:
    virtual void StartupModule() override {}
    virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FViewportCaptureModule, ViewportCapture)

