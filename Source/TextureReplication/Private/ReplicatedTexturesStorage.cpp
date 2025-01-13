// Fill out your copyright notice in the Description page of Project Settings.


#include "ReplicatedTexturesStorage.h"
#include "ReplicatedTextureComponent.h"

AReplicatedTexturesStorage::AReplicatedTexturesStorage()
{
	bReplicates = false;
}

void AReplicatedTexturesStorage::BeginDestroy()
{
	Super::BeginDestroy();

	UE_LOG(LogTemp, Warning, TEXT("Destroying a storage"));
	UReplicatedTextureComponent::textureStorage = nullptr;

}


