#pragma once

#include "Modules/ModuleManager.h"

class FAMAP5_EditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override {}
    virtual void ShutdownModule() override {}
};
