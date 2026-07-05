#include "PPR_AnimNode_RigidbodyDebug.h"
#include "PPR_AnimNode.h"
#include "Animation/AnimInstanceProxy.h"

FPPR_AnimNode_RigidbodyDebug::FPPR_AnimNode_RigidbodyDebug()
{
}

void FPPR_AnimNode_RigidbodyDebug::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
    for (FMmdRigidBodyData& RB : RigidBodies)
    {
        if (RB.BoneRef.BoneName.IsNone())
        {
            RB.BoneRef.BoneName = RB.BoneName;
        }
        RB.BoneRef.Initialize(RequiredBones);
        RB.ResolvedBoneIndex = RB.BoneRef.CachedCompactPoseIndex.GetInt();
        RB.ParentBoneIndex = (RB.BoneRef.CachedCompactPoseIndex != INDEX_NONE)
            ? RequiredBones.GetParentBoneIndex(RB.BoneRef.CachedCompactPoseIndex).GetInt()
            : INDEX_NONE;
        RB.CachedUETransform = FPPR_AnimNode::MmdToUETransform(RB.Position, RB.Rotation);
    }
}

bool FPPR_AnimNode_RigidbodyDebug::IsValidToEvaluate(
    const USkeleton* Skeleton,
    const FBoneContainer& RequiredBones)
{
    return true;
}

void FPPR_AnimNode_RigidbodyDebug::EvaluateSkeletalControl_AnyThread(
    FComponentSpacePoseContext& Output,
    TArray<FBoneTransform>& OutBoneTransforms)
{
    checkSlow(OutBoneTransforms.Num() == 0);
}
