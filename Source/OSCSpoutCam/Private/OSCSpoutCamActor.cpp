// Fill out your copyright notice in the Description page of Project Settings.


#include "OSCSpoutCamActor.h"

#include <string>
#include <oscpp/server.hpp>

#include "AllowWindowsPlatformTypes.h" 
#include <d3d11.h>
#include "Spout.h"
#include "HideWindowsPlatformTypes.h"

//////////////////////////////////////////////////////////////////////////

class AOSCSpoutCamActor::SpoutContext
{
public:

	ID3D11Device* D3D11Device = nullptr;
	spoutSenderNames senders;
	spoutDirectX sdx;

public:

	SpoutContext(const FName& Name,
		ID3D11Texture2D* sourceTexture)
		: Name(Name)
		, sourceTexture(sourceTexture)
	{
		D3D11Device = (ID3D11Device*)GDynamicRHI->RHIGetNativeDevice();

		D3D11_TEXTURE2D_DESC desc;
		sourceTexture->GetDesc(&desc);

		width = desc.Width;
		height = desc.Height;

		DXGI_FORMAT texFormat = desc.Format;
		if (desc.Format == DXGI_FORMAT_B8G8R8A8_TYPELESS) {
			texFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
		}

		verify(sdx.CreateSharedDX11Texture(D3D11Device, desc.Width, desc.Height, texFormat, &sendingTexture, sharedSendingHandle));

		ANSICHAR AnsiName[NAME_SIZE];
		Name.GetPlainANSIString(AnsiName);
		Name_str = AnsiName;

		verify(senders.CreateSender(AnsiName, desc.Width, desc.Height, sharedSendingHandle, texFormat));
	}

	~SpoutContext()
	{
		senders.ReleaseSenderName(Name_str.c_str());

		if (sendingTexture)
		{
			sendingTexture->Release();
			sendingTexture = nullptr;
		}
	}

	void Tick()
	{
		if (!deviceContext)
			D3D11Device->GetImmediateContext(&deviceContext);

		ENQUEUE_RENDER_COMMAND(Spout_Tick_RenderThread)([this](FRHICommandListImmediate& RHICmdList) {
			this->deviceContext->CopyResource(sendingTexture, sourceTexture);
			this->deviceContext->Flush();
		});

		D3D11_TEXTURE2D_DESC desc;
		sourceTexture->GetDesc(&desc);

		verify(senders.UpdateSender(Name_str.c_str(), desc.Width, desc.Height, sharedSendingHandle));
	}

	const FName& GetName() const { return Name; }

private:

	FName Name;
	std::string Name_str;
	unsigned int width = 0, height = 0;

	HANDLE sharedSendingHandle = nullptr;
	ID3D11Texture2D* sendingTexture = nullptr;
	ID3D11Texture2D* sourceTexture = nullptr;
	ID3D11DeviceContext* deviceContext = nullptr;
};

//////////////////////////////////////////////////////////////////////////

AOSCSpoutCamActor::AOSCSpoutCamActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	SetActorTickEnabled(true);
}

void AOSCSpoutCamActor::BeginPlay()
{
	ASceneCapture2D::BeginPlay();
	OpenPort(OSCPort);

	context.Reset();

	GWorld->GetFirstPlayerController()->bShowMouseCursor = 1;
	GWorld->GetFirstPlayerController()->bEnableMouseOverEvents = 0;
	GWorld->GetFirstPlayerController()->bEnableClickEvents = 0;
}

void AOSCSpoutCamActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ASceneCapture2D::EndPlay(EndPlayReason);
	ClosePort();
	context.Reset();
}

