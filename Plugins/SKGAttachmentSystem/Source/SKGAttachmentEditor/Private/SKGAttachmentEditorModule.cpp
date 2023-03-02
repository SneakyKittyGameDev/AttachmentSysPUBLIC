// Copyright Epic Games, Inc. All Rights Reserved.

#include "SKGAttachmentEditorModule.h"

#include "UnrealEdGlobals.h"
#include "Components/SKGAttachmentComponent.h"
#include "Editor/UnrealEdEngine.h"
#include "Visualizers/SKGAttachmentComponentVisualizer.h"

#define LOCTEXT_NAMESPACE "FSKGAttachmentEditorModule"

void FSKGAttachmentEditorModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	if (GUnrealEd)
	{
		const TSharedPtr<FSKGAttachmentComponentVisualizer> AttachmentComponentVisualizer = MakeShareable(new FSKGAttachmentComponentVisualizer());
		if (AttachmentComponentVisualizer.IsValid())
		{
			GUnrealEd->RegisterComponentVisualizer(USKGAttachmentComponent::StaticClass()->GetFName(), AttachmentComponentVisualizer);
			AttachmentComponentVisualizer->OnRegister();
		}
	}
}

void FSKGAttachmentEditorModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	if (GUnrealEd)
	{
		GUnrealEd->UnregisterComponentVisualizer(USKGAttachmentComponent::StaticClass()->GetFName());
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FSKGAttachmentEditorModule, SKGAttachmentEditor)