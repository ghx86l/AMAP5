#pragma once

#include "Animation/AnimNodeBase.h"
#include "BonePose.h"
#include "BoneContainer.h"
#include "ReferenceSkeleton.h"
#include "AnimNode.generated.h"

USTRUCT(BlueprintType)
struct AMAP5_RUNTIME_API FPmxIKLink
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "PMX")
	FBoneReference LinkBone;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PMX")
	bool bEnableLimitX = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PMX")
	bool bEnableLimitY = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PMX")
	bool bEnableLimitZ = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PMX")
	FVector MinLimitDeg = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PMX")
	FVector MaxLimitDeg = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct AMAP5_RUNTIME_API FPmxIKChain
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PMX", meta = (DisplayName = "Bone Class"))
	int32 BoneClass = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PMX", meta = (DisplayName = "Bone Index"))
	int32 BoneIndex = 0;

	UPROPERTY(EditAnywhere, Category = "PMX")
	FBoneReference IkBone;

	UPROPERTY(EditAnywhere, Category = "PMX")
	FBoneReference TargetBone;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PMX")
	int32 LoopCount = 40;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PMX")
	float UnitAngleDeg = 114.5916f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PMX")
	TArray<FPmxIKLink> Links;
};

USTRUCT(BlueprintType)
struct AMAP5_RUNTIME_API FPmxGrantEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PMX", meta = (DisplayName = "Bone Class"))
	int32 BoneClass = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PMX", meta = (DisplayName = "Bone Index"))
	int32 BoneIndex = 0;

	UPROPERTY(EditAnywhere, Category = "PMX")
	FBoneReference BoneName;

	UPROPERTY(EditAnywhere, Category = "PMX")
	FBoneReference GrantBoneName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PMX")
	float GrantWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PMX")
	bool bGrantRotation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PMX")
	bool bGrantTranslation = false;
};

