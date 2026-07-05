#include "PPE_Bridge.h"
#include "PPE_Node.h"
#include "PPE_Visualizer.h"
#include "PPE_DebugVisualizer.h"
#include "Modules/ModuleManager.h"
#include "Animation/AnimBlueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "IAnimNodeEditMode.h"
#include "AnimNodeEditMode.h"
#include "EditorModeRegistry.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphUtilities.h"
#include "AnimGraphNode_Root.h"
#include "InterchangeHelper.h"

#define LOCTEXT_NAMESPACE "MMDPhysicsEditor"

DEFINE_LOG_CATEGORY_STATIC(LogMMDPhysicsEditor, Log, All);

void FMMDPhysicsEditorModule::StartupModule()
{
    FEditorModeRegistry::Get().RegisterMode<FPPE_Visualizer>(
        TEXT("AnimGraph.SkeletalControl.MMDPhysics"),
        FText::FromString(TEXT("Pmx BulletPhysics")),
        FSlateIcon(),
        false);

    FEditorModeRegistry::Get().RegisterMode<FPPE_DebugVisualizer>(
        TEXT("AnimGraph.SkeletalControl.PmxRigidBodyDebug"),
        FText::FromString(TEXT("PMX RigidBody Debug")),
        FSlateIcon(),
        false);
}

void FMMDPhysicsEditorModule::ShutdownModule()
{
    FEditorModeRegistry::Get().UnregisterMode(TEXT("AnimGraph.SkeletalControl.MMDPhysics"));
    FEditorModeRegistry::Get().UnregisterMode(TEXT("AnimGraph.SkeletalControl.PmxRigidBodyDebug"));
}

static FName ResolveBoneNameInSkeleton(const FReferenceSkeleton& RefSkel, const FName& RawBoneName)
{
    const FString Converted = UE::Interchange::MakeName(RawBoneName.ToString(), true);

    for (int32 i = 0; i < RefSkel.GetRawBoneNum(); ++i)
    {
        if (RefSkel.GetBoneName(i).ToString().Equals(Converted, ESearchCase::CaseSensitive))
        {
            return RefSkel.GetBoneName(i);
        }
    }

    for (int32 c = 2; c < 100; ++c)
    {
        const FString Candidate = FString::Printf(TEXT("%s_%02d"), *Converted, c);
        for (int32 i = 0; i < RefSkel.GetRawBoneNum(); ++i)
        {
            if (RefSkel.GetBoneName(i).ToString().Equals(Candidate, ESearchCase::CaseSensitive))
            {
                return RefSkel.GetBoneName(i);
            }
        }
    }

    const int32 Idx = RefSkel.FindRawBoneIndex(FName(*Converted));
    if (Idx != INDEX_NONE)
    {
        return RefSkel.GetBoneName(Idx);
    }

    for (int32 c = 2; c < 100; ++c)
    {
        const FName Candidate = *FString::Printf(TEXT("%s_%02d"), *Converted, c);
        const int32 FallbackIdx = RefSkel.FindRawBoneIndex(Candidate);
        if (FallbackIdx != INDEX_NONE)
        {
            return RefSkel.GetBoneName(FallbackIdx);
        }
    }

    return NAME_None;
}

static UPPE_Node* FindOrCreatePmxPhysicsNode(
    UAnimBlueprint* AnimBlueprint, UEdGraph*& OutAnimGraph)
{
    OutAnimGraph = nullptr;
    TArray<UEdGraph*> AllGraphs;
    AnimBlueprint->GetAllGraphs(AllGraphs);
    for (UEdGraph* G : AllGraphs)
    {
        if (G && G->GetName() == TEXT("AnimGraph"))
        {
            OutAnimGraph = G;
            for (UEdGraphNode* N : G->Nodes)
            {
                if (UPPE_Node* P = Cast<UPPE_Node>(N))
                    return P;
            }
            break;
        }
    }
    if (!OutAnimGraph) return nullptr;

    FGraphNodeCreator<UPPE_Node> Creator(*OutAnimGraph);
    UPPE_Node* NewNode = Creator.CreateNode();
    Creator.Finalize();

    UAnimGraphNode_Root* RootNode = nullptr;
    for (UEdGraphNode* N : OutAnimGraph->Nodes)
    {
        if ((RootNode = Cast<UAnimGraphNode_Root>(N)) != nullptr) break;
    }
    if (RootNode)
    {
        NewNode->NodePosX = RootNode->NodePosX;
        NewNode->NodePosY = RootNode->NodePosY - 200;
    }

    return NewNode;
}

