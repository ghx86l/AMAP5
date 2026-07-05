#include "PPR_LoaderLibrary.h"
#include "PPR_Type.h"
#include "PPR_EditorBridge.h"
#include "Json.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogMMDPhysicsLoader, Log, All);

static bool JsonArrayToVector(const TArray<TSharedPtr<FJsonValue>>* Arr, FVector& Out)
{
    if (!Arr || Arr->Num() < 3) return false;
    Out = FVector((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber(), (*Arr)[2]->AsNumber());
    return true;
}

static bool TryGetIntField(const TSharedPtr<FJsonObject>& JsonObj,
                           const TCHAR* PrimaryName,
                           const TCHAR* SecondaryName,
                           int32& OutValue)
{
    return JsonObj->TryGetNumberField(PrimaryName, OutValue) ||
           JsonObj->TryGetNumberField(SecondaryName, OutValue);
}

static bool TryGetVectorField(const TSharedPtr<FJsonObject>& JsonObj,
                              const TCHAR* PrimaryName,
                              const TCHAR* SecondaryName,
                              FVector& OutValue)
{
    const TArray<TSharedPtr<FJsonValue>>* ArrVal = nullptr;
    if (JsonObj->TryGetArrayField(PrimaryName, ArrVal))
    {
        return JsonArrayToVector(ArrVal, OutValue);
    }
    if (JsonObj->TryGetArrayField(SecondaryName, ArrVal))
    {
        return JsonArrayToVector(ArrVal, OutValue);
    }
    return false;
}

bool UPPR_LoaderLibrary::JsonObjectToRigidBody(
    const TSharedPtr<FJsonObject>& JsonObj,
    FMmdRigidBodyData& OutData)
{
    if (!JsonObj.IsValid()) return false;

    FString Str;
    if (JsonObj->TryGetStringField(TEXT("name"), Str))      OutData.Name     = FName(*Str);
    if (JsonObj->TryGetStringField(TEXT("bone_name"), Str)) { OutData.BoneName = FName(*Str); OutData.BoneRef.BoneName = FName(*Str); }

    int32 IntVal = -1;
    if (JsonObj->TryGetNumberField(TEXT("group"),       IntVal)) OutData.Group     = IntVal;
    if (JsonObj->TryGetNumberField(TEXT("pass_group"),  IntVal)) OutData.PassGroup = IntVal;
    if (JsonObj->TryGetNumberField(TEXT("shape_type"),  IntVal))
        OutData.ShapeType = static_cast<EMmdRigidBodyShape>(FMath::Clamp(IntVal, 0, 2));

    const TArray<TSharedPtr<FJsonValue>>* ArrVal = nullptr;
    if (JsonObj->TryGetArrayField(TEXT("shape_size"), ArrVal)) JsonArrayToVector(ArrVal, OutData.ShapeSize);
    if (JsonObj->TryGetArrayField(TEXT("position"),   ArrVal)) JsonArrayToVector(ArrVal, OutData.Position);
    if (JsonObj->TryGetArrayField(TEXT("rotation"),   ArrVal) && ArrVal->Num() >= 4)
        OutData.Rotation = FQuat(
            (float)(*ArrVal)[0]->AsNumber(),
            (float)(*ArrVal)[1]->AsNumber(),
            (float)(*ArrVal)[2]->AsNumber(),
            (float)(*ArrVal)[3]->AsNumber());

    double Dbl = 0.0;
    if (JsonObj->TryGetNumberField(TEXT("mass"),            Dbl)) OutData.Mass           = (float)Dbl;
    if (JsonObj->TryGetNumberField(TEXT("linear_damping"),  Dbl)) OutData.LinearDamping  = (float)Dbl;
    if (JsonObj->TryGetNumberField(TEXT("angular_damping"), Dbl)) OutData.AngularDamping = (float)Dbl;
    if (JsonObj->TryGetNumberField(TEXT("restitution"),     Dbl)) OutData.Restitution    = (float)Dbl;
    if (JsonObj->TryGetNumberField(TEXT("friction"),        Dbl)) OutData.Friction       = (float)Dbl;

    if (JsonObj->TryGetNumberField(TEXT("mode"), IntVal))
        OutData.Mode = static_cast<EMmdRigidBodyMode>(FMath::Clamp(IntVal, 0, 2));

    return true;
}

bool UPPR_LoaderLibrary::JsonObjectToJoint(
    const TSharedPtr<FJsonObject>& JsonObj,
    FMmdJointData& OutData)
{
    if (!JsonObj.IsValid()) return false;

    FString Str;
    if (JsonObj->TryGetStringField(TEXT("name"), Str)) OutData.Name = FName(*Str);

    int32 IntVal = -1;
    if (TryGetIntField(JsonObj, TEXT("type"), TEXT("kind"), IntVal))
        OutData.Kind = static_cast<EMmdJointKind>(FMath::Clamp(IntVal, 0, 5));
    TryGetIntField(JsonObj, TEXT("rigidbody_index_a"), TEXT("body_a"), OutData.RigidBodyIndexA);
    TryGetIntField(JsonObj, TEXT("rigidbody_index_b"), TEXT("body_b"), OutData.RigidBodyIndexB);

    TryGetVectorField(JsonObj, TEXT("position"), TEXT("position"), OutData.Position);
    const TArray<TSharedPtr<FJsonValue>>* RotationValues = nullptr;
    if (JsonObj->TryGetArrayField(TEXT("rotation"), RotationValues) && RotationValues->Num() >= 4)
    {
        OutData.Rotation = FQuat(
            (float)(*RotationValues)[0]->AsNumber(),
            (float)(*RotationValues)[1]->AsNumber(),
            (float)(*RotationValues)[2]->AsNumber(),
            (float)(*RotationValues)[3]->AsNumber());
    }
    TryGetVectorField(JsonObj, TEXT("position_min"), TEXT("move_lo"), OutData.PositionMin);
    TryGetVectorField(JsonObj, TEXT("position_max"), TEXT("move_hi"), OutData.PositionMax);
    TryGetVectorField(JsonObj, TEXT("rotation_min"), TEXT("angle_lo"), OutData.RotationMin);
    TryGetVectorField(JsonObj, TEXT("rotation_max"), TEXT("angle_hi"), OutData.RotationMax);
    TryGetVectorField(JsonObj, TEXT("spring_position"), TEXT("spring_move"), OutData.SpringPosition);
    TryGetVectorField(JsonObj, TEXT("spring_rotation"), TEXT("spring_rotate"), OutData.SpringRotation);

    return true;
}

static bool ParseJsonArray(
    const FString& JsonString,
    const TCHAR* Label,
    TFunction<bool(const TSharedPtr<FJsonObject>&)> PerElement)
{
    TArray<TSharedPtr<FJsonValue>> JsonArray;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, JsonArray))
    {
        UE_LOG(LogMMDPhysicsLoader, Error, TEXT("Parse%s: JSON deserialization failed."), Label);
        return false;
    }
    for (const TSharedPtr<FJsonValue>& Val : JsonArray)
    {
        const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
        if (!Val->TryGetObject(ObjPtr) || !ObjPtr) continue;
        PerElement(*ObjPtr);
    }
    return true;
}

