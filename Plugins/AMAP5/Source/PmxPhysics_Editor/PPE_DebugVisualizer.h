#pragma once

#include "AnimNodeEditMode.h"
#include "PPE_DebugNode.h"
#include "PPR_AnimNode_RigidbodyDebug.h"

class FEditorViewportClient;
class FPrimitiveDrawInterface;
class USkeletalMeshComponent;
class UMaterialInstanceDynamic;
struct FViewportClick;

struct HPmxRigidBodyDebugHitProxy : HHitProxy
{
    DECLARE_HIT_PROXY()

    explicit HPmxRigidBodyDebugHitProxy(int32 InIndex)
        : HHitProxy(HPP_Wireframe)
        , RigidBodyIndex(InIndex)
    {}

    virtual EMouseCursor::Type GetMouseCursor() override
    {
        return EMouseCursor::Crosshairs;
    }

    int32 RigidBodyIndex;
};

class FPPE_DebugVisualizer : public FAnimNodeEditMode
{
public:
    FPPE_DebugVisualizer();

    virtual void EnterMode(UAnimGraphNode_Base* InEditorNode, FAnimNode_Base* InRuntimeNode) override;
    virtual void ExitMode() override;
    virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;
    virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click) override;
    virtual FVector GetWidgetLocation() const override;
    virtual void DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) override;

private:
    void RenderAllRigidBodies(FPrimitiveDrawInterface* PDI) const;
    void DrawRigidBodySphere(FPrimitiveDrawInterface* PDI, const FMmdRigidBodyData& RB, int32 Index, bool bSelected) const;
    void DrawRigidBodyBox(FPrimitiveDrawInterface* PDI, const FMmdRigidBodyData& RB, int32 Index, bool bSelected) const;
    void DrawRigidBodyCapsule(FPrimitiveDrawInterface* PDI, const FMmdRigidBodyData& RB, int32 Index, bool bSelected) const;
    void DrawBoneLink(FPrimitiveDrawInterface* PDI, const FMmdRigidBodyData& RB) const;
    void DrawTextItem(const FText& Text, FCanvas* Canvas, float X, float& Y, float FontHeight);

    FPPR_AnimNode_RigidbodyDebug* RuntimeNode = nullptr;
    UPPE_DebugNode* GraphNode = nullptr;
    int32 SelectedRigidBodyIndex = -1;
    TObjectPtr<UMaterialInstanceDynamic> PhysicsAssetBodyMaterial;
};
