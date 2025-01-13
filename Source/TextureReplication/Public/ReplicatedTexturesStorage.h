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

	// Do not use to find of texture is loaded
	// Instead use TMap::Contains to optimize
	// Use only  loadedTexturesNames for iterating existing TMap
	TArray<FString> loadedTexturesNames;
	
private:

	AReplicatedTexturesStorage();

	virtual void BeginDestroy() override;

};