bool UPPR_LoaderLibrary::ParseRigidBodyJson(const FString& JsonString,
                                             TArray<FMmdRigidBodyData>& OutRigidBodies)
{
    OutRigidBodies.Reset();
    bool bOk = ParseJsonArray(JsonString, TEXT("RigidBodyJson"),
        [&](const TSharedPtr<FJsonObject>& Obj) -> bool
        {
            FMmdRigidBodyData RB;
            if (JsonObjectToRigidBody(Obj, RB)) OutRigidBodies.Add(MoveTemp(RB));
            return true;
        });
    UE_LOG(LogMMDPhysicsLoader, Log, TEXT("ParseRigidBodyJson: parsed %d bodies."), OutRigidBodies.Num());
    return bOk;
}

bool UPPR_LoaderLibrary::ParseJointJson(const FString& JsonString,
                                        TArray<FMmdJointData>& OutJoints)
{
    OutJoints.Reset();
    bool bOk = ParseJsonArray(JsonString, TEXT("JointJson"),
        [&](const TSharedPtr<FJsonObject>& Obj) -> bool
        {
            FMmdJointData Joint;
            if (JsonObjectToJoint(Obj, Joint)) OutJoints.Add(MoveTemp(Joint));
            return true;
        });
    UE_LOG(LogMMDPhysicsLoader, Log, TEXT("ParseJointJson: parsed %d joints."), OutJoints.Num());
    return bOk;
}

