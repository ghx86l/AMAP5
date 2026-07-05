#include "PmxTransparencyFix.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/FileHelper.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "StaticParameterSet.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogAMAP5PmxFix, Log, All);

namespace
{
	struct FPmxReader
	{
		const uint8* Data = nullptr;
		int64 Size = 0;
		int64 Pos = 0;
		bool bError = false;

		bool Ensure(int64 Bytes)
		{
			if (bError || Pos + Bytes > Size)
			{
				bError = true;
				return false;
			}
			return true;
		}

		void Skip(int64 Bytes)
		{
			if (Ensure(Bytes))
			{
				Pos += Bytes;
			}
		}

		uint8 ReadU8()
		{
			if (!Ensure(1))
			{
				return 0;
			}
			return Data[Pos++];
		}

		int32 ReadI32()
		{
			if (!Ensure(4))
			{
				return 0;
			}
			int32 V = 0;
			FMemory::Memcpy(&V, Data + Pos, 4);
			Pos += 4;
			return V;
		}

		float ReadF32()
		{
			if (!Ensure(4))
			{
				return 0.0f;
			}
			float V = 0.0f;
			FMemory::Memcpy(&V, Data + Pos, 4);
			Pos += 4;
			return V;
		}

		FString ReadText(uint8 Encoding)
		{
			const int32 Len = ReadI32();
			if (Len < 0 || !Ensure(Len))
			{
				bError = true;
				return FString();
			}
			FString Result;
			if (Len > 0)
			{
				if (Encoding == 0)
				{
					const int32 NumChars = Len / 2;
					TArray<TCHAR> Chars;
					Chars.SetNumUninitialized(NumChars + 1);
					for (int32 i = 0; i < NumChars; ++i)
					{
						uint16 C = 0;
						FMemory::Memcpy(&C, Data + Pos + i * 2, 2);
						Chars[i] = (TCHAR)C;
					}
					Chars[NumChars] = 0;
					Result = FString(Chars.GetData());
				}
				else
				{
					FUTF8ToTCHAR Converter((const ANSICHAR*)(Data + Pos), Len);
					Result = FString(Converter.Length(), Converter.Get());
				}
			}
			Pos += Len;
			return Result;
		}
	};
}

bool FAMAP5_PmxTransparencyFix::ParsePmxMaterials(const FString& PmxFilePath, TArray<FAMAP5_PmxMaterialInfo>& OutMaterials, FString& OutError)
{
	OutMaterials.Reset();

	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *PmxFilePath))
	{
		OutError = FString::Printf(TEXT("Failed to load file: %s"), *PmxFilePath);
		return false;
	}

	FPmxReader R;
	R.Data = FileData.GetData();
	R.Size = FileData.Num();

	if (!R.Ensure(4) || FMemory::Memcmp(R.Data, "PMX ", 4) != 0)
	{
		OutError = TEXT("Invalid PMX signature.");
		return false;
	}
	R.Skip(4);
	R.ReadF32();

	const uint8 GlobalsCount = R.ReadU8();
	if (GlobalsCount < 8)
	{
		OutError = TEXT("Invalid PMX globals count.");
		return false;
	}
	const uint8 Encoding = R.ReadU8();
	const uint8 AddUVCount = R.ReadU8();
	const uint8 VertexIndexSize = R.ReadU8();
	const uint8 TextureIndexSize = R.ReadU8();
	R.ReadU8();
	const uint8 BoneIndexSize = R.ReadU8();
	R.ReadU8();
	R.ReadU8();
	R.Skip(GlobalsCount - 8);

	R.ReadText(Encoding);
	R.ReadText(Encoding);
	R.ReadText(Encoding);
	R.ReadText(Encoding);

	const int32 VertexCount = R.ReadI32();
	if (VertexCount < 0)
	{
		OutError = TEXT("Invalid vertex count.");
		return false;
	}
	for (int32 v = 0; v < VertexCount && !R.bError; ++v)
	{
		R.Skip(32 + AddUVCount * 16);
		const uint8 WeightType = R.ReadU8();
		switch (WeightType)
		{
		case 0:
			R.Skip(BoneIndexSize);
			break;
		case 1:
			R.Skip(BoneIndexSize * 2 + 4);
			break;
		case 2:
			R.Skip(BoneIndexSize * 4 + 16);
			break;
		case 3:
			R.Skip(BoneIndexSize * 2 + 4 + 36);
			break;
		case 4:
			R.Skip(BoneIndexSize * 4 + 16);
			break;
		default:
			R.bError = true;
			break;
		}
		R.Skip(4);
	}

	const int32 FaceIndexCount = R.ReadI32();
	if (FaceIndexCount < 0)
	{
		OutError = TEXT("Invalid face index count.");
		return false;
	}
	R.Skip((int64)FaceIndexCount * VertexIndexSize);

	const int32 TextureCount = R.ReadI32();
	if (TextureCount < 0)
	{
		OutError = TEXT("Invalid texture count.");
		return false;
	}
	for (int32 t = 0; t < TextureCount && !R.bError; ++t)
	{
		R.ReadText(Encoding);
	}

	const int32 MaterialCount = R.ReadI32();
	if (MaterialCount < 0)
	{
		OutError = TEXT("Invalid material count.");
		return false;
	}
	for (int32 m = 0; m < MaterialCount && !R.bError; ++m)
	{
		FAMAP5_PmxMaterialInfo Info;
		Info.Name = R.ReadText(Encoding);
		R.ReadText(Encoding);
		R.ReadF32();
		R.ReadF32();
		R.ReadF32();
		Info.Alpha = R.ReadF32();
		R.Skip(12 + 4 + 12);
		R.ReadU8();
		R.Skip(16 + 4);
		R.Skip(TextureIndexSize);
		R.Skip(TextureIndexSize);
		R.ReadU8();
		const uint8 SharedToonFlag = R.ReadU8();
		if (SharedToonFlag == 0)
		{
			R.Skip(TextureIndexSize);
		}
		else
		{
			R.ReadU8();
		}
		R.ReadText(Encoding);
		R.ReadI32();

		if (!R.bError)
		{
			OutMaterials.Add(MoveTemp(Info));
		}
	}

	if (R.bError)
	{
		OutError = TEXT("PMX parse error (unexpected end of data or unknown weight type).");
		return false;
	}
	return true;
}

