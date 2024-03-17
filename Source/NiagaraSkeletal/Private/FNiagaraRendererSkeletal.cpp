#include "FNiagaraRendererSkeletal.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraSkeletalRendererProperties.h"
#include "NiagaraSystemInstance.h"

FNiagaraParticleData::FNiagaraParticleData(const UNiagaraSkeletalRendererProperties* Properties, FNiagaraDataSet& Data, int32 ParticleIndex)
{
	
	FNiagaraDataSetReaderInt32<FNiagaraBool> EnabledReader = Properties->EnabledAccessor.GetReader(Data);
	const FNiagaraDataSetReaderFloat<FNiagaraPosition>  PositionAccessor = Properties->PositionAccessor.GetReader(Data);
	const FNiagaraDataSetReaderFloat<FVector3f> RotateAccessor = Properties->RotateAccessor.GetReader(Data);
	const FNiagaraDataSetReaderFloat<FVector3f> ScaleAccessor = Properties->ScaleAccessor.GetReader(Data);
	const FNiagaraDataSetReaderFloat<float> SkeletalAnimTimeAccessor = Properties->AnimTimeAccessor.GetReader(Data);
	const FNiagaraDataSetReaderInt32<int>  VisTagAccessor = Properties->VisTagAccessor.GetReader(Data);
	const FNiagaraDataSetReaderInt32<int> AnimIndexAccessor = Properties->AnimIndexAccessor.GetReader(Data);
	const FNiagaraDataSetReaderInt32<int32> UniqueIDReaderAccessor = Properties->UniqueIDAccessor.GetReader(Data);
	

	Enabled = EnabledReader.GetSafe(ParticleIndex,true);
	Position = PositionAccessor.GetSafe(ParticleIndex,FNiagaraPosition(ForceInit));
	Rotate = RotateAccessor.GetSafe(ParticleIndex,FVector3f::Zero());
	Scale = ScaleAccessor.GetSafe(ParticleIndex,FVector3f::One());
	SkeletalAnimTime = SkeletalAnimTimeAccessor.GetSafe(ParticleIndex,0.0f);
	VisTag = VisTagAccessor.GetSafe(ParticleIndex,0);
	AnimIndex = AnimIndexAccessor.GetSafe(ParticleIndex,0);
	UniqueID = UniqueIDReaderAccessor.GetSafe(ParticleIndex,-1);
	
}

FNiagaraRendererSkeletal::FNiagaraRendererSkeletal(ERHIFeatureLevel::Type FeatureLevel, const UNiagaraRendererProperties* InProps, const FNiagaraEmitterInstance* Emitter)
	:FNiagaraRenderer(FeatureLevel,InProps,Emitter)
{
	const UNiagaraSkeletalRendererProperties* Properties = CastChecked<const UNiagaraSkeletalRendererProperties>(InProps);
	ComponentPool.Reserve(Properties->ComponentCountLimit);
	
}

FNiagaraRendererSkeletal::~FNiagaraRendererSkeletal()
{
	check(ComponentPool.Num() == 0);
}

void FNiagaraRendererSkeletal::DestroyRenderState_Concurrent()
{
	AsyncTask(
			ENamedThreads::GameThread,
			[Pool_GT=MoveTemp(ComponentPool), Owner_GT=MoveTemp(SpawnedOwner)]()
			{
				// we do not reset ParticlesWithComponents here because it's possible the render state is destroyed without destroying the renderer. In this case we want to know which particles
				// had spawned some components previously
				for (auto& PoolEntry : Pool_GT)
				{
					if (USkeletalMeshComponent* Component = PoolEntry.Component.Get())
					{
						Component->DestroyComponent();
					}
				}

				if (AActor* OwnerActor = Owner_GT.Get())
				{
					OwnerActor->Destroy();
				}
			}
		);
	SpawnedOwner.Reset();
}

