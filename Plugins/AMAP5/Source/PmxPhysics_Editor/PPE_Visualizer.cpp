#include "PPE_Visualizer.h"
#include "PPE_Node.h"
#include "PPR_AnimNode.h"

#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "EditorViewportClient.h"
#include "EditorModes.h"
#include "IPersonaPreviewScene.h"
#include "SceneManagement.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 6
#include "SceneView.h"
#endif

#define LOCTEXT_NAMESPACE "PPE_Visualizer"

IMPLEMENT_HIT_PROXY(HMMDPhysicsRigidBodyHitProxy, HHitProxy)

namespace
{
    FLinearColor GetRigidBodyColor(EMmdRigidBodyMode Mode, bool bSelected)
    {
        switch (Mode)
        {
        case EMmdRigidBodyMode::FollowBone:          return FLinearColor(0.0f, 0.45f, 1.0f, 1.0f);
        case EMmdRigidBodyMode::PhysicsOnly:         return FLinearColor(1.0f, 0.9f, 0.0f, 1.0f);
        case EMmdRigidBodyMode::PhysicsAndBoneMerge: return FLinearColor(0.0f, 0.85f, 0.2f, 1.0f);
        default:                                      return FLinearColor(0.0f, 0.85f, 0.2f, 1.0f);
        }
    }

    const FMaterialRenderProxy* GetRigidBodyMaterialProxy(EMmdRigidBodyMode Mode)
    {
        if (!GEngine)
        {
            return nullptr;
        }

        switch (Mode)
        {
        case EMmdRigidBodyMode::FollowBone:
            return GEngine->ConstraintLimitMaterialZ ? GEngine->ConstraintLimitMaterialZ->GetRenderProxy() : nullptr;
        case EMmdRigidBodyMode::PhysicsOnly:
            return GEngine->ConstraintLimitMaterialPrismatic ? GEngine->ConstraintLimitMaterialPrismatic->GetRenderProxy() : nullptr;
        case EMmdRigidBodyMode::PhysicsAndBoneMerge:
            return GEngine->ConstraintLimitMaterialY ? GEngine->ConstraintLimitMaterialY->GetRenderProxy() : nullptr;
        default:
            return GEngine->ConstraintLimitMaterialY ? GEngine->ConstraintLimitMaterialY->GetRenderProxy() : nullptr;
        }
    }
} // namespace


FPPE_Visualizer::FPPE_Visualizer()
    : RuntimeNode(nullptr)
    , GraphNode(nullptr)
    , SelectedRigidBodyIndex(-1)
{}

void FPPE_Visualizer::EnterMode(UAnimGraphNode_Base* InEditorNode,
                                     FAnimNode_Base* InRuntimeNode)
{
    RuntimeNode = static_cast<FPPR_AnimNode*>(InRuntimeNode);
    GraphNode   = CastChecked<UPPE_Node>(InEditorNode);

    RuntimeNode->SelectedRigidBodyIndex = SelectedRigidBodyIndex;

    UMaterialInterface* BaseElemSelectedMaterial = LoadObject<UMaterialInterface>(
        nullptr, TEXT("/Engine/EditorMaterials/PhAT_UnselectedMaterial.PhAT_UnselectedMaterial"), nullptr,
        LOAD_None, nullptr);
    PhysicsAssetBodyMaterial = UMaterialInstanceDynamic::Create(
        BaseElemSelectedMaterial, GetTransientPackage());
    PhysicsAssetBodyMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.8f);

    FAnimNodeEditMode::EnterMode(InEditorNode, InRuntimeNode);
}

void FPPE_Visualizer::ExitMode()
{
    GraphNode   = nullptr;
    RuntimeNode = nullptr;
    PhysicsAssetBodyMaterial = nullptr;

    FAnimNodeEditMode::ExitMode();
}