void AOSCSpoutCamActor::Tick(float DeltaSeconds)
{
	ASceneCapture2D::Tick(DeltaSeconds);

	FMatrix Modelview, Projection;

	{
		std::lock_guard<std::mutex> lock(this->mutex);
		Modelview = this->Modelview.GetTransposed();
		Projection = this->Projection.GetTransposed();
	}

	// update modelview
	{
		FMatrix model_gl_to_ue4;
		model_gl_to_ue4.SetIdentity();

		auto& mat = model_gl_to_ue4;
		mat.M[0][0] = 0, mat.M[0][1] = 0, mat.M[0][2] = -1;
		mat.M[1][0] = 1, mat.M[1][1] = 0, mat.M[1][2] = 0;
		mat.M[2][0] = 0, mat.M[2][1] = 1, mat.M[2][2] = 0;

		FMatrix ConvertedModelView = model_gl_to_ue4 * Modelview * model_gl_to_ue4.GetTransposed();
		ConvertedModelView.ScaleTranslation(FVector(100));

		this->SetActorTransform(FTransform(ConvertedModelView), true);
	}

	// update projection
	{
		auto M = Projection;

		M.M[2][3] *= -1;
		M.M[2][2] *= 0;
		M.M[3][2] *= -100;

		M.M[2][0] *= -1;
		M.M[2][1] *= -1;

		USceneCaptureComponent2D* SC = this->GetCaptureComponent2D();
		SC->bUseCustomProjectionMatrix = true;
		SC->CustomProjectionMatrix = M;
	}

	//////////////////////////////////////////////////////////////////////////

	{
		USceneCaptureComponent2D* SC = this->GetCaptureComponent2D();
		if (!SC->TextureTarget) return;

		auto sourceTexture = (ID3D11Texture2D*)SC->TextureTarget->Resource->TextureRHI->GetTexture2D()->GetNativeResource();
		if (!sourceTexture) return;

		if (!context.IsValid())
		{
			context = TSharedPtr<SpoutContext>(new SpoutContext(PublishName, sourceTexture));
			return;
		}
		else if (PublishName != context->GetName())
		{
			context.Reset();
			return;
		}

		context->Tick();
	}
}

//////////////////////////////////////////////////////////////////////////

void AOSCSpoutCamActor::OpenPort(int port)
{
	ClosePort();

	m_Socket = FUdpSocketBuilder(TEXT("OSCCamera UDP socket"))
		.AsNonBlocking()
		.AsReusable()
		.BoundToPort(port)
		.Build();

	if (m_Socket != nullptr) {
		m_Receiver = new FUdpSocketReceiver(m_Socket, FTimespan::FromMilliseconds(100), TEXT("OSCCamera UDP receiver"));
		m_Receiver->OnDataReceived().BindUObject(this, &AOSCSpoutCamActor::UdpReceivedCallback);
		m_Receiver->Start();

		UE_LOG(LogTemp, Log, TEXT("Connected to UDP port %d"), port);
	}
}

void AOSCSpoutCamActor::ClosePort()
{
	if (m_Receiver)
	{
		m_Receiver->Exit();
		delete m_Receiver;
		m_Receiver = nullptr;
	}

	if (m_Socket)
	{
		m_Socket->Close();
		delete m_Socket;
		m_Socket = nullptr;
	}
}


void handlePacket(AOSCSpoutCamActor* self, const OSCPP::Server::Packet& packet)
{
	if (packet.isBundle())
	{
		OSCPP::Server::Bundle bundle(packet);
		OSCPP::Server::PacketStream packets(bundle.packets());
		while (!packets.atEnd()) {
			handlePacket(self, packets.next());
		}
	}
	else
	{
		OSCPP::Server::Message msg(packet);
		OSCPP::Server::ArgStream args(msg.args());

		auto addr = msg.address();
		if (std::string("/model") == addr)
		{
			FMatrix m;
			m.M[0][0] = args.float32();
			m.M[0][1] = args.float32();
			m.M[0][2] = args.float32();
			m.M[0][3] = args.float32();

			m.M[1][0] = args.float32();
			m.M[1][1] = args.float32();
			m.M[1][2] = args.float32();
			m.M[1][3] = args.float32();

			m.M[2][0] = args.float32();
			m.M[2][1] = args.float32();
			m.M[2][2] = args.float32();
			m.M[2][3] = args.float32();

			m.M[3][0] = args.float32();
			m.M[3][1] = args.float32();
			m.M[3][2] = args.float32();
			m.M[3][3] = args.float32();

			self->Modelview = m;
		}
		else if (std::string("/proj") == addr)
		{
			FMatrix m;
			m.M[0][0] = args.float32();
			m.M[0][1] = args.float32();
			m.M[0][2] = args.float32();
			m.M[0][3] = args.float32();

			m.M[1][0] = args.float32();
			m.M[1][1] = args.float32();
			m.M[1][2] = args.float32();
			m.M[1][3] = args.float32();

			m.M[2][0] = args.float32();
			m.M[2][1] = args.float32();
			m.M[2][2] = args.float32();
			m.M[2][3] = args.float32();

			m.M[3][0] = args.float32();
			m.M[3][1] = args.float32();
			m.M[3][2] = args.float32();
			m.M[3][3] = args.float32();

			self->Projection = m;
		}
	}
}

void AOSCSpoutCamActor::UdpReceivedCallback(const FArrayReaderPtr& data, const FIPv4Endpoint& ip)
{
	OSCPP::Server::Packet packet(data->GetData(), data->Num());

	{
		std::lock_guard<std::mutex> lock(this->mutex);
		handlePacket(this, packet);
	}
}