bool FMMDPhysicsEditorModule::WriteRigidBodiesToAnimGraph(
    UAnimBlueprint* AnimBlueprint,
    const TArray<FMmdRigidBodyData>& RigidBodies)
{
    TArray<FMmdJointData> EmptyJoints;
    return WritePhysicsToAnimGraph(AnimBlueprint, RigidBodies, EmptyJoints);
}

bool FMMDPhysicsEditorModule::WriteJointsToAnimGraph(
    UAnimBlueprint* AnimBlueprint,
    const TArray<FMmdJointData>& Joints)
{
    if (!AnimBlueprint)
    {
        UE_LOG(LogMMDPhysicsEditor, Error, TEXT("WriteJointsToAnimGraph: null AnimBlueprint."));
        return false;
    }

    UEdGraph* AnimGraph = nullptr;
    UPPE_Node* TargetNode = FindOrCreatePmxPhysicsNode(AnimBlueprint, AnimGraph);
    if (!TargetNode)
    {
        UE_LOG(LogMMDPhysicsEditor, Error, TEXT("WriteJointsToAnimGraph: AnimGraph not found in '%s'."), *AnimBlueprint->GetName());
        return false;
    }

    TargetNode->Modify();
    TargetNode->Node.Joints = Joints;
    TargetNode->ReconstructNode();
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
    FKismetEditorUtilities::CompileBlueprint(AnimBlueprint);

    UE_LOG(LogMMDPhysicsEditor, Log,
        TEXT("WriteJointsToAnimGraph: wrote %d joints to '%s'."),
        Joints.Num(), *AnimBlueprint->GetName());
    return true;
}

bool FMMDPhysicsEditorModule::WritePhysicsToAnimGraph(
    UAnimBlueprint* AnimBlueprint,
    const TArray<FMmdRigidBodyData>& RigidBodies,
    const TArray<FMmdJointData>& Joints)
{
    if (!AnimBlueprint)
    {
        UE_LOG(LogMMDPhysicsEditor, Error, TEXT("WritePhysicsToAnimGraph: null AnimBlueprint."));
        return false;
    }

    UEdGraph* AnimGraph = nullptr;
    UPPE_Node* TargetNode = FindOrCreatePmxPhysicsNode(AnimBlueprint, AnimGraph);
    if (!TargetNode)
    {
        UE_LOG(LogMMDPhysicsEditor, Error, TEXT("WritePhysicsToAnimGraph: AnimGraph not found in '%s'."), *AnimBlueprint->GetName());
        return false;
    }

    TargetNode->Modify();
    TargetNode->Node.Joints = Joints;

    TArray<FMmdRigidBodyData> FilteredBodies;
    if (USkeleton* Skeleton = AnimBlueprint->TargetSkeleton)
    {
        const FReferenceSkeleton& RefSkel = Skeleton->GetReferenceSkeleton();
        for (FMmdRigidBodyData RB : RigidBodies)
        {
            const FName Resolved = ResolveBoneNameInSkeleton(RefSkel, RB.BoneName);
            if (Resolved.IsNone())
            {
                continue;
            }
            RB.BoneName = Resolved;
            RB.BoneRef.BoneName = Resolved;
            FilteredBodies.Add(MoveTemp(RB));
        }
    }
    else
    {
        FilteredBodies = RigidBodies;
    }

    TargetNode->Node.RigidBodies = FilteredBodies;
    for (FMmdRigidBodyData& RB : TargetNode->Node.RigidBodies)
        RB.CachedUETransform = FPPR_AnimNode::MmdToUETransform(RB.Position, RB.Rotation);

    TargetNode->ReconstructNode();
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
    FKismetEditorUtilities::CompileBlueprint(AnimBlueprint);

    UE_LOG(LogMMDPhysicsEditor, Log,
        TEXT("WritePhysicsToAnimGraph: wrote %d bodies and %d joints to '%s'."),
        RigidBodies.Num(), Joints.Num(), *AnimBlueprint->GetName());
    return true;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMMDPhysicsEditorModule, PmxPhysics_Editor)
