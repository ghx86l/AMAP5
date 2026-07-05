#include "PPE_DebugNode.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "PmxRigidBodyDebug"

FText UPPE_DebugNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
    return LOCTEXT("NodeTitle", "MMD RigidBody Debug");
}

void UPPE_DebugNode::CopyNodeDataToPreviewNode(FAnimNode_Base* AnimNode)
{
    FPPR_AnimNode_RigidbodyDebug* PreviewNode = static_cast<FPPR_AnimNode_RigidbodyDebug*>(AnimNode);
    *PreviewNode = Node;
}

FEditorModeID UPPE_DebugNode::GetEditorMode() const
{
    return FEditorModeID(TEXT("AnimGraph.SkeletalControl.PmxRigidBodyDebug"));
}

FText UPPE_DebugNode::GetControllerDescription() const
{
    return LOCTEXT("ControllerDescription", "MMD RigidBody Debug");
}

const FAnimNode_SkeletalControlBase* UPPE_DebugNode::GetNode() const
{
    return &Node;
}

void UPPE_DebugNode::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
    Super::CustomizeDetails(DetailBuilder);
    CustomizeDetailDebugVisualizations(DetailBuilder);
}

void UPPE_DebugNode::CustomizeDetailDebugVisualizations(IDetailLayoutBuilder& DetailBuilder)
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
        [ CreateSeparator(TEXT("Rigidbody")) ]
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SUniformGridPanel)
            + SUniformGridPanel::Slot(0, 0)[ CreateDebugButton(TEXT("Rigidbody"), bEnableDebugDrawRigidBodies) ]
            + SUniformGridPanel::Slot(1, 0)[ CreateDebugButton(TEXT("BoneLink"),    bEnableDebugDrawBoneLink) ]
            + SUniformGridPanel::Slot(2, 0)[ CreateDebugButton(TEXT("Axis"),        bEnableDebugDrawAxis) ]
        ]
    ];
}

#undef LOCTEXT_NAMESPACE
