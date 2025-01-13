// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Async/AsyncWork.h"


/**
 * 
 */
class AsyncCompressTextureTask : FNonAbandonableTask
{

private:

	FString textureName;

public:
	AsyncCompressTextureTask(FString name);

	void DoWork();
};
