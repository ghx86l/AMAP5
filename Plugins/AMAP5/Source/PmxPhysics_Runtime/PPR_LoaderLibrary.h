#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PPR_Type.h"
#include "PPR_LoaderLibrary.generated.h"

class UAnimBlueprint;

UCLASS()
class PMXPHYSICS_RUNTIME_API UPPR_LoaderLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "MMD Physics|Loader",
              meta = (DisplayName = "Apply PMX Physics To AnimBlueprint"))
    static bool ApplyPmxPhysicsToAnimBlueprint(UAnimBlueprint* AnimBlueprint,
                                                const FString& JsonString);

    UFUNCTION(BlueprintCallable, Category = "MMD Physics|Loader",
              meta = (DisplayName = "Apply PMX Joints To AnimBlueprint"))
    static bool ApplyPmxJointsToAnimBlueprint(UAnimBlueprint* AnimBlueprint,
                                              const FString& JointJsonString);

    UFUNCTION(BlueprintCallable, Category = "MMD Physics|Loader",
              meta = (DisplayName = "Apply PMX Physics And Joints To AnimBlueprint"))
    static bool ApplyPmxPhysicsAndJointsToAnimBlueprint(UAnimBlueprint* AnimBlueprint,
                                                        const FString& RigidBodyJsonString,
                                                        const FString& JointJsonString);

    UFUNCTION(BlueprintCallable, Category = "MMD Physics|Loader")
    static bool ParseRigidBodyJson(const FString& JsonString,
                                   TArray<FMmdRigidBodyData>& OutRigidBodies);

    UFUNCTION(BlueprintCallable, Category = "MMD Physics|Loader")
    static bool ParseJointJson(const FString& JsonString,
                               TArray<FMmdJointData>& OutJoints);

private:
    static bool JsonObjectToRigidBody(const TSharedPtr<class FJsonObject>& JsonObj,
                                      FMmdRigidBodyData& OutData);
    static bool JsonObjectToJoint(const TSharedPtr<class FJsonObject>& JsonObj,
                                  FMmdJointData& OutData);
};