void FPPE_Visualizer::Render(const FSceneView*      View,
                                  FViewport*             Viewport,
                                  FPrimitiveDrawInterface* PDI)
{
    const USkeletalMeshComponent* SkelMeshComp =
        GetAnimPreviewScene().GetPreviewMeshComponent();

    if (SkelMeshComp &&
        SkelMeshComp->GetSkeletalMeshAsset() &&
        RuntimeNode &&
        GraphNode->bEnableDebugDrawRigidBodies)
    {
        RenderAllRigidBodies(PDI);
        PDI->SetHitProxy(nullptr);
    }

    if (SkelMeshComp &&
        SkelMeshComp->GetSkeletalMeshAsset() &&
        RuntimeNode &&
        GraphNode->bEnableDebugDrawJoints)
    {
        DrawAllJoints(PDI);
    }

    FAnimNodeEditMode::Render(View, Viewport, PDI);
}

void FPPE_Visualizer::RenderAllRigidBodies(FPrimitiveDrawInterface* PDI) const
{
    for (int32 i = 0; i < RuntimeNode->RigidBodies.Num(); i++)
    {
        const FMmdRigidBodyData& RB = RuntimeNode->RigidBodies[i];
        const bool bSelected = (i == SelectedRigidBodyIndex);

        const bool bIsFollowBone = (RB.Mode == EMmdRigidBodyMode::FollowBone);
        if (bIsFollowBone && !GraphNode->bEnableDebugDrawFollowBone) continue;
        if (!bIsFollowBone && !GraphNode->bEnableDebugDrawPhysics) continue;

        switch (RB.ShapeType)
        {
        case EMmdRigidBodyShape::Sphere:
            if (GraphNode->bEnableDebugDrawSphere)
                DrawRigidBodySphere(PDI, RB, i, bSelected);
            break;
        case EMmdRigidBodyShape::Box:
            if (GraphNode->bEnableDebugDrawBox)
                DrawRigidBodyBox(PDI, RB, i, bSelected);
            break;
        case EMmdRigidBodyShape::Capsule:
            if (GraphNode->bEnableDebugDrawCapsule)
                DrawRigidBodyCapsule(PDI, RB, i, bSelected);
            break;
        }

        if (GraphNode->bEnableDebugDrawBoneLink && RB.BoneRef.BoneIndex >= 0)
        {
            DrawBoneLink(PDI, RB);
        }

        if (bSelected && GraphNode->bEnableDebugDrawAxis)
        {
            DrawCoordinateSystem(PDI,
                RB.CachedUETransform.GetLocation(),
                RB.CachedUETransform.Rotator(),
                5.0f,
                SDPG_Foreground);
        }
    }
}

void FPPE_Visualizer::DrawRigidBodySphere(FPrimitiveDrawInterface* PDI,
                                               const FMmdRigidBodyData& RB,
                                               int32 Index,
                                               bool bSelected) const
{
    const float Radius = RB.ShapeSize.X;
    if (Radius <= 0.f) return;

    const FVector Center = RB.CachedUETransform.GetLocation();
    const FLinearColor Color = GetRigidBodyColor(RB.Mode, bSelected);
    const FMaterialRenderProxy* MaterialProxy = GetRigidBodyMaterialProxy(RB.Mode);

    PDI->SetHitProxy(new HMMDPhysicsRigidBodyHitProxy(Index));
    if (MaterialProxy)
    {
        DrawSphere(PDI, Center, FRotator::ZeroRotator, FVector(Radius), 24, 6, MaterialProxy, SDPG_World);
    }
    DrawWireSphere(PDI, Center, Color, Radius, 24, SDPG_World);
    PDI->SetHitProxy(nullptr);
}

void FPPE_Visualizer::DrawRigidBodyBox(FPrimitiveDrawInterface* PDI,
                                            const FMmdRigidBodyData& RB,
                                            int32 Index,
                                            bool bSelected) const
{
    const FVector Extent = RB.ShapeSize;
    if (Extent.IsNearlyZero()) return;

    const FTransform& T  = RB.CachedUETransform;
    const FLinearColor Color = GetRigidBodyColor(RB.Mode, bSelected);
    const FMaterialRenderProxy* MaterialProxy = GetRigidBodyMaterialProxy(RB.Mode);

    PDI->SetHitProxy(new HMMDPhysicsRigidBodyHitProxy(Index));
    if (MaterialProxy)
    {
        DrawBox(PDI, T.ToMatrixWithScale(), Extent, MaterialProxy, SDPG_World);
    }
    DrawWireBox(PDI, T.ToMatrixWithScale(), FBox(-Extent, Extent), Color, SDPG_World);
    PDI->SetHitProxy(nullptr);
}

