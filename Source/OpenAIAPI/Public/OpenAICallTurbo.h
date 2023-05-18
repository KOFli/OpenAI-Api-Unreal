// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "OpenAIDefinitions.h"
#include "HttpModule.h"
#include "OpenAICallTurbo.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnTurboResponseRecievedPin, const TArray<FChatCompletion>&, completions, const FString&, errorMessage, const FCompletionInfo&, completionInfo, bool, Success);

UCLASS()
class OPENAIAPI_API UOpenAICallTurbo : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	UOpenAICallTurbo();
	~UOpenAICallTurbo();

	EOATurboModel model = EOATurboModel::TURBO;
	TArray<FOAChatMessage> messages;
	FOASettings settings;

	UPROPERTY(BlueprintAssignable, Category = "OpenAI")
	FOnTurboResponseRecievedPin Finished;

private:
	OpenAIValueMapping mapping;

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true"), Category = "OpenAI")
	static UOpenAICallTurbo* OpenAICallTurbo(EOATurboModel model,const TArray<FOAChatMessage>& msg, FOASettings settings);

	virtual void Activate() override;
	void OnResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool WasSuccessful);
};
