// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"
#include "Style.h"

class FIK_LoaderCommands : public TCommands<FIK_LoaderCommands>
{
public:

	FIK_LoaderCommands()
		: TCommands<FIK_LoaderCommands>(TEXT("IK_Loader"), NSLOCTEXT("Contexts", "IK_Loader", "IK_Loader Plugin"), NAME_None, FIK_LoaderStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > PluginAction;
};
