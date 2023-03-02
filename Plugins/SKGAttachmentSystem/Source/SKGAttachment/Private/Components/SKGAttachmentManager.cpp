// Copyright 2023, Dakota Dawe, All rights reserved


#include "Components/SKGAttachmentManager.h"
#include "Components/SKGAttachmentComponent.h"
#include "Interfaces/SKGAttachmentInterface.h"

#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Misc/SKGAttachmentFunctionLibrary.h"

// Sets default values for this component's properties
USKGAttachmentManager::USKGAttachmentManager()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	bSpawnDefaultPartsFromPreset = true;
	InitalOnAttachmentUpdatedDelay = 0.25f;
	bInitDone = false;
	bEssentialAttachmentsValid = false;
	NewMaxAttachments = 10;
}

// Called when the game starts
void USKGAttachmentManager::BeginPlay()
{
	Super::BeginPlay();
	if (bOverrideMaxAttachments)
	{
		USKGAttachmentFunctionLibrary::MaxAttachmentStack = NewMaxAttachments;
	}
}

void USKGAttachmentManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USKGAttachmentManager, AttachmentComponents);
}

void USKGAttachmentManager::SetMasterPoseComponent(USkeletalMeshComponent* SkeletalMeshComponent, float Delay) const
{
	if (SkeletalMeshComponent)
	{
		AActor* OwningActor = GetOwner();
		if (OwningActor && OwningActor->GetClass()->ImplementsInterface(USKGAttachmentInterface::StaticClass()))
		{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 1
			SkeletalMeshComponent->SetLeaderPoseComponent(Cast<USkeletalMeshComponent>(Cast<USkeletalMeshComponent>(ISKGAttachmentInterface::Execute_GetMesh(OwningActor))));
#else
			SkeletalMeshComponent->SetMasterPoseComponent(Cast<USkeletalMeshComponent>(Cast<USkeletalMeshComponent>(ISKGAttachmentInterface::Execute_GetMesh(OwningActor))));
#endif
		}
	}
}

bool USKGAttachmentManager::AttachmentExists(USKGAttachmentComponent* AttachmentComponent)
{
	for (const USKGAttachmentComponent* Component : AttachmentComponents)
	{
		if (AttachmentComponent == Component)
		{
			return true;
		}
	}

	return false;
}

void USKGAttachmentManager::OnRep_AttachmentComponents()
{
	HandleCachingAttachmentComponents();
}

void USKGAttachmentManager::AttachmentChanged(USKGAttachmentComponent* AttachmentComponent)
{
	if (IsValid(AttachmentComponent) && CachedEssentialAttachmentComponents.Contains(AttachmentComponent))
	{
		if (AttachmentComponent->HasAttachment())
		{
			HandleEssentialAttachmentValidation();
		}
		else
		{
			bEssentialAttachmentsValid = false;
		}
	}
}

void USKGAttachmentManager::DestroyAllAttachments()
{
	if (HasAuthority())
	{
		TArray<AActor*> ActorsToDestroy;
		ActorsToDestroy.Reserve(CachedAttachmentComponents.Num());
		for (int i = 0; i < CachedAttachmentComponents.Num(); ++i)
		{
			USKGAttachmentComponent* AttachmentComponent = CachedAttachmentComponents[i];
			if (AttachmentComponent)
			{
				AActor* Attachment = AttachmentComponent->GetAttachment();
				if (IsValid(Attachment))
				{
					ActorsToDestroy.Emplace(Attachment);
				}
			}
		}
		CachedAttachmentComponents.Empty();
		for (AActor* Actor : ActorsToDestroy)
		{
			Actor->Destroy();
		}
	}
}

void USKGAttachmentManager::DestroyAll()
{
	if (HasAuthority())
	{
		DestroyAllAttachments();
		if (GetOwner())
		{
			GetOwner()->Destroy();
		}
	}
}

void USKGAttachmentManager::AddAttachment(USKGAttachmentComponent* AttachmentComponent)
{
	if (IsValid(AttachmentComponent))
	{
		AttachmentComponents.Add(AttachmentComponent);
		OnRep_AttachmentComponents();
	}
}

