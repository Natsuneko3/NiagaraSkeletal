// Copyright Natsu Neko, Inc. All Rights Reserved.


#include "NiagaraSkeletalRendererProperties.h"
#include "Engine/SkinnedAssetCommon.h"
#include "AssetThumbnail.h"
#include "FNiagaraRendererSkeletal.h"
#include "NiagaraConstants.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraModule.h"
#include "Styling/SlateIconFinder.h"



FNiagaraVariable UNiagaraSkeletalRendererProperties::Particles_Age;
FNiagaraVariable UNiagaraSkeletalRendererProperties::Particles_Rotate;
FNiagaraVariable UNiagaraSkeletalRendererProperties::Particles_AnimIndex;
FNiagaraVariable UNiagaraSkeletalRendererProperties::Particles_Enabled;
TArray<TWeakObjectPtr<UNiagaraSkeletalRendererProperties>> UNiagaraSkeletalRendererProperties::SkeletalRendererPropertiesToDeferredInit;

#define LOCTEXT_NAMESPACE "UNiagaraSkeletalRendererProperties"

FNiagaraSkeletalReference::FNiagaraSkeletalReference()
	: SkeletalMesh(nullptr)
	, SkeletalMeshUserParameterBinding(FNiagaraTypeDefinition(UObject::StaticClass()))
{
}


UNiagaraSkeletalRendererProperties::UNiagaraSkeletalRendererProperties()
{
	AttributeBindings.Reserve(7);
	AttributeBindings.Add(&PositionBinding);
	AttributeBindings.Add(&RotationBinding);
	AttributeBindings.Add(&ScaleBinding);
	AttributeBindings.Add(&AnimTimeBinding);
	AttributeBindings.Add(&AnimIndexBinding);
	AttributeBindings.Add(&RendererVisibilityTagBinding);
	AttributeBindings.Add(&EnabledBinding);
	if(SkeletalMeshes.Num() == 0)
	{
		SkeletalMeshes.AddDefaulted();
	}
	SkeletalMeshes.Shrink();
}

void UNiagaraSkeletalRendererProperties::PostLoad()
{
	Super::PostLoad();
	
	PostLoadBindings(GetCurrentSourceMode());
}

void UNiagaraSkeletalRendererProperties::PostInitProperties()
{
	Super::PostInitProperties();
	InitDefaultAttributes();
	InitBindings();
}

void UNiagaraSkeletalRendererProperties::InitCDOPropertiesAfterModuleStartup()
{
	InitDefaultAttributes();

	UNiagaraSkeletalRendererProperties* CDO = CastChecked<UNiagaraSkeletalRendererProperties>(StaticClass()->GetDefaultObject());
	CDO->InitBindings();

	for (TWeakObjectPtr<UNiagaraSkeletalRendererProperties>& WeakProceduralRendererProperties : SkeletalRendererPropertiesToDeferredInit)
	{
		if (WeakProceduralRendererProperties.Get())
		{
			WeakProceduralRendererProperties->InitBindings();
		}
	}
}

FNiagaraRenderer* UNiagaraSkeletalRendererProperties::CreateEmitterRenderer(ERHIFeatureLevel::Type FeatureLevel, const FNiagaraEmitterInstance* Emitter,
                                                                            const FNiagaraSystemInstanceController& InController)
{
	FNiagaraRenderer* NewRenderer = new FNiagaraRendererSkeletal(FeatureLevel, this, Emitter);
	NewRenderer->Initialize(this, Emitter, InController);
	return NewRenderer;
}

