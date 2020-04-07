// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <mutex>

#include "CoreMinimal.h"
#include "Networking.h"
#include "Sockets.h"
#include "Engine.h"
#include "Engine/SceneCapture2D.h"

#include "OSCSpoutCamActor.generated.h"

/**
 * 
 */
UCLASS()
class OSCSPOUTCAM_API AOSCSpoutCamActor : public ASceneCapture2D
{
	GENERATED_BODY()
	
	std::mutex mutex;
	FSocket *m_Socket = nullptr;
	FUdpSocketReceiver *m_Receiver = nullptr;

	void OpenPort(int port);
	void ClosePort();

	void UdpReceivedCallback(const FArrayReaderPtr& data, const FIPv4Endpoint& ip);

	class SpoutContext;
	TSharedPtr<SpoutContext> context;

public:

	FMatrix Modelview;
	FMatrix Projection;

	AOSCSpoutCamActor();

	UPROPERTY(EditInstanceOnly, Category = "OSCCamera")
	int OSCPort = 12000;

	UPROPERTY(EditInstanceOnly, Category = "OSCCamera")
	FName PublishName = "OSCSpoutCam";

	UPROPERTY(EditInstanceOnly, Category = "OSCCamera")
	bool AutoSetViewTargetWhenBeginPlay = true;

	void BeginPlay() override;
	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	void Tick(float DeltaSeconds) override;
};
