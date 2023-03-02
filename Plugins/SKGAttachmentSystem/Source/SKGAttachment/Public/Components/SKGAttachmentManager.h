// Copyright 2023, Dakota Dawe, All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SKGAttachmentManager.generated.h"

class USKGAttachmentComponent;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAttachmentsCached);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class SKGATTACHMENT_API USKGAttachmentManager : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	USKGAttachmentManager();

protected:
	// Whether or not to override the max attachment setting. Should only be done by 1 attachment manager
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SKGAttachment | Default")
	bool bOverrideMaxAttachments;
	// Sets the max attachment stack. For example 3 would be barrel, muzzle device, suppressor (3 attachments down a chain)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SKGAttachment | Default", meta = (EditCondition = "bOverrideMaxAttachments", EditConditionHides))
	int32 NewMaxAttachments;
	// Whether or not to allow spawning default parts from a preset (if holo sight is default part and you load a save without a sight, it will auto load the holosight to)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SKGAttachment | Default")
	bool bSpawnDefaultPartsFromPreset;
	// Optimization for construction. Will not call the OnUpdated functions until delay has passed and item is fully constructed
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SKGAttachment | Default")
	float InitalOnAttachmentUpdatedDelay;
	bool bInitDone;
	
	UPROPERTY(ReplicatedUsing = OnRep_AttachmentComponents)
	TArray<USKGAttachmentComponent*> AttachmentComponents;
	UFUNCTION()
	void OnRep_AttachmentComponents();
	UPROPERTY()
	TArray<USKGAttachmentComponent*> CachedAttachmentComponents;
	UPROPERTY()
	TArray<USKGAttachmentComponent*> CachedEssentialAttachmentComponents;
	bool bEssentialAttachmentsValid;
	void HandleEssentialAttachmentValidation();

	FTimerHandle TCacheTimerHandle;
	void CacheAttachmentComponents();
	void CallOnAttachmentUpdated();
	
protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	bool HasAuthority() const { return GetOwner() && GetOwner()->HasAuthority() ? true : false; }
	
public:
	bool GetShouldSpawnDefaultOnPreset() const { return bSpawnDefaultPartsFromPreset; }
	void AttachmentChanged(USKGAttachmentComponent* AttachmentComponent);

	// Destroys all attachments (not the owner of the manager)
	UFUNCTION(BlueprintCallable, Category = "SKGAttachment | Attachment")
	void DestroyAllAttachments();
	// Destroys the owner and all attachments
	UFUNCTION(BlueprintCallable, Category = "SKGAttachment | Attachment")
	void DestroyAll();
	UFUNCTION(BlueprintCallable, Category = "SKGAttachment | Attachment")
	void AddAttachment(USKGAttachmentComponent* AttachmentComponent);
	UFUNCTION(BlueprintPure, Category = "SKGAttachment | Attachment")
	TArray<USKGAttachmentComponent*> GetAllAttachmentComponents(bool bReCache);
	UFUNCTION(BlueprintCallable, Category = "SKGAttachment | Attachment")
	void HandleCachingAttachmentComponents(float OverrideDelay = 0.0f);
	UFUNCTION(BlueprintPure, Category = "SKGAttachment | Attachment")
	TArray<USKGAttachmentComponent*> GetAttachmentComponents() const { return AttachmentComponents; }
	UFUNCTION(BlueprintCallable, Category = "SKGAttachment | Attachment")
	bool AttachmentExists(USKGAttachmentComponent* AttachmentComponent);
	UFUNCTION(BlueprintCallable, Category = "SKGAttachment | Attachment")
	TArray<AActor*> GetAttachmentsOfType(const TSubclassOf<AActor> Type);
	UFUNCTION(BlueprintPure, Category = "SKGAttachment | Attachment")
	FString SerializeAttachments(FString& Error) const;

	UPROPERTY(BlueprintAssignable, Category = "SKGAttachment | Delegates")
	FOnAttachmentsCached OnAttachmentsCached;

	void SetMasterPoseComponent(USkeletalMeshComponent* SkeletalMeshComponent, float Delay = 0.0f) const;
	
	template <typename Type>
	TArray<Type*> GetAttachmentsOfType();
};
