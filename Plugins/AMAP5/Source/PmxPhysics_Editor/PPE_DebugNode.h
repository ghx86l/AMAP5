#pragma once

#include "CoreMinimal.h"
#include "AnimGraphNode_SkeletalControlBase.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "PPR_AnimNode_RigidbodyDebug.h"
#include "PPE_DebugNode.generated.h"

UCLASS()
class PMXPHYSICS_EDITOR_API UPPE_DebugNode : public UAnimGraphNode_SkeletalControlBase
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = Settings)
    FPPR_AnimNode_RigidbodyDebug Node;

    UPROPERTY(EditAnywhere, Category = "Debug Draw")
    bool bEnableDebugDrawRigidBodies = true;

    UPROPERTY(EditAnywhere, Category = "Debug Draw")
    bool bEnableDebugDrawBoneLink = true;

    UPROPERTY(EditAnywhere, Category = "Debug Draw")
    bool bEnableDebugDrawAxis = false;

    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    virtual void CopyNodeDataToPreviewNode(FAnimNode_Base* AnimNode) override;
    virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

protected:
    virtual FEditorModeID GetEditorMode() const override;
    virtual FText GetControllerDescription() const override;
    virtual const FAnimNode_SkeletalControlBase* GetNode() const override;

private:
    void CustomizeDetailDebugVisualizations(IDetailLayoutBuilder& DetailBuilder);

    mutable FNodeTitleTextTable CachedNodeTitles;
};
