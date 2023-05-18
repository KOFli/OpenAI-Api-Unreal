// Fill out your copyright notice in the Description page of Project Settings.


#include "OpenAICallTurbo.h"
#include "OpenAIUtils.h"
#include "Http.h"
#include "OpenAIParser.h"

UOpenAICallTurbo::UOpenAICallTurbo()
{
}

UOpenAICallTurbo::~UOpenAICallTurbo()
{
}

UOpenAICallTurbo* UOpenAICallTurbo::OpenAICallTurbo(EOATurboModel model, const TArray<FOAChatMessage>& msg, FOASettings settings)
{
	UOpenAICallTurbo* BPNode = NewObject<UOpenAICallTurbo>();
	BPNode->model = model;
	BPNode->messages = msg;
	BPNode->settings = settings;
	return BPNode;
}

void UOpenAICallTurbo::Activate()
{
	FString _apiKey;
	if (UOpenAIUtils::getUseApiKeyFromEnvironmentVars())
		_apiKey = UOpenAIUtils::GetEnvironmentVariable(TEXT("OPENAI_API_KEY"));
	else
		_apiKey = UOpenAIUtils::getApiKey();


	// checking parameters are valid
	if (_apiKey.IsEmpty())
	{
		Finished.Broadcast({}, TEXT("Api key is not set"), {}, false);
	}
	else if (messages.IsEmpty())
	{
		Finished.Broadcast({}, TEXT("content is empty"), {}, false);
	}
	else if (settings.stopSequences.Num() > 4)
	{
		Finished.Broadcast({}, TEXT("You can only include up to 4 Stop Sequences"), {}, false);
	}
	else if (settings.stopSequences.Contains(""))
	{
		Finished.Broadcast({}, TEXT("One or more Stop Sequences has no value"), {}, false);
	}

	auto HttpRequest = FHttpModule::Get().CreateRequest();

	FString apiModel;
	switch (model)
	{
	case EOATurboModel::TURBO:
		apiModel = "gpt-3.5-turbo";
		break;
	case EOATurboModel::TURBO_0301:
		apiModel = "gpt-3.5-turbo-0301";
		break;
	}

	// convert parameters to strings
	FString tempHeader = "Bearer ";
	tempHeader += _apiKey;

	// set headers
	FString url = TEXT("https://api.openai.com/v1/chat/completions");
	HttpRequest->SetURL(url);
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetHeader(TEXT("Authorization"), tempHeader);

	//build payload
	TSharedPtr<FJsonObject> _payloadObject = MakeShareable(new FJsonObject());
	_payloadObject->SetStringField(TEXT("model"), apiModel);
	TArray<TSharedPtr<FJsonValue>> messageDetails = {};
	for(auto msg : messages){
		TSharedPtr<FJsonObject> detail = MakeShareable(new FJsonObject());
		detail->SetStringField(TEXT("role"), msg.Role);
		detail->SetStringField(TEXT("content"), msg.Content);
		messageDetails.Add(MakeShareable(new FJsonValueObject(detail)));
	}
	_payloadObject->SetArrayField(TEXT("messages"), messageDetails);
	_payloadObject->SetNumberField(TEXT("max_tokens"), settings.maxTokens);
	_payloadObject->SetNumberField(TEXT("temperature"), FMath::Clamp(settings.temperature, 0.0f, 1.0f));
	_payloadObject->SetNumberField(TEXT("top_p"), FMath::Clamp(settings.topP, 0.0f, 1.0f));
	_payloadObject->SetNumberField(TEXT("n"), settings.numCompletions);
	if (!(settings.presencePenalty == 0))
		_payloadObject->SetNumberField(TEXT("presence_penalty"), FMath::Clamp(settings.presencePenalty, 0.0f, 1.0f));
	if (!(settings.frequencyPenalty == 0))
		_payloadObject->SetNumberField(TEXT("frequency_penalty"), FMath::Clamp(settings.frequencyPenalty, 0.0f, 1.0f));
	if (settings.stopSequences.Num() != 0)
	{
		TArray<TSharedPtr<FJsonValue>> StopSequences;
		for (int i = 0; i < settings.stopSequences.Num(); i++)
		{
			StopSequences.Add(MakeShareable(new FJsonValueString(settings.stopSequences[i])));
		}
		_payloadObject->SetArrayField(TEXT("stop"), StopSequences);
	}


	// convert payload to string
	FString _payload;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&_payload);
	FJsonSerializer::Serialize(_payloadObject.ToSharedRef(), Writer);

	// commit request
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetContentAsString(_payload);

	if (HttpRequest->ProcessRequest())
	{
		HttpRequest->OnProcessRequestComplete().BindUObject(this, &UOpenAICallTurbo::OnResponse);
	}
	else
	{
		Finished.Broadcast({}, ("Error sending request"), {}, false);
	}
}

void UOpenAICallTurbo::OnResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool WasSuccessful)
{
	if (!WasSuccessful)
	{
		UE_LOG(LogTemp, Warning, TEXT("Error processing request. \n%s \n%s"), *Response->GetContentAsString(), *Response->GetURL());
		if (Finished.IsBound())
		{
			Finished.Broadcast({}, *Response->GetContentAsString(), {}, false);
		}

		return;
	}

	TSharedPtr<FJsonObject> responseObject;
	TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
	if (FJsonSerializer::Deserialize(reader, responseObject))
	{
		bool err = responseObject->HasField("error");

		if (err)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s"), *Response->GetContentAsString());
			Finished.Broadcast({}, TEXT("Api error"), {}, false);
			return;
		}

		OpenAIParser parser;
		TArray<FChatCompletion> _out;
		FCompletionInfo _info = parser.ParseCompletionInfo(*responseObject);
		switch (model)
		{
		case EOATurboModel::TURBO:
			_info.model = "gpt-3.5-turbo";
			break;
		case EOATurboModel::TURBO_0301:
			_info.model = "gpt-3.5-turbo-0301";
			break;
		}

		auto CompletionsObject = responseObject->GetArrayField(TEXT("choices"));
		for (auto& elem : CompletionsObject)
		{
			_out.Add(parser.ParseChatCompletion(*elem->AsObject()));
		}

		Finished.Broadcast(_out, "", _info, true);
	}
}