void FNiagaraRendererSkeletal::PostSystemTick_GameThread(const UNiagaraRendererProperties* InProperties, const FNiagaraEmitterInstance* Emitter)
{
	FNiagaraSystemInstance* SystemInstance = Emitter->GetParentSystemInstance();

	//Bail if we don't have the required attributes to render this emitter.
	const UNiagaraSkeletalRendererProperties* Properties = CastChecked<const UNiagaraSkeletalRendererProperties>(InProperties);
	if (!SystemInstance || !Properties || Properties->SkeletalMeshes.Num() == 0 || SimTarget == ENiagaraSimTarget::GPUComputeSim)
	{
		return;
	}
#if WITH_EDITORONLY_DATA
	if (SystemInstance->GetIsolateEnabled() && !Emitter->GetEmitterHandle().IsIsolated())
	{
		ResetComponentPool(true);
		return;
	}
#endif
	USceneComponent* AttachComponent = SystemInstance->GetAttachComponent();
	if (!AttachComponent)
	{
		// we can't attach the components anywhere, so just bail
		return;
	}
	
	//const FNiagaraParameterStore& ParameterStore = Emitter->GetRendererBoundVariables();
	
	FNiagaraDataSet& Data = Emitter->GetData();
	FNiagaraDataBuffer& ParticleData = Data.GetCurrentDataChecked();
	
	const bool bIsRendererEnabled = IsRendererEnabled(InProperties, Emitter);
	
	TMap<int32, int32> ParticlesWithComponents;
	TArray<int32> FreeList;
	if (Properties->bAssignComponentsOnParticleID && ComponentPool.Num() > 0)
	{
		FreeList.Reserve(ComponentPool.Num());

		// Determine the slots that were assigned to particles last frame
		TMap<int32, int32> UsedSlots;
		UsedSlots.Reserve(ComponentPool.Num());
		for (int32 EntryIndex = 0; EntryIndex < ComponentPool.Num(); ++EntryIndex)
		{
			FComponentPoolEntry& Entry = ComponentPool[EntryIndex];
			if (Entry.LastAssignedToParticleID >= 0)
			{
				UsedSlots.Emplace(Entry.LastAssignedToParticleID, EntryIndex);
			}
			else
			{
				FreeList.Add(EntryIndex);
			}
		}
	
		// Ensure the final list only contains particles that are alive and enabled
		ParticlesWithComponents.Reserve(UsedSlots.Num());
		for (uint32 ParticleIndex = 0; ParticleIndex < ParticleData.GetNumInstances(); ParticleIndex++)
		{
			FNiagaraParticleData PerParticleData = FNiagaraParticleData(Properties,Data,ParticleIndex);
			int32 ParticleID = PerParticleData.UniqueID;
			int32 PoolIndex;
			
			if (UsedSlots.RemoveAndCopyValue(ParticleID, PoolIndex))
			{
				if (PerParticleData.Enabled)
				{
					ParticlesWithComponents.Emplace(ParticleID, PoolIndex);
				}
				else
				{
					// Particle has disabled components since last tick, ensure the component for this entry gets deactivated before re-use
					USceneComponent* Component = ComponentPool[PoolIndex].Component.Get();
					if (Component && Component->IsActive())
					{
						Component->Deactivate();
						Component->SetVisibility(false, true);
					}
					FreeList.Add(PoolIndex);
					ComponentPool[PoolIndex].LastAssignedToParticleID = -1;
				}
			}
		}

		// Any remaining in the used slots are now free to be reclaimed, due to their particles either dying or having their component disabled
		for (TPair<int32, int32> UsedSlot : UsedSlots)
		{
			// Particle has died since last tick, ensure the component for this entry gets deactivated before re-use
			USceneComponent* Component = ComponentPool[UsedSlot.Value].Component.Get();
			if (Component && Component->IsActive())
			{
				Component->Deactivate();
				Component->SetVisibility(false, true);
			}
			FreeList.Add(UsedSlot.Value);
			ComponentPool[UsedSlot.Value].LastAssignedToParticleID = -1;
		}
	}

	const int32 MaxComponents = Properties->ComponentCountLimit;
	int32 ComponentCount = 0;
	
	for(uint32 ParticleIndex = 0;ParticleIndex<ParticleData.GetNumInstances();ParticleIndex++)
	{
		FNiagaraParticleData PerParticleData = FNiagaraParticleData(Properties,Data,ParticleIndex);
		if (!bIsRendererEnabled || !PerParticleData.Enabled)
		{
			// Skip particles that don't want a component
			continue;
		}
		
		int32 ParticleID = -1;
		int32 PoolIndex = -1;
		if (Properties->bAssignComponentsOnParticleID)
		{
			// Get the particle ID and see if we have any components already assigned to the particle
			ParticleID = PerParticleData.UniqueID;
			ParticlesWithComponents.RemoveAndCopyValue(ParticleID, PoolIndex);
		}

		if (PoolIndex == -1 && ComponentCount + ParticlesWithComponents.Num() >= MaxComponents)
		{
			// The pool is full and there aren't any unused slots to claim
			continue;
		}

		// Acquire a component for this particle
		USkeletalMeshComponent* SkeletalMeshComponent = nullptr;
		if (PoolIndex == -1)
		{
			// Start by trying to pull from the pool
			if (!Properties->bAssignComponentsOnParticleID)
			{
				// We can just take the next slot
				PoolIndex = ComponentCount < ComponentPool.Num() ? ComponentCount : -1;
			}
			else if (FreeList.Num())
			{
				PoolIndex = FreeList.Pop(false);
			}
		}

		if (PoolIndex >= 0)
		{
			SkeletalMeshComponent = ComponentPool[PoolIndex].Component.Get();
		}

		bool bCreateNewComponent = !SkeletalMeshComponent || SkeletalMeshComponent->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed);
		
		
		if(!Properties->SkeletalMeshes.IsValidIndex(PerParticleData.VisTag)||!Properties->SkeletalMeshes[PerParticleData.VisTag].SkeletalMesh)
		{
			return;
		}
		
		if(bCreateNewComponent)
		{
			AActor* OwnerActor = SpawnedOwner.Get();
			if (OwnerActor == nullptr)
			{
				OwnerActor = AttachComponent->GetOwner();
				if (OwnerActor == nullptr)
				{
					// NOTE: This can happen with spawned systems
					OwnerActor = AttachComponent->GetWorld()->SpawnActor<AActor>();
					OwnerActor->SetFlags(RF_Transient);
					SpawnedOwner = OwnerActor;
				}
			}
			

			int32 AnimeIndex = FMath::Min(PerParticleData.AnimIndex,Properties->Animations.Num() - 1);
			int32 SkeletalIndex = FMath::Min(PerParticleData.VisTag,Properties->Animations.Num() - 1);
			
			SkeletalMeshComponent = NewObject<USkeletalMeshComponent>(OwnerActor);
			SkeletalMeshComponent->SetFlags(RF_Transient);
			SkeletalMeshComponent->SetupAttachment(AttachComponent);
			SkeletalMeshComponent->RegisterComponent();
			SkeletalMeshComponent->AddTickPrerequisiteComponent(AttachComponent);
			SkeletalMeshComponent->SetSkeletalMesh(Properties->SkeletalMeshes[SkeletalIndex].SkeletalMesh);
			SkeletalMeshComponent->OverrideAnimationData(Properties->Animations[AnimeIndex],true,false,0.0f);
			SetSkeletalMaterials(Properties,SkeletalMeshComponent,Emitter,&PerParticleData);

			if (Emitter->GetCachedEmitterData()->bLocalSpace)
			{
				SkeletalMeshComponent->SetAbsolute(false, false, false);
			}
			else
			{
				SkeletalMeshComponent->SetAbsolute(true, true, true);
			}

			if (PoolIndex >= 0)
			{
				// This should only happen if the component was destroyed externally
				ComponentPool[PoolIndex].Component = SkeletalMeshComponent;
				
			}
			else
			{
				// Add a new pool entry
				PoolIndex = ComponentPool.Num();
				ComponentPool.AddDefaulted_GetRef().Component = SkeletalMeshComponent;
				
			}
		}
		
		const FNiagaraLWCConverter LwcConverter = SystemInstance->GetLWCConverter(Emitter->GetCachedEmitterData()->bLocalSpace);
		FVector Position = LwcConverter.ConvertSimulationPositionToWorld(PerParticleData.Position);
		
		SkeletalMeshComponent->SetFlags(RF_Transient);
		SkeletalMeshComponent->SetupAttachment(AttachComponent);
		SkeletalMeshComponent->AddTickPrerequisiteComponent(AttachComponent);
		//SkeletalMeshComponent->SetSkeletalMesh(Properties->SkeletalMeshes[VisTag].SkeletalMesh);
		
		FTransform Transform(FRotator(PerParticleData.Rotate.X, PerParticleData.Rotate.Y, PerParticleData.Rotate.Z), Position, FVector(PerParticleData.Scale));
		SkeletalMeshComponent->SetRelativeTransform(Transform);
		SkeletalMeshComponent->SetVisibility(PerParticleData.Enabled);
		//SkeletalMeshComponent->MeshObject->
		SkeletalMeshComponent->SetActive(true);
		SkeletalMeshComponent->SetPosition(PerParticleData.SkeletalAnimTime);

		FComponentPoolEntry& PoolEntry = ComponentPool[PoolIndex];
		PoolEntry.LastAssignedToParticleID = ParticleID;
		++ComponentCount;
		
		if (ComponentCount >= MaxComponents)
		{
			// We've hit our prescribed limit
			break;
		}
	}
	
	//Free some component which they particle is dead
	if (ComponentCount < ComponentPool.Num())
	{
		// go over the pooled components we didn't need this tick to see if we can destroy some and deactivate the rest
		for (int32 PoolIndex = 0; PoolIndex < ComponentPool.Num(); ++PoolIndex)
		{
			FComponentPoolEntry& PoolEntry = ComponentPool[PoolIndex];
			if (Properties->bAssignComponentsOnParticleID)
			{
				if (PoolEntry.LastAssignedToParticleID >= 0)
				{
					// This one's in use
					continue;
				}
			}
			else if (PoolIndex < ComponentCount)
			{
				continue;
			}

			USceneComponent* Component = PoolEntry.Component.Get();
			if (!Component)
			{
				if (Component)
				{
					Component->DestroyComponent();
				}

				// destroy the component pool slot
				ComponentPool.RemoveAtSwap(PoolIndex, 1, false);
				--PoolIndex;
				continue;
			}
			else if (Component->IsActive())
			{
				Component->Deactivate();
				Component->SetVisibility(false, true);
			}
		}
	}
}

