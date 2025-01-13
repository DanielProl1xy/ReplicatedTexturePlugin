// Fill out your copyright notice in the Description page of Project Settings.


#include "AsyncCompressTextureTask.h"

AsyncCompressTextureTask::AsyncCompressTextureTask(FString name)
	: textureName(name)
{

}

void AsyncCompressTextureTask::DoWork()
{
	UE_LOG(LogTemp, Warning, TEXT("Do work"));
}