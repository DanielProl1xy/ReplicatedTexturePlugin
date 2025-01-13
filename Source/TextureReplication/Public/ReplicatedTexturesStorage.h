// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ReplicatedTexturesStorage.generated.h"

UCLASS()
class AReplicatedTexturesStorage : public AActor
{
	GENERATED_BODY()

public:

	UPROPERTY(VisibleAnywhere, Category = "Texture Replication")
	TMap<FString, TObjectPtr<UTexture2D>> replicatedTextures;

	TMap<FString, TArray<uint8>> textureBuffers;

	// Do not use for look ups
	// Instead use TMap::Contains for better performance
	// Use only  loadedTexturesNames to iterate existing TMap
	TArray<FString> loadedTexturesNames;
	
private:

	AReplicatedTexturesStorage();

	virtual void BeginDestroy() override;

};