void FNiagaraRendererSkeletal::OnSystemComplete_GameThread(const UNiagaraRendererProperties* InProperties, const FNiagaraEmitterInstance* Emitter)
{
	ResetComponentPool(true);
}

void FNiagaraRendererSkeletal::SetSkeletalMaterials(const UNiagaraSkeletalRendererProperties* Properties,USkeletalMeshComponent* SkeletalMeshComponent,const FNiagaraEmitterInstance* Emitter,FNiagaraParticleData* PerParticleData)
{
	
	if (Properties->MaterialParameters.HasAnyBindings())
	{
		ProcessMaterialParameterBindings(Properties->MaterialParameters, Emitter, MakeArrayView(BaseMaterials_GT));
	}
	
	for(int i = 0;i<BaseMaterials_GT.Num();++i)
	{
		SkeletalMeshComponent->SetMaterial(i,BaseMaterials_GT[i]);
	}
	
}

void FNiagaraRendererSkeletal::ResetComponentPool(bool bResetOwner)
{
	for (FComponentPoolEntry& PoolEntry : ComponentPool)
	{
		if (PoolEntry.Component.IsValid())
		{
			PoolEntry.Component->DestroyComponent();
		}
	}
	ComponentPool.SetNum(0, false);

	if (bResetOwner)
	{
		if (AActor* OwnerActor = SpawnedOwner.Get())
		{
			SpawnedOwner.Reset();
			OwnerActor->Destroy();
		}
	}
}
