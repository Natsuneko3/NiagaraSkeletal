// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraSkeletal.h"

#include "NiagaraEditorModule.h"
#include "NiagaraEditorModule.h"
#include "NiagaraSkeletalRendererProperties.h"
#include "ShaderCore.h"
#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FNiagaraSkeletalModule"

class FNiagaraEditorModule;

void FNiagaraSkeletalModule::StartupModule()
{
	const FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("NiagaraSkeletal"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/NiagaraSkeletal"), PluginShaderDir);

	UNiagaraSkeletalRendererProperties::InitCDOPropertiesAfterModuleStartup();
#if WITH_EDITOR
	FNiagaraEditorModule& NiagaraEditorModule = FNiagaraEditorModule::Get();
	NiagaraEditorModule.RegisterRendererCreationInfo(FNiagaraRendererCreationInfo(
		UNiagaraSkeletalRendererProperties::StaticClass()->GetDisplayNameText(),
		FText::FromString(UNiagaraSkeletalRendererProperties::StaticClass()->GetDescription()),
		UNiagaraSkeletalRendererProperties::StaticClass()->GetClassPathName(),
		FNiagaraRendererCreationInfo::FRendererFactory::CreateLambda([](UObject* OuterEmitter)
		{
			UNiagaraSkeletalRendererProperties* NewRenderer = NewObject<UNiagaraSkeletalRendererProperties>(OuterEmitter, NAME_None, RF_Transactional);
			return NewRenderer;
		})));
#endif
}

void FNiagaraSkeletalModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FNiagaraSkeletalModule, NiagaraSkeletal)