void UNiagaraSkeletalRendererProperties::GetUsedMaterials(const FNiagaraEmitterInstance* InEmitter, TArray<UMaterialInterface*>& OutMaterials) const
{
	for (const FNiagaraSkeletalReference& Entry : SkeletalMeshes)
	{
		if (Entry.SkeletalMesh)
		{
			TArray<FSkeletalMaterial> &SkeletalMaterials = Entry.SkeletalMesh->GetMaterials();
			const int32 MaxIndex = FMath::Max(SkeletalMaterials.Num(), Entry.OverrideMaterials.Num());
			OutMaterials.Reserve(MaxIndex);
			for (int i = 0; i < MaxIndex; i++)
			{
				if (Entry.OverrideMaterials.IsValidIndex(i) )
				{
					if(Entry.OverrideMaterials[i].UserParamBinding.Parameter.IsValid())
					{
						UMaterialInterface* OverrideMatUsedBinding  = Cast<UMaterialInterface>(InEmitter->FindBinding(Entry.OverrideMaterials[i].UserParamBinding.Parameter));
				
						OutMaterials.Add(ToRawPtr(OverrideMatUsedBinding));
					}else
					{
						OutMaterials.Add(ToRawPtr(Entry.OverrideMaterials[i].ExplicitMat));
					}
					
				}
				else if (SkeletalMaterials.IsValidIndex(i))
				{
					OutMaterials.Add(SkeletalMaterials[i].MaterialInterface);
				}
			}
		}
	}
	
}
//store parameter and material binding data 
bool UNiagaraSkeletalRendererProperties::PopulateRequiredBindings(FNiagaraParameterStore& InParameterStore)
{
	bool bAnyAdded = Super::PopulateRequiredBindings(InParameterStore);

	for (const FNiagaraVariableAttributeBinding* Binding : AttributeBindings)
	{
		if (Binding && Binding->CanBindToHostParameterMap())
		{
			InParameterStore.AddParameter(Binding->GetParamMapBindableVariable(), false);
			bAnyAdded = true;
		}
	}
	for (FNiagaraMaterialAttributeBinding& MaterialParamBinding : MaterialParameters.AttributeBindings)
	{
		InParameterStore.AddParameter(MaterialParamBinding.GetParamMapBindableVariable(), false);
		bAnyAdded = true;
	}
	for (const FNiagaraSkeletalReference& Entry : SkeletalMeshes)
	{
		FNiagaraVariable Variable = Entry.SkeletalMeshUserParameterBinding.Parameter;
		if (Variable.IsValid())
		{
			InParameterStore.AddParameter(Variable, false);
			bAnyAdded = true;
		}
	}

	return bAnyAdded;
	
}
// transfer data to renderer
void UNiagaraSkeletalRendererProperties::CacheFromCompiledData(const FNiagaraDataSetCompiledData* CompiledData)
{
	Super::CacheFromCompiledData(CompiledData);
	InitParticleDataSetAccessor(PositionAccessor,CompiledData,PositionBinding);
	InitParticleDataSetAccessor(RotateAccessor,CompiledData,RotationBinding);
	InitParticleDataSetAccessor(ScaleAccessor,CompiledData,PositionBinding);
	InitParticleDataSetAccessor(AnimTimeAccessor,CompiledData,AnimTimeBinding);
	InitParticleDataSetAccessor(VisTagAccessor,CompiledData,RendererVisibilityTagBinding);
	InitParticleDataSetAccessor(AnimIndexAccessor,CompiledData,AnimIndexBinding);
	InitParticleDataSetAccessor(EnabledAccessor,CompiledData,EnabledBinding);
	UniqueIDAccessor.Init(CompiledData, FName("UniqueID"));
}


void UNiagaraSkeletalRendererProperties::GetRendererWidgets(const FNiagaraEmitterInstance* InEmitter, TArray<TSharedPtr<SWidget>>& OutWidgets, TSharedPtr<FAssetThumbnailPool> InThumbnailPool) const
{
	TSharedRef<SWidget> DefaultThumbnailWidget = SNew(SImage)
		.Image(FSlateIconFinder::FindIconBrushForClass(StaticClass()));

	int32 ThumbnailSize = 32;
	for(const FNiagaraSkeletalReference& Entry : SkeletalMeshes)
	{
		TSharedPtr<SWidget> ThumbnailWidget = DefaultThumbnailWidget;
		
		if (Entry.SkeletalMesh != nullptr)
		{
			TSharedPtr<FAssetThumbnail> AssetThumbnail = MakeShareable(new FAssetThumbnail(Entry.SkeletalMesh , ThumbnailSize, ThumbnailSize, InThumbnailPool));
			ThumbnailWidget = AssetThumbnail->MakeThumbnailWidget();
		}
		
		OutWidgets.Add(ThumbnailWidget);
	}

	if (SkeletalMeshes.Num() == 0)
	{
		OutWidgets.Add(DefaultThumbnailWidget);
	}
}

const FSlateBrush* UNiagaraSkeletalRendererProperties::GetStackIcon() const
{
	return FSlateIconFinder::FindIconBrushForClass(GetClass());
}

