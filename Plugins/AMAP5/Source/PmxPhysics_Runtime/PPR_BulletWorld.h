#pragma once
#include "PPR_Type.h"

struct FBulletWorld
{
    FBulletWorld();
    ~FBulletWorld();

    void Initialize(const TArray<FMmdRigidBodyData>& InRigidBodies,
                    const TArray<FMmdJointData>& InJoints,
                    float InERP,
                    const FVector& InGravityDirection);
    void Shutdown();
    void UpdateKinematics(const TArray<FMmdRigidBodyData>& InRigidBodies,
                          const TArray<FTransform>& InWorldTransforms,
                          float DeltaTime);
    void ResetBodies(const TArray<FTransform>& InWorldTransforms);
    void StepSimulation(float DeltaTime, int32 MaxSubSteps, float InFixedTimeStep);
    void GetSimulatedTransforms(TArray<FTransform>& OutTransforms) const;
    void GetEffectiveModes(TArray<EMmdRigidBodyMode>& OutModes) const;

    bool bInitialized = false;

private:
    struct FImpl;
    FImpl* Impl = nullptr;
};


