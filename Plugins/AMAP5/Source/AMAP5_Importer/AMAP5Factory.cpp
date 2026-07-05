#include "AMAP5Factory.h"

#include "Reader.h"
#include "Structs.h"
#include "Utils.h"
#include "Log.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimInstance.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "UObject/SoftObjectPath.h"
#include "BoneWeights.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/Blueprint.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetCommon.h"
#include "IImageWrapperModule.h"
#include "ImageCore.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "MeshDescription.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ReferenceSkeleton.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Animation/MorphTarget.h"
#include "Misc/EngineVersionComparison.h"
#include "SkeletalMeshAttributes.h"
#include "StaticMeshAttributes.h"
#include "UObject/Package.h"
#include "Framework/Application/SlateApplication.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"
#if UE_VERSION_OLDER_THAN(5,3,0)
#else
#include "InterchangeHelper.h"
#endif

#define LOCTEXT_NAMESPACE "AMAP5Factory"

#if UE_VERSION_OLDER_THAN(5,2,0)
typedef uint8 AMAP5_BONE_WEIGHT_TYPE;
#define AMAP5_MAX_BONE_WEIGHT_F 255.0f
#else
typedef uint16 AMAP5_BONE_WEIGHT_TYPE;
#define AMAP5_MAX_BONE_WEIGHT_F 65535.0f
#endif

class SAMAP5OptionWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAMAP5OptionWindow) {}
		SLATE_ARGUMENT(UAMAP5Factory*, Factory)
		SLATE_ARGUMENT(TSharedPtr<SWindow>, WidgetWindow)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		WidgetWindow = InArgs._WidgetWindow;

		FPropertyEditorModule& PropertyEditorModule =
			FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.bAllowSearch = false;
		DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
		DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
		DetailsView->SetObject(InArgs._Factory);

		ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.0f)
			[
				SNew(SBox)
				.WidthOverride(420.0f)
				[
					DetailsView->AsShared()
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(8.0f)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(4.0f)
				+ SUniformGridPanel::Slot(0, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("Import", "Import"))
					.OnClicked(this, &SAMAP5OptionWindow::OnImport)
				]
				+ SUniformGridPanel::Slot(1, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("Cancel", "Cancel"))
					.OnClicked(this, &SAMAP5OptionWindow::OnCancel)
				]
			]
		];
	}

	bool ShouldImport() const
	{
		return bShouldImport;
	}

private:
	FReply OnImport()
	{
		bShouldImport = true;
		if (WidgetWindow.IsValid())
		{
			WidgetWindow.Pin()->RequestDestroyWindow();
		}
		return FReply::Handled();
	}

	FReply OnCancel()
	{
		bShouldImport = false;
		if (WidgetWindow.IsValid())
		{
			WidgetWindow.Pin()->RequestDestroyWindow();
		}
		return FReply::Handled();
	}

	TSharedPtr<IDetailsView> DetailsView;
	TWeakPtr<SWindow> WidgetWindow;
	bool bShouldImport = false;
};

namespace
{
	FString MakeAssetName(const FString& In, const FString& Fallback)
	{
		FString Name = FPmxUtils::SanitizePackagePath(In.IsEmpty() ? Fallback : In);
		Name.ReplaceInline(TEXT("."), TEXT("_"));
		return Name.IsEmpty() ? Fallback : Name;
	}

	UPackage* MakeAssetPackage(const FString& ParentPackageName, const FString& AssetName)
	{
		const FString Folder = FPackageName::GetLongPackagePath(ParentPackageName);
		return CreatePackage(*(Folder / AssetName));
	}

	FVector ConvertPmxPosition(const FVector3f& P, float ImportScale)
	{
		const FTransform MMDToUE(
			FQuat(FVector3d::XAxisVector, FMath::DegreesToRadians(90.0)),
			FVector3d::ZeroVector,
			FVector3d(ImportScale));
		return MMDToUE.TransformPosition(FVector(P.X, P.Y, P.Z));
	}

	FVector ConvertPmxDirection(const FVector3f& D)
	{
		const FQuat MMDToUE(FVector3d::XAxisVector, FMath::DegreesToRadians(90.0));
		return MMDToUE.RotateVector(FVector(D.X, D.Y, D.Z));
	}

	FVector ConvertPmxDelta(const FVector3f& D, float ImportScale)
	{
		const FQuat MMDToUE(FVector3d::XAxisVector, FMath::DegreesToRadians(90.0));
		return MMDToUE.RotateVector(FVector(D.X, D.Y, D.Z)) * ImportScale;
	}

	TArray<FString> BuildBoneNames(const FPmxModel& Model)
	{
		TArray<FString> Names;
		Names.Add(TEXT("Root"));
		Names.Append(FPmxUtils::BuildUniqueBoneNames(Model));
		return Names;
	}


	void RegisterAsset(UObject* Asset)
	{
		if (!Asset) return;
		FAssetRegistryModule::AssetCreated(Asset);
		Asset->MarkPackageDirty();
		if (UPackage* Package = Asset->GetOutermost())
		{
			Package->MarkPackageDirty();
		}
	}

	FString ResolveTextureFile(const FString& PmxFile, const FString& TexturePath)
	{
		FString Path = TexturePath;
		Path.ReplaceInline(TEXT("\\"), TEXT("/"));
		Path.TrimStartAndEndInline();
		if (Path.IsEmpty()) return FString();

		if (!FPaths::IsRelative(Path) && FPaths::FileExists(Path))
		{
			return Path;
		}

		const FString BaseDir = FPaths::GetPath(PmxFile);
		FString Candidate = FPaths::ConvertRelativePathToFull(BaseDir / Path);
		FPaths::NormalizeFilename(Candidate);
		return FPaths::FileExists(Candidate) ? Candidate : FString();
	}

	UTexture2D* CreateTextureAsset(const FString& ParentPackageName, const FString& AssetName, const FString& TextureFile, bool& bOutHasAlpha)
	{
		bOutHasAlpha = false;
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *TextureFile) || Bytes.IsEmpty()) { return nullptr; }
		IImageWrapperModule& Wrapper = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		const EImageFormat Format = Wrapper.DetectImageFormat(Bytes.GetData(), Bytes.Num());
		FImage Image;
		if (Format == EImageFormat::Invalid || !Wrapper.DecompressImage(Bytes.GetData(), Bytes.Num(), Image))
		{
			UE_LOG(LogPMXImporter, Error, TEXT("AMAP5 Factory: texture decode failed: %s"), *TextureFile);
			return nullptr;
		}
		Image.ChangeFormat(ERawImageFormat::BGRA8, EGammaSpace::sRGB);
		const int64 PixelCount = static_cast<int64>(Image.SizeX) * Image.SizeY;
		const int64 ExpectedByteCount = PixelCount * static_cast<int64>(sizeof(FColor));
		if (Image.SizeX <= 0 || Image.SizeY <= 0 || Image.NumSlices != 1 || Image.RawData.Num() < ExpectedByteCount) { return nullptr; }
		FColor* Pixels = reinterpret_cast<FColor*>(Image.RawData.GetData());
		bool bAnyNonZeroAlpha = false;
		for (int64 Index = 0; Index < PixelCount; ++Index)
		{
			bOutHasAlpha |= Pixels[Index].A != 255;
			bAnyNonZeroAlpha |= Pixels[Index].A != 0;
		}
		if (bOutHasAlpha && !bAnyNonZeroAlpha)
		{
			for (int64 Index = 0; Index < PixelCount; ++Index)
			{
				Pixels[Index].A = 255;
			}
			bOutHasAlpha = false;
			UE_LOG(LogPMXImporter, Log, TEXT("AMAP5 Factory: normalized unused all-zero alpha: %s"), *TextureFile);
		}
		UPackage* Package = MakeAssetPackage(ParentPackageName, AssetName);
		UTexture2D* Texture = FindObject<UTexture2D>(Package, *AssetName);
		const bool bNew = Texture == nullptr;
		if (!Texture) { Texture = NewObject<UTexture2D>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional); }
		if (!Texture) { return nullptr; }
		Texture->Modify();
		Texture->PreEditChange(nullptr);
		Texture->SRGB = true;
		Texture->CompressionSettings = TC_Default;
		Texture->AddressX = TA_Wrap;
		Texture->AddressY = TA_Wrap;