TArray<USKGAttachmentComponent*> USKGAttachmentManager::GetAllAttachmentComponents(bool bReCache)
{
	if (bReCache)
	{
		CacheAttachmentComponents();
	}
	
	return CachedAttachmentComponents;
}

void USKGAttachmentManager::HandleCachingAttachmentComponents(float OverrideDelay)
{
	if (!bInitDone && (OverrideDelay > 0.0f || InitalOnAttachmentUpdatedDelay > 0.0f))
	{
		if (OverrideDelay > InitalOnAttachmentUpdatedDelay)
		{
			InitalOnAttachmentUpdatedDelay = OverrideDelay;
		}
		GetWorld()->GetTimerManager().SetTimer(TCacheTimerHandle, this, &USKGAttachmentManager::CacheAttachmentComponents, InitalOnAttachmentUpdatedDelay, false);
	}
	else
	{
		CacheAttachmentComponents();
	}
}

void USKGAttachmentManager::HandleEssentialAttachmentValidation()
{
	bEssentialAttachmentsValid = true;
	for (const USKGAttachmentComponent* Component : CachedEssentialAttachmentComponents)
	{
		if (!IsValid(Component) || !Component->HasAttachment())
		{
			bEssentialAttachmentsValid = false;
			break;
		}
	}
}

void USKGAttachmentManager::CacheAttachmentComponents()
{
	bInitDone = true;
	
	CachedAttachmentComponents.Empty();
	CachedAttachmentComponents.Reserve(12);
	CachedAttachmentComponents.Append(AttachmentComponents);
	CachedEssentialAttachmentComponents.Empty();
	CachedEssentialAttachmentComponents.Reserve(AttachmentComponents.Num());
	for (USKGAttachmentComponent* AttachmentComponent : AttachmentComponents)
	{
		if (IsValid(AttachmentComponent))
		{
			AActor* Actor = AttachmentComponent->GetAttachment();
			if (IsValid(Actor) && Actor->GetClass()->ImplementsInterface(USKGAttachmentInterface::StaticClass()))
			{
				CachedAttachmentComponents.Append(ISKGAttachmentInterface::Execute_GetAllAttachmentComponents(Actor, true));
			}

			if (AttachmentComponent->IsEssential())
			{
				CachedEssentialAttachmentComponents.Add(AttachmentComponent);
			}
		}
	}
	CachedEssentialAttachmentComponents.Shrink();

	HandleEssentialAttachmentValidation();
	CallOnAttachmentUpdated();
}

void USKGAttachmentManager::CallOnAttachmentUpdated()
{
	if (GetOwner() && GetOwner()->GetClass()->ImplementsInterface(USKGAttachmentInterface::StaticClass()))
	{
		ISKGAttachmentInterface::Execute_OnAttachmentUpdated(GetOwner());
	}
	OnAttachmentsCached.Broadcast();
}

TArray<AActor*> USKGAttachmentManager::GetAttachmentsOfType(const TSubclassOf<AActor> Type)
{
	TArray<AActor*> Attachments;
	if (Type)
	{
		for (const USKGAttachmentComponent* Component : AttachmentComponents)
		{
			if (Component && Component->GetAttachment() && Component->GetAttachment()->GetClass() == Type)
			{
				Attachments.Add(Component->GetAttachment());
			}
		}
	}
	return Attachments;
}

FString USKGAttachmentManager::SerializeAttachments(FString& Error) const
{
	return USKGAttachmentFunctionLibrary::SerializeAttachmentParent(GetOwner(), Error);
}

template <typename Type>
TArray<Type*> USKGAttachmentManager::GetAttachmentsOfType()
{
	TArray<Type*> Attachments;
	
	for (const USKGAttachmentComponent* Component : AttachmentComponents)
	{
		if (Component)
		{
			if (Type* Casted = Component->GetAttachment<Type>())
			{
				Attachments.Add(Casted);
			}
		}
	}

	return Attachments;
}
