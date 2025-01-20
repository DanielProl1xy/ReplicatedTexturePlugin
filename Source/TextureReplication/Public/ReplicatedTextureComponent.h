// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ReplicatedTexturesStorage.h"
#include "ReplicatedTextureComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTextureReady, const FString&, name, UTexture2D*, texture);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnQueueEmpty);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAllJobsDone);

DECLARE_LOG_CATEGORY_EXTERN(LogReplicaetdTexture, Log, All);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class UReplicatedTextureComponent : public UActorComponent
{

	GENERATED_BODY()

public:

	const static uint64 maxChunkSize = 1024 * 50; 
	const static uint64 maxBufferSize = 1024 * 500; 

	static AReplicatedTexturesStorage* textureStorage;

	UPROPERTY(EditDefaultsOnly)
	bool bPauseReplication;

private:

	UPROPERTY(VisibleAnywhere)
	TArray<FString> namedQueue;

	UPROPERTY(Replicated, VisibleAnywhere, ReplicatedUsing = RepNotifyAllJobDone)
	bool bAllJobsDone;

	// Valid on the server only
	UPROPERTY(VisibleAnywhere)
	bool bClientJobDone;

	FString currentDownloadName;
	bool isDownloading;
	bool chunkReady;


public:

	// Triggered when new texture loaded and decompressed
	UPROPERTY(BlueprintAssignable)
	FOnTextureReady OnTextureReady;

	// Triggered when local queue is emtpy
	UPROPERTY(BlueprintAssignable)
	FOnQueueEmpty OnQueueEmpty;

	// Triggered when Server and Client jobs are done
	UPROPERTY(BlueprintAssignable)
	FOnAllJobsDone OnAllJobsDone;

public:	
	UReplicatedTextureComponent();

public:

	UFUNCTION(BlueprintCallable, Category = "Texture Replication")
	bool ReplicateTexrure(UTexture2D* texture, const FString& name);

	UFUNCTION(BlueprintCallable, Category = "Texture Replication")
	bool ReplicateTexrureFromFile(const FString& path, const FString& name);

	UFUNCTION(BlueprintCallable, Category = "Texture Replication")
	const TArray<FString>& GetLoadedTexturesNames() const;

	// Returns texture by name if found and lodaed
	// Otherwise returns null
	UFUNCTION(BlueprintCallable, Category = "Texture Replication")
	const UTexture2D* FindTexture(const FString& name) const;

	// Pauses replication only within this component
	// Call it only from server
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "Texture Replication")
	void SetPauseReplication(bool pause);

	UFUNCTION(BlueprintCallable, Category = "Texture Replication")
	bool GetAllJobsDone() const { return bAllJobsDone; }

	UFUNCTION(BlueprintCallable, Category = "Texture Replication")
	bool GetClientJobDone() const { return bClientJobDone; }

public:	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

	void GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const override;


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

	UFUNCTION(Server, Reliable)
	void queueEmtpyServer();

private:

	UFUNCTION()
	void RepNotifyAllJobDone();

	uint64 getChunk(const FString& name, uint64 begin, TArray<uint8>& chunk) const;

	// Recieve and save chunk as needed
	void recieveChunk(const TArray<uint8>& chunk, bool end, const FString& textureName);

	void replicateTextureToAll(const FString& name);

	void preReplicateTexture(UTexture2D* texture, const FString& name);

	void beginReplicateTexture(const FString& name);

	void beginReplicateSource(const FString& name, const FImage& source);

	void postReplicateTexture(UTexture2D* texture, const FString& name);

	bool shouldReplicateTexture(const FString& name);

	bool compressImage(const FImage& image, const FString& name);

	bool compressTexture(const FString& name);

	void notifyQueueEmtpy();
};
