#include "AnimGraphNode.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"

#include "InterchangeHelper.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/Skeleton.h"
#include "ReferenceSkeleton.h"
#include "AnimGraphNode_Root.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#if WITH_EDITOR
#include "EdGraphUtilities.h"
#endif
#include "Json.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "IK_Loader"

FText UIK_LoaderAnimGraphNode_PmxIKLoad::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
    return LOCTEXT("NodeTitle", "Pmx BoneConstraint");
}

FText UIK_LoaderAnimGraphNode_PmxIKLoad::GetTooltipText() const
{
    return LOCTEXT("NodeTooltip", "PMX bone constraint node.");
}

FText UIK_LoaderAnimGraphNode_PmxIKLoad::GetMenuCategory() const
{
    return LOCTEXT("MenuCategory", "PMX");
}

FString UIK_LoaderAnimGraphNode_PmxIKLoad::GetNodeCategory() const
{
    return TEXT("PMX");
}

FLinearColor UIK_LoaderAnimGraphNode_PmxIKLoad::GetNodeTitleColor() const
{
	return FLinearColor(0.75f, 0.75f, 0.10f);
}

void UIK_LoaderAnimGraphNode_PmxIKLoad::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
    UClass* ActionKey = GetClass();
    if (ActionRegistrar.IsOpenForRegistration(ActionKey))
    {
        UBlueprintNodeSpawner* Spawner = UBlueprintNodeSpawner::Create(GetClass());
        ActionRegistrar.AddBlueprintAction(ActionKey, Spawner);
    }
}

void UIK_LoaderAnimGraphNode_PmxIKLoad::CustomizePinData(UEdGraphPin* Pin, FName SourcePropertyName, int32 ArrayIndex) const
{
	Super::CustomizePinData(Pin, SourcePropertyName, ArrayIndex);

	if (SourcePropertyName == GET_MEMBER_NAME_CHECKED(FAnimNode_PmxIKLoad, IKAlpha))
	{
		if (Node.CCDIK.IsValidIndex(ArrayIndex))
		{
			Pin->PinFriendlyName = FText::FromString(Node.CCDIK[ArrayIndex].IkBone.BoneName.ToString());
		}
	}
}

static int32 ReadIntField(const TSharedPtr<FJsonObject>& Obj, const TCHAR* FieldName, int32 DefaultValue)
{
	double Number = 0.0;
	if (Obj.IsValid() && Obj->TryGetNumberField(FieldName, Number))
	{
		return (int32)Number;
	}
	return DefaultValue;
}

static void SortPmxCCDIK(TArray<FPmxIKChain>& CCDIK)
{
	CCDIK.Sort([](const FPmxIKChain& A, const FPmxIKChain& B)
	{
		if (A.BoneClass != B.BoneClass)
		{
			return A.BoneClass < B.BoneClass;
		}
		return A.BoneIndex < B.BoneIndex;
	});
}

static void SortPmxGrants(TArray<FPmxGrantEntry>& Grants)
{
	Grants.Sort([](const FPmxGrantEntry& A, const FPmxGrantEntry& B)
	{
		if (A.BoneClass != B.BoneClass)
		{
			return A.BoneClass < B.BoneClass;
		}
		return A.BoneIndex < B.BoneIndex;
	});
}