#if WITH_EDITORONLY_DATA
		Texture->CompressionNoAlpha = !bOutHasAlpha;
		Texture->DeferCompression = true;
		Texture->MipGenSettings = FMath::IsPowerOfTwo(Image.SizeX) && FMath::IsPowerOfTwo(Image.SizeY) ? TMGS_FromTextureGroup : TMGS_NoMipmaps;
		Texture->Source.Init(Image.SizeX, Image.SizeY, 1, 1, ETextureSourceFormat::TSF_BGRA8, Image.RawData.GetData());
#endif
		if (!Texture->AssetImportData) { Texture->AssetImportData = NewObject<UAssetImportData>(Texture, TEXT("AssetImportData")); }
		Texture->AssetImportData->Update(TextureFile);
		Texture->PostEditChange();
		Texture->UpdateResource();
		if (bNew) { RegisterAsset(Texture); } else { Texture->MarkPackageDirty(); Package->MarkPackageDirty(); }
		return Texture;
	}

	void SetTextureParameters(UMaterialInstanceConstant* MI, UTexture* Texture, const TArray<FName>& ParamNames)
	{
		if (!MI || !Texture)
		{
			return;
		}

		for (const FName& ParamName : ParamNames)
		{
			MI->SetTextureParameterValueEditorOnly(ParamName, Texture);
		}
	}


	UMaterialInstanceConstant* CreateMaterialAsset(
		const FString& ParentPackageName,
		const FString& AssetName,
		const FPmxMaterial& PmxMaterial,
		UTexture2D* BaseTexture,
		UTexture2D* SphereTexture,
		EAMAP5ImportMaterialType MaterialType,
		bool bBaseTextureHasAlpha)
	{
		UPackage* Package = MakeAssetPackage(ParentPackageName, AssetName);
		UMaterialInstanceConstant* MI = NewObject<UMaterialInstanceConstant>(
			Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
		if (!MI) return nullptr;

		const bool bTwoSidedForParent = ((PmxMaterial.DrawingFlags & 0x01) != 0);
		const float OpacityForParent = FMath::Clamp(PmxMaterial.Diffuse.A, 0.0f, 1.0f);
		const bool bTranslucentForParent = OpacityForParent < 0.999f;
		const bool bMaskedForParent = !bTranslucentForParent && bBaseTextureHasAlpha;
		UE_LOG(LogPMXImporter, Log, TEXT("[Factory.MaterialFlags] Name='%s' DrawingFlags=0x%02X TwoSided=%d Opacity=%.3f Translucent=%d Masked=%d"),
			*PmxMaterial.Name, (int32)PmxMaterial.DrawingFlags, (bTwoSidedForParent ? 1 : 0), OpacityForParent, (bTranslucentForParent ? 1 : 0), (bMaskedForParent ? 1 : 0));
		const TCHAR* ParentMaterialPath = nullptr;
		switch (MaterialType)
		{
		case EAMAP5ImportMaterialType::MToonLit:
			if (bTranslucentForParent)
			{
				ParentMaterialPath = bTwoSidedForParent
					? TEXT("/VRM4U/MaterialUtil/UE5/Material/MI_VrmMToonOptLitTranslucentTwoSided.MI_VrmMToonOptLitTranslucentTwoSided")
					: TEXT("/VRM4U/MaterialUtil/UE5/Material/MI_VrmMToonOptLitTranslucent.MI_VrmMToonOptLitTranslucent");
			}
			else
			{
				ParentMaterialPath = bTwoSidedForParent
					? TEXT("/VRM4U/MaterialUtil/UE5/Material/MI_VrmMToonOptLitOpaqueTwoSided.MI_VrmMToonOptLitOpaqueTwoSided")
					: TEXT("/VRM4U/MaterialUtil/UE5/Material/MI_VrmMToonOptLitOpaque.MI_VrmMToonOptLitOpaque");
			}
			break;

		case EAMAP5ImportMaterialType::MToonUnlit:
			if (bTranslucentForParent)
			{
				ParentMaterialPath = bTwoSidedForParent
					? TEXT("/VRM4U/MaterialUtil/UE5/Material/MI_VrmMToonOptUnlitTranslucentTwoSided.MI_VrmMToonOptUnlitTranslucentTwoSided")
					: TEXT("/VRM4U/MaterialUtil/UE5/Material/MI_VrmMToonOptUnlitTranslucent.MI_VrmMToonOptUnlitTranslucent");
			}
			else
			{
				ParentMaterialPath = bTwoSidedForParent
					? TEXT("/VRM4U/MaterialUtil/UE5/Material/MI_VrmMToonOptUnlitOpaqueTwoSided.MI_VrmMToonOptUnlitOpaqueTwoSided")
					: TEXT("/VRM4U/MaterialUtil/UE5/Material/MI_VrmMToonOptUnlitOpaque.MI_VrmMToonOptUnlitOpaque");
			}
			break;

		case EAMAP5ImportMaterialType::Subsurface:
			ParentMaterialPath = bTwoSidedForParent
				? TEXT("/VRM4U/MaterialUtil/UE5/Material/MI_VrmMToonOptSSSTwoSided.MI_VrmMToonOptSSSTwoSided")
				: TEXT("/VRM4U/MaterialUtil/UE5/Material/MI_VrmMToonOptSSS.MI_VrmMToonOptSSS");
			break;

		case EAMAP5ImportMaterialType::Unlit:
			ParentMaterialPath = bTranslucentForParent
				? TEXT("/VRM4U/MaterialUtil/MI_VrmUnlitTransparent.MI_VrmUnlitTransparent")
				: TEXT("/VRM4U/MaterialUtil/MI_VrmUnlit.MI_VrmUnlit");
			break;

		case EAMAP5ImportMaterialType::DefaultSubsurfaceProfile:
		case EAMAP5ImportMaterialType::SubsurfaceProfile:
		default:
			ParentMaterialPath = bTwoSidedForParent
				? TEXT("/VRM4U/MaterialUtil/UE5/Material/MI_VrmMToonOptSSSProfileTwoSided.MI_VrmMToonOptSSSProfileTwoSided")
				: TEXT("/VRM4U/MaterialUtil/UE5/Material/MI_VrmMToonOptSSSProfile.MI_VrmMToonOptSSSProfile");
			break;
		}
		UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, ParentMaterialPath);
		if (!Parent)
		{
			UE_LOG(LogPMXImporter, Error, TEXT("AMAP5 Factory: VRM4U parent material not found: %s"), ParentMaterialPath);
			Parent = UMaterial::GetDefaultMaterial(MD_Surface);
		}
		MI->SetParentEditorOnly(Parent);
		MI->BasePropertyOverrides.bOverride_TwoSided = true;
		MI->BasePropertyOverrides.TwoSided = bTwoSidedForParent;
		MI->BasePropertyOverrides.bOverride_BlendMode = true;
		if (bMaskedForParent)
		{
			MI->BasePropertyOverrides.BlendMode = EBlendMode::BLEND_Masked;
			MI->BasePropertyOverrides.bOverride_OpacityMaskClipValue = true;
			MI->BasePropertyOverrides.OpacityMaskClipValue = 0.5f;
		}
		else
		{
			MI->BasePropertyOverrides.BlendMode = bTranslucentForParent ? EBlendMode::BLEND_Translucent : EBlendMode::BLEND_Opaque;
		}

		const FLinearColor BaseColor = PmxMaterial.Diffuse;
		const FLinearColor ShadeColor(
			FMath::Clamp(PmxMaterial.Ambient.X, 0.0f, 1.0f),
			FMath::Clamp(PmxMaterial.Ambient.Y, 0.0f, 1.0f),
			FMath::Clamp(PmxMaterial.Ambient.Z, 0.0f, 1.0f),
			1.0f);
		const TArray<FName> ColorNames = {
			TEXT("BaseColor"), TEXT("DiffuseColor"), TEXT("Color"), TEXT("mtoon_Color"), TEXT("BaseColorTint"), TEXT("gltf_basecolor")
		};
		for (const FName& Param : ColorNames)
		{
			MI->SetVectorParameterValueEditorOnly(Param, BaseColor);
		}
		MI->SetVectorParameterValueEditorOnly(TEXT("mtoon_ShadeColor"), ShadeColor);
		MI->SetVectorParameterValueEditorOnly(TEXT("mtoon_OutlineColor"), PmxMaterial.EdgeColor);
		MI->SetVectorParameterValueEditorOnly(TEXT("pmx.edge.color"), PmxMaterial.EdgeColor);
		MI->SetVectorParameterValueEditorOnly(TEXT("pmx.specular.rgb"), FLinearColor(PmxMaterial.Specular.X, PmxMaterial.Specular.Y, PmxMaterial.Specular.Z, 1.0f));
		MI->SetVectorParameterValueEditorOnly(TEXT("pmx.ambient.rgb"), FLinearColor(PmxMaterial.Ambient.X, PmxMaterial.Ambient.Y, PmxMaterial.Ambient.Z, 1.0f));

		SetTextureParameters(MI, BaseTexture, {
			TEXT("BaseColorTexture"), TEXT("MainTexture"), TEXT("DiffuseTexture"),
			TEXT("mtoon_tex_MainTex"), TEXT("gltf_tex_diffuse")
		});
		if (SphereTexture && PmxMaterial.SphereMode != 0)
		{
			SetTextureParameters(MI, SphereTexture, {
				TEXT("mtoon_tex_SphereAdd"), TEXT("mtoon_tex_MatCap")
			});
		}

		MI->SetScalarParameterValueEditorOnly(TEXT("Opacity"), OpacityForParent);
		MI->SetScalarParameterValueEditorOnly(TEXT("mtoon_Cutoff"), 0.0f);
		MI->SetScalarParameterValueEditorOnly(TEXT("mtoon_BlendMode"), bTranslucentForParent ? 2.0f : 0.0f);
		MI->SetScalarParameterValueEditorOnly(TEXT("mtoon_CullMode"), bTwoSidedForParent ? 0.0f : 2.0f);
		MI->SetScalarParameterValueEditorOnly(TEXT("mtoon_OutlineWidth"), PmxMaterial.EdgeSize);
		MI->SetScalarParameterValueEditorOnly(TEXT("pmx.sphere.mode"), static_cast<float>(PmxMaterial.SphereMode));
		MI->SetScalarParameterValueEditorOnly(TEXT("pmx.edge.size"), PmxMaterial.EdgeSize);
		MI->SetScalarParameterValueEditorOnly(TEXT("pmx.specular.power"), PmxMaterial.SpecularStrength);

		MI->SetStaticSwitchParameterValueEditorOnly(TEXT("bTwoSided"), bTwoSidedForParent);
		MI->SetStaticSwitchParameterValueEditorOnly(TEXT("bTranslucentHint"), bTranslucentForParent);
		MI->SetStaticSwitchParameterValueEditorOnly(TEXT("pmx.toon.mode"), static_cast<bool>(PmxMaterial.SharedToonFlag));
		MI->SetStaticSwitchParameterValueEditorOnly(TEXT("pmx.edge.draw"), PmxMaterial.EdgeSize > 0.0f);

		MI->PostEditChange();
		RegisterAsset(MI);
		return MI;
	}

	USkeleton* CreateSkeletonAsset(const FString& ParentPackageName, const FString& AssetName, const FPmxModel& Model)
	{
		UPackage* Package = MakeAssetPackage(ParentPackageName, AssetName);
		USkeleton* Skeleton = NewObject<USkeleton>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
		RegisterAsset(Skeleton);
		return Skeleton;
	}

	bool BuildReferenceSkeleton(USkeletalMesh* SkeletalMesh, USkeleton* Skeleton, const FPmxModel& Model, TArray<int32>& OutBoneToRefIndex, float ImportScale)
	{
		if (!SkeletalMesh || !Skeleton || Model.Bones.Num() <= 0) return false;

		FReferenceSkeleton RefSkeleton;
		{
			FReferenceSkeletonModifier Modifier(RefSkeleton, nullptr);
			Modifier.Add(FMeshBoneInfo(FName(TEXT("Root")), TEXT("Root"), INDEX_NONE), FTransform::Identity);

			const TArray<FString> BoneNames = FPmxUtils::BuildUniqueBoneNames(Model);
			TArray<FVector> WorldPositions;
			WorldPositions.SetNum(Model.Bones.Num());
			for (int32 BoneIndex = 0; BoneIndex < Model.Bones.Num(); ++BoneIndex)
			{
				WorldPositions[BoneIndex] = ConvertPmxPosition(Model.Bones[BoneIndex].Position, ImportScale);
			}

			OutBoneToRefIndex.Init(INDEX_NONE, Model.Bones.Num());
			TArray<uint8> VisitState;
			VisitState.Init(0, Model.Bones.Num());

			TFunction<int32(int32)> AddBoneRecursive = [&](int32 BoneIndex) -> int32
			{
				if (!Model.Bones.IsValidIndex(BoneIndex)) return 0;
				if (OutBoneToRefIndex[BoneIndex] != INDEX_NONE) return OutBoneToRefIndex[BoneIndex];
				if (VisitState[BoneIndex] == 1) return 0;

				VisitState[BoneIndex] = 1;
				const FPmxBone& Bone = Model.Bones[BoneIndex];
				int32 ParentRefIndex = 0;
				FVector ParentWorld = FVector::ZeroVector;
				if (Model.Bones.IsValidIndex(Bone.ParentBoneIndex) && Bone.ParentBoneIndex != BoneIndex)
				{
					ParentRefIndex = AddBoneRecursive(Bone.ParentBoneIndex);
					ParentWorld = WorldPositions[Bone.ParentBoneIndex];
				}

				const FName BoneName(*BoneNames[BoneIndex]);
				const FVector Local = WorldPositions[BoneIndex] - ParentWorld;
				Modifier.Add(FMeshBoneInfo(BoneName, BoneName.ToString(), ParentRefIndex), FTransform(FQuat::Identity, Local));
				OutBoneToRefIndex[BoneIndex] = RefSkeleton.GetRawBoneNum() - 1;
				VisitState[BoneIndex] = 2;
				return OutBoneToRefIndex[BoneIndex];
			};

			for (int32 BoneIndex = 0; BoneIndex < Model.Bones.Num(); ++BoneIndex)
			{
				AddBoneRecursive(BoneIndex);
			}
		}

		if (RefSkeleton.GetRawBoneNum() <= 1) return false;

		USkeletalMesh* TempMesh = NewObject<USkeletalMesh>(GetTransientPackage(), NAME_None, RF_Transient);
		if (!TempMesh) return false;

		TempMesh->SetRefSkeleton(RefSkeleton);
		TempMesh->CalculateInvRefMatrices();

		if (!Skeleton->MergeAllBonesToBoneTree(TempMesh))
		{
			UE_LOG(LogPMXImporter, Error, TEXT("AMAP5 Factory: MergeAllBonesToBoneTree failed."));
			return false;
		}

		SkeletalMesh->SetSkeleton(Skeleton);
		SkeletalMesh->SetRefSkeleton(Skeleton->GetReferenceSkeleton());

		FReferenceSkeletonModifier MeshRefModifier(SkeletalMesh->GetRefSkeleton(), Skeleton);
		const FReferenceSkeleton& SrcRef = TempMesh->GetRefSkeleton();
		for (int32 RawIndex = 0; RawIndex < SrcRef.GetRawBoneNum(); ++RawIndex)
		{
			const int32 DstIndex = SkeletalMesh->GetRefSkeleton().FindRawBoneIndex(SrcRef.GetBoneName(RawIndex));
			if (DstIndex != INDEX_NONE)
			{
				MeshRefModifier.UpdateRefPoseTransform(DstIndex, SrcRef.GetRawRefBonePose()[RawIndex]);
			}
		}

		SkeletalMesh->CalculateInvRefMatrices();
		return true;
	}

	bool BuildSkeletalMeshLODModel(
		const FPmxModel& Model,
		const TArray<int32>& BoneToRefIndex,
		USkeletalMesh* Mesh,
		TMap<int32, TArray<int32>>& OutPmxVertToGlobal,
		FBox& OutBounds, float ImportScale)
	{
		if (!Mesh || Model.Vertices.IsEmpty() || Model.Indices.IsEmpty())
		{
			return false;
		}

		FSkeletalMeshModel* ImportedModel = Mesh->GetImportedModel();
		if (!ImportedModel)
		{
			return false;
		}
		ImportedModel->LODModels.Empty();
		ImportedModel->LODModels.Add(new FSkeletalMeshLODModel());
		FSkeletalMeshLODModel& LODModel = ImportedModel->LODModels[0];

		struct FSectionBuild
		{
			int32 MaterialIndex = 0;
			int32 MaxInfluences = 1;
			TArray<FSoftSkinVertex> Soft;
			TArray<int32> BoneMap;
			TArray<uint32> LocalIndices;
			TArray<int32> LocalSourcePmx;
			TArray<FVector3f> NormalWs;
			TArray<FVector3f> TangentAccum;
			TArray<FVector3f> BitangentAccum;
		};

		const int32 MaterialCount = FMath::Max(Model.Materials.Num(), 1);
		TArray<FSectionBuild> Sections;
		Sections.SetNum(MaterialCount);
		for (int32 m = 0; m < MaterialCount; ++m)
		{
			Sections[m].MaterialIndex = m;
		}

		FVector3f BoundMin(0.0f, 0.0f, 0.0f);
		FVector3f BoundMax(0.0f, 0.0f, 0.0f);
		bool bHasBounds = false;
		TSet<int32> ActiveBoneRefIndices;

		auto AddSectionVertex = [&](FSectionBuild& S, int32 PmxIndex) -> int32
		{
			const FPmxVertex& V = Model.Vertices[PmxIndex];

			FSoftSkinVertex Soft;
			const FVector Pos = ConvertPmxPosition(V.Position, ImportScale);
			Soft.Position = FVector3f(Pos);
			Soft.Color = FColor::White;
			Soft.TangentX = FVector3f::ZeroVector;
			Soft.TangentY = FVector3f::ZeroVector;
			Soft.TangentZ = FVector4f(0.0f, 0.0f, 1.0f, 1.0f);
			for (int32 u = 0; u < MAX_TEXCOORDS; ++u)
			{
				Soft.UVs[u] = FVector2f(0.0f, 0.0f);
			}
			Soft.UVs[0] = V.UV;
			for (int32 k = 0; k < MAX_TOTAL_INFLUENCES; ++k)
			{
				Soft.InfluenceBones[k] = 0;
				Soft.InfluenceWeights[k] = 0;
			}

			struct FInf { int32 Ref; float W; };
			TArray<FInf, TInlineAllocator<MAX_TOTAL_INFLUENCES>> Infs;
			const int32 PairCount = FMath::Min(V.BoneIndices.Num(), V.BoneWeights.Num());
			float WeightSum = 0.0f;
			for (int32 w = 0; w < PairCount; ++w)
			{
				const int32 PmxBone = V.BoneIndices[w];
				const float Weight = V.BoneWeights[w];
				if (PmxBone < 0 || !BoneToRefIndex.IsValidIndex(PmxBone))
				{
					continue;
				}
				const int32 Ref = BoneToRefIndex[PmxBone];
				if (Ref < 0 || Weight <= 0.0f || !FMath::IsFinite(Weight))
				{
					continue;
				}

				const float ClampedWeight = FMath::Clamp(Weight, 0.0f, 1.0f);
				FInf* Existing = Infs.FindByPredicate([Ref](const FInf& Inf) { return Inf.Ref == Ref; });
				if (Existing)
				{
					Existing->W += ClampedWeight;
				}
				else
				{
					Infs.Add({ Ref, ClampedWeight });
				}
				ActiveBoneRefIndices.Add(Ref);
				WeightSum += ClampedWeight;
			}
			Infs.Sort([](const FInf& A, const FInf& B) { return A.W > B.W; });
			if (Infs.Num() > MAX_TOTAL_INFLUENCES)
			{
				Infs.SetNum(MAX_TOTAL_INFLUENCES);
				WeightSum = 0.0f;
				for (const FInf& Inf : Infs)
				{
					WeightSum += Inf.W;
				}
			}
			if (Infs.Num() == 0)
			{
				UE_LOG(LogPMXImporter, Verbose, TEXT("[Factory.SkinWeights] Vertex (PmxIndex=%d) has no valid influence; falling back to root bone 0 (weight 1.0)"), PmxIndex);
				Infs.Add({ 0, 1.0f });
				WeightSum = 1.0f;
			}
			if (WeightSum <= 0.0f)
			{
				WeightSum = 1.0f;
			}

			const int32 MaxQuant = (int32)AMAP5_MAX_BONE_WEIGHT_F;
			int32 AccumQuant = 0;
			int32 MaxInfIndex = 0;
			for (int32 k = 0; k < Infs.Num(); ++k)
			{
				const int32 Local = S.BoneMap.AddUnique(Infs[k].Ref);
				Soft.InfluenceBones[k] = (FBoneIndexType)Local;
				const float Normalized = Infs[k].W / WeightSum;
				const int32 Quant = FMath::Clamp(FMath::RoundToInt(Normalized * AMAP5_MAX_BONE_WEIGHT_F), 0, MaxQuant);
				Soft.InfluenceWeights[k] = (AMAP5_BONE_WEIGHT_TYPE)Quant;
				AccumQuant += Quant;
				if (Infs[k].W > Infs[MaxInfIndex].W)
				{
					MaxInfIndex = k;
				}
			}
			if (AccumQuant != MaxQuant)
			{
				const int32 Fixed = (int32)Soft.InfluenceWeights[MaxInfIndex] + (MaxQuant - AccumQuant);
				Soft.InfluenceWeights[MaxInfIndex] = (AMAP5_BONE_WEIGHT_TYPE)FMath::Clamp(Fixed, 0, MaxQuant);
			}
			S.MaxInfluences = FMath::Max(S.MaxInfluences, Infs.Num());

			const int32 Local = S.Soft.Add(Soft);
			S.LocalSourcePmx.Add(PmxIndex);
			S.NormalWs.Add(FVector3f(ConvertPmxDirection(V.Normal)));
			S.TangentAccum.Add(FVector3f::ZeroVector);
			S.BitangentAccum.Add(FVector3f::ZeroVector);

			if (!bHasBounds)
			{
				BoundMin = Soft.Position;
				BoundMax = Soft.Position;
				bHasBounds = true;
			}
			else
			{
				BoundMin.X = FMath::Min(BoundMin.X, Soft.Position.X);
				BoundMin.Y = FMath::Min(BoundMin.Y, Soft.Position.Y);
				BoundMin.Z = FMath::Min(BoundMin.Z, Soft.Position.Z);
				BoundMax.X = FMath::Max(BoundMax.X, Soft.Position.X);
				BoundMax.Y = FMath::Max(BoundMax.Y, Soft.Position.Y);
				BoundMax.Z = FMath::Max(BoundMax.Z, Soft.Position.Z);
			}
			return Local;
		};

		auto AddTriangle = [&](FSectionBuild& S, int32 A, int32 B, int32 C)
		{
			if (A == B || B == C || C == A)
			{
				return;
			}

			const FVector3f PA(ConvertPmxPosition(Model.Vertices[A].Position, ImportScale));
			const FVector3f PB(ConvertPmxPosition(Model.Vertices[B].Position, ImportScale));
			const FVector3f PC(ConvertPmxPosition(Model.Vertices[C].Position, ImportScale));
			if (FVector3f::CrossProduct(PB - PA, PC - PA).IsNearlyZero())
			{
				return;
			}

			const int32 L0 = AddSectionVertex(S, A);
			const int32 L1 = AddSectionVertex(S, C);
			const int32 L2 = AddSectionVertex(S, B);
			S.LocalIndices.Add((uint32)L0);
			S.LocalIndices.Add((uint32)L1);
			S.LocalIndices.Add((uint32)L2);

			const FVector3f P0 = S.Soft[L0].Position;
			const FVector3f P1 = S.Soft[L1].Position;
			const FVector3f P2 = S.Soft[L2].Position;
			const FVector2f W0 = S.Soft[L0].UVs[0];
			const FVector2f W1 = S.Soft[L1].UVs[0];
			const FVector2f W2 = S.Soft[L2].UVs[0];

			const FVector3f E1 = P1 - P0;
			const FVector3f E2 = P2 - P0;
			const FVector2f D1 = W1 - W0;
			const FVector2f D2 = W2 - W0;
			const float Denom = D1.X * D2.Y - D2.X * D1.Y;
			if (FMath::Abs(Denom) > SMALL_NUMBER)
			{
				const float R = 1.0f / Denom;
				const FVector3f Tan = (E1 * D2.Y - E2 * D1.Y) * R;
				const FVector3f Bit = (E2 * D1.X - E1 * D2.X) * R;
				S.TangentAccum[L0] += Tan; S.TangentAccum[L1] += Tan; S.TangentAccum[L2] += Tan;
				S.BitangentAccum[L0] += Bit; S.BitangentAccum[L1] += Bit; S.BitangentAccum[L2] += Bit;
			}
		};

		int32 IndexCursor = 0;
		for (int32 MatIndex = 0; MatIndex < Model.Materials.Num(); ++MatIndex)
		{
			FSectionBuild& S = Sections[MatIndex];
			const int32 SurfaceCount = FMath::Max(0, Model.Materials[MatIndex].SurfaceCount);
			const int32 TriCount = SurfaceCount / 3;
			for (int32 t = 0; t < TriCount; ++t)
			{
				if (!Model.Indices.IsValidIndex(IndexCursor + 2))
				{
					break;
				}
				const int32 I0 = Model.Indices[IndexCursor + 0];
				const int32 I1 = Model.Indices[IndexCursor + 1];
				const int32 I2 = Model.Indices[IndexCursor + 2];
				IndexCursor += 3;
				if (!Model.Vertices.IsValidIndex(I0) || !Model.Vertices.IsValidIndex(I1) || !Model.Vertices.IsValidIndex(I2))
				{
					continue;
				}
				AddTriangle(S, I0, I1, I2);
			}
		}

		while (Model.Indices.IsValidIndex(IndexCursor + 2))
		{
			const int32 I0 = Model.Indices[IndexCursor + 0];
			const int32 I1 = Model.Indices[IndexCursor + 1];
			const int32 I2 = Model.Indices[IndexCursor + 2];
			IndexCursor += 3;
			if (!Model.Vertices.IsValidIndex(I0) || !Model.Vertices.IsValidIndex(I1) || !Model.Vertices.IsValidIndex(I2))
			{
				continue;
			}
			AddTriangle(Sections[0], I0, I1, I2);
		}

		LODModel.Sections.Empty();
		TArray<uint32> GlobalIndices;
		int32 RunningVertex = 0;
		int32 RunningIndex = 0;
		int32 UsedSections = 0;

		for (int32 s = 0; s < Sections.Num(); ++s)
		{
			FSectionBuild& S = Sections[s];
			if (S.Soft.Num() == 0 || S.LocalIndices.Num() == 0)
			{
				continue;
			}

			for (int32 i = 0; i < S.Soft.Num(); ++i)
			{
				FVector3f N = S.NormalWs[i];
				if (N.IsNearlyZero())
				{
					N = FVector3f(0.0f, 0.0f, 1.0f);
				}
				N = N.GetSafeNormal();

				FVector3f T = S.TangentAccum[i];
				T = T - N * FVector3f::DotProduct(N, T);
				if (T.IsNearlyZero())
				{
					T = FVector3f::CrossProduct(N, FVector3f(0.0f, 0.0f, 1.0f));
					if (T.IsNearlyZero())
					{
						T = FVector3f::CrossProduct(N, FVector3f(1.0f, 0.0f, 0.0f));
					}
				}
				T = T.GetSafeNormal();

				const FVector3f Cross = FVector3f::CrossProduct(N, T);
				const float Handed = (FVector3f::DotProduct(Cross, S.BitangentAccum[i]) < 0.0f) ? -1.0f : 1.0f;

				S.Soft[i].TangentX = T;
				S.Soft[i].TangentY = Cross * Handed;
				S.Soft[i].TangentZ = FVector4f(N.X, N.Y, N.Z, Handed);
			}

			FSkelMeshSection Section;
			Section.MaterialIndex = (uint16)S.MaterialIndex;
			Section.BaseVertexIndex = RunningVertex;
			Section.BaseIndex = RunningIndex;
			Section.NumTriangles = S.LocalIndices.Num() / 3;
			Section.NumVertices = S.Soft.Num();
			Section.SoftVertices = S.Soft;
			Section.MaxBoneInfluences = FMath::Clamp(S.MaxInfluences, 1, (int32)MAX_TOTAL_INFLUENCES);
			Section.bDisabled = false;
#if !UE_VERSION_OLDER_THAN(4,24,0)
			Section.OriginalDataSectionIndex = UsedSections;
#endif
			if (S.BoneMap.Num() > 0)
			{
				Section.BoneMap.SetNum(S.BoneMap.Num());
				for (int32 b = 0; b < S.BoneMap.Num(); ++b)
				{
					Section.BoneMap[b] = (FBoneIndexType)S.BoneMap[b];
				}
			}
			else
			{
				Section.BoneMap.Add((FBoneIndexType)0);
			}

			for (int32 li = 0; li < S.LocalSourcePmx.Num(); ++li)
			{
				OutPmxVertToGlobal.FindOrAdd(S.LocalSourcePmx[li]).Add(RunningVertex + li);
			}
			for (int32 li = 0; li < S.LocalIndices.Num(); ++li)
			{
				GlobalIndices.Add((uint32)(RunningVertex + (int32)S.LocalIndices[li]));
			}

			RunningVertex += S.Soft.Num();
			RunningIndex += S.LocalIndices.Num();
			LODModel.Sections.Add(Section);
			++UsedSections;
		}

		if (LODModel.Sections.Num() == 0)
		{
			return false;
		}

		LODModel.NumVertices = RunningVertex;
		LODModel.NumTexCoords = 1;
		LODModel.IndexBuffer = GlobalIndices;

		const int32 BoneNum = Mesh->GetRefSkeleton().GetRawBoneNum();
		LODModel.RequiredBones.Empty();
		for (int32 b = 0; b < BoneNum; ++b)
		{
			LODModel.RequiredBones.Add((FBoneIndexType)b);
		}

		// AMAP5: ActiveBoneIndices must reflect only bones with real vertex weight (VRM4U-compliant).
		// Marking every bone active regardless of weight produces fake weights: bones with zero
		// real influence are then treated as weighted, so Persona's skeleton tree never shows them
		// as empty bones.
		{
			const FReferenceSkeleton& FinalRefSkeleton = Mesh->GetRefSkeleton();
			for (int32 b = 0; b < BoneNum; ++b)
			{
				if (!ActiveBoneRefIndices.Contains(b))
				{
					continue;
				}
				int32 ParentIndex = FinalRefSkeleton.GetParentIndex(b);
				while (ParentIndex >= 0 && !ActiveBoneRefIndices.Contains(ParentIndex))
				{
					ActiveBoneRefIndices.Add(ParentIndex);
					ParentIndex = FinalRefSkeleton.GetParentIndex(ParentIndex);
				}
			}
			TArray<int32> SortedActiveBones = ActiveBoneRefIndices.Array();
			SortedActiveBones.Sort();
			LODModel.ActiveBoneIndices.Empty(SortedActiveBones.Num());
			for (const int32 b : SortedActiveBones)
			{
				LODModel.ActiveBoneIndices.Add((FBoneIndexType)b);
			}
		}

		OutBounds = bHasBounds ? FBox((FVector)BoundMin, (FVector)BoundMax) : FBox(FVector(-1.0, -1.0, -1.0), FVector(1.0, 1.0, 1.0));
		return true;
	}

	void BuildMorphTargets(
		const FPmxModel& Model,
		USkeletalMesh* Mesh,
		const TMap<int32, TArray<int32>>& PmxVertToGlobal, float ImportScale)
	{
		if (!Mesh)
		{
			return;
		}
		FSkeletalMeshModel* ImportedModel = Mesh->GetImportedModel();
		if (!ImportedModel || ImportedModel->LODModels.Num() == 0)
		{
			return;
		}
		const TArray<FSkelMeshSection>& Sections = ImportedModel->LODModels[0].Sections;

		auto AccumulateVertexMorphs =
			[&Model](auto&& Self, int32 MorphIndex, float Weight, TMap<int32, FVector3f>& OutOffsets, TSet<int32>& ActiveMorphs) -> void
		{
			if (!Model.Morphs.IsValidIndex(MorphIndex) || FMath::IsNearlyZero(Weight))
			{
				return;
			}
			if (ActiveMorphs.Contains(MorphIndex))
			{
				UE_LOG(LogPMXImporter, Warning, TEXT("AMAP5 Factory: cyclic morph reference detected at morph index %d"), MorphIndex);
				return;
			}

			ActiveMorphs.Add(MorphIndex);
			const FPmxMorph& SourceMorph = Model.Morphs[MorphIndex];

			if (SourceMorph.VertexMorphs.Num() > 0)
			{
				for (const FPmxVertexMorph& VertexMorph : SourceMorph.VertexMorphs)
				{
					if (VertexMorph.VertexIndex < 0)
					{
						continue;
					}
					OutOffsets.FindOrAdd(VertexMorph.VertexIndex) += VertexMorph.Offset * Weight;
				}
			}

			if (SourceMorph.GroupMorphs.Num() > 0)
			{
				for (const FPmxGroupMorph& GroupMorph : SourceMorph.GroupMorphs)
				{
					Self(Self, GroupMorph.MorphIndex, Weight * GroupMorph.MorphRatio, OutOffsets, ActiveMorphs);
				}
			}

			ActiveMorphs.Remove(MorphIndex);
		};

		TArray<FString> MorphNameList;
		TArray<UMorphTarget*> MorphTargetList;
		TMap<FString, UMorphTarget*> MorphNameToTargetMap;

		for (int32 mi = 0; mi < Model.Morphs.Num(); ++mi)
		{
			TMap<int32, FVector3f> PmxVertexOffsets;
			TSet<int32> ActiveMorphs;
			AccumulateVertexMorphs(AccumulateVertexMorphs, mi, 1.0f, PmxVertexOffsets, ActiveMorphs);
			if (PmxVertexOffsets.Num() == 0)
			{
				continue;
			}

			TMap<int32, FVector3f> GlobalVertexOffsets;
			for (const TPair<int32, FVector3f>& Pair : PmxVertexOffsets)
			{
				const TArray<int32>* Globals = PmxVertToGlobal.Find(Pair.Key);
				if (!Globals)
				{
					continue;
				}

				const FVector3f Offset = FVector3f(ConvertPmxDelta(Pair.Value, ImportScale));
				for (const int32 GlobalVertexIndex : *Globals)
				{
					GlobalVertexOffsets.FindOrAdd(GlobalVertexIndex) += Offset;
				}
			}
			if (GlobalVertexOffsets.Num() == 0)
			{
				continue;
			}

			TArray<int32> SortedVertexIndices;
			GlobalVertexOffsets.GetKeys(SortedVertexIndices);
			SortedVertexIndices.Sort();

			TArray<FMorphTargetDelta> Deltas;
			Deltas.Reserve(SortedVertexIndices.Num());
			for (const int32 GlobalVertexIndex : SortedVertexIndices)
			{
				const FVector3f* Offset = GlobalVertexOffsets.Find(GlobalVertexIndex);
				if (!Offset || Offset->IsNearlyZero())
				{
					continue;
				}

				FMorphTargetDelta Delta;
				Delta.SourceIdx = (uint32)GlobalVertexIndex;
				Delta.PositionDelta = *Offset;
				Delta.TangentZDelta = FVector3f::ZeroVector;
				Deltas.Add(Delta);
			}
			if (Deltas.Num() == 0)
			{
				continue;
			}

			const FString UniqueMorphNameString = FPmxUtils::BuildUniqueRawMorphName(Model, mi);
			MorphNameList.AddUnique(UniqueMorphNameString);

			UMorphTarget* MorphTarget = NewObject<UMorphTarget>(Mesh, *UniqueMorphNameString);
			MorphTarget->PopulateDeltas(Deltas, 0, Sections);
			UE_LOG(LogPMXImporter, Log, TEXT("[Factory.Morph %d] RawName='%s' ObjectName='%s' Deltas=%d Valid=%d"),
				mi, *Model.Morphs[mi].Name, *UniqueMorphNameString, Deltas.Num(), MorphTarget->HasValidData() ? 1 : 0);
			if (MorphTarget->HasValidData())
			{
				MorphTargetList.Add(MorphTarget);
				MorphNameToTargetMap.Add(UniqueMorphNameString, MorphTarget);
			}
		}

		if (MorphTargetList.Num() == 0)
		{
			return;
		}

#if WITH_EDITOR
#if !UE_VERSION_OLDER_THAN(5,8,0)
		FMeshDescription* MeshDescription = Mesh->GetMeshDescription(0);
		if (MeshDescription)
		{
			FSkeletalMeshAttributes MeshAttributes(*MeshDescription);
			MeshAttributes.Register();
			for (const FString& MorphName : MorphNameList)
			{
				UMorphTarget** MorphTargetPtr = MorphNameToTargetMap.Find(MorphName);
				if (!MorphTargetPtr || !(*MorphTargetPtr))
				{
					continue;
				}

				UMorphTarget* MorphTarget = *MorphTargetPtr;
				if (MorphTarget->GetMorphLODModels().Num() == 0)
				{
					continue;
				}

				if (MeshAttributes.RegisterMorphTargetAttribute(*MorphName, false))
				{
					TVertexAttributesRef<FVector3f> PositionDelta = MeshAttributes.GetVertexMorphPositionDelta(*MorphName);
					const FMorphTargetLODModel& MorphLODModel = MorphTarget->GetMorphLODModels()[0];
					for (const FMorphTargetDelta& Delta : MorphLODModel.Vertices)
					{
						FVertexID VertexID(Delta.SourceIdx);
						if (MeshDescription->IsVertexValid(VertexID))
						{
							PositionDelta.Set(VertexID, Delta.PositionDelta);
						}
					}
				}
			}
			Mesh->CommitMeshDescription(0);
		}
#endif
#endif

		Mesh->GetMorphTargets().Empty();

#if WITH_EDITOR
#if !UE_VERSION_OLDER_THAN(5,3,0)
		if (USkeleton* MeshSkeleton = Mesh->GetSkeleton())
		{
			TArray<FName> CurveNames;
			MeshSkeleton->GetCurveMetaDataNames(CurveNames);
			for (const FName CurveName : CurveNames)
			{
				FCurveMetaData* CurveMetaData = MeshSkeleton->GetCurveMetaData(CurveName);
				if (CurveMetaData && CurveMetaData->Type.bMorphtarget)
				{
					MeshSkeleton->RemoveCurveMetaData(CurveName);
				}
			}
		}
#endif
#endif

#if UE_VERSION_OLDER_THAN(5,8,0)
		for (int32 i = 0; i < MorphTargetList.Num(); ++i)
		{
			UMorphTarget* MorphTarget = MorphTargetList[i];
			if (i == MorphTargetList.Num() - 1)
			{
				Mesh->RegisterMorphTarget(MorphTarget);
			}
			else
			{
				Mesh->RegisterMorphTarget(MorphTarget, false);
			}
		}
		Mesh->InitMorphTargets();
#else
		for (UMorphTarget* MorphTarget : MorphTargetList)
		{
			Mesh->RegisterMorphTarget(MorphTarget, false);
		}
		Mesh->InitMorphTargets(true);
		Mesh->SetMorphTargets(MorphTargetList);
#endif

		if (USkeleton* MeshSkeleton = Mesh->GetSkeleton())
		{
			for (const FString& MorphName : MorphNameList)
			{
				FCurveMetaData* CurveMetaData = MeshSkeleton->GetCurveMetaData(*MorphName);
				if (CurveMetaData)
				{
					CurveMetaData->Type.bMorphtarget = true;
					continue;
				}
				MeshSkeleton->AccumulateCurveMetaData(*MorphName, false, true);
			}
		}
		// Note: Build()/PostEditChange() intentionally NOT called here.
		// A single consolidated call happens in the caller after this function returns,
		// so morph target data exists before the SkeletalMesh derived-data key is computed
		// (calling Build() twice with an unchanged key can serve the earlier, morph-less
		// cached derived data on the second call).
	}

	UAnimBlueprint* CreateAnimBlueprintAsset(const FString& ParentPackageName, const FString& AssetName, USkeleton* Skeleton, USkeletalMesh* Mesh)
	{
		// VRM4U仕様準拠: /VRM4U/Util/Actor/latest/ABP_PostProcessBase を複製し、
		// TargetSkeleton/PreviewMeshを設定した上でSkeletalMesh側のPostProcessAnimBlueprintへ割り当てる。
		FSoftObjectPath TemplatePath(TEXT("/VRM4U/Util/Actor/latest/ABP_PostProcessBase.ABP_PostProcessBase"));
		UObject* TemplateObj = TemplatePath.TryLoad();
		if (!TemplateObj)
		{
			UE_LOG(LogPMXImporter, Error, TEXT("AMAP5 Factory: VRM4U ABP_PostProcessBase template not found: %s"), *TemplatePath.ToString());
			return nullptr;
		}

		UPackage* Package = MakeAssetPackage(ParentPackageName, AssetName);
		UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(
			StaticDuplicateObject(TemplateObj, Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional));
		if (!AnimBlueprint)
		{
			UE_LOG(LogPMXImporter, Error, TEXT("AMAP5 Factory: failed to duplicate ABP_PostProcessBase template."));
			return nullptr;
		}

		AnimBlueprint->MarkPackageDirty();
		AnimBlueprint->TargetSkeleton = Skeleton;
		if (Mesh)
		{
			AnimBlueprint->SetPreviewMesh(Mesh);
		}
		FKismetEditorUtilities::CompileBlueprint(AnimBlueprint);
		RegisterAsset(AnimBlueprint);

		if (Mesh)
		{
			if (UBlueprintGeneratedClass* BpClass = Cast<UBlueprintGeneratedClass>(AnimBlueprint->GeneratedClass))
			{
				Mesh->SetPostProcessAnimBlueprint(BpClass);
				Mesh->PostEditChange();
			}
		}

		return AnimBlueprint;
	}
}

