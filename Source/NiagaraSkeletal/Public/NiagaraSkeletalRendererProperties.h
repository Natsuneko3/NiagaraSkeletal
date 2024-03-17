// Copyright Natsu Neko, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraCommon.h"
#include "NiagaraDataSetAccessor.h"
#include "NiagaraRendererProperties.h"

#include "NiagaraSkeletalRendererProperties.generated.h"

class FNiagaraEmitterInstance;

USTRUCT()
struct FNiagaraSkeletalReference
{

	GENERATED_USTRUCT_BODY()
	FNiagaraSkeletalReference();
	UPROPERTY(EditAnywhere,Category = "Skeletal")
	TObjectPtr<USkeletalMesh> SkeletalMesh;
	
	UPROPERTY(EditAnywhere,Category = "Skeletal")
	FNiagaraUserParameterBinding SkeletalMeshUserParameterBinding;
	
	UPROPERTY(EditAnywhere,Category = "Skeletal")
	TArray<FNiagaraMeshMaterialOverride> OverrideMaterials;
	
};

UCLASS(editinlinenew,MinimalAPI, meta = (DisplayName = "Skeletal Renderer"))
class  UNiagaraSkeletalRendererProperties : public UNiagaraRendererProperties
{
	GENERATED_BODY()
public:
	UNiagaraSkeletalRendererProperties();
	//UObject Interface
	virtual void PostLoad() override;
	virtual void PostInitProperties() override;
	//UObject Interface END

	static void InitCDOPropertiesAfterModuleStartup();

	//~ UNiagaraRendererProperties interface
	virtual FNiagaraRenderer* CreateEmitterRenderer(ERHIFeatureLevel::Type FeatureLevel, const FNiagaraEmitterInstance* Emitter, const FNiagaraSystemInstanceController& InController) override;
	virtual class FNiagaraBoundsCalculator* CreateBoundsCalculator() override { return nullptr; }
	virtual bool IsSimTargetSupported(ENiagaraSimTarget InSimTarget) const override { return InSimTarget == ENiagaraSimTarget::CPUSim; };
	virtual void GetUsedMaterials(const FNiagaraEmitterInstance* InEmitter, TArray<UMaterialInterface*>& OutMaterials) const override;
	virtual bool PopulateRequiredBindings(FNiagaraParameterStore& InParameterStore)  override;
	virtual void CacheFromCompiledData(const FNiagaraDataSetCompiledData* CompiledData) override;

#if WITH_EDITORONLY_DATA
	virtual void GetRendererWidgets(const FNiagaraEmitterInstance* InEmitter, TArray<TSharedPtr<SWidget>>& OutWidgets, TSharedPtr<FAssetThumbnailPool> InThumbnailPool) const override;
	virtual const FSlateBrush* GetStackIcon() const override;
	virtual void GetRendererTooltipWidgets(const FNiagaraEmitterInstance* InEmitter, TArray<TSharedPtr<SWidget>>& OutWidgets, TSharedPtr<FAssetThumbnailPool> InThumbnailPool) const override;
	virtual const TArray<FNiagaraVariable>& GetOptionalAttributes() override;
	virtual void GetRendererFeedback(const FVersionedNiagaraEmitter& InEmitter, TArray<FNiagaraRendererFeedback>& OutErrors, TArray<FNiagaraRendererFeedback>& OutWarnings, TArray<FNiagaraRendererFeedback>& OutInfo) const override;
#endif // WITH_EDITORONLY_DATA
	
	virtual bool NeedsSystemPostTick() const override { return true; }
	virtual bool NeedsSystemCompletion() const override { return true; }
	virtual bool NeedsMIDsForMaterials() const override { return MaterialParameters.HasAnyBindings(); }
	
	UPROPERTY(EditAnywhere, Category = "SkeletalRendering")
	TArray<FNiagaraSkeletalReference> SkeletalMeshes;

	UPROPERTY(EditAnywhere, Category = "SkeletalRendering")
	TArray<TObjectPtr<UAnimationAsset>> Animations;

	UPROPERTY(EditAnywhere, Category = "SkeletalRendering")
	int32 RendererVisibility;

	UPROPERTY(EditAnywhere, Category = "SkeletalRendering", meta = (ClampMin = 1))
	uint32 ComponentCountLimit = 30;

	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "SkeletalRendering")
	bool bAssignComponentsOnParticleID = true;

	UPROPERTY(EditAnywhere,Category = "Bindings")
	FNiagaraVariableAttributeBinding PositionBinding;

	UPROPERTY(EditAnywhere,Category = "Bindings")
	FNiagaraVariableAttributeBinding RotationBinding;

	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding ScaleBinding;
	
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding AnimTimeBinding;

	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding AnimIndexBinding;
	
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding EnabledBinding;
	
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding RendererVisibilityTagBinding;

	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraRendererMaterialParameters MaterialParameters;
	
	FNiagaraDataSetAccessor<FNiagaraPosition>	PositionAccessor;
	FNiagaraDataSetAccessor<FVector3f>	RotateAccessor;
	FNiagaraDataSetAccessor<FVector3f>  ScaleAccessor;
	FNiagaraDataSetAccessor<FNiagaraBool>		EnabledAccessor;
	FNiagaraDataSetAccessor<float>		AnimTimeAccessor;
	FNiagaraDataSetAccessor<int32>		VisTagAccessor;
	FNiagaraDataSetAccessor<int32>		AnimIndexAccessor;
	FNiagaraDataSetAccessor<int32>		UniqueIDAccessor;
	
	
protected:
	void InitBindings();
	static void InitDefaultAttributes();
	static FNiagaraVariable Particles_Age;
	static FNiagaraVariable Particles_Rotate;
	static FNiagaraVariable Particles_AnimIndex;
	static FNiagaraVariable Particles_Enabled;
private:
	static TArray<TWeakObjectPtr<UNiagaraSkeletalRendererProperties>> SkeletalRendererPropertiesToDeferredInit;
	
};
