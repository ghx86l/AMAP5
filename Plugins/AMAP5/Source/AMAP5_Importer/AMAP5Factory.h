#pragma once

#include "CoreMinimal.h"
#include "EditorReimportHandler.h"
#include "Factories/Factory.h"
#include "AMAP5Factory.generated.h"

class USkeletalMesh;

UENUM()
enum class EAMAP5ImportMaterialType : uint8
{
	DefaultSubsurfaceProfile UMETA(DisplayName = "Default (Subsurface Profile)"),
	MToonLit UMETA(DisplayName = "MToon Lit"),
	MToonUnlit UMETA(DisplayName = "MToon Unlit"),
	Subsurface UMETA(DisplayName = "Subsurface"),
	SubsurfaceProfile UMETA(DisplayName = "Subsurface Profile"),
	Unlit UMETA(DisplayName = "Unlit")
};

UCLASS()
class AMAP5_IMPORTER_API UAMAP5Factory : public UFactory, public FReimportHandler
{
	GENERATED_UCLASS_BODY()

	virtual bool FactoryCanImport(const FString& Filename) override;
	virtual UClass* ResolveSupportedClass() override;
	virtual UObject* FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;
	virtual UObject* FactoryCreateText(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn) override;

	virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
	virtual EReimportResult::Type Reimport(UObject* Obj) override;
	virtual int32 GetPriority() const override;

	UPROPERTY(EditAnywhere, Category = "Material")
	EAMAP5ImportMaterialType MaterialType = EAMAP5ImportMaterialType::DefaultSubsurfaceProfile;

	UPROPERTY(EditAnywhere, Category = "Material", meta = (ClampMin = "0.01"))
	float ImportScale = 10.0f;

private:
	FString FullFileName;
	TObjectPtr<USkeletalMesh> ReimportMesh = nullptr;
};
