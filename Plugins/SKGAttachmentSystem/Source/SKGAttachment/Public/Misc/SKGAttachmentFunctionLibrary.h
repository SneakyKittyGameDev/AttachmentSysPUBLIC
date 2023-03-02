// Copyright 2023, Dakota Dawe, All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SKGAttachmentFunctionLibrary.generated.h"

class USKGAttachmentManager;
class USKGAttachmentComponent;
class UMeshComponent;

UCLASS()
class SKGATTACHMENT_API USKGAttachmentFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	static int32 MaxAttachmentStack;
	static USKGAttachmentManager* GetOwningAttachmentManager(AActor* Actor);
	static USKGAttachmentManager* GetDirectOwnersAttachmentManager(AActor* Actor);
	static TArray<USKGAttachmentComponent*> CreateCacheFromAttachmentComponents(TArray<USKGAttachmentComponent*>& AttachmentComponents);
	static UMeshComponent* SetupAttachmentMesh(AActor* Actor);
	static void PrintError(const UObject* WorldObject, const FString& Error);

	static FSKGAttachmentParent GetAttachmentStruct(AActor* AttachmentParent, FString& Error);
	UFUNCTION(BlueprintCallable, Category = "SKGAttachment | Serialize")
	static FString SerializeAttachmentParent(AActor* AttachmentParent, FString& Error);
	UFUNCTION(BlueprintCallable, Category = "SKGAttachment | Serialize")
	static FSKGAttachmentParent DeserializeAttachmentString(const FString& JsonString, FString& Error);
	UFUNCTION(BlueprintCallable, Category = "SKGAttachment | Serialize")
	static AActor* ConstructAttachmentParent(AActor* WorldActor, FSKGAttachmentParent AttachmentStruct, FString& Error);

	UFUNCTION(BlueprintCallable, Category = "SKGAttachment | File")
	static bool SaveStringToFile(const FString& Path, const FString& FileName, const FString& FileContent);
	UFUNCTION(BlueprintPure, Category = "SKGAttachment | File")
	static bool LoadFileToString(const FString& Path, const FString& FileName, FString& OutString);
	UFUNCTION(BlueprintPure, Category = "SKGAttachment | File")
	static bool GetAllFiles(FString Path, TArray<FString>& OutFiles);
};
