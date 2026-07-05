#pragma once

#include "CoreMinimal.h"
#include "BoneContainer.h"
#include "PPR_Type.generated.h"

UENUM(BlueprintType)
enum class EMmdRigidBodyShape : uint8
{
    Sphere  = 0,
    Box     = 1,
    Capsule = 2,
};

UENUM(BlueprintType)
enum class EMmdRigidBodyMode : uint8
{
    FollowBone          = 0,
    PhysicsOnly         = 1,
    PhysicsAndBoneMerge = 2,
};

UENUM(BlueprintType)
enum class EMmdJointKind : uint8
{
    Spring6Dof = 0,
    SixDof     = 1,
    Point2Point = 2,
    ConeTwist  = 3,
    Slider     = 4,
    Hinge      = 5,
};

USTRUCT(BlueprintType)
struct PMXPHYSICS_RUNTIME_API FMmdRigidBodyData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMD|RigidBody")
    FName Name;

    UPROPERTY()
    FName BoneName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMD|RigidBody")
    int32 Group = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMD|RigidBody")
    int32 PassGroup = 0xFFFF;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMD|RigidBody|Shape")
    EMmdRigidBodyShape ShapeType = EMmdRigidBodyShape::Sphere;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMD|RigidBody|Shape")
    FVector ShapeSize = FVector(1.0f, 1.0f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMD|RigidBody|Transform")
    FVector Position = FVector::ZeroVector;

    UPROPERTY()
    FQuat Rotation = FQuat::Identity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMD|RigidBody|Physics", meta=(ClampMin="0"))
    float Mass = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMD|RigidBody|Physics", meta=(ClampMin="0", ClampMax="1"))
    float LinearDamping = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMD|RigidBody|Physics", meta=(ClampMin="0", ClampMax="1"))
    float AngularDamping = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMD|RigidBody|Physics", meta=(ClampMin="0", ClampMax="1"))
    float Restitution = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMD|RigidBody|Physics", meta=(ClampMin="0", ClampMax="1"))
    float Friction = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMD|RigidBody|Physics")
    EMmdRigidBodyMode Mode = EMmdRigidBodyMode::FollowBone;

    UPROPERTY()
    int32 ResolvedBoneIndex = -1;

    UPROPERTY()
    int32 ParentBoneIndex = -1;

    UPROPERTY(EditAnywhere, Category = "MMD|RigidBody")
    FBoneReference BoneRef;

    UPROPERTY()
    FTransform CachedUETransform = FTransform::Identity;

    UPROPERTY()
    FVector PMXWorldPosCS = FVector::ZeroVector;

    UPROPERTY()
    FVector BoneCSRestPos = FVector::ZeroVector;

    UPROPERTY()
    FTransform BoneToRBTransform = FTransform::Identity;
};

USTRUCT(BlueprintType)
struct PMXPHYSICS_RUNTIME_API FMmdJointData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMD|Joint")
    FName Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMD|Joint")
    EMmdJointKind Kind = EMmdJointKind::Spring6Dof;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMD|Joint")
    int32 RigidBodyIndexA = -1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMD|Joint")
    int32 RigidBodyIndexB = -1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMD|Joint|Transform")
    FVector Position = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMD|Joint|Transform")
    FQuat Rotation = FQuat::Identity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMD|Joint|Limit")
    FVector PositionMin = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMD|Joint|Limit")
    FVector PositionMax = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMD|Joint|Limit")
    FVector RotationMin = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMD|Joint|Limit")
    FVector RotationMax = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMD|Joint|Spring")
    FVector SpringPosition = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMD|Joint|Spring")
    FVector SpringRotation = FVector::ZeroVector;

    UPROPERTY()
    FTransform CachedUETransform = FTransform::Identity;

    UPROPERTY()
    FTransform OffsetFromBodyA = FTransform::Identity;
};