static bool ParsePmxCCDIKFromPayload(const FString& Payload, TArray<FPmxIKChain>& OutCCDIK)
{
	OutCCDIK.Reset();
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Payload);
	TArray<TSharedPtr<FJsonValue>> RootArray;
	if (!FJsonSerializer::Deserialize(Reader, RootArray))
	{
		return false;
	}

	for (const TSharedPtr<FJsonValue>& V : RootArray)
	{
		if (!V.IsValid() || V->Type != EJson::Object)
		{
			continue;
		}
		const TSharedPtr<FJsonObject> Obj = V->AsObject();
		if (!Obj.IsValid())
		{
			continue;
		}

		FPmxIKChain Chain;
		FString IkBoneStr;
		FString TargetBoneStr;
		Chain.BoneClass = ReadIntField(Obj, TEXT("BoneClass"), Chain.BoneClass);
		Chain.BoneIndex = ReadIntField(Obj, TEXT("BoneIndex"), ReadIntField(Obj, TEXT("bone_index"), Chain.BoneIndex));
		Chain.LoopCount = Obj->GetIntegerField(TEXT("loop"));
		Chain.UnitAngleDeg = (float)Obj->GetNumberField(TEXT("angle"));
		if (Obj->TryGetStringField(TEXT("ik_bone"), IkBoneStr))
		{
			Chain.IkBone.BoneName = FName(*UE::Interchange::MakeName(IkBoneStr, true));
		}
		if (Obj->TryGetStringField(TEXT("target_bone"), TargetBoneStr))
		{
			Chain.TargetBone.BoneName = FName(*UE::Interchange::MakeName(TargetBoneStr, true));
		}

		const TArray<TSharedPtr<FJsonValue>>* LinksArrayPtr = nullptr;
		if (Obj->TryGetArrayField(TEXT("links"), LinksArrayPtr) && LinksArrayPtr)
		{
			for (const TSharedPtr<FJsonValue>& LV : *LinksArrayPtr)
			{
				if (!LV.IsValid() || LV->Type != EJson::Object)
				{
					continue;
				}
				const TSharedPtr<FJsonObject> LObj = LV->AsObject();
				if (!LObj.IsValid())
				{
					continue;
				}

				FPmxIKLink Link;
				FString LinkBoneStr;
				if (LObj->TryGetStringField(TEXT("link_bone"), LinkBoneStr))
				{
					Link.LinkBone.BoneName = FName(*UE::Interchange::MakeName(LinkBoneStr, true));
				}
					Link.bEnableLimitX = LObj->GetBoolField(TEXT("enable_lim_x"));
					Link.bEnableLimitY = LObj->GetBoolField(TEXT("enable_lim_y"));
					Link.bEnableLimitZ = LObj->GetBoolField(TEXT("enable_lim_z"));
					const TArray<TSharedPtr<FJsonValue>>* MinArray = nullptr;
				const TArray<TSharedPtr<FJsonValue>>* MaxArray = nullptr;
				if (LObj->TryGetArrayField(TEXT("min_lim"), MinArray) && LObj->TryGetArrayField(TEXT("max_lim"), MaxArray))
				{
					if (MinArray && MaxArray && MinArray->Num() == 3 && MaxArray->Num() == 3)
					{
						Link.MinLimitDeg = FVector(
							(float)(*MinArray)[0]->AsNumber(),
							(float)(*MinArray)[1]->AsNumber(),
							(float)(*MinArray)[2]->AsNumber());
						Link.MaxLimitDeg = FVector(
							(float)(*MaxArray)[0]->AsNumber(),
							(float)(*MaxArray)[1]->AsNumber(),
							(float)(*MaxArray)[2]->AsNumber());
					}
				}
				Chain.Links.Add(Link);
			}
		}

		OutCCDIK.Add(Chain);
	}

	SortPmxCCDIK(OutCCDIK);
	return OutCCDIK.Num() > 0;
}

static bool ParsePmxGrantsFromPayload(const FString& Payload, TArray<FPmxGrantEntry>& OutGrants)
{
	OutGrants.Reset();
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Payload);
	TArray<TSharedPtr<FJsonValue>> RootArray;
	if (!FJsonSerializer::Deserialize(Reader, RootArray))
	{
		return false;
	}

	for (const TSharedPtr<FJsonValue>& V : RootArray)
	{
		if (!V.IsValid() || V->Type != EJson::Object)
		{
			continue;
		}
		const TSharedPtr<FJsonObject> Obj = V->AsObject();
		if (!Obj.IsValid())
		{
			continue;
		}

		FPmxGrantEntry Entry;
		FString BoneStr;
		FString GrantBoneStr;
		Entry.BoneClass = ReadIntField(Obj, TEXT("BoneClass"), Entry.BoneClass);
		Entry.BoneIndex = ReadIntField(Obj, TEXT("BoneIndex"), ReadIntField(Obj, TEXT("bone_index"), Entry.BoneIndex));
		if (Obj->TryGetStringField(TEXT("bone_name"), BoneStr))
		{
			Entry.BoneName.BoneName = FName(*UE::Interchange::MakeName(BoneStr, true));
		}
		if (Obj->TryGetStringField(TEXT("grant_bone_name"), GrantBoneStr))
		{
			Entry.GrantBoneName.BoneName = FName(*UE::Interchange::MakeName(GrantBoneStr, true));
		}
		Entry.GrantWeight = (float)Obj->GetNumberField(TEXT("grant_weight"));
		Entry.bGrantRotation = Obj->GetBoolField(TEXT("grant_rotation"));
		Entry.bGrantTranslation = Obj->GetBoolField(TEXT("grant_translation"));

		OutGrants.Add(Entry);
	}

	SortPmxGrants(OutGrants);
	return OutGrants.Num() > 0;
}

