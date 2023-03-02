// Copyright 2023, Dakota Dawe, All rights reserved


#include "Components/SKGAttachmentComponent.h"
#include "Interfaces/SKGAttachmentInterface.h"
#include "Components/SKGAttachmentPreviewStatic.h"
#include "Components/SKGAttachmentPreviewSkeletal.h"
#include "Misc/PDA_AttachmentCompatibility.h"
#include "Misc/SKGAttachmentFunctionLibrary.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SKGAttachmentManager.h"
#include "Engine/AssetManager.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Net/UnrealNetwork.h"

const FName SKGATTACHMENT_OVERLAP_TAG_NAME = FName("SKGOverlap");
const FName SKGATTACHMENT_MESH_TAG_NAME = FName("SKGAttachment");

// Sets default values for this component's properties
USKGAttachmentComponent::USKGAttachmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bUseAttachParentBound = true;

	bAllowClientAuthorativeAttachmentChange = true;
	bLoadAsyncByDefault = true;
	bIsEssential = false;
	
	Minimum = 0.0f;
	Maximum = 0.0f;
	DisplayAttachmentIndex = INDEX_NONE;
	DisplayAttachmentMinMax = 0.0f;
	bSetAttachmentInitialOffsetAtDisplayMinMax = false;
	bSetInitialOffset = false;
	OffsetSnapDistance = 0.0f;

	PreviewStatic = CreateDefaultSubobject<USKGAttachmentPreviewStatic>(TEXT("PreviewStaticMesh"));
	PreviewStatic->SetupAttachment(GetAttachmentRoot());
	PreviewStatic->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewStatic->SetHiddenInGame(true);
	PreviewStatic->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
	
	AttachmentTransformReference = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComponent"));
	AttachmentTransformReference->SetupAttachment(PreviewStatic);
	
	PreviewSkeletal = CreateDefaultSubobject<USKGAttachmentPreviewSkeletal>(TEXT("PreviewSkeletalMesh"));
	PreviewSkeletal->SetupAttachment(PreviewStatic);
	PreviewSkeletal->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewSkeletal->SetHiddenInGame(true);
	PreviewSkeletal->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
	
	SetIsReplicatedByDefault(true);

	bUseRightVector = true;
	bAutoSetMasterPoseComponent = false;

	bCanAddAttachment = true;
}

void USKGAttachmentComponent::BeginPlay()
{
	Super::BeginPlay();
	PreviewSkeletal->DestroyComponent();
	PreviewStatic->DestroyComponent();

	if (!GetAttachmentManager() && HasAuthority())
	{
		DestroyComponent();
		return;
	}
	
	if (HasAuthority())
	{
		HandleAttachmentConstruction();
		if (GetDirectOwnersAttachmentManager())
		{
			DirectParentAttachmentManager->AddAttachment(this);
		}
	}

#if WITH_EDITOR
	CachedPossibleAttachments = GetPossibleAttachments();
#endif
}

void USKGAttachmentComponent::HandleAttachmentConstruction()
{
	if (!DefaultAttachment.IsNull())
	{
		bool bSpawnDefaultParts = true;
		if (GetAttachmentManager())
		{
			AActor* OwningActor = GetAttachmentManager()->GetOwner();
			if (IsValid(OwningActor) && OwningActor->GetClass()->ImplementsInterface(USKGAttachmentInterface::StaticClass()))
			{
				bSpawnDefaultParts = ISKGAttachmentInterface::Execute_GetShouldSpawnDefaultOnPreset(OwningActor);
			}
		}

		if (bSpawnDefaultParts)
		{
			if (RandomDefaultAttachment.bRandomAttachment)
			{
				const int32 StartRange = RandomDefaultAttachment.bAllowNoSpawn ? INDEX_NONE : 0;
				const int32 DataAssetCount = AllPossibleAttachments.Num();
				const int32 DataAssetIndex = FMath::RandRange(StartRange, DataAssetCount - 1);
				
				if (DataAssetIndex > INDEX_NONE)
				{
					const int32 AttachmentCount = AllPossibleAttachments[DataAssetIndex]->Attachments.Num();
					if (AttachmentCount > 0)
					{
						const int32 AttachmentIndex = FMath::RandRange(0, AttachmentCount - 1);
						DefaultAttachment = AllPossibleAttachments[DataAssetIndex]->Attachments[AttachmentIndex].ActorClass;
					}
				}
				else
				{
					DefaultAttachment = nullptr;
				}
			}

			if (bLoadAsyncByDefault)
			{
				if (UAssetManager* AssetManager = UAssetManager::GetIfValid())
				{
					const TArray<FSoftObjectPath> Paths = { DefaultAttachment.ToSoftObjectPath() };
					const FStreamableDelegate Delegate = FStreamableDelegate::CreateUObject(this, &USKGAttachmentComponent::OnAssetsLoaded, DefaultAttachment);
					AssetManager->LoadAssetList(Paths, Delegate);
					return;
				}
			}
			
			// Incase async loading fails or load synconous is default option
			AddAttachment(DefaultAttachment.LoadSynchronous());
			DefaultAttachment.ResetWeakPtr();
		}
	}
}

void USKGAttachmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USKGAttachmentComponent, Attachment);
}

void USKGAttachmentComponent::OnAssetsLoaded(TSoftClassPtr<AActor> AttachmentSoftClassPtr)
{
	AddAttachment(AttachmentSoftClassPtr.Get());
	AttachmentSoftClassPtr.ResetWeakPtr();
}

void USKGAttachmentComponent::OnRep_Attachment()
{
	if (IsValid(Attachment) && Attachment->GetClass()->ImplementsInterface(USKGAttachmentInterface::StaticClass()))
	{
		ISKGAttachmentInterface::Execute_SetMinMaxOffset(Attachment, Minimum, Maximum);
		ISKGAttachmentInterface::Execute_OnAttachmentUpdated(Attachment);
	}
	if (GetAttachmentManager())
	{
		ISKGAttachmentInterface::Execute_OnAttachmentChanged(GetAttachmentManager()->GetOwner(), this);
		GetAttachmentManager()->AttachmentChanged(this);
	}

	SetMasterPoseComponent();
}

USKGAttachmentManager* USKGAttachmentComponent::GetDirectOwnersAttachmentManager()
{
	if (DirectParentAttachmentManager.IsValid())
	{
		return DirectParentAttachmentManager.Get();
	}

	DirectParentAttachmentManager = USKGAttachmentFunctionLibrary::GetDirectOwnersAttachmentManager(GetOwner());
	
	return DirectParentAttachmentManager.Get();
}

USKGAttachmentManager* USKGAttachmentComponent::GetAttachmentManager()
{
	if (AttachmentManager.IsValid())
	{
		return AttachmentManager.Get();
	}

	AttachmentManager = USKGAttachmentFunctionLibrary::GetOwningAttachmentManager(GetOwner());
	
	return AttachmentManager.Get();
}

