// Copyright (c) 2025 Jeonghyeon Ha. All Rights Reserved.

#include "AMAP5_Importer.h"
#include "Modules/ModuleManager.h"
#include "Log.h"

DEFINE_LOG_CATEGORY(LogPMXImporter);

void FAMAP5_ImporterModule::StartupModule()
{
	UE_LOG(LogPMXImporter, Display, TEXT("AMAP5 PMX Factory module started"));
}

void FAMAP5_ImporterModule::ShutdownModule()
{
	UE_LOG(LogPMXImporter, Verbose, TEXT("AMAP5 PMX Factory module shutdown"));
}

IMPLEMENT_MODULE(FAMAP5_ImporterModule, AMAP5_Importer);