#if WITH_EDITOR
static UIK_LoaderAnimGraphNode_PmxIKLoad* FindOrCreateIKNode(UEdGraph* AnimGraph, UAnimBlueprint* AnimBlueprint, bool& bOutCreated)
{
	bOutCreated = false;
	UIK_LoaderAnimGraphNode_PmxIKLoad* TargetNode = nullptr;
	for (UEdGraphNode* N : AnimGraph->Nodes)
	{
		TargetNode = Cast<UIK_LoaderAnimGraphNode_PmxIKLoad>(N);
		if (TargetNode)
		{
			return TargetNode;
		}
	}

	FGraphNodeCreator<UIK_LoaderAnimGraphNode_PmxIKLoad> NodeCreator(*AnimGraph);
	TargetNode = NodeCreator.CreateNode();
	NodeCreator.Finalize();
	bOutCreated = true;

	if (TargetNode)
	{
		UAnimGraphNode_Root* RootNode = nullptr;
		for (UEdGraphNode* N : AnimGraph->Nodes)
		{
			RootNode = Cast<UAnimGraphNode_Root>(N);
			if (RootNode)
			{
				break;
			}
		}
		if (RootNode)
		{
			TargetNode->NodePosX = RootNode->NodePosX;
			TargetNode->NodePosY = RootNode->NodePosY - 200;
		}
	}

	return TargetNode;
}

bool UIK_LoaderImportLibrary::apply_pmx_ik_to_anim_blueprint(UAnimBlueprint* AnimBlueprint, const FString& Payload)
{
	if (AnimBlueprint == nullptr)
	{
		return false;
	}

	UEdGraph* AnimGraph = nullptr;
	TArray<UEdGraph*> Graphs;
	AnimBlueprint->GetAllGraphs(Graphs);
	for (UEdGraph* G : Graphs)
	{
		if (G && G->GetName() == TEXT("AnimGraph"))
		{
			AnimGraph = G;
			break;
		}
	}
	if (AnimGraph == nullptr)
	{
		return false;
	}

	bool bCreated = false;
	UIK_LoaderAnimGraphNode_PmxIKLoad* TargetNode = FindOrCreateIKNode(AnimGraph, AnimBlueprint, bCreated);
	if (TargetNode == nullptr)
	{
		return false;
	}

	TArray<FPmxIKChain> CCDIK;
	if (!ParsePmxCCDIKFromPayload(Payload, CCDIK))
	{
		return false;
	}

	TargetNode->Node.CCDIK = CCDIK;

	const int32 OldNum_IK = TargetNode->Node.IKAlpha.Num();
	TargetNode->Node.IKAlpha.SetNum(CCDIK.Num());
	for (int32 i = OldNum_IK; i < TargetNode->Node.IKAlpha.Num(); ++i)
	{
		TargetNode->Node.IKAlpha[i] = 1.0f;
	}

	TargetNode->ReconstructNode();

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
	FKismetEditorUtilities::CompileBlueprint(AnimBlueprint);
	return true;
}