bool UPPR_LoaderLibrary::ApplyPmxPhysicsToAnimBlueprint(
    UAnimBlueprint* AnimBlueprint,
    const FString& JsonString)
{
    if (!AnimBlueprint)
    {
        UE_LOG(LogMMDPhysicsLoader, Error, TEXT("ApplyPmxPhysicsToAnimBlueprint: AnimBlueprint is null."));
        return false;
    }

    TArray<FMmdRigidBodyData> ParsedBodies;
    if (!ParseRigidBodyJson(JsonString, ParsedBodies)) return false;

#if WITH_EDITOR
    static const FName EditorModuleName = TEXT("PmxPhysics_Editor");
    IPPR_EditorBridge& Bridge = FModuleManager::LoadModuleChecked<IPPR_EditorBridge>(EditorModuleName);
    return Bridge.WriteRigidBodiesToAnimGraph(AnimBlueprint, ParsedBodies);
#else
    return ParsedBodies.Num() > 0;
#endif
}

bool UPPR_LoaderLibrary::ApplyPmxJointsToAnimBlueprint(
    UAnimBlueprint* AnimBlueprint,
    const FString& JointJsonString)
{
    if (!AnimBlueprint)
    {
        UE_LOG(LogMMDPhysicsLoader, Error, TEXT("ApplyPmxJointsToAnimBlueprint: AnimBlueprint is null."));
        return false;
    }

    TArray<FMmdJointData> ParsedJoints;
    if (!ParseJointJson(JointJsonString, ParsedJoints)) return false;

#if WITH_EDITOR
    static const FName EditorModuleName = TEXT("PmxPhysics_Editor");
    IPPR_EditorBridge& Bridge = FModuleManager::LoadModuleChecked<IPPR_EditorBridge>(EditorModuleName);
    return Bridge.WriteJointsToAnimGraph(AnimBlueprint, ParsedJoints);
#else
    return ParsedJoints.Num() > 0;
#endif
}

bool UPPR_LoaderLibrary::ApplyPmxPhysicsAndJointsToAnimBlueprint(
    UAnimBlueprint* AnimBlueprint,
    const FString& RigidBodyJsonString,
    const FString& JointJsonString)
{
    if (!AnimBlueprint)
    {
        UE_LOG(LogMMDPhysicsLoader, Error, TEXT("ApplyPmxPhysicsAndJointsToAnimBlueprint: AnimBlueprint is null."));
        return false;
    }

    TArray<FMmdRigidBodyData> ParsedBodies;
    if (!ParseRigidBodyJson(RigidBodyJsonString, ParsedBodies)) return false;

    TArray<FMmdJointData> ParsedJoints;
    if (!JointJsonString.IsEmpty() && !ParseJointJson(JointJsonString, ParsedJoints)) return false;

#if WITH_EDITOR
    static const FName EditorModuleName = TEXT("PmxPhysics_Editor");
    IPPR_EditorBridge& Bridge = FModuleManager::LoadModuleChecked<IPPR_EditorBridge>(EditorModuleName);
    return Bridge.WritePhysicsToAnimGraph(AnimBlueprint, ParsedBodies, ParsedJoints);
#else
    return ParsedBodies.Num() > 0;
#endif
}
