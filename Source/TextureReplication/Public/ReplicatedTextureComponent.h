// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ReplicatedTexturesStorage.h"
#include "ReplicatedTextureComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTextureReady, const FString&, name, UTexture2D*, texture);

DECLARE_LOG_CATEGORY_EXTERN(LogReplicaetdTexture, Log, All);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class UReplicatedTextureComponent : public UActorComponent
{

	GENERATED_BODY()

public:

	const static uint64 maxChunkSize = 1024 * 1; // 1kb
	const static uint64 maxBufferSize = 1024 * 150; // 150kb


	static AReplicatedTexturesStorage* textureStorage;

	UPROPERTY(EditDefaultsOnly)
	bool bPauseReplication;

private:

	UPROPERTY(VisibleAnywhere)
	TArray<FString> namedQueue;

	UPROPERTY(VisibleAnywhere)
	TArray<FString> ownedTexturesNames;

	FString currentDownloadName;
	bool isDownloading;
	bool chunkReady;

public:

	UPROPERTY(BlueprintAssignable)
	FOnTextureReady OnTextureReady;

public:	
	UReplicatedTextureComponent();

public:

	UFUNCTION(BlueprintCallable, Category = "Texture Replication")
	bool ReplicateTexrure(UTexture2D* texture, const FString& name);

	UFUNCTION(BlueprintCallable, Category = "Texture Replication")
	const TArray<FString>& GetLoadedTexturesNames() const;

	// Use to find texture by name, Returns texxture if found and lodaed
	// Otherwise returns null
	UFUNCTION(BlueprintCallable, Category = "Texture Replication")
	const UTexture2D* FindTexture(const FString& name) const;

	// Puases replication only within this component
	// Call it only from server
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "Texture Replication")
	void SetPauseReplication(bool pause);

public:	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;


private:

	UFUNCTION(Server, Reliable, WithValidation)
	void replicateTextureServer(const FString& name);

	UFUNCTION(Client, Reliable)
	void replicateTextureOwner(const FString& name);

	// Accept chunk from owner on server
	UFUNCTION(Server, Reliable, WithValidation)
	void replicateChunkServer(const TArray<uint8>& chunk, bool end, const FString& textureName);


	// Accept chunk from server on owner
	UFUNCTION(Client, Reliable)
	void replicateChunkOwner(const TArray<uint8>& chunk, bool end, const FString& textureName);

	// Call from client to fetch textures with server
	UFUNCTION(Server, Reliable)
	void fetchTextures();

	// Ask owner to send next chunk of texture
	UFUNCTION(Client, Reliable)
	void askChunkOwner(const FString& name, uint64 begin);

	UFUNCTION(Server, Reliable, WithValidation)
	void askChunkServer(const FString& name, uint64 begin);

private:

	uint64 getChunk(const FString& name, uint64 begin, TArray<uint8>& chunk) const;

	// Recieve and save chunk as needed, returns true if texture is loaded properly
	void recieveChunk(const TArray<uint8>& chunk, bool end, const FString& textureName);

	void replicateTextureToAll(const FString& name);

	void preReplicateTexture(UTexture2D* texture, const FString& name);

	void beginReplicateTexture(const FString& name);

	void postReplicateTexture(UTexture2D* texture, const FString& name);

	bool shouldReplicateTexture(const FString& name);

	void compressTexture(const FString& name);

};


// TODO: CancelReplicateTexture(name) - Disable replication of this texture 
// TODO: Async compress/decompress tasks

