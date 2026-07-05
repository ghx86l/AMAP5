#pragma once

#include "Modules/ModuleManager.h"
#include "PPR_Type.h"

class UAnimBlueprint;

class IPPR_EditorBridge : public IModuleInterface
{
public:
    virtual bool WriteRigidBodiesToAnimGraph(
        UAnimBlueprint* AnimBlueprint,
        const TArray<FMmdRigidBodyData>& RigidBodies) = 0;
    virtual bool WriteJointsToAnimGraph(
        UAnimBlueprint* AnimBlueprint,
        const TArray<FMmdJointData>& Joints) = 0;
    virtual bool WritePhysicsToAnimGraph(
        UAnimBlueprint* AnimBlueprint,
        const TArray<FMmdRigidBodyData>& RigidBodies,
        const TArray<FMmdJointData>& Joints) = 0;
};