USTRUCT(BlueprintInternalUseOnly)
struct AMAP5_RUNTIME_API FAnimNode_PmxIKLoad : public FAnimNode_Base
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Links)
	FPoseLink SourcePose;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinShownByDefault))
	float Alpha = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (DisplayName = "IK alpha", PinShownByDefault))
	TArray<float> IKAlpha;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PMX", meta = (DisplayName = "CCDIK"))
	TArray<FPmxIKChain> CCDIK;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PMX")
	TArray<FPmxGrantEntry> Grants;

	FAnimNode_PmxIKLoad()
	{
	}

	static float WrapPI(float X)
	{
		const float TAU = 2.0f * PI;
		X = FMath::Fmod(X + PI, TAU);
		if (X < 0.0f)
		{
			X += TAU;
		}
		return X - PI;
	}

	template <typename T>
	static bool NearlyZero(T V)
	{
		return FMath::Abs(V) <= (T)KINDA_SMALL_NUMBER;
	}

	static bool InInterval(float X, float Lo, float Hi)
	{
		if (Lo == Hi)
		{
			return X == Lo;
		}
		if (Hi >= Lo)
		{
			return (X >= Lo && X <= Hi);
		}
		return (X >= Lo || X <= Hi);
	}

	static float ShortestArc(float A, float B)
	{
		return FMath::Abs(WrapPI(A - B));
	}

	static TPair<float, float> ClampAngle(float A, float Mi, float Ma)
	{
		A = WrapPI(A);
		Mi = WrapPI(Mi);
		Ma = WrapPI(Ma);

		if (Mi == Ma)
		{
			return { Mi, ShortestArc(A, Mi) };
		}

		if (InInterval(A, Mi, Ma))
		{
			return { A, 0.0f };
		}

		const float DMi = ShortestArc(A, Mi);
		const float DMa = ShortestArc(A, Ma);
		if (DMi < DMa)
		{
			return { Mi, DMi };
		}
		return { Ma, DMa };
	}

	static float AngleAroundAxis(const FQuat& Q, const FVector& Axis)
	{
		const FVector V(Q.X, Q.Y, Q.Z);
		return WrapPI(2.0f * FMath::Atan2(FVector::DotProduct(Axis, V), Q.W));
	}

	static FQuat LimitRotationByPmxLink(const FQuat& Q, const FPmxIKLink& Link)
	{
		if (!Link.bEnableLimitX && !Link.bEnableLimitY && !Link.bEnableLimitZ)
		{
			return Q;
		}

		const FVector AxisX(1.0f, 0.0f, 0.0f);
		const FVector AxisY(0.0f, 0.0f, 1.0f);
		const FVector AxisZ(0.0f, -1.0f, 0.0f);

		const float HiX = FMath::DegreesToRadians(Link.MaxLimitDeg.X);
		const float HiY = FMath::DegreesToRadians(Link.MaxLimitDeg.Y);
		const float HiZ = FMath::DegreesToRadians(Link.MaxLimitDeg.Z);
		const float LoX = FMath::DegreesToRadians(Link.MinLimitDeg.X);
		const float LoY = FMath::DegreesToRadians(Link.MinLimitDeg.Y);
		const float LoZ = FMath::DegreesToRadians(Link.MinLimitDeg.Z);

		const bool LockX = (!Link.bEnableLimitX);
		const bool LockY = (!Link.bEnableLimitY);
		const bool LockZ = (!Link.bEnableLimitZ);

		if (LockX && LockY && LockZ)
		{
			return FQuat::Identity;
		}

		bool bHinge = false;
		FVector HingeAxis = AxisX;
		float HingeLo = LoX;
		float HingeHi = HiX;

		if (LockX && LockY)
		{
			bHinge = true;
			HingeAxis = AxisZ;
			HingeLo = LoZ;
			HingeHi = HiZ;
		}
		else if (LockY && LockZ)
		{
			bHinge = true;
			HingeAxis = AxisX;
			HingeLo = LoX;
			HingeHi = HiX;
		}
		else if (LockX && LockZ)
		{
			bHinge = true;
			HingeAxis = AxisY;
			HingeLo = LoY;
			HingeHi = HiY;
		}

		auto HingeLambda = [&](const FVector& Axis, float L, float H) -> FQuat
		{
			float A = AngleAroundAxis(Q, Axis);
			auto Clamped = ClampAngle(A, L, H);
			float B = Clamped.Key;
			float D = Clamped.Value;
			if (D > 0.0f)
			{
				auto ClampedRev = ClampAngle(-A, L, H);
				if (ClampedRev.Value < D)
				{
					B = ClampedRev.Key;
				}
			}
			return FQuat(Axis, B);
		};

		if (bHinge)
		{
			return HingeLambda(HingeAxis, HingeLo, HingeHi).GetNormalized();
		}

		const FQuat XQ = HingeLambda(AxisX, LoX, HiX);
		const FQuat YQ = HingeLambda(AxisY, LoY, HiY);
		const FQuat ZQ = HingeLambda(AxisZ, LoZ, HiZ);

		constexpr float PSI = PI * 0.5f;
		if (LoX > -PSI && HiX < PSI)
		{
			return (ZQ * XQ * YQ).GetNormalized();
		}
		if (LoY > -PSI && HiY < PSI)
		{
			return (XQ * YQ * ZQ).GetNormalized();
		}
		return (YQ * ZQ * XQ).GetNormalized();
	}

	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override
	{
		SourcePose.Initialize(Context);
	}

	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override
	{
		SourcePose.CacheBones(Context);
	}

	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override
	{
		SourcePose.Update(Context);
	}

	static int32 FindBoneIndexByExactName(const FBoneContainer& BoneContainer, const FString& BoneNameString)
	{
		const FReferenceSkeleton& ReferenceSkeleton = BoneContainer.GetReferenceSkeleton();
		const TArray<FBoneIndexType>& BoneIndices = BoneContainer.GetBoneIndicesArray();

		for (const FBoneIndexType BoneIndex : BoneIndices)
		{
			const int32 RawBoneIndex = (int32)BoneIndex;
			if (RawBoneIndex < 0 || RawBoneIndex >= ReferenceSkeleton.GetRawBoneNum())
			{
				continue;
			}

			if (ReferenceSkeleton.GetBoneName(RawBoneIndex).ToString().Equals(BoneNameString, ESearchCase::CaseSensitive))
			{
				return RawBoneIndex;
			}
		}

		return INDEX_NONE;
	}

	static int32 FindBoneIndexByResolvedName(const FBoneContainer& BoneContainer, FName BoneName)
	{
		const FString BaseStr = BoneName.ToString();
		const int32 ExactIndex = FindBoneIndexByExactName(BoneContainer, BaseStr);
		if (ExactIndex != INDEX_NONE)
		{
			return ExactIndex;
		}

		for (int32 c = 2; c < 100; ++c)
		{
			const FString CandidateString = FString::Printf(TEXT("%s_%02d"), *BaseStr, c);
			const int32 FallbackIndex = FindBoneIndexByExactName(BoneContainer, CandidateString);
			if (FallbackIndex != INDEX_NONE)
			{
				return FallbackIndex;
			}
		}

		const int32 Index = BoneContainer.GetPoseBoneIndexForBoneName(BoneName);
		if (Index != INDEX_NONE)
		{
			return Index;
		}

		for (int32 c = 2; c < 100; ++c)
		{
			const FName Candidate = *FString::Printf(TEXT("%s_%02d"), *BaseStr, c);
			const int32 FallbackIndex = BoneContainer.GetPoseBoneIndexForBoneName(Candidate);
			if (FallbackIndex != INDEX_NONE)
			{
				return FallbackIndex;
			}
		}

		return INDEX_NONE;
	}

	static FTransform GetReferenceLocalTransform(const FBoneContainer& BoneContainer, FCompactPoseBoneIndex BoneIndex)
	{
		const FReferenceSkeleton& RefSkeleton = BoneContainer.GetReferenceSkeleton();
		const int32 SkeletonIndex = BoneContainer.GetSkeletonPoseIndexFromCompactPoseIndex(BoneIndex).GetInt();
		if (SkeletonIndex >= 0 && SkeletonIndex < RefSkeleton.GetRefBonePose().Num())
		{
			return RefSkeleton.GetRefBonePose()[SkeletonIndex];
		}
		return FTransform::Identity;
	}

	virtual void Evaluate_AnyThread(FPoseContext& Output) override
	{
		SourcePose.Evaluate(Output);

		const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
		if (ClampedAlpha <= 0.0f)
		{
			return;
		}

		FCompactPose BasePose;
		BasePose.CopyBonesFrom(Output.Pose);

		FCSPose<FCompactPose> CSPose;
		CSPose.InitPose(Output.Pose);
		const FBoneContainer& BoneContainer = Output.Pose.GetBoneContainer();

		auto ApplyGrant = [&](const FPmxGrantEntry& Grant) -> bool
		{
			if (!Grant.bGrantRotation && !Grant.bGrantTranslation)
			{
				return true;
			}

			const int32 TargetMeshIndex = FindBoneIndexByResolvedName(BoneContainer, Grant.BoneName.BoneName);
			const int32 GrantMeshIndex = FindBoneIndexByResolvedName(BoneContainer, Grant.GrantBoneName.BoneName);
			if (TargetMeshIndex == INDEX_NONE || GrantMeshIndex == INDEX_NONE)
			{
				return false;
			}

			const FCompactPoseBoneIndex TargetIndex = BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(TargetMeshIndex));
			const FCompactPoseBoneIndex GrantIndex = BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(GrantMeshIndex));
			FTransform TargetLocal = Output.Pose[TargetIndex];
			const FTransform GrantLocal = Output.Pose[GrantIndex];
			const FTransform GrantRefLocal = GetReferenceLocalTransform(BoneContainer, GrantIndex);
			const float Weight = Grant.GrantWeight;

			if (Grant.bGrantRotation)
			{
				const FQuat GrantDelta = (GrantLocal.GetRotation() * GrantRefLocal.GetRotation().Inverse()).GetNormalized();
				const FQuat GrantRotation = FQuat::Slerp(FQuat::Identity, GrantDelta, Weight).GetNormalized();
				TargetLocal.SetRotation((GrantRotation * TargetLocal.GetRotation()).GetNormalized());
			}

			if (Grant.bGrantTranslation)
			{
				const FVector GrantTranslation = GrantLocal.GetTranslation() - GrantRefLocal.GetTranslation();
				TargetLocal.SetTranslation(TargetLocal.GetTranslation() + GrantTranslation * Weight);
			}

			TargetLocal.NormalizeRotation();
			Output.Pose[TargetIndex] = TargetLocal;
			CSPose.InitPose(Output.Pose);
			return true;
		};

		auto ApplyIKChain = [&](const FPmxIKChain& Chain, int32 ChainIndex) -> bool
		{
			const int32 IkMeshIndex = FindBoneIndexByResolvedName(BoneContainer, Chain.IkBone.BoneName);
			const int32 EffectorMeshIndex = FindBoneIndexByResolvedName(BoneContainer, Chain.TargetBone.BoneName);
			if (IkMeshIndex == INDEX_NONE || EffectorMeshIndex == INDEX_NONE)
			{
				return false;
			}

			const FCompactPoseBoneIndex IkIndex = BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(IkMeshIndex));
			const FCompactPoseBoneIndex EffectorIndex = BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(EffectorMeshIndex));

			TArray<FCompactPoseBoneIndex> ChainBoneIndices;
			ChainBoneIndices.Reserve(Chain.Links.Num() + 1);
			TArray<const FPmxIKLink*> ChainLinkDefs;
			ChainLinkDefs.Reserve(Chain.Links.Num());

			bool bChainValid = true;
			for (int32 LinkIdx = Chain.Links.Num() - 1; LinkIdx >= 0; --LinkIdx)
			{
				const FPmxIKLink& Link = Chain.Links[LinkIdx];
				const int32 LinkMeshIndex = FindBoneIndexByResolvedName(BoneContainer, Link.LinkBone.BoneName);
				if (LinkMeshIndex == INDEX_NONE)
				{
					bChainValid = false;
					break;
				}
				ChainBoneIndices.Add(BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(LinkMeshIndex)));
				ChainLinkDefs.Add(&Link);
			}
			if (!bChainValid || ChainBoneIndices.Num() == 0)
			{
				return false;
			}

			ChainBoneIndices.Add(EffectorIndex);

			const FVector GoalLocation = CSPose.GetComponentSpaceTransform(IkIndex).GetLocation();

			TArray<FTransform> CSTransforms;
			CSTransforms.SetNum(ChainBoneIndices.Num());
			for (int32 i = 0; i < ChainBoneIndices.Num(); ++i)
			{
				CSTransforms[i] = CSPose.GetComponentSpaceTransform(ChainBoneIndices[i]);
			}

			const TArray<FTransform> PreIKCSTransforms = CSTransforms;

			TArray<FTransform> LocalRel;
			LocalRel.SetNum(CSTransforms.Num());
			LocalRel[0] = FTransform::Identity;
			for (int32 i = 1; i < CSTransforms.Num(); ++i)
			{
				LocalRel[i] = CSTransforms[i].GetRelativeTransform(CSTransforms[i - 1]);
				LocalRel[i].NormalizeRotation();
			}

			const FReferenceSkeleton& RefSkeleton = BoneContainer.GetReferenceSkeleton();
			TArray<FQuat> RefLocalRots;
			RefLocalRots.SetNum(CSTransforms.Num());
			for (int32 i = 0; i < ChainBoneIndices.Num(); ++i)
			{
				const int32 SkeletonIdx = BoneContainer.GetSkeletonPoseIndexFromCompactPoseIndex(ChainBoneIndices[i]).GetInt();
				if (SkeletonIdx >= 0 && SkeletonIdx < RefSkeleton.GetRefBonePose().Num())
				{
					RefLocalRots[i] = RefSkeleton.GetRefBonePose()[SkeletonIdx].GetRotation();
				}
				else
				{
					RefLocalRots[i] = FQuat::Identity;
				}
				RefLocalRots[i].Normalize();
			}

			const float UnitAngleRad = FMath::DegreesToRadians(FMath::Max(0.0f, Chain.UnitAngleDeg));
			const int32 Iterations = FMath::Max(0, Chain.LoopCount);

			for (int32 Iter = 0; Iter < Iterations; ++Iter)
			{
				const FVector EffectorLocation = CSTransforms.Last().GetLocation();
				if (FVector::DistSquared(EffectorLocation, GoalLocation) < 0.0001f)
				{
					break;
				}

				for (int32 BoneIdx = CSTransforms.Num() - 2; BoneIdx >= 0; --BoneIdx)
				{
					const FVector BoneLocation = CSTransforms[BoneIdx].GetLocation();
					const FVector CurrentEffector = CSTransforms.Last().GetLocation();

					FVector ToEffector = CurrentEffector - BoneLocation;
					FVector ToGoal = GoalLocation - BoneLocation;
					if (ToEffector.IsNearlyZero() || ToGoal.IsNearlyZero())
					{
						continue;
					}

					ToEffector.Normalize();
					ToGoal.Normalize();

					FQuat DeltaRot = FQuat::FindBetweenNormals(ToEffector, ToGoal);
					FVector Axis;
					float Angle;
					DeltaRot.ToAxisAndAngle(Axis, Angle);

					if (!FMath::IsNearlyZero(UnitAngleRad) && Angle > UnitAngleRad)
					{
						DeltaRot = FQuat(Axis, UnitAngleRad);
					}

					FQuat NewRotation = (DeltaRot * CSTransforms[BoneIdx].GetRotation()).GetNormalized();
					CSTransforms[BoneIdx].SetRotation(NewRotation);

					if (BoneIdx > 0)
					{
						LocalRel[BoneIdx] = CSTransforms[BoneIdx].GetRelativeTransform(CSTransforms[BoneIdx - 1]);
						LocalRel[BoneIdx].NormalizeRotation();
					}

					if (BoneIdx < ChainLinkDefs.Num())
					{
						const FPmxIKLink* LinkDef = ChainLinkDefs[BoneIdx];
						if (LinkDef && (LinkDef->bEnableLimitX || LinkDef->bEnableLimitY || LinkDef->bEnableLimitZ))
						{
							FTransform ParentCS;
							bool bHasParent = false;
							if (BoneIdx > 0)
							{
								ParentCS = CSTransforms[BoneIdx - 1];
								bHasParent = true;
							}
							else
							{
								const FCompactPoseBoneIndex ParentIndex = BoneContainer.GetParentBoneIndex(ChainBoneIndices[BoneIdx]);
								if (ParentIndex != INDEX_NONE)
								{
									ParentCS = CSPose.GetComponentSpaceTransform(ParentIndex);
									bHasParent = true;
								}
							}

							if (bHasParent)
							{
								FTransform Local = CSTransforms[BoneIdx].GetRelativeTransform(ParentCS);
								Local.NormalizeRotation();

								const FQuat RefLocal = RefLocalRots[BoneIdx];
								const FQuat Delta = (Local.GetRotation() * RefLocal.Inverse()).GetNormalized();
								const FQuat ClampedDelta = LimitRotationByPmxLink(Delta, *LinkDef);
								const FQuat ClampedLocalRot = (ClampedDelta * RefLocal).GetNormalized();
								Local.SetRotation(ClampedLocalRot);
								Local.NormalizeRotation();
								CSTransforms[BoneIdx] = Local * ParentCS;
								CSTransforms[BoneIdx].NormalizeRotation();
								if (BoneIdx > 0)
								{
									LocalRel[BoneIdx] = Local;
								}
							}
						}
					}

					for (int32 ChildIdx = BoneIdx + 1; ChildIdx < CSTransforms.Num(); ++ChildIdx)
					{
						CSTransforms[ChildIdx] = LocalRel[ChildIdx] * CSTransforms[ChildIdx - 1];
						CSTransforms[ChildIdx].NormalizeRotation();
					}
				}
			}

			const float ChainAlpha = IKAlpha.IsValidIndex(ChainIndex) ? FMath::Clamp(IKAlpha[ChainIndex], 0.0f, 1.0f) : 1.0f;
			if (ChainAlpha < 1.0f)
			{
				for (int32 i = 0; i < CSTransforms.Num(); ++i)
				{
					FTransform Blended = PreIKCSTransforms[i];
					Blended.BlendWith(CSTransforms[i], ChainAlpha);
					Blended.NormalizeRotation();
					CSTransforms[i] = Blended;
				}
			}

			for (int32 i = 0; i < ChainBoneIndices.Num(); ++i)
			{
				CSPose.SetComponentSpaceTransform(ChainBoneIndices[i], CSTransforms[i]);
			}

			FCSPose<FCompactPose>::ConvertComponentPosesToLocalPosesSafe(CSPose, Output.Pose);
			CSPose.InitPose(Output.Pose);
			return true;
		};

		struct FConstraintEntry
		{
			int32 BoneClass = 0;
			int32 BoneIndex = 0;
			int32 EvalType = 0;
			int32 SourceIndex = 0;
		};

		TArray<FConstraintEntry> ConstraintEntries;
		ConstraintEntries.Reserve(Grants.Num() + CCDIK.Num());
		for (int32 i = 0; i < Grants.Num(); ++i)
		{
			const FPmxGrantEntry& Grant = Grants[i];
			if (Grant.bGrantRotation || Grant.bGrantTranslation)
			{
				ConstraintEntries.Add({ Grant.BoneClass, Grant.BoneIndex, 0, i });
			}
		}
		for (int32 i = 0; i < CCDIK.Num(); ++i)
		{
			const FPmxIKChain& Chain = CCDIK[i];
			ConstraintEntries.Add({ Chain.BoneClass, Chain.BoneIndex, 1, i });
		}

		ConstraintEntries.Sort([](const FConstraintEntry& A, const FConstraintEntry& B)
		{
			if (A.BoneClass != B.BoneClass)
			{
				return A.BoneClass < B.BoneClass;
			}
			if (A.BoneIndex != B.BoneIndex)
			{
				return A.BoneIndex < B.BoneIndex;
			}
			return A.EvalType < B.EvalType;
		});

		TSet<int32> FailedBoneClasses;
		for (const FConstraintEntry& Entry : ConstraintEntries)
		{
			if (FailedBoneClasses.Contains(Entry.BoneClass))
			{
				continue;
			}
			bool bSuccess = false;
			if (Entry.EvalType == 0)
			{
				bSuccess = ApplyGrant(Grants[Entry.SourceIndex]);
			}
			else
			{
				bSuccess = ApplyIKChain(CCDIK[Entry.SourceIndex], Entry.SourceIndex);
			}
			if (!bSuccess)
			{
				FailedBoneClasses.Add(Entry.BoneClass);
			}
		}

		FCSPose<FCompactPose>::ConvertComponentPosesToLocalPosesSafe(CSPose, Output.Pose);

		if (ClampedAlpha < 1.0f)
		{
			FCompactPose IKPose;
			IKPose.CopyBonesFrom(Output.Pose);
			Output.Pose.CopyBonesFrom(BasePose);
			for (const FCompactPoseBoneIndex BoneIndex : Output.Pose.ForEachBoneIndex())
			{
				FTransform T = Output.Pose[BoneIndex];
				T.BlendWith(IKPose[BoneIndex], ClampedAlpha);
				T.NormalizeRotation();
				Output.Pose[BoneIndex] = T;
			}
		}
	}


	virtual void GatherDebugData(FNodeDebugData& DebugData) override
	{
		FString DebugLine = DebugData.GetNodeName(this);
		DebugData.AddDebugItem(DebugLine);
		SourcePose.GatherDebugData(DebugData);
	}
};