int32 FAMAP5_PmxTransparencyFix::ApplyDitherAlphaToFolder(const FString& ContentFolderPath, const TArray<FAMAP5_PmxMaterialInfo>& Materials)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	TArray<FAssetData> Assets;
	AssetRegistryModule.Get().GetAssetsByPath(FName(*ContentFolderPath), Assets, true);

	int32 PatchedCount = 0;
	const FName SwitchName(TEXT("bUseDitherAlpha"));
	const FName ScalarName(TEXT("DitherAlpha"));

	for (const FAssetData& AssetData : Assets)
	{
		if (AssetData.AssetClassPath != UMaterialInstanceConstant::StaticClass()->GetClassPathName())
		{
			continue;
		}

		const FString AssetName = AssetData.AssetName.ToString();

		const FAMAP5_PmxMaterialInfo* Matched = nullptr;
		for (const FAMAP5_PmxMaterialInfo& Info : Materials)
		{
			const FString Sanitized = ObjectTools::SanitizeObjectName(Info.Name);
			if (AssetName.Equals(Sanitized, ESearchCase::IgnoreCase) ||
				AssetName.EndsWith(FString(TEXT("_")) + Sanitized, ESearchCase::IgnoreCase))
			{
				Matched = &Info;
				break;
			}
		}
		if (!Matched)
		{
			UE_LOG(LogAMAP5PmxFix, Warning, TEXT("No PMX material matched MIC: %s"), *AssetName);
			continue;
		}
		if (Matched->Alpha >= 1.0f - KINDA_SMALL_NUMBER)
		{
			UE_LOG(LogAMAP5PmxFix, Log, TEXT("Skipped %s (alpha=1)"), *AssetName);
			continue;
		}

		UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(AssetData.GetAsset());
		if (!MIC)
		{
			continue;
		}

		FStaticParameterSet StaticParams = MIC->GetStaticParameters();
		bool bFound = false;
		for (FStaticSwitchParameter& Param : StaticParams.StaticSwitchParameters)
		{
			if (Param.ParameterInfo.Name == SwitchName)
			{
				Param.Value = true;
				Param.bOverride = true;
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			StaticParams.StaticSwitchParameters.Add(
				FStaticSwitchParameter(FMaterialParameterInfo(SwitchName), true, true, FGuid()));
		}
		MIC->UpdateStaticPermutation(StaticParams);

		MIC->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(ScalarName), Matched->Alpha);
		MIC->PostEditChange();
		MIC->MarkPackageDirty();

		UE_LOG(LogAMAP5PmxFix, Log, TEXT("Patched %s: bUseDitherAlpha=true, DitherAlpha=%f (PMX material: %s)"), *AssetName, Matched->Alpha, *Matched->Name);
		++PatchedCount;
	}

	return PatchedCount;
}

void FAMAP5_PmxTransparencyFix::ExecuteFixCommand(const TArray<FString>& Args)
{
	if (Args.Num() < 2)
	{
		UE_LOG(LogAMAP5PmxFix, Error, TEXT("Usage: AMAP5.FixPmxTransparency <PmxFilePath> <ContentFolderPath>"));
		return;
	}

	const FString PmxPath = Args[0];
	const FString FolderPath = Args[1];

	TArray<FAMAP5_PmxMaterialInfo> Materials;
	FString Error;
	if (!ParsePmxMaterials(PmxPath, Materials, Error))
	{
		UE_LOG(LogAMAP5PmxFix, Error, TEXT("%s"), *Error);
		return;
	}
	UE_LOG(LogAMAP5PmxFix, Log, TEXT("Parsed %d materials from %s"), Materials.Num(), *PmxPath);

	const int32 Patched = ApplyDitherAlphaToFolder(FolderPath, Materials);
	UE_LOG(LogAMAP5PmxFix, Log, TEXT("Patched %d material instances under %s"), Patched, *FolderPath);
}

static FAutoConsoleCommand GAMAP5FixPmxTransparencyCommand(
	TEXT("AMAP5.FixPmxTransparency"),
	TEXT("Apply PMX material alpha to VRM4U-generated MICs. Args: <PmxFilePath> <ContentFolderPath>"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&FAMAP5_PmxTransparencyFix::ExecuteFixCommand)
);
