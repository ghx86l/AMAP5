#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FAMAP5_ImporterModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
