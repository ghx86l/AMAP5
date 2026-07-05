#pragma once

#include "Modules/ModuleManager.h"
#include "PPR_EditorBridge.h"

class FMMDPhysicsEditorModule : public IPPR_EditorBridge
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
    virtual bool WriteRigidBodiesToAnimGraph(UAnimBlueprint* AnimBlueprint, const TArray<FMmdRigidBodyData>& RigidBodies) override;
    virtual bool WriteJointsToAnimGraph(UAnimBlueprint* AnimBlueprint, const TArray<FMmdJointData>& Joints) override;
    virtual bool WritePhysicsToAnimGraph(UAnimBlueprint* AnimBlueprint, const TArray<FMmdRigidBodyData>& RigidBodies, const TArray<FMmdJointData>& Joints) override;
};
