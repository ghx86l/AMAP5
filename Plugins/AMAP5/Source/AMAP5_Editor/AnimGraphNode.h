#pragma once

#include "AnimGraphNode_Base.h"
#include "AnimNode.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AnimGraphNode.generated.h"

UCLASS()
class AMAP5_EDITOR_API UIK_LoaderAnimGraphNode_PmxIKLoad : public UAnimGraphNode_Base
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = Settings)
    FAnimNode_PmxIKLoad Node;

    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    virtual FText GetTooltipText() const override;
    virtual FText GetMenuCategory() const override;
    virtual FString GetNodeCategory() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
    virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
    virtual void CustomizePinData(UEdGraphPin* Pin, FName SourcePropertyName, int32 ArrayIndex) const override;
};


UCLASS()
class AMAP5_EDITOR_API UIK_LoaderImportLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "IK_Loader")
	static bool apply_pmx_ik_to_anim_blueprint(class UAnimBlueprint* AnimBlueprint, const FString& Payload);

	UFUNCTION(BlueprintCallable, Category = "IK_Loader")
	static bool apply_pmx_grants_to_anim_blueprint(class UAnimBlueprint* AnimBlueprint, const FString& Payload);

	UFUNCTION(BlueprintCallable, Category = "IK_Loader")
	static bool apply_pmx_constraints_to_anim_blueprint(class UAnimBlueprint* AnimBlueprint, const FString& IKPayload, const FString& GrantPayload);
};
