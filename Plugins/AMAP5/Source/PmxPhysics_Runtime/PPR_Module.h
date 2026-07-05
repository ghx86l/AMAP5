// Copyright 2025 Your Project. All Rights Reserved.
// 配置場所: BulletPhysicsPlugin/Source/PmxPhysics_Runtime/Public/PPR_PmxPhysicsModule.h

#pragma once

#include "Modules/ModuleManager.h"

class FMMDPhysicsRuntimeModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
