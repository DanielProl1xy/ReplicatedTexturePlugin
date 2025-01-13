// Fill out your copyright notice in the Description page of Project Settings.


#include "ReplicatedTextureComponent.h"
#include "ImageUtils.h"
#include "Compression/OodleDataCompressionUtil.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY(LogReplicaetdTexture);

AReplicatedTexturesStorage* UReplicatedTextureComponent::textureStorage = nullptr;

UReplicatedTextureComponent::UReplicatedTextureComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicated(true);
	isDownloading = false;
	chunkReady = true;
}

void UReplicatedTextureComponent::BeginPlay()
{
	Super::BeginPlay();

	if (textureStorage == nullptr)
	{
		textureStorage = Cast<AReplicatedTexturesStorage>(UGameplayStatics::GetActorOfClass(GetWorld()
			, AReplicatedTexturesStorage::StaticClass()));

		// Create one storage, if it doesn't exist
		if (!IsValid(textureStorage))
		{
			textureStorage = GetWorld()->SpawnActor<AReplicatedTexturesStorage>();

			UE_LOG(LogReplicaetdTexture, Log, TEXT("Creating new storage with owner, none existed."));
		}
	}

	if (GetNetMode() == NM_Client)
	{
		fetchTextures();
	}
}

void UReplicatedTextureComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsValid(textureStorage)) return;

	if (bPauseReplication) return;

	if (!isDownloading && !namedQueue.IsEmpty())
	{
		currentDownloadName = namedQueue[0];

		UE_LOG(LogReplicaetdTexture, Log, TEXT("Started downloadning texture \"%s\""), *currentDownloadName);

		isDownloading = true;
		chunkReady = true;
		textureStorage->textureBuffers.Add(currentDownloadName, TArray<uint8>());
	}

	if (isDownloading && chunkReady)
	{
		uint64 begin = textureStorage->textureBuffers.Find(currentDownloadName)->Num();
		if (GetNetMode() == NM_ListenServer || GetNetMode() == NM_DedicatedServer)
		{
			askChunkOwner(currentDownloadName, begin);
		}
		if (GetNetMode() == NM_Client)
		{
			askChunkServer(currentDownloadName, begin);
		}
		chunkReady = false;
	}
}

void UReplicatedTextureComponent::fetchTextures_Implementation()
{
	UE_LOG(LogReplicaetdTexture, Log, TEXT("Fetching textures(%d)")
	, textureStorage->loadedTexturesNames.Num());

	for (const FString& name : textureStorage->loadedTexturesNames)
	{
		replicateTextureOwner(name);
	}
}

bool UReplicatedTextureComponent::ReplicateTexrure(UTexture2D* texture, const FString& name)
{
	if (!shouldReplicateTexture(name)) return false;

	preReplicateTexture(texture, name);
	beginReplicateTexture(name);

	return true;
}

bool UReplicatedTextureComponent::ReplicateTexrureFromBuffer(UTexture2D* texture, const TArray<uint8>& buffer, const FString& name)
{
	if (!shouldReplicateTexture(name)) return false;
	
	textureStorage->textureBuffers.Add(name, buffer);

	preReplicateTexture(texture, name);
	beginReplicateTexture(name);

	return true;
}

bool UReplicatedTextureComponent::shouldReplicateTexture(const FString& name)
{
	if (GetNetMode() == NM_Standalone) return false;

	if (name.IsEmpty())
	{
		UE_LOG(LogReplicaetdTexture, Warning, TEXT("To replicate texture name must be not emtpy!"), *name);
		return false;
	}

	if (textureStorage->replicatedTextures.Contains(name))
	{
		UE_LOG(LogReplicaetdTexture, Warning, TEXT("Can not replicate texture with name \"%s\", it is already replicated"), *name);
		return false;
	}

	return true;
}


void UReplicatedTextureComponent::preReplicateTexture(UTexture2D* texture, const FString& name)
{
	textureStorage->replicatedTextures.Add(name, texture);
	textureStorage->loadedTexturesNames.Add(name);
	ownedTexturesNames.Add(name);
}