void UNiagaraSkeletalRendererProperties::GetRendererTooltipWidgets(const FNiagaraEmitterInstance* InEmitter, TArray<TSharedPtr<SWidget>>& OutWidgets,
	TSharedPtr<FAssetThumbnailPool> InThumbnailPool) const
{
	//OutWidgets.Add(SNew(SImage).Image(FSlateIconFinder::FindIconBrushForClass(GetClass())));
	TSharedRef<SWidget> DefaultGeoCacheTooltip = SNew(STextBlock)
		.Text(LOCTEXT("SkeletaleRenderer", "Skeletale Renderer"));
	
	TArray<TSharedPtr<SWidget>> RendererWidgets;
	if (SkeletalMeshes.Num() > 0)
	{
		GetRendererWidgets(InEmitter, RendererWidgets, InThumbnailPool);
	}
	
	for(int32 Index = 0; Index < SkeletalMeshes.Num(); Index++)
	{
		const FNiagaraSkeletalReference& Entry = SkeletalMeshes[Index];
		
		TSharedPtr<SWidget> TooltipWidget = DefaultGeoCacheTooltip;		
		// we make sure to reuse the asset widget as a thumbnail if the geometry cache is valid
		if(Entry.SkeletalMesh)
		{
			TooltipWidget = RendererWidgets[Index];
		}

		// we override the previous thumbnail tooltip with a text indicating user parameter binding, if it exists
		if(Entry.SkeletalMeshUserParameterBinding.Parameter.IsValid())
		{
			TooltipWidget = SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("GeoCacheBoundTooltip", "Geometry cache slot is bound to user parameter {0}"), FText::FromName(Entry.SkeletalMeshUserParameterBinding.Parameter.GetName())));
		}
		
		OutWidgets.Add(TooltipWidget);
	}

	if (SkeletalMeshes.Num() == 0)
	{
		OutWidgets.Add(DefaultGeoCacheTooltip);
	}
}

const TArray<FNiagaraVariable>& UNiagaraSkeletalRendererProperties::GetOptionalAttributes()
{
	static TArray<FNiagaraVariable> Attrs;
	if (Attrs.Num() == 0)
	{
		Attrs.Add(SYS_PARAM_PARTICLES_POSITION);
		Attrs.Add(SYS_PARAM_PARTICLES_SCALE);
		Attrs.Add(SYS_PARAM_PARTICLES_VISIBILITY_TAG);
		Attrs.Add(Particles_Age);
		Attrs.Add(Particles_Rotate);
		Attrs.Add(Particles_AnimIndex);
		Attrs.Add(Particles_Enabled);
	}
	return Attrs;
}

void UNiagaraSkeletalRendererProperties::GetRendererFeedback(const FVersionedNiagaraEmitter& InEmitter,
	TArray<FNiagaraRendererFeedback>& OutErrors, TArray<FNiagaraRendererFeedback>& OutWarnings,
	TArray<FNiagaraRendererFeedback>& OutInfo) const
{
	Super::GetRendererFeedback(InEmitter, OutErrors, OutWarnings, OutInfo);

	if (MaterialParameters.HasAnyBindings())
	{
		TArray<UMaterialInterface*> Materials;
		GetUsedMaterials(nullptr, Materials);
		
		//MaterialParameters.GetFeedback(Materials,OutWarnings);//GetFeedback(Materials, OutWarnings);
		
	}
}


template<typename T>
FNiagaraVariableAttributeBinding CreateDefaultBinding(FNiagaraVariable DefaultValue, const T& DefaultValueData)
{
	DefaultValue.SetValue(DefaultValueData);
	FNiagaraVariableAttributeBinding Binding;
	Binding.Setup(DefaultValue, DefaultValue);
	return Binding;
}
void UNiagaraSkeletalRendererProperties::InitBindings()
{
	if(!PositionBinding.IsValid())
	{
		PositionBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_POSITION);
		RotationBinding = CreateDefaultBinding(Particles_Rotate,FVector3f(0.0f));
		ScaleBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_SCALE);
		RendererVisibilityTagBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_VISIBILITY_TAG);
		AnimTimeBinding = CreateDefaultBinding(Particles_Age,0.0f);
		AnimIndexBinding = CreateDefaultBinding(Particles_AnimIndex,0);
		EnabledBinding = CreateDefaultBinding(Particles_Enabled,true);
	}
}

void UNiagaraSkeletalRendererProperties::InitDefaultAttributes()
{
	if(!Particles_Age.IsValid())
	{
		Particles_Age = FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(),TEXT("Particles.Age"));
	}
	if(!Particles_Rotate.IsValid())
	{
		Particles_Rotate = FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(),TEXT("Particles.Rotate"));
	}
	if(!Particles_AnimIndex.IsValid())
	{
		Particles_AnimIndex = FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(),TEXT("Particles.AnimIndex"));
	}
	if(!Particles_Enabled.IsValid())
	{
		Particles_Enabled = FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(),TEXT("Particles.Visibility"));
	}
}
