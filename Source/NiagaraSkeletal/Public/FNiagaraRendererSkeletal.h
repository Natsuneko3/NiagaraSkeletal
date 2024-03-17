#pragma once
#include "Engine/EngineTypes.h"
#include "NiagaraRenderer.h"

class UNiagaraSkeletalRendererProperties;



struct FNiagaraParticleData
{
public:
	FNiagaraParticleData(const UNiagaraSkeletalRendererProperties* Properties,FNiagaraDataSet& Data,int32 ParticleIndex);
	FNiagaraPosition Position;
	FVector3f Rotate ;
	FVector3f Scale ;
	float SkeletalAnimTime ;
	int  VisTag;
	int AnimIndex ;
	int32 UniqueID ;
	bool Enabled;

};



class FNiagaraRendererSkeletal : public FNiagaraRenderer
{
public:

	FNiagaraRendererSkeletal(ERHIFeatureLevel::Type FeatureLevel, const UNiagaraRendererProperties *InProps, const FNiagaraEmitterInstance* Emitter);
	virtual ~FNiagaraRendererSkeletal();

	//FNiagaraRenderer interface
	virtual void DestroyRenderState_Concurrent() override;
	virtual void PostSystemTick_GameThread(const UNiagaraRendererProperties* InProperties, const FNiagaraEmitterInstance* Emitter) override;
	virtual void OnSystemComplete_GameThread(const UNiagaraRendererProperties* InProperties, const FNiagaraEmitterInstance* Emitter) override;
	//FNiagaraRenderer interface END

private:
	struct FComponentPoolEntry
	{
		TWeakObjectPtr<USkeletalMeshComponent> Component;
		double LastActiveTime = 0.0;
		int32 LastAssignedToParticleID = -1;
	};
	

	// if the niagara component is not attached to an actor, we need to spawn and keep track of a temporary actor
	TWeakObjectPtr<AActor> SpawnedOwner;

	void ResetComponentPool(bool bResetOwner);
	// all of the spawned components
	TArray<FComponentPoolEntry> ComponentPool;

	void SetSkeletalMaterials(const UNiagaraSkeletalRendererProperties* Properties,USkeletalMeshComponent* SkeletalMeshComponent,const FNiagaraEmitterInstance* Emitter,FNiagaraParticleData* PerParticleData);
	
};
