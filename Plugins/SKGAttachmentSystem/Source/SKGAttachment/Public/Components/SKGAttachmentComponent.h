// Copyright 2023, Dakota Dawe, All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "SKGAttachmentDataTypes.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "SKGAttachmentComponent.generated.h"

class AActor;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class USKGAttachmentManager;
class UPDA_AttachmentCompatibility;
class USKGAttachmentPreviewStatic;
class USKGAttachmentPreviewSkeletal;

DECLARE_DYNAMIC_DELEGATE_OneParam(FOnCompatibilityComplete, bool, bFoundMatch);

UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class SKGATTACHMENT_API USKGAttachmentComponent : public USceneComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	USKGAttachmentComponent();

protected:
	// All the possible attachments this component can have. Is used for Attachment compatibility and customization
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SKGAttachment | Customization")
	bool bAllowClientAuthorativeAttachmentChange;
	// Loads the assets in asynchronously by default
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SKGAttachment | Customization")
	bool bLoadAsyncByDefault;
	// Whether this attachment is essential such as a barrel for a firearm
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SKGAttachment | Customization")
	bool bIsEssential;
	// This will auto set the master pose (leader pose if ue5) component for your skeletal mesh by using the GetMesh function on the parent
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SKGAttachment | Customization")
	bool bAutoSetMasterPoseComponent;
	// Attachment compatibility data assets. Used for filtering/checking for if an attachment is compatible with this slot/component
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SKGAttachment | Customization")
	TArray<UPDA_AttachmentCompatibility*> AllPossibleAttachments;
	// If this is set this Attachment will be spawned by default and attached to the firearm/attachment
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SKGAttachment | Customization")
	TSoftClassPtr<AActor> DefaultAttachment;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SKGAttachment | Customization")
	FSKGAttachmentRandomDefault RandomDefaultAttachment;
	// This is used for whatever, I use it for the firearm customizer. Use as you wish
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SKGAttachment | Customization")
	FName ComponentName;
	// The minimum distance you can move the Attachment towards your camera (Up the rail). Recommended leave at 0 except for specific cases (look at the stock on the M4)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SKGAttachment | Customization")
	float Minimum;
	// The maximum distance you can move the Attachment away from the camera (Down the rail)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SKGAttachment | Customization")
	float Maximum;
	// This will place the Attachment upon its first spawning at the location in the editor (Display Attachment Min Max)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SKGAttachment | Customization")
	bool bSetAttachmentInitialOffsetAtDisplayMinMax;
	// Snap distance. Picatinny rail = 1.0, M-LOK = 4.0, Keymod = 2.0, No Snap = 0.0
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SKGAttachment | Customization")
	float OffsetSnapDistance;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SKGAttachment | Customization")
	bool bUseRightVector;
	
	UPROPERTY(BlueprintReadOnly, Category = "SKGAttachment | Customization")
	USceneComponent* AttachmentTransformReference;
	
	// Slide the Attachment up/down the rail so you can visualize and figure out the Minimum and Maximum values
	UPROPERTY(EditDefaultsOnly, Category = "SKGAttachment | Preview")
	float DisplayAttachmentMinMax;
	// Sets the preview Attachment to be the mesh of the index of Possible Attachments (0 = first Attachment in Possible Attachments)
	UPROPERTY(EditDefaultsOnly, Category = "SKGAttachment | Preview")
	int32 DisplayAttachmentIndex;

	UPROPERTY(EditDefaultsOnly, Category = "SKGAttachment | Preview")
	USKGAttachmentPreviewStatic* PreviewStatic;
	UPROPERTY(EditDefaultsOnly, Category = "SKGAttachment | Preview")
	USKGAttachmentPreviewSkeletal* PreviewSkeletal;

	UPROPERTY(ReplicatedUsing = OnRep_Attachment, BlueprintReadOnly, Category = "SKGAttachment")
	AActor* Attachment;
	UFUNCTION()
	void OnRep_Attachment();

	TWeakObjectPtr<USKGAttachmentManager> DirectParentAttachmentManager;
	TWeakObjectPtr<USKGAttachmentManager> AttachmentManager;
	
	FTransform CurrentAttachmentTransform;
	int OldAmount;
	bool bSetInitialOffset;
	TArray<float> CachedSnapPointOffsets;
	
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UFUNCTION()
	void OnAssetsLoaded(TSoftClassPtr<AActor> AttachmentSoftClassPtr);

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditDefaultsOnly, Category = "SKGAttachment | Preview")
	FSKGAttachmentComponentPreview AttachmentComponentPreview;
	TArray<FSKGDataAssetAttachment> CachedPossibleAttachments;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostInitProperties() override;
#endif
	
	bool HasAuthority() const { return IsValid(GetOwner()) ? GetOwner()->HasAuthority() : false; }

	UFUNCTION(Server, Unreliable, WithValidation)
	void Server_AddAttachment(TSubclassOf<AActor> AttachmentClass, bool bDestroyCurrentAttachment);
	UFUNCTION(Server, Unreliable, WithValidation)
	void Server_AddExistingAttachment(AActor* INAttachment, bool bDestroyCurrentAttachment);
	UFUNCTION(Server, Unreliable, WithValidation)
	void Server_DestroyCurrentAttachment();

	void HandleAttachmentConstruction();

	void AddNewAttachment(AActor* INAttachment, bool bDestroyCurrentAttachment = true);

	UPROPERTY()
	TArray<FSKGAttachmentOverlap> OverlappedAttachments;

	bool IsMovementInverted() const;

	bool bCanAddAttachment;
	