void UReplicatedTextureComponent::beginReplicateTexture(const FString& name)
{
	if (GetNetMode() == NM_ListenServer || GetNetMode() == NM_DedicatedServer)
	{
		replicateTextureToAll(name);
	}
	else
	{
		replicateTextureServer(name);
	}
}

void UReplicatedTextureComponent::postReplicateTexture(UTexture2D* texture, const FString& name)
{
	textureStorage->replicatedTextures.Add(name, texture);
	textureStorage->loadedTexturesNames.Add(name);

#if !UE_SERVER || UE_EDITOR
	// Skip dedicated server
	OnTextureReady.Broadcast(name, texture);

#endif // !UE_SERVER || UE_EDITORe

	if (GetNetMode() == NM_ListenServer || GetNetMode() == NM_DedicatedServer)
	{
		replicateTextureToAll(name);
	}
}


void UReplicatedTextureComponent::compressTexture(const FString& name)
{
	TObjectPtr<UTexture2D>* texture = textureStorage->replicatedTextures.Find(name);

	FImage image;
	FImageUtils::GetTexture2DSourceImage(texture->Get(), image);

	TArray64<uint8> buffer;
	FImageUtils::CompressImage(buffer, TEXT("png"), image, -5);

	UE_LOG(LogReplicaetdTexture, Log, TEXT("Texture \"%s\" compressed size = %d"), *name, buffer.Num());

	textureStorage->textureBuffers.Add(name, (TArray<uint8>)buffer);
}

void UReplicatedTextureComponent::replicateTextureServer_Implementation(const FString& name)
{
	if (textureStorage->replicatedTextures.Contains(name) || namedQueue.Contains(name))
	{
		UE_LOG(LogReplicaetdTexture, Warning, TEXT("Texture with name \"%s\" is already loaded or in queue, skipping"), *name);
		return;
	}

	ownedTexturesNames.Add(name);
	namedQueue.Add(name);

	UE_LOG(LogReplicaetdTexture, Log, TEXT("Added texture \"%s\" for replication queue"), *name);
}

void UReplicatedTextureComponent::replicateTextureOwner_Implementation(const FString& name)
{
	if (textureStorage->replicatedTextures.Find(name) != nullptr || namedQueue.Contains(name))
	{
		UE_LOG(LogReplicaetdTexture, Warning, TEXT("Texture with name \"%s\" is already loaded or in queue, skipping"), *name);
		return;
	}

	namedQueue.Add(name);

	UE_LOG(LogReplicaetdTexture, Log, TEXT("Added texture \"%s\" for replication queue"), *name);
}


bool UReplicatedTextureComponent::replicateChunkServer_Validate(const TArray<uint8>& chunk, bool end, const FString& textureName)
{
	if (!textureStorage->textureBuffers.Contains(textureName))
	{
		UE_LOG(LogReplicaetdTexture, Error, TEXT("Recieved buffer with name \"%s\", but it doesn't exist."), *textureName);
		return false;
	}
	if (textureStorage->replicatedTextures.Contains(textureName))
	{
		UE_LOG(LogReplicaetdTexture, Error, TEXT("Recieved buffer with name \"%s\", but it is already loaded."), *textureName);
		return false;
	}
	return true;
}

void UReplicatedTextureComponent::replicateChunkServer_Implementation(const TArray<uint8>& chunk, bool end, const FString& textureName)
{
	recieveChunk(chunk, end, textureName);
}

void UReplicatedTextureComponent::replicateChunkOwner_Implementation(const TArray<uint8>& chunk, bool end, const FString& textureName)
{
	recieveChunk(chunk, end, textureName);
}

void UReplicatedTextureComponent::askChunkOwner_Implementation(const FString& name, uint64 begin)
{
	if (!textureStorage->textureBuffers.Contains(name))
	{
		AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [name, this, begin] {
			compressTexture(name);

			// After compress finished - repeat request 
			AsyncTask(ENamedThreads::GameThread, [name, this, begin] {
				askChunkOwner(name, begin);
				});
			});
		return;
	}

	TArray<uint8> chunk;
	uint64 left = getChunk(name, begin, chunk);
	replicateChunkServer(chunk, left <= 0, name);
	//UE_LOG(LogReplicaetdTexture, Warning, TEXT("Sending chunk with size %04d"), chunk.Num());
}