UAMAP5Factory::UAMAP5Factory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Formats.Add(TEXT("pmx;AMAP5 PMX Model"));
	SupportedClass = USkeletalMesh::StaticClass();
	bCreateNew = false;
	bEditorImport = true;
	ImportPriority = 100;
}

bool UAMAP5Factory::FactoryCanImport(const FString& Filename)
{
	FullFileName.Empty();
	if (FPaths::GetExtension(Filename).Equals(TEXT("pmx"), ESearchCase::IgnoreCase))
	{
		FullFileName = Filename;
		UE_LOG(LogPMXImporter, Log, TEXT("[Factory.CanImport] Claims '%s' (legacy factory path candidate)"), *Filename);
		return true;
	}
	return false;
}

UClass* UAMAP5Factory::ResolveSupportedClass()
{
	return USkeletalMesh::StaticClass();
}

UObject* UAMAP5Factory::FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	UE_LOG(LogPMXImporter, Log, TEXT("[Factory.CreateBinary] ===== LEGACY FACTORY PATH ACTIVE ===== Name='%s' File='%s'"), *InName.ToString(), *FullFileName);
	bOutOperationCanceled = false;

	if (FullFileName.IsEmpty())
	{
		return nullptr;
	}

	if (!ReimportMesh)
	{
		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(LOCTEXT("OptionsTitle", "AMAP5 Import Options"))
			.SizingRule(ESizingRule::Autosized)
			.SupportsMaximize(false)
			.SupportsMinimize(false);

		TSharedPtr<SAMAP5OptionWindow> OptionWindow;
		Window->SetContent(
			SAssignNew(OptionWindow, SAMAP5OptionWindow)
			.Factory(this)
			.WidgetWindow(Window));

		FSlateApplication::Get().AddModalWindow(Window, nullptr);
		if (!OptionWindow.IsValid() || !OptionWindow->ShouldImport())
		{
			bOutOperationCanceled = true;
			return nullptr;
		}
	}

	FPmxModel Model;
	if (!PMXReader::LoadPmxFromFile(FullFileName, Model) || Model.Vertices.IsEmpty() || Model.Indices.IsEmpty())
	{
		UE_LOG(LogPMXImporter, Error, TEXT("AMAP5 Factory: PMX read failed or mesh is empty: %s"), *FullFileName);
		return nullptr;
	}

	UE_LOG(LogPMXImporter, Log, TEXT("[Factory.CreateBinary] Loaded: Vertices=%d Indices=%d Bones=%d Materials=%d Morphs=%d"),
		Model.Vertices.Num(), Model.Indices.Num(), Model.Bones.Num(), Model.Materials.Num(), Model.Morphs.Num());

	UPackage* MeshPackage = Cast<UPackage>(InParent) ? Cast<UPackage>(InParent) : InParent->GetOutermost();
	if (!MeshPackage)
	{
		return nullptr;
	}
	const FString ParentPackageName = MeshPackage->GetName();
	const FString BaseName = MakeAssetName(InName.ToString(), FPaths::GetBaseFilename(FullFileName));

	const FString MeshAssetName = TEXT("0_SM_") + BaseName;
	USkeleton* Skeleton = CreateSkeletonAsset(ParentPackageName, TEXT("0_SK_") + BaseName, Model);
	USkeletalMesh* Mesh = ReimportMesh ? ReimportMesh.Get() : NewObject<USkeletalMesh>(
		MakeAssetPackage(ParentPackageName, MeshAssetName), *MeshAssetName, RF_Public | RF_Standalone | RF_Transactional);
	TArray<int32> BoneToRefIndex;
		if (!Mesh || !Skeleton || !BuildReferenceSkeleton(Mesh, Skeleton, Model, BoneToRefIndex, ImportScale))
	{
		return nullptr;
	}

	TSet<int32> RequiredTextureIndices;
	TSet<int32> ToonTextureIndices;
	for (const FPmxMaterial& Material : Model.Materials)
	{
		if (Model.Textures.IsValidIndex(Material.TextureIndex))
		{
			RequiredTextureIndices.Add(Material.TextureIndex);
		}
		if (Material.SphereMode != 0 && Model.Textures.IsValidIndex(Material.SphereTextureIndex))
		{
			RequiredTextureIndices.Add(Material.SphereTextureIndex);
		}
		if (!Material.SharedToonFlag && Model.Textures.IsValidIndex(Material.ToonTextureIndex))
		{
			ToonTextureIndices.Add(Material.ToonTextureIndex);
		}
	}

	TMap<int32, UTexture2D*> Textures;
	TMap<int32, bool> TextureAlphaByIndex;
	for (int32 TextureIndex = 0; TextureIndex < Model.Textures.Num(); ++TextureIndex)
	{
		if (ToonTextureIndices.Contains(TextureIndex) && !RequiredTextureIndices.Contains(TextureIndex))
		{
			UE_LOG(LogPMXImporter, Verbose, TEXT("AMAP5 Factory: Skipping toon-only texture index %d"), TextureIndex);
			continue;
		}
		const FString TextureFile = ResolveTextureFile(FullFileName, Model.Textures[TextureIndex].TexturePath);
		if (TextureFile.IsEmpty()) continue;

		const FString TexName = TEXT("T_") + MakeAssetName(FPaths::GetBaseFilename(TextureFile), FString::Printf(TEXT("Tex_%d"), TextureIndex));
		bool bTextureHasAlpha = false;
		if (UTexture2D* Texture = CreateTextureAsset(ParentPackageName, TexName, TextureFile, bTextureHasAlpha))
		{
			Textures.Add(TextureIndex, Texture);
			TextureAlphaByIndex.Add(TextureIndex, bTextureHasAlpha);
		}
	}

	const auto GetTextureByIndex = [&](int32 TextureIndex) -> UTexture2D*
	{
		return TextureIndex >= 0 ? Textures.FindRef(TextureIndex) : nullptr;
	};

	TMap<FString, int32> UsedMaterialLabels;
	TArray<UMaterialInterface*> Materials;
	for (int32 MatIndex = 0; MatIndex < Model.Materials.Num(); ++MatIndex)
	{
		const FPmxMaterial& PmxMat = Model.Materials[MatIndex];
		UTexture2D* BaseTexture = GetTextureByIndex(PmxMat.TextureIndex);
		UTexture2D* SphereTexture = PmxMat.SphereMode != 0 ? GetTextureByIndex(PmxMat.SphereTextureIndex) : nullptr;
		const bool bBaseTextureHasAlpha = PmxMat.TextureIndex >= 0 ? TextureAlphaByIndex.FindRef(PmxMat.TextureIndex) : false;
		UE_LOG(LogPMXImporter, Log, TEXT("[Factory.Material %d] Name='%s' BaseTexResolved=%d BaseTexHasAlpha=%d SphereTexResolved=%d ToonTextureImport=Disabled"),
			MatIndex, *PmxMat.Name, (BaseTexture ? 1 : 0), (bBaseTextureHasAlpha ? 1 : 0), (SphereTexture ? 1 : 0));
		const FString MatLabel = MakeAssetName(PmxMat.Name, FString::Printf(TEXT("Mat_%d"), MatIndex));
		int32& MatLabelCount = UsedMaterialLabels.FindOrAdd(MatLabel);
		const FString UniqueMatLabel = MatLabelCount > 0 ? FString::Printf(TEXT("%s_%d"), *MatLabel, MatLabelCount) : MatLabel;
		++MatLabelCount;
		const FString MatName = TEXT("MI_") + UniqueMatLabel;
		UMaterialInstanceConstant* MI = CreateMaterialAsset(ParentPackageName, MatName, PmxMat, BaseTexture, SphereTexture, MaterialType, bBaseTextureHasAlpha);
		Materials.Add(MI ? Cast<UMaterialInterface>(MI) : UMaterial::GetDefaultMaterial(MD_Surface));
	}
	if (Materials.IsEmpty())
	{
		Materials.Add(UMaterial::GetDefaultMaterial(MD_Surface));
	}

	Mesh->GetMaterials().Reset();
	for (int32 MatIndex = 0; MatIndex < Materials.Num(); ++MatIndex)
	{
		FString SlotName = Model.Materials.IsValidIndex(MatIndex) ? Model.Materials[MatIndex].Name : FString();
		SlotName.TrimStartAndEndInline();
		if (SlotName.IsEmpty()) SlotName = FString::Printf(TEXT("Mat_%d"), MatIndex);
		Mesh->GetMaterials().Add(FSkeletalMaterial(Materials[MatIndex], FName(*SlotName), FName(*SlotName)));
	}

	if (Mesh->GetLODInfo(0) == nullptr)
	{
		Mesh->AddLODInfo();
	}

	TMap<int32, TArray<int32>> PmxVertToGlobal;
	FBox MeshBounds(ForceInit);
	if (!BuildSkeletalMeshLODModel(Model, BoneToRefIndex, Mesh, PmxVertToGlobal, MeshBounds, ImportScale))
	{
		return nullptr;
	}
	if (FSkeletalMeshModel* ImportedModelForGuid = Mesh->GetImportedModel())
	{
		ImportedModelForGuid->GenerateNewGUID();
	}
	Mesh->SetImportedBounds(FBoxSphereBounds(MeshBounds));
	Mesh->SetPhysicsAsset(nullptr);
	if (UAssetImportData* ImportData = Mesh->GetAssetImportData())
	{
		ImportData->Update(FullFileName);
	}
	Mesh->CalculateInvRefMatrices();
	BuildMorphTargets(Model, Mesh, PmxVertToGlobal, ImportScale);
	{
		FProperty* ChangedProperty = FindFProperty<FProperty>(USkeletalMesh::StaticClass(), TEXT("Materials"));
		if (ChangedProperty)
		{
			Mesh->PreEditChange(ChangedProperty);
			FPropertyChangedEvent PropertyUpdateStruct(ChangedProperty);
			Mesh->PostEditChangeProperty(PropertyUpdateStruct);
		}
		Mesh->PostEditChange();
	}
	Mesh->CalculateExtendedBounds();
	RegisterAsset(Mesh);

	if (Skeleton)
	{
#if WITH_EDITORONLY_DATA
		Skeleton->SetPreviewMesh(Mesh);
#endif
		Skeleton->RecreateBoneTree(Mesh);
		RegisterAsset(Skeleton);
	}

	CreateAnimBlueprintAsset(ParentPackageName, TEXT("ABP_Post_") + BaseName, Skeleton, Mesh);
	return Mesh;
}

