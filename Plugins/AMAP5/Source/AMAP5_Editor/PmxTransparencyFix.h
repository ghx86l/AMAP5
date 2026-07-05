#pragma once

#include "CoreMinimal.h"

struct FAMAP5_PmxMaterialInfo
{
	FString Name;
	float Alpha = 1.0f;
};

class FAMAP5_PmxTransparencyFix
{
public:
	static bool ParsePmxMaterials(const FString& PmxFilePath, TArray<FAMAP5_PmxMaterialInfo>& OutMaterials, FString& OutError);

	static int32 ApplyDitherAlphaToFolder(const FString& ContentFolderPath, const TArray<FAMAP5_PmxMaterialInfo>& Materials);

	static void ExecuteFixCommand(const TArray<FString>& Args);
};