#if WITH_EDITOR
void USKGAttachmentComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	FString PropertyName = PropertyChangedEvent.GetPropertyName().ToString();
	if (PropertyName.Equals("PossibleAttachments"))
	{
		PropertyName = "DisplayAttachmentIndex";
		if (DisplayAttachmentIndex > CachedPossibleAttachments.Num())
		{
			DisplayAttachmentIndex = CachedPossibleAttachments.Num() - 1;
		}
	}

	if (PropertyName.Equals(TEXT("bUseRightVector")))
	{
		DisplayAttachmentMinMax = FMath::Clamp(DisplayAttachmentMinMax, Minimum, Maximum);
		int Amount = (DisplayAttachmentMinMax / OffsetSnapDistance);
		Amount *= OffsetSnapDistance;
		
		if (bUseRightVector)
		{
			PreviewStatic->SetRelativeLocation(FVector(0.0f, Amount, 0.0f));
		}
		else
		{
			PreviewStatic->SetRelativeLocation(FVector(Amount, 0.0f, 0.0f));
		}
	}
	else if (PropertyName.Equals(TEXT("DisplayAttachmentMinMax")))
	{
		DisplayAttachmentMinMax = FMath::Clamp(DisplayAttachmentMinMax, Minimum, Maximum);
		int Amount = (DisplayAttachmentMinMax / OffsetSnapDistance);
		Amount *= OffsetSnapDistance;
		if (Amount <= Maximum && Amount >= Minimum && Amount != OldAmount)
		{
			OldAmount = Amount;
			if (bUseRightVector)
			{
				PreviewStatic->SetRelativeLocation(FVector(0.0f, Amount, 0.0f));
			}
			else
			{
				PreviewStatic->SetRelativeLocation(FVector(Amount, 0.0f, 0.0f));
			}
		}
	}
	else if (PropertyName.Contains(TEXT("DisplayAttachmentIndex")))
	{
		if (PreviewStatic && PreviewSkeletal)
		{
			if (UActorComponent* ActorComponent = GetMeshComponentFromAttachment())
			{
				UE_LOG(LogTemp, Warning, TEXT("FOUND VALID MESH: %s"), *GetName());
				if (const UStaticMeshComponent* AttachmentStaticMesh = Cast<UStaticMeshComponent>(ActorComponent))
				{
					PreviewStatic->SetStaticMesh(AttachmentStaticMesh->GetStaticMesh());
					PreviewStatic->SetHiddenInGame(true);
					PreviewSkeletal->SetSkeletalMesh(nullptr);
				}
				else if (const USkeletalMeshComponent* AttachmentSkeletalMesh = Cast<USkeletalMeshComponent>(ActorComponent))
				{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 1
					PreviewSkeletal->SetSkeletalMeshAsset(AttachmentSkeletalMesh->GetSkeletalMeshAsset());
#else
					PreviewSkeletal->SetSkeletalMesh(AttachmentSkeletalMesh->SkeletalMesh);
#endif
					PreviewSkeletal->SetHiddenInGame(true);
					PreviewStatic->SetStaticMesh(nullptr);
				}
			}
			else
			{
				PreviewStatic->SetStaticMesh(nullptr);
				PreviewStatic->SetStaticMesh(nullptr);
			}
		}
	}
	else if (PropertyName.Contains(TEXT("Preview")))
	{
		DisplayAttachmentIndex = INDEX_NONE;
		if (PropertyName.Contains(TEXT("Static")))
		{
			if (PreviewStatic)
			{
				PreviewSkeletal->SetSkeletalMesh(nullptr);
			}
		}
		else if (PropertyName.Contains(TEXT("Skeletal")))
		{
			if (PreviewSkeletal)
			{
				PreviewStatic->SetStaticMesh(nullptr);
			}
		}
	}
}

void USKGAttachmentComponent::PostInitProperties()
{
	Super::PostInitProperties();
}

UActorComponent* USKGAttachmentComponent::GetMeshComponentFromAttachment() const
{
	if (DisplayAttachmentIndex > -1)
	{
		int32 SelectedDisplayIndex = -1;
		const int32 DataAssetCount = AllPossibleAttachments.Num();
		UPDA_AttachmentCompatibility* SelectedDataAsset = nullptr;
		if (DataAssetCount)
		{
			int32 AttachmentCount = 0;
			int32 SubtractCount = 0;
			for (UPDA_AttachmentCompatibility* DataAsset : AllPossibleAttachments)
			{
				if (DataAsset)
				{
					AttachmentCount += DataAsset->Attachments.Num();
					if (DisplayAttachmentIndex + 1 > AttachmentCount)
					{
					}
					else
					{
						SelectedDataAsset = DataAsset;
						SelectedDisplayIndex = (DisplayAttachmentIndex) - SubtractCount;

						break;
					}
					SubtractCount = AttachmentCount;
				}
			}
		}
		
		if (SelectedDataAsset && SelectedDisplayIndex > -1)
		{
			if (const UBlueprintGeneratedClass* BPClass = Cast<UBlueprintGeneratedClass>(SelectedDataAsset->Attachments[SelectedDisplayIndex].ActorClass.LoadSynchronous()))
			{
				const TArray<USCS_Node*>& ActorBlueprintNodes = BPClass->SimpleConstructionScript->GetAllNodes();
				UE_LOG(LogTemp, Warning, TEXT("NodeCount: %d     %s"), ActorBlueprintNodes.Num(), *GetName());
	
				for (const USCS_Node* Node : ActorBlueprintNodes)
				{
					if (Node && Node->ComponentTemplate)
					{
						if (Node->ComponentClass == UStaticMeshComponent::StaticClass() || Node->ComponentClass == USkeletalMeshComponent::StaticClass())
						{
							if (Node->ComponentTemplate->ComponentHasTag(SKGATTACHMENT_MESH_TAG_NAME))
							{
								return Node->ComponentTemplate;
							}
						}
					}
				}
			}
		}
	}
	return nullptr;
}

