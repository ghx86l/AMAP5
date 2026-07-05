#pragma once

#include "CoreMinimal.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "PPR_Type.h"
#include "PPR_AnimNode_RigidbodyDebug.generated.h"

USTRUCT(BlueprintInternalUseOnly)
struct PMXPHYSICS_RUNTIME_API FPPR_AnimNode_RigidbodyDebug : public FAnimNode_SkeletalControlBase
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Rigidbody")
    TArray<FMmdRigidBodyData> RigidBodies;

    UPROPERTY()
    int32 SelectedRigidBodyIndex = -1;

    FPPR_AnimNode_RigidbodyDebug();

    virtual void EvaluateSkeletalControl_AnyThread(
        FComponentSpacePoseContext& Output,
        TArray<FBoneTransform>& OutBoneTransforms) override;

    virtual bool IsValidToEvaluate(
        const USkeleton* Skeleton,
        const FBoneContainer& RequiredBones) override;

    virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
};
