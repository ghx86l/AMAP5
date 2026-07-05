#include "Command.h"

#define LOCTEXT_NAMESPACE "IK_Loader"

void FIK_LoaderCommands::RegisterCommands()
{
    UI_COMMAND(
        PluginAction,
        "IK_Loader",
        "IK_Loader Plugin Action",
        EUserInterfaceActionType::Button,
        FInputChord()
    );
}

#undef LOCTEXT_NAMESPACE