int32 USKGAttachmentComponent::GetEditorAttachmentCurrentSnapPointIndex() const
{
	const float Clamped = FMath::Clamp(DisplayAttachmentMinMax, Minimum, Maximum);
	const int Amount = Clamped / OffsetSnapDistance;
	return IsMovementInverted() ? -Amount : Amount;
}

TArray<float> USKGAttachmentComponent::GetEditorSnapDistancePoints() const
{
	TArray<float> SnapPoints;
	if (OffsetSnapDistance > 0.0f)
	{
		bool bInverted = false;
		if (Minimum < 0.0f)
		{
			bInverted = true;
		}
		const uint8 ElementCount = (bInverted ? -Minimum : Maximum) / OffsetSnapDistance;

		SnapPoints.Reserve(ElementCount);
		SnapPoints.Add(0.0f);
		
		float Offset = 0.0f;
		for (uint8 i = 0; i < ElementCount; ++i)
		{
			Offset += bInverted ? -OffsetSnapDistance : OffsetSnapDistance;
			SnapPoints.Add(Offset);
		}
	}

	return SnapPoints;
}

TArray<FVector> USKGAttachmentComponent::GetEditorSnapPoints() const
{
	TArray<FVector> WorldSnapPointOffsets;
	
	if (OffsetSnapDistance > 0.0f)
	{
		TArray<float> SnapDistancePoints = GetEditorSnapDistancePoints();

		WorldSnapPointOffsets.Reserve(SnapDistancePoints.Num());
		const FVector DirectionalVector = bUseRightVector ? GetRightVector() : GetForwardVector();

		for (const float Offset : SnapDistancePoints)
		{
			const FVector PointLocation = GetComponentLocation() + DirectionalVector * Offset;
			WorldSnapPointOffsets.Add(PointLocation);
		}
	}
	
	return WorldSnapPointOffsets;
}

#endif

FTransform USKGAttachmentComponent::GetAttachmentTransform() const
{
	return AttachmentTransformReference->GetComponentTransform();
}

bool USKGAttachmentComponent::Server_DestroyCurrentAttachment_Validate()
{
	return bAllowClientAuthorativeAttachmentChange ? true : false;
}

void USKGAttachmentComponent::Server_DestroyCurrentAttachment_Implementation()
{
	DestroyCurrentAttachment();
}

TArray<AActor*> USKGAttachmentComponent::GetOverlappedHiddenActors()
{
	TArray<AActor*> HiddenAttachments;
	HiddenAttachments.Reserve(OverlappedAttachments.Num());

	for (const FSKGAttachmentOverlap& AttachmentOverlap : OverlappedAttachments)
	{
		if (IsValid(AttachmentOverlap.Attachment))
		{
			if (AttachmentOverlap.Attachment->IsHidden())
			{
				HiddenAttachments.Add(AttachmentOverlap.Attachment);
			}
		}
	}
	
	return HiddenAttachments;
}

void USKGAttachmentComponent::HideOverlappedAndUnhideUnoverlappedAttachments()
{
	for (const FSKGAttachmentOverlap AttachmentOverlap : OverlappedAttachments)
	{
		if (IsValid(AttachmentOverlap.Attachment) && AttachmentOverlap.Attachment->GetClass()->ImplementsInterface(USKGAttachmentInterface::StaticClass()))
		{
			if (AttachmentOverlap.bIsOverlapped)
			{
				ISKGAttachmentInterface::Execute_OnAttachmentOverlapped(AttachmentOverlap.Attachment);
			}
			else
			{
				ISKGAttachmentInterface::Execute_OnAttachmentOverlapEnd(AttachmentOverlap.Attachment);
			}
		}
	}
}

bool USKGAttachmentComponent::Server_SetAttachmentDormant_Validate(ENetDormancy NewDormantState)
{
	return true;
}

void USKGAttachmentComponent::Server_SetAttachmentDormant_Implementation(ENetDormancy NewDormantState)
{
	SetAttachmentDormant(NewDormantState);
}

