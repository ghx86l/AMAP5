#include "PPR_AnimNode.h"
#include "PPR_BulletWorld.h"
#include "MovieSceneSequencePlayer.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimationRuntime.h"
#include "DrawDebugHelpers.h"

FTransform FPPR_AnimNode::MmdToUETransform(
    const FVector& MmdPosition,
    const FQuat& MmdRotation)
{
    return FTransform(MmdRotation, MmdPosition, FVector::OneVector);
}

FPPR_AnimNode::FPPR_AnimNode() {}

FPPR_AnimNode::~FPPR_AnimNode()
{
    for (FSequencePlayerBinding& Binding : SequenceBindings)
    {
        if (Binding.Player.IsValid())
            Binding.Player->OnSequenceUpdated().Remove(Binding.Handle);
    }
    SequenceBindings.Reset();

    delete BulletWorld;
    BulletWorld = nullptr;
}

void FPPR_AnimNode::ResetDynamics(ETeleportType InTeleportType)
{
    bNeedBulletReset = true;
}

void FPPR_AnimNode::OnInitializeAnimInstance(const FAnimInstanceProxy* InProxy, const UAnimInstance* InAnimInstance)
{
    FAnimNode_SkeletalControlBase::OnInitializeAnimInstance(InProxy, InAnimInstance);

    for (FSequencePlayerBinding& Binding : SequenceBindings)
    {
        if (Binding.Player.IsValid())
            Binding.Player->OnSequenceUpdated().Remove(Binding.Handle);
    }
    SequenceBindings.Reset();

    if (!InAnimInstance) return;
    UWorld* World = InAnimInstance->GetWorld();
    if (!World) return;

    for (TObjectIterator<UMovieSceneSequencePlayer> It; It; ++It)
    {
        UMovieSceneSequencePlayer* Player = *It;
        if (!IsValid(Player) || Player->GetWorld() != World) continue;

        FSequencePlayerBinding Binding;
        Binding.Player = Player;
        Binding.Handle = Player->OnSequenceUpdated().AddLambda(
            [this](const UMovieSceneSequencePlayer& InPlayer, FFrameTime Current, FFrameTime Previous)
            {
                if (!InPlayer.IsPlaying() && Current != Previous)
                {
                    bNeedBulletReset = true;
                }
            });
        SequenceBindings.Add(Binding);
    }
}

void FPPR_AnimNode::RebuildCachedTransforms()
{
    for (FMmdRigidBodyData& RB : RigidBodies)
    {
        RB.CachedUETransform = MmdToUETransform(RB.Position, RB.Rotation);
        RB.PMXWorldPosCS     = RB.CachedUETransform.GetLocation();
    }
}

void FPPR_AnimNode::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
    RebuildCachedTransforms();

    for (FMmdRigidBodyData& RB : RigidBodies)
    {
        if (RB.BoneRef.BoneName.IsNone())
            RB.BoneRef.BoneName = RB.BoneName;

        RB.BoneRef.Initialize(RequiredBones);
        RB.ResolvedBoneIndex = RB.BoneRef.CachedCompactPoseIndex.GetInt();
        RB.ParentBoneIndex = INDEX_NONE;

        if (RB.BoneRef.CachedCompactPoseIndex != INDEX_NONE)
        {
            RB.ParentBoneIndex = RequiredBones.GetParentBoneIndex(RB.BoneRef.CachedCompactPoseIndex).GetInt();
            const FTransform BoneRest = FAnimationRuntime::GetComponentSpaceRefPose(
                RB.BoneRef.CachedCompactPoseIndex, RequiredBones);
            RB.BoneCSRestPos     = BoneRest.GetLocation();
            RB.BoneToRBTransform = RB.CachedUETransform * BoneRest.Inverse();
        }
    }

    if (RigidBodies.Num() > 0)
    {
        if (!BulletWorld)
            BulletWorld = new FBulletWorld();
        BulletWorld->Initialize(RigidBodies, Joints, ERP, GravityDirection);
        bNeedBulletReset = true;
    }

    for (FMmdJointData& Joint : Joints)
    {
        const FTransform JointRest(Joint.Rotation, Joint.Position, FVector::OneVector);
        Joint.CachedUETransform = JointRest;
        if (!RigidBodies.IsValidIndex(Joint.RigidBodyIndexA)) continue;
        Joint.OffsetFromBodyA = JointRest * RigidBodies[Joint.RigidBodyIndexA].CachedUETransform.Inverse();
    }
}

bool FPPR_AnimNode::IsValidToEvaluate(const USkeleton* Skeleton,
                                      const FBoneContainer& RequiredBones)
{
    return true;
}

