#include "PPE_Node.h"
#include "PPE_Bridge.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PPE_Node)

#define LOCTEXT_NAMESPACE "MMDPhysicsEditor"

UPPE_Node::UPPE_Node(const FObjectInitializer& OI)
    : Super(OI)
{}

FText UPPE_Node::GetControllerDescription() const
{
    return LOCTEXT("PmxBulletPhysics", "Pmx BulletPhysics");
}

FText UPPE_Node::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
    FFormatNamedArguments Args;
    Args.Add(TEXT("Desc"), GetControllerDescription());
    Args.Add(TEXT("Count"), Node.RigidBodies.Num());

    if (TitleType == ENodeTitleType::ListView || TitleType == ENodeTitleType::MenuTitle)
    {
        return FText::Format(LOCTEXT("NodeTitle_List", "{Desc}"), Args);
    }
    return FText::Format(LOCTEXT("NodeTitle_Full", "{Desc}"), Args);
}

FEditorModeID UPPE_Node::GetEditorMode() const
{
    return TEXT("AnimGraph.SkeletalControl.MMDPhysics");
}

void UPPE_Node::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    for (FMmdRigidBodyData& RB : Node.RigidBodies)
    {
        RB.CachedUETransform = FPPR_AnimNode::MmdToUETransform(RB.Position, RB.Rotation);
    }
    ReconstructNode();
}

void UPPE_Node::CopyNodeDataToPreviewNode(FAnimNode_Base* AnimNode)
{
    FPPR_AnimNode* MMDNode = static_cast<FPPR_AnimNode*>(AnimNode);
    MMDNode->RigidBodies           = Node.RigidBodies;
    MMDNode->Joints                = Node.Joints;
    MMDNode->bDebugDrawRigidBodies = Node.bDebugDrawRigidBodies;
    MMDNode->MaxSubSteps           = Node.MaxSubSteps;

    for (FMmdRigidBodyData& RB : MMDNode->RigidBodies)
    {
        if (RB.BoneRef.BoneName.IsNone())
        {
            RB.BoneRef.BoneName = RB.BoneName;
        }
        RB.BoneRef.CachedCompactPoseIndex = FCompactPoseBoneIndex(INDEX_NONE);
    }
}

void UPPE_Node::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
    Super::CustomizeDetails(DetailBuilder);
    CustomizeDetailDebugVisualizations(DetailBuilder);

    auto CategorySorter = [](const TMap<FName, IDetailCategoryBuilder*>& Categories)
    {
        int32 Order = 0;
        auto SafeSetOrder = [&Categories, &Order](const FName& CategoryName)
        {
            if (IDetailCategoryBuilder* const* Builder = Categories.Find(CategoryName))
            {
                (*Builder)->SetSortOrder(Order++);
            }
        };

        SafeSetOrder(FName("Visualization"));
        SafeSetOrder(FName("Functions"));
        SafeSetOrder(FName("Rigidbody"));
        SafeSetOrder(FName("Joint"));
        SafeSetOrder(FName("Physics World"));
        SafeSetOrder(FName("Tag"));
        SafeSetOrder(FName("Alpha"));
        SafeSetOrder(FName("Performance"));
        SafeSetOrder(FName("Bindings"));
    };

    DetailBuilder.SortCategories(CategorySorter);
}

void UPPE_Node::CustomizeDetailDebugVisualizations(IDetailLayoutBuilder& DetailBuilder)
{
    IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(TEXT("Visualization"), FText::FromString(TEXT("Visualization")));
    FDetailWidgetRow& WidgetRow = Category.AddCustomRow(
        LOCTEXT("DebugVisRow", "DebugVisualization"));

    auto CreateDebugButton = [&](const FString& Label, bool& Flag) -> TSharedRef<SButton>
    {
        return SNew(SButton)
            .HAlign(HAlign_Center)
            .VAlign(VAlign_Center)
            .OnClicked_Lambda([&Flag]() { Flag = !Flag; return FReply::Handled(); })
            .ButtonColorAndOpacity_Lambda([&Flag]()
            {
                return Flag
                    ? FAppStyle::Get().GetSlateColor("Colors.AccentGreen")
                    : FAppStyle::Get().GetSlateColor("Colors.AccentRed");
            })
            .Content()
            [
                SNew(STextBlock)
                    .Text(FText::FromString(Label))
                    .Font(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 9))
            ];
    };

    auto CreateSeparator = [&](const FString& Label) -> TSharedRef<SWidget>
    {
        return SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(0.01f).VAlign(VAlign_Center)
            [ SNew(SSeparator) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(FMargin(2.f, 0.f)).VAlign(VAlign_Center)
            [ SNew(STextBlock).Text(FText::FromString(Label)).Font(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 9)) ]
            + SHorizontalBox::Slot().FillWidth(0.9f).VAlign(VAlign_Center)
            [ SNew(SSeparator) ];
    };

    WidgetRow
    [
        SNew(SVerticalBox)

        + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f))
        [ CreateSeparator(TEXT("Debug")) ]
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SUniformGridPanel)
            .SlotPadding(FMargin(2.f, 0.f))
            + SUniformGridPanel::Slot(0, 0)[ CreateDebugButton(TEXT("Rigidbody"),  bEnableDebugDrawRigidBodies) ]
            + SUniformGridPanel::Slot(1, 0)[ CreateDebugButton(TEXT("Link Bone"),  bEnableDebugDrawBoneLink) ]
            + SUniformGridPanel::Slot(2, 0)[ CreateDebugButton(TEXT("Axis"),       bEnableDebugDrawAxis) ]
        ]

        + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f))
        [ CreateSeparator(TEXT("Shape")) ]
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SUniformGridPanel)
            .SlotPadding(FMargin(2.f, 0.f))
            + SUniformGridPanel::Slot(0, 0)[ CreateDebugButton(TEXT("Sphere"),  bEnableDebugDrawSphere) ]
            + SUniformGridPanel::Slot(1, 0)[ CreateDebugButton(TEXT("Box"),     bEnableDebugDrawBox) ]
            + SUniformGridPanel::Slot(2, 0)[ CreateDebugButton(TEXT("Capsule"), bEnableDebugDrawCapsule) ]
        ]

        + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f))
        [ CreateSeparator(TEXT("Type")) ]
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SUniformGridPanel)
            .SlotPadding(FMargin(2.f, 0.f))
            + SUniformGridPanel::Slot(0, 0)[ CreateDebugButton(TEXT("Follow Bone"), bEnableDebugDrawFollowBone) ]
            + SUniformGridPanel::Slot(1, 0)[ CreateDebugButton(TEXT("Physics"),     bEnableDebugDrawPhysics) ]
        ]

        + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f))
        [ CreateSeparator(TEXT("Joint")) ]
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SUniformGridPanel)
            .SlotPadding(FMargin(2.f, 0.f))
            + SUniformGridPanel::Slot(0, 0)[ CreateDebugButton(TEXT("Joint"), bEnableDebugDrawJoints) ]
        ]
    ];
}

#undef LOCTEXT_NAMESPACE