bool USKGAttachmentComponent::Server_FlushAttachmentDormancy_Validate()
{
	return true;
}

void USKGAttachmentComponent::Server_FlushAttachmentDormancy_Implementation()
{
	FlushAttachmentDormancy();
}

void USKGAttachmentComponent::SetAttachmentDormant(ENetDormancy NewDormantState)
{
	if (IsValid(Attachment) && Attachment->NetDormancy != NewDormantState)
	{
		if (HasAuthority())
		{
			Attachment->SetNetDormancy(NewDormantState);
		}
		else
		{
			Server_SetAttachmentDormant(NewDormantState);
		}
	}
}

void USKGAttachmentComponent::FlushAttachmentDormancy()
{
	if (IsValid(Attachment) && (Attachment->NetDormancy != ENetDormancy::DORM_Awake || Attachment->NetDormancy != ENetDormancy::DORM_Never))
	{
		if (HasAuthority())
		{
			Attachment->FlushNetDormancy();
		}
		else
		{
			Server_FlushAttachmentDormancy();
		}
	}
}

void USKGAttachmentComponent::DestroyCurrentAttachment()
{
	if (!HasAuthority())
	{
		if (bAllowClientAuthorativeAttachmentChange)
		{
			Server_DestroyCurrentAttachment();
		}
	}
	else
	{
		if (IsValid(Attachment) && Attachment->GetClass()->ImplementsInterface(USKGAttachmentInterface::StaticClass()))
		{
			for (const USKGAttachmentComponent* AttachmentComponent : ISKGAttachmentInterface::Execute_GetAllAttachmentComponents(Attachment, false))
			{
				if (AttachmentComponent && IsValid(AttachmentComponent->Attachment))
				{
					AttachmentComponent->Attachment->Destroy();
				}
			}
			OnRep_Attachment();
			ISKGAttachmentInterface::Execute_OnAttachmentRemoved(Attachment);
		}
	}
}

TArray<FSKGDataAssetAttachment> USKGAttachmentComponent::GetPossibleAttachments() const
{
	TArray<FSKGDataAssetAttachment> Attachments;
	int32 ReserveCount = 0;
	for (const UPDA_AttachmentCompatibility* DataAsset : AllPossibleAttachments)
	{
		if (DataAsset)
		{
			ReserveCount += DataAsset->Attachments.Num();
		}
	}
	Attachments.Reserve(ReserveCount);
	
	for (UPDA_AttachmentCompatibility* DataAsset : AllPossibleAttachments)
	{
		if (DataAsset)
		{
			for (FSKGDataAssetAttachment& PossibleAttachment : DataAsset->Attachments)
			{
				Attachments.Add(PossibleAttachment);
			}
		}
	}

	return Attachments;
}

FVector USKGAttachmentComponent::GetMaxOffsetLocation() const
{
	FVector EndLocation = GetComponentLocation() + GetComponentRotation().Vector() * Maximum;
	if (IsValid(Attachment) && Attachment->GetClass()->ImplementsInterface(USKGAttachmentInterface::StaticClass()))
	{
		const float Distance = ISKGAttachmentInterface::Execute_GetAttachmentOffset(Attachment);
		const FVector AttachmentRightVector = Attachment->GetActorRightVector();
		
		if (IsMovementInverted())
		{
			EndLocation = GetComponentLocation() + AttachmentRightVector * (Minimum);
		}
		else
		{
			FVector AttachmentLocation = Attachment->GetActorLocation();
			AttachmentLocation.Z -= ISKGAttachmentInterface::Execute_GetAttachmentAttachOffset(Attachment).GetLocation().Z;
			EndLocation = AttachmentLocation + AttachmentRightVector * (Maximum - Distance);
		}
	}
	return EndLocation;
}

TArray<float> USKGAttachmentComponent::GetSnapPoints()
{
	if (CachedSnapPointOffsets.Num() == 0 && OffsetSnapDistance > 0.0f)
	{
		if (IsValid(Attachment))
		{
			const uint8 ElementCount = (IsMovementInverted() ? -Minimum : Maximum) / OffsetSnapDistance;

			CachedSnapPointOffsets.Reserve(ElementCount);
			CachedSnapPointOffsets.Add(0.0f);
			
			float Offset = 0.0f;
			for (uint8 i = 0; i < ElementCount; ++i)
			{
				Offset += IsMovementInverted() ? -OffsetSnapDistance : OffsetSnapDistance;
				CachedSnapPointOffsets.Add(Offset);
			}
		}
	}

	return CachedSnapPointOffsets;
}