bool UReplicatedTextureComponent::askChunkServer_Validate(const FString& name, uint64 begin)
{
	return textureStorage->replicatedTextures.Contains(name);
}

void UReplicatedTextureComponent::askChunkServer_Implementation(const FString& name, uint64 begin)
{
	if (!textureStorage->textureBuffers.Contains(name))
	{
		AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [name, this, begin] {
			compressTexture(name);

			// After compress finished - repeat request 
			AsyncTask(ENamedThreads::GameThread, [name, this, begin] {
				askChunkServer(name, begin);
			});
		});
		return;
	}

	TArray<uint8> chunk;
	uint64 left = getChunk(name, begin, chunk);
	replicateChunkOwner(chunk, left <= 0, name);
	//UE_LOG(LogReplicaetdTexture, Warning, TEXT("Sending chunk with size %04d"), chunk.Num());
}

uint64 UReplicatedTextureComponent::getChunk(const FString& name, uint64 begin, TArray<uint8>& chunk) const
{
	TArray<uint8>* savedBuffer = textureStorage->textureBuffers.Find(name);

	savedBuffer = textureStorage->textureBuffers.Find(name);

	uint64 left = savedBuffer->Num() - begin;
	uint64 chunkSize = FMath::Min(left, maxChunkSize);
	
	chunk.AddUninitialized(chunkSize);
	FMemory::Memcpy(chunk.GetData(), savedBuffer->GetData() + begin, chunkSize);

	return savedBuffer->Num() - (chunkSize + begin);
}

void UReplicatedTextureComponent::recieveChunk(const TArray<uint8>& chunk, bool end, const FString& textureName)
{
	TArray<uint8>* recv = textureStorage->textureBuffers.Find(textureName);

	recv->Append(chunk.GetData(), chunk.Num());

	//UE_LOG(LogReplicaetdTexture, Warning, TEXT("Recieving chunk with size %d, (%d loaded)"), chunk.Num(), recv->Num());

	chunkReady = true;

	if (end)
	{

		isDownloading = false;
		namedQueue.RemoveSingleSwap(textureName);

		// Decompress texture nad validate
		// Remove from buffers, if it's invalid
		AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [textureName, this] {

			TArray<uint8>* buff = textureStorage->textureBuffers.Find(textureName);

			UTexture2D* texture = FImageUtils::ImportBufferAsTexture2D(*buff);

			AsyncTask(ENamedThreads::GameThread, [textureName, texture, this, buff] {
				if (IsValid(texture))
				{
					postReplicateTexture(texture, textureName);
					UE_LOG(LogReplicaetdTexture, Log, TEXT("Texture is ready, total compressed size is %d"), buff->Num());
				}
				else
				{
					UE_LOG(LogReplicaetdTexture, Warning, TEXT("Couldn't decompress a texture for some reason"));
					textureStorage->textureBuffers.Remove(textureName);
				}
			});

		});
	}
}

void UReplicatedTextureComponent::replicateTextureToAll(const FString& name)
{
	TArray<AActor*> players;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerController::StaticClass(), players);

	for (const AActor* pca : players)
	{
		const APlayerController* player = Cast<APlayerController>(pca);

		if (player == GetOwner() || player->IsLocalPlayerController()) continue;

		UReplicatedTextureComponent* repl = Cast<UReplicatedTextureComponent>(
			player->GetComponentByClass(UReplicatedTextureComponent::StaticClass())
		);

		if(IsValid(repl))
			repl->replicateTextureOwner(name);
	}
}

void UReplicatedTextureComponent::SetPauseReplication_Implementation(bool pause)
{
	bPauseReplication = pause;
}


const TArray<FString>& UReplicatedTextureComponent::GetLoadedTexturesNames() const
{
	return textureStorage->loadedTexturesNames;
}

const UTexture2D* UReplicatedTextureComponent:: FindTexture(const FString& name) const
{
	const TObjectPtr<UTexture2D>* texture = textureStorage->replicatedTextures.Find(name);

	if (texture == nullptr)
		return nullptr;
	else
		return texture->Get();
}