UObject* UAMAP5Factory::FactoryCreateText(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn)
{
	UE_LOG(LogPMXImporter, Log, TEXT("[Factory.CreateText] Called (returns null; text import unsupported) Name='%s'"), *InName.ToString());
	return nullptr;
}

bool UAMAP5Factory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	if (USkeletalMesh* Mesh = Cast<USkeletalMesh>(Obj))
	{
		if (UAssetImportData* ImportData = Mesh->GetAssetImportData())
		{
			ImportData->ExtractFilenames(OutFilenames);
			return true;
		}
	}
	return false;
}

void UAMAP5Factory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	if (USkeletalMesh* Mesh = Cast<USkeletalMesh>(Obj))
	{
		if (UAssetImportData* ImportData = Mesh->GetAssetImportData(); ImportData && ensure(NewReimportPaths.Num() == 1))
		{
			ImportData->UpdateFilenameOnly(NewReimportPaths[0]);
		}
	}
}

EReimportResult::Type UAMAP5Factory::Reimport(UObject* Obj)
{
	USkeletalMesh* Mesh = Cast<USkeletalMesh>(Obj);
	if (!Mesh)
	{
		return EReimportResult::Failed;
	}
	UAssetImportData* ImportData = Mesh->GetAssetImportData();
	if (!ImportData || ImportData->GetSourceData().SourceFiles.Num() == 0)
	{
		return EReimportResult::Failed;
	}

	FullFileName = UAssetImportData::ResolveImportFilename(ImportData->GetSourceData().SourceFiles[0].RelativeFilename, Mesh->GetOutermost());
	ReimportMesh = Mesh;
	const uint8* Dummy = nullptr;
	bool bCanceled = false;
	UObject* Result = FactoryCreateBinary(USkeletalMesh::StaticClass(), Mesh->GetOutermost(), Mesh->GetFName(), Mesh->GetFlags(), nullptr, TEXT("pmx"), Dummy, Dummy, GWarn, bCanceled);
	ReimportMesh = nullptr;
	FullFileName.Empty();
	return Result ? EReimportResult::Succeeded : EReimportResult::Failed;
}

int32 UAMAP5Factory::GetPriority() const
{
	return ImportPriority;
}

#undef LOCTEXT_NAMESPACE