TArray<FVector> USKGAttachmentComponent::GetWorldSnapPoints()
{
	TArray<FVector> WorldSnapPointOffsets;
	
	if (OffsetSnapDistance > 0.0f)
	{
		GetSnapPoints();
		WorldSnapPointOffsets.Reserve(CachedSnapPointOffsets.Num());
		const FVector DirectionalVector = bUseRightVector ? GetRightVector() : GetForwardVector();

		for (const float Offset : CachedSnapPointOffsets)
		{
			const FVector PointLocation = GetComponentLocation() + DirectionalVector * Offset;
			WorldSnapPointOffsets.Add(PointLocation);
		}
	}
	
	return WorldSnapPointOffsets;
}

int32 USKGAttachmentComponent::GetAttachmentCurrentSnapPointIndex() const
{
	const bool bValidAttachment = IsValid(Attachment);
	if (bValidAttachment && Attachment->GetClass()->ImplementsInterface(USKGAttachmentInterface::StaticClass()))
	{
		return bValidAttachment ? (IsMovementInverted() ? -1 : 1) * ISKGAttachmentInterface::Execute_GetAttachmentOffset(Attachment) / OffsetSnapDistance : -1;
	}
	return -1;
}

bool USKGAttachmentComponent::Server_AddAttachment_Validate(TSubclassOf<AActor> AttachmentClass, bool bDestroyCurrentAttachment)
{
	return bAllowClientAuthorativeAttachmentChange ? true : false;
}

void USKGAttachmentComponent::Server_AddAttachment_Implementation(TSubclassOf<AActor> AttachmentClass, bool bDestroyCurrentAttachment)
{
	AddAttachment(AttachmentClass, bDestroyCurrentAttachment);
}

AActor* USKGAttachmentComponent::AddAttachment(TSubclassOf<AActor> AttachmentClass, bool bDestroyCurrentAttachment)
{
	if (!IsAttachmentCompatible(AttachmentClass) || !GetOwner())
	{
		return Attachment;
	}

	AActor* CurrentAttachment = Attachment;

	if (!HasAuthority())
	{
		if (bAllowClientAuthorativeAttachmentChange)
		{
			Server_AddAttachment(AttachmentClass, bDestroyCurrentAttachment);
		}
	}
	else
	{
		if (AttachmentClass)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = GetOwner();

			AActor* SpawnedAttachment = GetWorld()->SpawnActor<AActor>(AttachmentClass, SpawnParams);
			AddNewAttachment(SpawnedAttachment, bDestroyCurrentAttachment);
		}
	}
	return bDestroyCurrentAttachment ? Attachment : CurrentAttachment;
}

bool USKGAttachmentComponent::Server_AddExistingAttachment_Validate(AActor* INAttachment, bool bDestroyCurrentAttachment)
{
	return bAllowClientAuthorativeAttachmentChange ? true : false;
}

void USKGAttachmentComponent::Server_AddExistingAttachment_Implementation(AActor* INAttachment, bool bDestroyCurrentAttachment)
{
	AddExistingAttachment(INAttachment, bDestroyCurrentAttachment);
}

AActor* USKGAttachmentComponent::AddExistingAttachment(AActor* INAttachment, bool bDestroyCurrentAttachment)
{
	if ((IsValid(INAttachment) && !IsAttachmentCompatible(INAttachment->GetClass())) || !GetOwner())
	{
		return Attachment;
	}

	AActor* CurrentAttachment = Attachment;
	
	if (!HasAuthority())
	{
		if (bAllowClientAuthorativeAttachmentChange)
		{
			Server_AddExistingAttachment(INAttachment, bDestroyCurrentAttachment);
		}
	}
	else
	{
		AddNewAttachment(INAttachment, bDestroyCurrentAttachment);
	}
	return bDestroyCurrentAttachment ? Attachment : CurrentAttachment;
}