void FPPR_AnimNode::EvaluateSkeletalControl_AnyThread(
    FComponentSpacePoseContext& Output,
    TArray<FBoneTransform>& OutBoneTransforms)
{
    const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();

    for (FMmdRigidBodyData& RB : RigidBodies)
    {
        if (RB.BoneRef.CachedCompactPoseIndex == INDEX_NONE && !RB.BoneRef.BoneName.IsNone())
        {
            RB.BoneRef.Initialize(BoneContainer);
            RB.ResolvedBoneIndex = RB.BoneRef.CachedCompactPoseIndex.GetInt();
            RB.ParentBoneIndex = INDEX_NONE;
            if (RB.BoneRef.CachedCompactPoseIndex != INDEX_NONE)
            {
                RB.ParentBoneIndex = BoneContainer.GetParentBoneIndex(RB.BoneRef.CachedCompactPoseIndex).GetInt();
                const FTransform BoneRest = FAnimationRuntime::GetComponentSpaceRefPose(
                    RB.BoneRef.CachedCompactPoseIndex, BoneContainer);
                RB.BoneCSRestPos     = BoneRest.GetLocation();
                RB.BoneToRBTransform = RB.CachedUETransform * BoneRest.Inverse();
            }
        }
        if (!RB.BoneRef.IsValidToEvaluate(BoneContainer))
            continue;

        const FCompactPoseBoneIndex CompactIdx    = RB.BoneRef.GetCompactPoseIndex(BoneContainer);
        const FTransform            BoneTransform = Output.Pose.GetComponentSpaceTransform(CompactIdx);
        RB.CachedUETransform = RB.BoneToRBTransform * BoneTransform;
    }

    if (!BulletWorld || !BulletWorld->bInitialized)
    {
        checkSlow(OutBoneTransforms.Num() == 0);
        return;
    }

    TArray<FTransform> KinematicTransforms;
    KinematicTransforms.SetNum(RigidBodies.Num());
    for (int32 i = 0; i < RigidBodies.Num(); i++)
        KinematicTransforms[i] = RigidBodies[i].CachedUETransform;

    const float DeltaTime = Output.AnimInstanceProxy->GetDeltaSeconds();
    if (DeltaTime > 2.0f)
        bNeedBulletReset = true;

    if (bNeedBulletReset)
    {
        BulletWorld->ResetBodies(KinematicTransforms);
        bNeedBulletReset = false;
        checkSlow(OutBoneTransforms.Num() == 0);
        return;
    }

    BulletWorld->UpdateKinematics(RigidBodies, KinematicTransforms, DeltaTime);
    BulletWorld->StepSimulation(DeltaTime, MaxSubSteps, 1.f / static_cast<float>(TimeStepFPS));

    TArray<FTransform> SimulatedTransforms;
    BulletWorld->GetSimulatedTransforms(SimulatedTransforms);

    TArray<EMmdRigidBodyMode> EffectiveModes;
    BulletWorld->GetEffectiveModes(EffectiveModes);

    for (int32 i = 0; i < RigidBodies.Num(); i++)
    {
        const FMmdRigidBodyData& RB     = RigidBodies[i];
        const EMmdRigidBodyMode EffMode = EffectiveModes[i];

        if (EffMode == EMmdRigidBodyMode::FollowBone) continue;
        if (!RB.BoneRef.IsValidToEvaluate(BoneContainer)) continue;

        const FTransform& RBWorld  = SimulatedTransforms[i];
        RigidBodies[i].CachedUETransform = RBWorld;

        const FCompactPoseBoneIndex CompactIdx = RB.BoneRef.GetCompactPoseIndex(BoneContainer);
        FTransform BoneTransform;

        if (EffMode == EMmdRigidBodyMode::PhysicsOnly)
        {
            BoneTransform = RB.BoneToRBTransform.Inverse() * RBWorld;
        }
        else
        {
            const FTransform CurrentBone = Output.Pose.GetComponentSpaceTransform(CompactIdx);
            const FTransform PhysDerived = RB.BoneToRBTransform.Inverse() * RBWorld;

            BoneTransform = FTransform(
                PhysDerived.GetRotation(),
                CurrentBone.GetLocation(),
                CurrentBone.GetScale3D());
        }

        OutBoneTransforms.Add(FBoneTransform(CompactIdx, BoneTransform));
    }

    for (FMmdJointData& Joint : Joints)
    {
        if (!RigidBodies.IsValidIndex(Joint.RigidBodyIndexA)) continue;
        Joint.CachedUETransform = Joint.OffsetFromBodyA * RigidBodies[Joint.RigidBodyIndexA].CachedUETransform;
    }

    OutBoneTransforms.Sort(FCompareBoneTransformIndex());
}

void FPPR_AnimNode::GatherDebugData(FNodeDebugData& DebugData)
{
    FString DebugLine = DebugData.GetNodeName(this);
    DebugLine += FString::Printf(TEXT("(RigidBodies: %d, Joints: %d)"), RigidBodies.Num(), Joints.Num());
    DebugData.AddDebugItem(DebugLine);
    ComponentPose.GatherDebugData(DebugData);
}