bool UIK_LoaderImportLibrary::apply_pmx_grants_to_anim_blueprint(UAnimBlueprint* AnimBlueprint, const FString& Payload)
{
	if (AnimBlueprint == nullptr)
	{
		return false;
	}

	UEdGraph* AnimGraph = nullptr;
	TArray<UEdGraph*> Graphs;
	AnimBlueprint->GetAllGraphs(Graphs);
	for (UEdGraph* G : Graphs)
	{
		if (G && G->GetName() == TEXT("AnimGraph"))
		{
			AnimGraph = G;
			break;
		}
	}
	if (AnimGraph == nullptr)
	{
		return false;
	}

	bool bCreated = false;
	UIK_LoaderAnimGraphNode_PmxIKLoad* TargetNode = FindOrCreateIKNode(AnimGraph, AnimBlueprint, bCreated);
	if (TargetNode == nullptr)
	{
		return false;
	}

	TArray<FPmxGrantEntry> Grants;
	if (!ParsePmxGrantsFromPayload(Payload, Grants))
	{
		return false;
	}

	TargetNode->Node.Grants = Grants;
	TargetNode->ReconstructNode();

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
	FKismetEditorUtilities::CompileBlueprint(AnimBlueprint);
	return true;
}

bool UIK_LoaderImportLibrary::apply_pmx_constraints_to_anim_blueprint(UAnimBlueprint* AnimBlueprint, const FString& IKPayload, const FString& GrantPayload)
{
	if (AnimBlueprint == nullptr)
	{
		return false;
	}

	UEdGraph* AnimGraph = nullptr;
	TArray<UEdGraph*> Graphs;
	AnimBlueprint->GetAllGraphs(Graphs);
	for (UEdGraph* G : Graphs)
	{
		if (G && G->GetName() == TEXT("AnimGraph"))
		{
			AnimGraph = G;
			break;
		}
	}
	if (AnimGraph == nullptr)
	{
		return false;
	}

	FGraphNodeCreator<UIK_LoaderAnimGraphNode_PmxIKLoad> NodeCreator(*AnimGraph);
	UIK_LoaderAnimGraphNode_PmxIKLoad* TargetNode = NodeCreator.CreateNode();
	NodeCreator.Finalize();
	if (TargetNode == nullptr)
	{
		return false;
	}

	TArray<FPmxIKChain> CCDIK;
	if (ParsePmxCCDIKFromPayload(IKPayload, CCDIK))
	{
		TargetNode->Node.CCDIK = CCDIK;

		const int32 OldNum_C = TargetNode->Node.IKAlpha.Num();
		TargetNode->Node.IKAlpha.SetNum(CCDIK.Num());
		for (int32 i = OldNum_C; i < TargetNode->Node.IKAlpha.Num(); ++i)
		{
			TargetNode->Node.IKAlpha[i] = 1.0f;
		}
	}

	TArray<FPmxGrantEntry> Grants;
	if (ParsePmxGrantsFromPayload(GrantPayload, Grants))
	{
		TargetNode->Node.Grants = Grants;
	}

	TargetNode->ReconstructNode();

	UAnimGraphNode_Root* RootNode = nullptr;
	for (UEdGraphNode* N : AnimGraph->Nodes)
	{
		RootNode = Cast<UAnimGraphNode_Root>(N);
		if (RootNode)
		{
			break;
		}
	}
	if (RootNode)
	{
		TargetNode->NodePosX = RootNode->NodePosX;
		TargetNode->NodePosY = RootNode->NodePosY - 200;
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
	FKismetEditorUtilities::CompileBlueprint(AnimBlueprint);
	return true;
}
#else
bool UIK_LoaderImportLibrary::apply_pmx_ik_to_anim_blueprint(UAnimBlueprint* AnimBlueprint, const FString& Payload)
{
	return false;
}

bool UIK_LoaderImportLibrary::apply_pmx_grants_to_anim_blueprint(UAnimBlueprint* AnimBlueprint, const FString& Payload)
{
	return false;
}

bool UIK_LoaderImportLibrary::apply_pmx_constraints_to_anim_blueprint(UAnimBlueprint* AnimBlueprint, const FString& IKPayload, const FString& GrantPayload)
{
	return false;
}
#endif


#undef LOCTEXT_NAMESPACE
