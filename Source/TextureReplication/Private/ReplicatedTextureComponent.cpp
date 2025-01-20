// Fill out your copyright notice in the Description page of Project Settings.


#include "ReplicatedTextureComponent.h"
#include "ImageUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY(LogReplicaetdTexture);

AReplicatedTexturesStorage* UReplicatedTextureComponent::textureStorage = nullptr;

UReplicatedTextureComponent::UReplicatedTextureComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicated(true);
	isDownloading = false;
	chunkReady = true;
	bClientJobDone = true;
	bAllJobsDone = true;
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

void UReplicatedTextureComponent::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UReplicatedTextureComponent, bAllJobsDone);
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

bool UReplicatedTextureComponent::ReplicateTexrureFromFile(const FString& path, const FString& name)
{
	if (!shouldReplicateTexture(name)) return false;

	FImage img;
	if (!FImageUtils::LoadImage(*path, img))
	{
		UE_LOG(LogReplicaetdTexture, Error, TEXT("Failed to load image from file %s"), *path);
		return false;
	}

	UTexture2D* texture = FImageUtils::CreateTexture2DFromImage(img);

	if (!IsValid(texture))
	{
		UE_LOG(LogReplicaetdTexture, Error, TEXT("Failed to create texture from image %s"), *path);
		return false;
	}

	preReplicateTexture(texture, name);
	beginReplicateSource(name, img);
	
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
}

void UReplicatedTextureComponent::beginReplicateTexture(const FString& name)
{
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [name, this] {
		bool succeed = compressTexture(name);
		
		AsyncTask(ENamedThreads::GameThread, [name, this, succeed] {
			if(!succeed)
			{
				// If failed to compress - abord replication
				textureStorage->replicatedTextures.Remove(name);
				textureStorage->loadedTexturesNames.RemoveSingleSwap(name);
				return;
			}

			if (GetNetMode() == NM_ListenServer || GetNetMode() == NM_DedicatedServer)
			{
				replicateTextureToAll(name);
			}
			else
			{
				replicateTextureServer(name);
			}
		});
	});
}

void UReplicatedTextureComponent::beginReplicateSource(const FString& name, const FImage& source)
{
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [name, source, this] {
		bool succeed = compressImage(source, name);

		AsyncTask(ENamedThreads::GameThread, [name, this, succeed] {
			if (!succeed)
			{
				// If failed to compress - abord replication
				textureStorage->replicatedTextures.Remove(name);
				textureStorage->loadedTexturesNames.RemoveSingleSwap(name);
				return;
			}

			if (GetNetMode() == NM_ListenServer || GetNetMode() == NM_DedicatedServer)
			{
				replicateTextureToAll(name);
			}
			else
			{
				replicateTextureServer(name);
			}
			});
		});
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

	if (namedQueue.IsEmpty())
	{
		notifyQueueEmtpy();
	}
}

bool UReplicatedTextureComponent::compressImage(const FImage& image, const FString& name)
{
	TArray64<uint8> buffer;
	if (!FImageUtils::CompressImage(buffer, TEXT("png"), image, -5))
	{
		UE_LOG(LogReplicaetdTexture, Error, TEXT("Couldn't compress image \"%s\""), *name);
		return false;
	}

	UE_LOG(LogReplicaetdTexture, Log, TEXT("Texture \"%s\" compressed size = %d"), *name, buffer.Num());

	textureStorage->textureBuffers.Add(name, (TArray<uint8>)buffer);
	return true;
}


bool UReplicatedTextureComponent::compressTexture(const FString& name)
{
	TObjectPtr<UTexture2D>* texture = textureStorage->replicatedTextures.Find(name);

	FImage image;
	if (!FImageUtils::GetTexture2DSourceImage(texture->Get(), image))
	{
		UE_LOG(LogReplicaetdTexture, Error, TEXT("Couldn't get source image \"%s\""), *name);
		return false;
	}


	TArray64<uint8> buffer;
	if (!FImageUtils::CompressImage(buffer, TEXT("png"), image, -5))
	{
		UE_LOG(LogReplicaetdTexture, Error, TEXT("Couldn't compress image \"%s\""), *name);
		return false;
	}

	UE_LOG(LogReplicaetdTexture, Log, TEXT("Texture \"%s\" compressed size = %d"), *name, buffer.Num());

	textureStorage->textureBuffers.Add(name, (TArray<uint8>)buffer);
	return true;
}

