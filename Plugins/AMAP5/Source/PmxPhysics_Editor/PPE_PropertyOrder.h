#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"

class IDetailChildrenBuilder;
class IPropertyHandle;

class FPPE_PropertyOrderCustomization : public IPropertyTypeCustomization
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
};