public:
	UFUNCTION(BlueprintPure, Category = "SKGAttachment | Attachment")
	USKGAttachmentManager* GetDirectOwnersAttachmentManager();
	UFUNCTION(BlueprintPure, Category = "SKGAttachment | Attachment")
	USKGAttachmentManager* GetAttachmentManager();
	UFUNCTION(BlueprintPure, Category = "SKGAttachment | Attachment")
	bool IsEssential() const { return bIsEssential; }
	void SetMasterPoseComponent();
	void OverlappedWithAttachment(FSKGAttachmentOverlap OverlappedAttachment);
	UFUNCTION(BlueprintPure, Category = "SKGAttachment | Attachment")
	FName GetComponentName() const { return ComponentName; }
	UFUNCTION(BlueprintPure, Category = "SKGAttachment | Attachment")
	FText GetComponentNameText() const { return FText::FromName(ComponentName); }
	UFUNCTION(BlueprintPure, Category = "SKGAttachment | Attachment")
	FString GetComponentNameString() const { return ComponentName.ToString(); }
	
	UFUNCTION(BlueprintPure, Category = "SKGAttachment | Attachment")
	FTransform GetAttachmentTransform() const;
	UFUNCTION(BlueprintPure, Category = "SKGAttachment | Attachment")
	AActor* GetAttachment() const { return Attachment; }
	UFUNCTION(BlueprintPure, Category = "SKGAttachment | Attachment", meta = (DeterminesOutputType = "ActorClass"))
	AActor* GetAttachmentCastedBP(TSubclassOf<AActor> ActorClass) const { return Attachment; }
	UFUNCTION(BlueprintPure, Category = "SKGAttachment | Attachment")
	TArray<FSKGDataAssetAttachment> GetPossibleAttachments() const;
	UFUNCTION(BlueprintPure, Category = "SKGAttachment | Attachment")
	FVector2D GetMinMaxOffset() const { return FVector2D(Minimum, Maximum); }
	UFUNCTION(BlueprintPure, Category = "SKGAttachment | Attachment")
	FVector GetMaxOffsetLocation() const;
	UFUNCTION(BlueprintPure, Category = "SKGAttachment | Attachment")
	TArray<float> GetSnapPoints();
	UFUNCTION(BlueprintPure, Category = "SKGAttachment | Attachment")
	TArray<FVector> GetWorldSnapPoints();
	UFUNCTION(BlueprintPure, Category = "SKGAttachment | Attachment")
	int32 GetAttachmentCurrentSnapPointIndex() const;
	UFUNCTION(BlueprintPure, Category = "SKGAttachment | Attachment")
	float GetOffsetSnapDistance() const { return OffsetSnapDistance; }
	UFUNCTION(BlueprintPure, Category = "SKGAttachment | Attachment")
	TArray<FSKGAttachmentOverlap>& GetOverlappedAttachments() { return OverlappedAttachments; }
	UFUNCTION(BlueprintPure, Category = "SKGAttachment | Attachment")
	TArray<AActor*> GetOverlappedHiddenActors();
	UFUNCTION(BlueprintCallable, Category = "SKGAttachment | Attachment")
	void HideOverlappedAndUnhideUnoverlappedAttachments();
	UFUNCTION(BlueprintCallable, Category = "SKGAttachment | Attachment")
	void ClearOverlappedAttachmentsArray() { OverlappedAttachments.Empty(); }
	
	template <class T>
	T* GetAttachment() const { return Cast<T>(Attachment); }

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SetAttachmentDormant(ENetDormancy NewDormantState);
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_FlushAttachmentDormancy();
	UFUNCTION(BlueprintCallable, Category = "SKGAttachment | Attachment")
	void SetAttachmentDormant(ENetDormancy NewDormantState);
	UFUNCTION(BlueprintCallable, Category = "SKGAttachment | Attachment")
	void FlushAttachmentDormancy();
	
	UFUNCTION(BlueprintCallable, Category = "SKGAttachment | Attachment")
	void DestroyCurrentAttachment();
	/**
	 * If DestroyCurrentAttachment is false it will return the previously existing Attachment
	 * if you had a holo sight and then added a thermal sight it will return the holo sight
	**/
	UFUNCTION(BlueprintCallable, Category = "SKGAttachment | Attachment")
	AActor* AddAttachment(TSubclassOf<AActor> AttachmentClass, bool bDestroyCurrentAttachment = true);
	/**
	 * If DestroyCurrentAttachment is false it will return the previously existing Attachment
	 * if you had a holo sight and then added a thermal sight it will return the holo sight
	**/
	UFUNCTION(BlueprintCallable, Category = "SKGAttachment | Attachment")
	AActor* AddExistingAttachment(AActor* INAttachment, bool bDestroyCurrentAttachment = true);
	UFUNCTION(BlueprintPure, Category = "SKGAttachment | Attachment")
	bool IsAttachmentCompatible(TSubclassOf<AActor> AttachmentClass);

	UFUNCTION(BlueprintPure, Category = "SKGAttachment | Attachment")
	bool HasAttachment() const;

#if WITH_EDITOR
	const FSKGAttachmentComponentPreview& GetAttachmentComponentPreview() const { return AttachmentComponentPreview; }
	UActorComponent* GetMeshComponentFromAttachment() const;
	int32 GetEditorAttachmentCurrentSnapPointIndex() const;
	TArray<float> GetEditorSnapDistancePoints() const;
	TArray<FVector> GetEditorSnapPoints() const;
	FVector GetDirectionVector() const  { return bUseRightVector ? GetRightVector() : GetForwardVector(); }
#endif
};
