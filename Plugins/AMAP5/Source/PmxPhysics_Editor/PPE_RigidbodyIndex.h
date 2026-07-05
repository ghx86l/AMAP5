#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "Types/SlateEnums.h"

class IDetailChildrenBuilder;
class IPropertyHandle;
class SWidget;

class FPPE_RigidbodyIndexCustomization : public IPropertyTypeCustomization
{
public:
    static TSharedRef<IPropertyTypeCustomization> MakeInstance();
    static FName GetCustomizedStructName();

    virtual void CustomizeHeader(
        TSharedRef<IPropertyHandle> StructPropertyHandle,
        FDetailWidgetRow& HeaderRow,
        IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

    virtual void CustomizeChildren(
        TSharedRef<IPropertyHandle> StructPropertyHandle,
        IDetailChildrenBuilder& ChildBuilder,
        IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

private:
    struct FRigidbodyIndexOption
    {
        FRigidbodyIndexOption(int32 InIndex, const FText& InText)
            : Index(InIndex)
            , Text(InText)
        {}

        int32 Index;
        FText Text;
    };

    void BuildOptions(const TSharedRef<IPropertyHandle>& StructPropertyHandle);
    void AddRigidbodyIndexRow(IDetailChildrenBuilder& ChildBuilder, TSharedRef<IPropertyHandle> PropertyHandle);
    TSharedRef<SWidget> MakeOptionWidget(TSharedPtr<FRigidbodyIndexOption> Option) const;
    FText GetValueText(TSharedPtr<IPropertyHandle> PropertyHandle) const;
    void OnSelectionChanged(
        TSharedPtr<FRigidbodyIndexOption> Selection,
        ESelectInfo::Type SelectInfo,
        TSharedPtr<IPropertyHandle> PropertyHandle);
    TSharedPtr<FRigidbodyIndexOption> FindOption(int32 Index) const;
    TSharedPtr<FRigidbodyIndexOption> GetSelectedOption(TSharedPtr<IPropertyHandle> PropertyHandle) const;

    TArray<TSharedPtr<FRigidbodyIndexOption>> RigidbodyOptions;
};