void FPPE_Visualizer::DrawRigidBodyCapsule(FPrimitiveDrawInterface* PDI,
                                                const FMmdRigidBodyData& RB,
                                                int32 Index,
                                                bool bSelected) const
{
    const float Radius     = RB.ShapeSize.X;
    const float HalfHeight = RB.ShapeSize.Y * 0.5f;
    if (Radius <= 0.f || HalfHeight <= 0.f) return;

    const FTransform& T = RB.CachedUETransform;
    const FVector Center = T.GetLocation();
    const FQuat Q = T.GetRotation();
    const FVector XAxis = Q.GetAxisX();
    const FVector YAxis = Q.GetAxisY();
    const FVector ZAxis = Q.GetAxisZ();
    const FLinearColor Color = GetRigidBodyColor(RB.Mode, bSelected);
    const FMaterialRenderProxy* MaterialProxy = GetRigidBodyMaterialProxy(RB.Mode);

    PDI->SetHitProxy(new HMMDPhysicsRigidBodyHitProxy(Index));
    if (MaterialProxy)
    {
        DrawCylinder(PDI, Center, XAxis, YAxis, ZAxis, Radius, HalfHeight, 25, MaterialProxy, SDPG_World);
        DrawSphere(PDI, Center + ZAxis * HalfHeight, Q.Rotator(), FVector(Radius), 24, 6, MaterialProxy, SDPG_World);
        DrawSphere(PDI, Center - ZAxis * HalfHeight, Q.Rotator(), FVector(Radius), 24, 6, MaterialProxy, SDPG_World);
    }
    DrawWireCapsule(PDI,
        Center, XAxis, YAxis, ZAxis,
        Color,
        Radius,
        HalfHeight + Radius,
        25,
        SDPG_World);
    PDI->SetHitProxy(nullptr);
}

void FPPE_Visualizer::DrawAllJoints(FPrimitiveDrawInterface* PDI) const
{
    static const FLinearColor JointColor(0.7f, 0.4f, 1.0f, 1.0f);
    for (const FMmdJointData& Joint : RuntimeNode->Joints)
    {
        DrawWireBox(PDI, Joint.CachedUETransform.ToMatrixWithScale(), FBox(FVector(-0.5f), FVector(0.5f)), JointColor, SDPG_World);
    }
}

void FPPE_Visualizer::DrawBoneLink(FPrimitiveDrawInterface* PDI,
                                        const FMmdRigidBodyData& RB) const
{
    if (RB.BoneRef.BoneIndex < 0 ||
        RuntimeNode->ForwardedPose.GetPose().GetNumBones() <= 0)
    {
        return;
    }

    const FTransform BoneTransform =
        RuntimeNode->ForwardedPose.GetComponentSpaceTransform(
            RB.BoneRef.GetCompactPoseIndex(
                RuntimeNode->ForwardedPose.GetPose().GetBoneContainer()));

    const USkeletalMeshComponent* SkelComp =
        GetAnimPreviewScene().GetPreviewMeshComponent();
    if (!SkelComp) return;

    const FTransform CompWorldTrans = SkelComp->GetComponentTransform();
    const FVector BoneWorldPos =
        CompWorldTrans.TransformPosition(BoneTransform.GetLocation());
    const FVector RBWorldPos =
        CompWorldTrans.TransformPosition(RB.CachedUETransform.GetLocation());

    DrawDashedLine(PDI,
        RBWorldPos,
        BoneWorldPos,
        FLinearColor(0.5f, 0.5f, 0.5f),
        1.0f,
        SDPG_Foreground);
}