void USKGAttachmentComponent::AddNewAttachment(AActor* INAttachment, bool bDestroyCurrentAttachment)
{
	if (HasAuthority() && IsValid(INAttachment) && INAttachment->GetClass()->ImplementsInterface(USKGAttachmentInterface::StaticClass()))
	{
		float AttachmentOffset = 0.0f;
		if (bDestroyCurrentAttachment && IsValid(Attachment))
		{
			AttachmentOffset = ISKGAttachmentInterface::Execute_GetAttachmentOffset(Attachment);
			DestroyCurrentAttachment();
		}
		INAttachment->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		if (!GetAttachmentTransform().Equals(FTransform()))
		{
			INAttachment->SetActorTransform(GetAttachmentTransform());
		}
		Attachment = INAttachment;

		ISKGAttachmentInterface::Execute_SetOwningAttachmentComponent(Attachment, this);
		
		if (!bSetInitialOffset && bSetAttachmentInitialOffsetAtDisplayMinMax && DisplayAttachmentMinMax <= Maximum && DisplayAttachmentMinMax >= Minimum)
		{
			bSetInitialOffset = true;
			ISKGAttachmentInterface::Execute_SetOffset(Attachment, DisplayAttachmentMinMax);
		}
		else
		{
			ISKGAttachmentInterface::Execute_SetOffset(Attachment, AttachmentOffset);
		}
		ISKGAttachmentInterface::Execute_SetSnapDistance(Attachment, OffsetSnapDistance);
		OnRep_Attachment();
	}
}

bool USKGAttachmentComponent::IsMovementInverted() const
{
	if (IsValid(Attachment) && Attachment->GetClass()->ImplementsInterface(USKGAttachmentInterface::StaticClass()))
	{
		return ISKGAttachmentInterface::Execute_IsMovementInverted(Attachment);
	}

	if (Minimum < 0.0f)
	{
		return true;
	}
	
	return false;
}

void USKGAttachmentComponent::SetMasterPoseComponent()
{
	if (bAutoSetMasterPoseComponent)
	{
		AActor* Actor = GetAttachment();
		if (IsValid(Actor))
		{
			if (Actor->GetClass()->ImplementsInterface(USKGAttachmentInterface::StaticClass()))
			{
				if (USkeletalMeshComponent* ActorMesh = Cast<USkeletalMeshComponent>(ISKGAttachmentInterface::Execute_GetMesh(Actor)))
				{
					if (GetAttachmentManager())
					{
						AttachmentManager->SetMasterPoseComponent(ActorMesh, 5.0f);
					}
				}
			}
		}
	}
}

void USKGAttachmentComponent::OverlappedWithAttachment(FSKGAttachmentOverlap OverlappedAttachment)
{
	bool bMatchedAttachment = false;
	bool bRemovedElement = false;
	for (uint8 i = 0; i < OverlappedAttachments.Num(); ++i)
	{
		FSKGAttachmentOverlap& AttachmentOverlap = OverlappedAttachments[i];
		if (AttachmentOverlap == OverlappedAttachment)
		{
			bMatchedAttachment = true;
			AttachmentOverlap.bIsOverlapped = OverlappedAttachment.bIsOverlapped;
		}
		else if (!IsValid(AttachmentOverlap.Attachment))
		{
			bRemovedElement = true;
			OverlappedAttachments.RemoveAt(i, 1, false);
		}
	}

	if (bRemovedElement)
	{
		OverlappedAttachments.Shrink();
	}
	
	if (!bMatchedAttachment)
	{
		OverlappedAttachments.Add(OverlappedAttachment);
	}
}

bool USKGAttachmentComponent::IsAttachmentCompatible(TSubclassOf<AActor> AttachmentClass)
{
	if (!AttachmentClass)
	{
		return false;
	}

	for (UPDA_AttachmentCompatibility* DataAsset : AllPossibleAttachments)
	{
		if (DataAsset)
		{
			for (FSKGDataAssetAttachment& PossibleAttachment : DataAsset->Attachments)
			{
				if (PossibleAttachment.ActorClass.Get() == AttachmentClass && PossibleAttachment.bEnabledForUse)
				{
					return true;
				}
			}
		}
	}
	return false;
}

bool USKGAttachmentComponent::HasAttachment() const
{
	return IsValid(Attachment);
}