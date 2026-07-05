#pragma once

#include "AnimGraphNode_SkeletalControlBase.h"
#include "PPR_AnimNode.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "PPE_Node.generated.h"

UCLASS()
class PMXPHYSICS_EDITOR_API UPPE_Node : public UAnimGraphNode_SkeletalControlBase
{
    GENERATED_UCLASS_BODY()

public:
    UPROPERTY(EditAnywhere, Category = Settings)
    FPPR_AnimNode Node;

    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
    virtual void CopyNodeDataToPreviewNode(FAnimNode_Base* AnimNode) override;
    virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
    void CustomizeDetailDebugVisualizations(IDetailLayoutBuilder& DetailBuilder);

protected:
    virtual FEditorModeID GetEditorMode() const override;
    virtual FText GetControllerDescription() const override;
    virtual const FAnimNode_SkeletalControlBase* GetNode() const override { return &Node; }

public:
    UPROPERTY()
    bool bEnableDebugDrawRigidBodies = true;

    UPROPERTY()
    bool bEnableDebugDrawAxis = true;

    UPROPERTY()
    bool bEnableDebugDrawBoneLink = true;

    UPROPERTY()
    bool bEnableDebugDrawSphere = true;

    UPROPERTY()
    bool bEnableDebugDrawBox = true;

    UPROPERTY()
    bool bEnableDebugDrawCapsule = true;

    UPROPERTY()
    bool bEnableDebugDrawFollowBone = true;

    UPROPERTY()
    bool bEnableDebugDrawPhysics = true;

    UPROPERTY()
    bool bEnableDebugDrawJoints = true;

private:
    mutable FNodeTitleTextTable CachedNodeTitles;
};