bool UReplicatedTextureComponent::replicateTextureServer_Validate(const FString& name)
{
	return !name.IsEmpty();
}

void UReplicatedTextureComponent::replicateTextureServer_Implementation(const FString& name)
{
	if (textureStorage->replicatedTextures.Contains(name))
	{
		UE_LOG(LogReplicaetdTexture, Warning, TEXT("Texture with name \"%s\" is already loaded, skipping"), *name);
		if (namedQueue.IsEmpty()) {
			notifyQueueEmtpy();
		}
		return;
	}

	bAllJobsDone = false;

	namedQueue.AddUnique(name);

	UE_LOG(LogReplicaetdTexture, Log, TEXT("Added texture \"%s\" for replication queue"), *name);
}

void UReplicatedTextureComponent::replicateTextureOwner_Implementation(const FString& name)
{
	if (textureStorage->replicatedTextures.Contains(name))
	{
		UE_LOG(LogReplicaetdTexture, Warning, TEXT("Texture with name \"%s\" is already loaded, skipping"), *name);
		if (namedQueue.IsEmpty()) {
			notifyQueueEmtpy();
		}
		return;
	}

	namedQueue.AddUnique(name);

	UE_LOG(LogReplicaetdTexture, Log, TEXT("Added texture \"%s\" for replication queue"), *name);
}


bool UReplicatedTextureComponent::replicateChunkServer_Validate(const TArray<uint8>& chunk, bool end, const FString& textureName)
{
	if (!textureStorage->textureBuffers.Contains(textureName))
	{
		UE_LOG(LogReplicaetdTexture, Error, TEXT("Recieved buffer with name \"%s\", but it doesn't exist."), *textureName);
		return false;
	}

	if (chunk.Num() > maxChunkSize)
	{
		UE_LOG(LogReplicaetdTexture, Error, TEXT("Recieved chunk with size bigger than max"));
		return false;
	}

	if (textureStorage->textureBuffers.Find(textureName)->Num() + chunk.Num() > maxBufferSize)
	{
		UE_LOG(LogReplicaetdTexture, Error, TEXT("Buffer is bigger than max"));
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
	TArray<uint8> chunk;
	uint64 left = getChunk(name, begin, chunk);
	replicateChunkServer(chunk, left <= 0, name);
	//UE_LOG(LogReplicaetdTexture, Warning, TEXT("Sending chunk with size %04d"), chunk.Num());
}

bool UReplicatedTextureComponent::askChunkServer_Validate(const FString& name, uint64 begin)
{
	if (!textureStorage->textureBuffers.Contains(name)) return false;

	TArray<uint8>* savedBuffer = textureStorage->textureBuffers.Find(name);

	return savedBuffer->IsValidIndex(begin);
}

void UReplicatedTextureComponent::askChunkServer_Implementation(const FString& name, uint64 begin)
{
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
					UE_LOG(LogReplicaetdTexture, Error, TEXT("Couldn't decompress a texture for some reason"));
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

		UReplicatedTextureComponent* repl = Cast<UReplicatedTextureComponent>(
			player->GetComponentByClass(UReplicatedTextureComponent::StaticClass())
		);

		if (IsValid(repl))
		{
			repl->bClientJobDone = false;
			repl->bAllJobsDone = false;
			repl->replicateTextureOwner(name);
		}
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

void UReplicatedTextureComponent::RepNotifyAllJobDone()
{
	if (bAllJobsDone)
	{
		UE_LOG(LogReplicaetdTexture, Log, TEXT("All jobs are finished!"));
		OnAllJobsDone.Broadcast();
	}
}


void UReplicatedTextureComponent::queueEmtpyServer_Implementation()
{
	bClientJobDone = true;
	bAllJobsDone = namedQueue.IsEmpty();
	RepNotifyAllJobDone();
}

void UReplicatedTextureComponent::notifyQueueEmtpy()
{
	UE_LOG(LogReplicaetdTexture, Log, TEXT("Local queue is empty!"));

	// Client finished downloading
	if (GetNetMode() == NM_Client)
	{
		queueEmtpyServer();
		OnQueueEmpty.Broadcast();
	}
	// Server finished downloading
	else
	{
		bAllJobsDone = bClientJobDone;
		OnQueueEmpty.Broadcast();
		RepNotifyAllJobDone();
	}
}