bool FPPE_Visualizer::HandleClick(FEditorViewportClient* InViewportClient,
                                       HHitProxy*             HitProxy,
                                       const FViewportClick&  Click)
{
    bool bResult = FAnimNodeEditMode::HandleClick(InViewportClient, HitProxy, Click);

    if (HitProxy && HitProxy->IsA(HMMDPhysicsRigidBodyHitProxy::StaticGetType()))
    {
        const HMMDPhysicsRigidBodyHitProxy* Proxy =
            static_cast<HMMDPhysicsRigidBodyHitProxy*>(HitProxy);

        SelectedRigidBodyIndex = Proxy->RigidBodyIndex;

        if (RuntimeNode)
        {
            RuntimeNode->SelectedRigidBodyIndex = SelectedRigidBodyIndex;
        }
        bResult = true;
    }
    else
    {
        SelectedRigidBodyIndex = -1;
        if (RuntimeNode)
        {
            RuntimeNode->SelectedRigidBodyIndex = -1;
        }
    }

    return bResult;
}

FVector FPPE_Visualizer::GetWidgetLocation() const
{
    if (RuntimeNode &&
        RuntimeNode->RigidBodies.IsValidIndex(SelectedRigidBodyIndex))
    {
        return RuntimeNode->RigidBodies[SelectedRigidBodyIndex]
            .CachedUETransform.GetLocation();
    }
    return GetAnimPreviewScene().GetPreviewMeshComponent()->GetComponentLocation();
}

void FPPE_Visualizer::DrawHUD(FEditorViewportClient* ViewportClient,
                                   FViewport*             Viewport,
                                   const FSceneView*      View,
                                   FCanvas*               Canvas)
{
    float FontWidth, FontHeight;
    GEngine->GetSmallFont()->GetCharSize(TEXT('L'), FontWidth, FontHeight);

    constexpr float XOffset = 5.0f;
    float DrawPositionY = Viewport->GetSizeXY().Y / Canvas->GetDPIScale()
                         - (3.f + FontHeight) - 100.f / Canvas->GetDPIScale();

    if (RuntimeNode && RuntimeNode->RigidBodies.IsValidIndex(SelectedRigidBodyIndex))
    {
        const FMmdRigidBodyData& RB =
            RuntimeNode->RigidBodies[SelectedRigidBodyIndex];

        const FString ShapeStr = (RB.ShapeType == EMmdRigidBodyShape::Sphere)  ? TEXT("Sphere")  :
                                 (RB.ShapeType == EMmdRigidBodyShape::Box)     ? TEXT("Box")     :
                                                                                  TEXT("Capsule");
        const FString ModeStr  = (RB.Mode == EMmdRigidBodyMode::FollowBone)          ? TEXT("FollowBone")          :
                                 (RB.Mode == EMmdRigidBodyMode::PhysicsOnly)         ? TEXT("PhysicsOnly")         :
                                                                                        TEXT("PhysicsAndBoneMerge");

        DrawTextItem(FText::FromString(
            FString::Printf(TEXT("[%d] %s  Shape:%s  Mode:%s"),
                SelectedRigidBodyIndex,
                *RB.Name.ToString(),
                *ShapeStr,
                *ModeStr)),
            Canvas, XOffset, DrawPositionY, FontHeight);

        DrawTextItem(FText::FromString(
            FString::Printf(TEXT("Bone: %s (idx=%d)"),
                *RB.BoneName.ToString(),
                RB.ResolvedBoneIndex)),
            Canvas, XOffset, DrawPositionY, FontHeight);
    }
    else
    {
        if (RuntimeNode)
        {
            DrawTextItem(FText::FromString(
                FString::Printf(TEXT("MMD RigidBodies: %d  (click to select)"),
                    RuntimeNode->RigidBodies.Num())),
                Canvas, XOffset, DrawPositionY, FontHeight);
        }
    }

    FAnimNodeEditMode::DrawHUD(ViewportClient, Viewport, View, Canvas);
}

void FPPE_Visualizer::DrawTextItem(const FText& Text, FCanvas* Canvas,
                                        float X, float& Y, float FontHeight)
{
    FCanvasTextItem TextItem(FVector2D::ZeroVector, Text,
                             GEngine->GetSmallFont(), FLinearColor::White);
    TextItem.EnableShadow(FLinearColor::Black);
    Canvas->DrawItem(TextItem, X, Y);
    Y -= (3.f + FontHeight);
}

#undef LOCTEXT_NAMESPACE
