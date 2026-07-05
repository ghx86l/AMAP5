#pragma once

#include "CoreMinimal.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "PPR_Type.h"
#include "PPR_AnimNode.generated.h"

struct FBulletWorld;

USTRUCT(BlueprintInternalUseOnly)
struct PMXPHYSICS_RUNTIME_API FPPR_AnimNode : public FAnimNode_SkeletalControlBase
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Rigidbody")
    TArray<FMmdRigidBodyData> RigidBodies;

    UPROPERTY(EditAnywhere, Category = "Joint")
    TArray<FMmdJointData> Joints;

    UPROPERTY(EditAnywhere, Category = "Physics World", meta = (DisplayName = "Substep", ClampMin = "2", ClampMax = "16"))
    int32 MaxSubSteps = 3;

    UPROPERTY(EditAnywhere, Category = "Physics World", meta = (DisplayName = "Time Step", ClampMin = "60", ClampMax = "600"))
    int32 TimeStepFPS = 90;

    UPROPERTY(EditAnywhere, Category = "Physics World", meta = (DisplayName = "ERP", ClampMin = "0.01", ClampMax = "0.5"))
    float ERP = 0.5f;

    UPROPERTY(EditAnywhere, Category = "Physics World", meta = (DisplayName = "Gravity", ClampMin = "-1.0", ClampMax = "1.0"))
    FVector GravityDirection = FVector(0.f, 0.f, -1.f);

    UPROPERTY()
    bool bDebugDrawRigidBodies = true;

    UPROPERTY()
    int32 SelectedRigidBodyIndex = -1;

    FPPR_AnimNode();
    virtual ~FPPR_AnimNode();

    virtual void GatherDebugData(FNodeDebugData& DebugData) override;

    virtual void EvaluateSkeletalControl_AnyThread(
        FComponentSpacePoseContext& Output,
        TArray<FBoneTransform>& OutBoneTransforms) override;

    virtual bool IsValidToEvaluate(
        const USkeleton* Skeleton,
        const FBoneContainer& RequiredBones) override;

    virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;

    virtual void ResetDynamics(ETeleportType InTeleportType) override;

    virtual void OnInitializeAnimInstance(const FAnimInstanceProxy* InProxy, const UAnimInstance* InAnimInstance) override;

    static FTransform MmdToUETransform(const FVector& MmdPosition, const FQuat& MmdRotation);

    void RebuildCachedTransforms();

private:
    struct FSequencePlayerBinding
    {
        TWeakObjectPtr<class UMovieSceneSequencePlayer> Player;
        FDelegateHandle Handle;
    };

    FBulletWorld* BulletWorld = nullptr;
    bool bNeedBulletReset = true;
    TArray<FSequencePlayerBinding> SequenceBindings;
};


