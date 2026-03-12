
#include "CubeApp.h"

#include "../math/MathUtils.h"
#include "../graphics/GpuUploadBuffer.h"
#include "ObjLoader.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
				   PSTR cmdLine, int showCmd)
{

#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    try
    {
        CubeApp theApp(hInstance);
        if(!theApp.Initialize())
            return 0;

        return theApp.Run();
    }
    catch(DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}

CubeApp::CubeApp(HINSTANCE hInstance)
: AppBase(hInstance)
{
	mMainWndCaption = L"Lab4";
}

CubeApp::~CubeApp()
{
}

bool CubeApp::Initialize()
{
    if(!AppBase::Initialize())
		return false;
		

    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));
 
    BuildDescriptorHeaps();
	BuildConstantBuffers();
    BuildRootSignature();
    BuildShadersAndInputLayout();
    BuildSponzaGeometry();
    BuildPSO();


    ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);


    FlushCommandQueue();

	return true;
}

void CubeApp::OnResize()
{
	AppBase::OnResize();


    XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f*MathUtils::Pi, AspectRatio(), 1.0f, 1000.0f);
    XMStoreFloat4x4(&mProj, P);
}

void CubeApp::Update(const FrameTimer& gt)
{
	// Движение камеры по WASD (FPS-стиль)
	float deltaTime = gt.DeltaTime();
	float moveSpeed = mCameraSpeed * deltaTime;
	
	// Вычисляем направление взгляда камеры на основе углов
	// mTheta - горизонтальный угол (yaw), mPhi - вертикальный угол (pitch)
	float forwardX = sinf(mPhi) * cosf(mTheta);
	float forwardZ = sinf(mPhi) * sinf(mTheta);
	float forwardY = cosf(mPhi);
	
	// Вектор вперед (направление взгляда камеры)
	XMVECTOR forward = XMVectorSet(forwardX, forwardY, forwardZ, 0.0f);
	forward = XMVector3Normalize(forward);
	
	// Вектор вправо (перпендикулярно вперед, горизонтально)
	XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR right = XMVector3Cross(forward, worldUp);
	right = XMVector3Normalize(right);
	
	// Вектор вверх относительно камеры (перпендикулярно forward и right)
	XMVECTOR up = XMVector3Cross(right, forward);
	up = XMVector3Normalize(up);
	
	// Движение камеры
	XMVECTOR moveDir = XMVectorZero();
	if (mKeyW) {
		// Вперед - по направлению взгляда (только горизонтально)
		XMVECTOR forwardHoriz = XMVectorSet(forwardX, 0.0f, forwardZ, 0.0f);
		forwardHoriz = XMVector3Normalize(forwardHoriz);
		moveDir = XMVectorAdd(moveDir, forwardHoriz);
	}
	if (mKeyS) {
		// Назад - против направления взгляда (только горизонтально)
		XMVECTOR forwardHoriz = XMVectorSet(forwardX, 0.0f, forwardZ, 0.0f);
		forwardHoriz = XMVector3Normalize(forwardHoriz);
		moveDir = XMVectorSubtract(moveDir, forwardHoriz);
	}
	if (mKeyD) moveDir = XMVectorSubtract(moveDir, right);    // Вправо
	if (mKeyA) moveDir = XMVectorAdd(moveDir, right);  // Влево
	
	// Нормализуем и умножаем на скорость
	XMVECTOR moveDirLength = XMVector3Length(moveDir);
	float length = XMVectorGetX(moveDirLength);
	
	if (length > 0.001f) {
		moveDir = XMVector3Normalize(moveDir);
		moveDir = XMVectorScale(moveDir, moveSpeed);
		
		// Обновляем позицию камеры
		XMVECTOR cameraPosVec = XMLoadFloat3(&mCameraPos);
		cameraPosVec = XMVectorAdd(cameraPosVec, moveDir);
		XMStoreFloat3(&mCameraPos, cameraPosVec);
	}
	
	// Вычисляем точку, на которую смотрит камера (впереди по направлению взгляда)
	XMVECTOR pos = XMLoadFloat3(&mCameraPos);
	XMVECTOR lookAt = XMVectorAdd(pos, forward);
	XMVECTOR upVec = worldUp;
	
	XMMATRIX view = XMMatrixLookAtLH(pos, lookAt, upVec);
	XMStoreFloat4x4(&mView, view);

	XMMATRIX world = XMLoadFloat4x4(&mWorld);
	XMMATRIX proj  = XMLoadFloat4x4(&mProj);
	XMMATRIX wvp   = world * view * proj;

	ObjectConstants obj{};


	XMStoreFloat4x4(&obj.World, XMMatrixTranspose(world));
	XMStoreFloat4x4(&obj.WorldInvTranspose, XMMatrixTranspose(MathUtils::InverseTranspose(world)));
	XMStoreFloat4x4(&obj.WorldViewProj, XMMatrixTranspose(wvp));

	// Камера
	obj.EyePosW = mCameraPos;

	obj.LightDirW   = XMFLOAT3(0.577f, -0.577f, 0.577f);
	obj.LightColor  = XMFLOAT3(1.0f, 1.0f, 1.0f);
	obj.AmbientK    = 0.15f;
	obj.SpecPower   = 64.0f;

	mObjectCB->CopyData(0, obj);
}

void CubeApp::Draw(const FrameTimer& gt)
{

    ThrowIfFailed(mDirectCmdListAlloc->Reset());

    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), mPSO.Get()));

    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            CurrentBackBuffer(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        mCommandList->ResourceBarrier(1, &barrier);
    }

    mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::White, 0, nullptr);
    mCommandList->ClearDepthStencilView(
        DepthStencilView(),
        D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
        1.0f, 0, 0, nullptr);

    const auto rtv = CurrentBackBufferView();
    const auto dsv = DepthStencilView();
    mCommandList->OMSetRenderTargets(1, &rtv, TRUE, &dsv);

    ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvHeap.Get() };
    mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    const auto vbv = mSponzaGeo->VertexBufferView();
    mCommandList->IASetVertexBuffers(0, 1, &vbv);

    const auto ibv = mSponzaGeo->IndexBufferView();
    mCommandList->IASetIndexBuffer(&ibv);

    mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    mCommandList->SetGraphicsRootDescriptorTable(0, mCbvHeap->GetGPUDescriptorHandleForHeapStart());

    mCommandList->DrawIndexedInstanced(
        mBoxGeo->DrawArgs["sponza"].IndexCount,
        1, 0, 0, 0);

    {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            CurrentBackBuffer(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT);
        mCommandList->ResourceBarrier(1, &barrier);
    }

    ThrowIfFailed(mCommandList->Close());

    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    ThrowIfFailed(mSwapChain->Present(0, 0));
    mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

    // Wait until frame commands are complete.
    FlushCommandQueue();
}

void CubeApp::OnMouseDown(WPARAM btnState, int x, int y)
{
    mLastMousePos.x = x;
    mLastMousePos.y = y;

    SetCapture(mhMainWnd);
}

void CubeApp::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void CubeApp::OnMouseMove(WPARAM btnState, int x, int y)
{
    if((btnState & MK_LBUTTON) != 0)
    {
        float dx = XMConvertToRadians(0.25f*static_cast<float>(x - mLastMousePos.x));
        float dy = XMConvertToRadians(0.25f*static_cast<float>(y - mLastMousePos.y));

        mTheta -= dx;
        mPhi += dy;

        mPhi = MathUtils::Clamp(mPhi, 0.1f, MathUtils::Pi - 0.1f);
    }
    else if((btnState & MK_RBUTTON) != 0)
    {
        // ПКМ - движение камеры вперед/назад
        float dy = 0.01f*static_cast<float>(y - mLastMousePos.y);

        // Вычисляем направление взгляда
        float forwardX = sinf(mPhi) * cosf(mTheta);
        float forwardZ = sinf(mPhi) * sinf(mTheta);
        float forwardY = cosf(mPhi);
        
        XMVECTOR forward = XMVectorSet(forwardX, forwardY, forwardZ, 0.0f);
        forward = XMVector3Normalize(forward);
        
        // Движение вперед/назад
        XMVECTOR move = XMVectorScale(forward, -dy);
        XMVECTOR cameraPosVec = XMLoadFloat3(&mCameraPos);
        cameraPosVec = XMVectorAdd(cameraPosVec, move);
        XMStoreFloat3(&mCameraPos, cameraPosVec);
    }

    mLastMousePos.x = x;
    mLastMousePos.y = y;
}

void CubeApp::BuildDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
    cbvHeapDesc.NumDescriptors = 1;
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.NodeMask = 0;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&cbvHeapDesc,
        IID_PPV_ARGS(&mCbvHeap)));
}

void CubeApp::BuildConstantBuffers()
{
	mObjectCB = std::make_unique<GpuUploadBuffer<ObjectConstants>>(md3dDevice.Get(), 1, true);

	UINT objCBByteSize = Dx12Utils::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	D3D12_GPU_VIRTUAL_ADDRESS cbAddress = mObjectCB->Resource()->GetGPUVirtualAddress();
    int boxCBufIndex = 0;
	cbAddress += boxCBufIndex*objCBByteSize;

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
	cbvDesc.BufferLocation = cbAddress;
	cbvDesc.SizeInBytes = Dx12Utils::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	md3dDevice->CreateConstantBufferView(
		&cbvDesc,
		mCbvHeap->GetCPUDescriptorHandleForHeapStart());
}

void CubeApp::BuildRootSignature()
{
	CD3DX12_ROOT_PARAMETER slotRootParameter[1];

	CD3DX12_DESCRIPTOR_RANGE cbvTable;
	cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter, 0, nullptr, 
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if(errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(&mRootSignature)));
}

void CubeApp::BuildShadersAndInputLayout()
{
	mvsByteCode = Dx12Utils::CompileShader(
		L"content/shaders/phong.hlsl",
		nullptr,
		"VS",
		"vs_5_0"
	);

	mpsByteCode = Dx12Utils::CompileShader(
		L"content/shaders/phong.hlsl",
		nullptr,
		"PS",
		"ps_5_0"
	);

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}

//void CubeApp::BuildBoxGeometry()
//{
//    const XMFLOAT4 cubeColor = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f); // красный
//
//    // 24 вершины: по 4 на каждую грань, чтобы нормали были "плоские" (не усреднялись).
//    std::array<Vertex, 24> vertices =
//    {
//        // Front (z = -1), normal (0,0,-1)
//        Vertex{{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, cubeColor},
//        Vertex{{-1.0f, +1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, cubeColor},
//        Vertex{{+1.0f, +1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, cubeColor},
//        Vertex{{+1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, cubeColor},
//
//        // Back (z = +1), normal (0,0,+1)
//        Vertex{{-1.0f, -1.0f, +1.0f}, {0.0f, 0.0f, +1.0f}, cubeColor},
//        Vertex{{+1.0f, -1.0f, +1.0f}, {0.0f, 0.0f, +1.0f}, cubeColor},
//        Vertex{{+1.0f, +1.0f, +1.0f}, {0.0f, 0.0f, +1.0f}, cubeColor},
//        Vertex{{-1.0f, +1.0f, +1.0f}, {0.0f, 0.0f, +1.0f}, cubeColor},
//
//        // Left (x = -1), normal (-1,0,0)
//        Vertex{{-1.0f, -1.0f, +1.0f}, {-1.0f, 0.0f, 0.0f}, cubeColor},
//        Vertex{{-1.0f, +1.0f, +1.0f}, {-1.0f, 0.0f, 0.0f}, cubeColor},
//        Vertex{{-1.0f, +1.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, cubeColor},
//        Vertex{{-1.0f, -1.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, cubeColor},
//
//        // Right (x = +1), normal (+1,0,0)
//        Vertex{{+1.0f, -1.0f, -1.0f}, {+1.0f, 0.0f, 0.0f}, cubeColor},
//        Vertex{{+1.0f, +1.0f, -1.0f}, {+1.0f, 0.0f, 0.0f}, cubeColor},
//        Vertex{{+1.0f, +1.0f, +1.0f}, {+1.0f, 0.0f, 0.0f}, cubeColor},
//        Vertex{{+1.0f, -1.0f, +1.0f}, {+1.0f, 0.0f, 0.0f}, cubeColor},
//
//        // Top (y = +1), normal (0,+1,0)
//        Vertex{{-1.0f, +1.0f, -1.0f}, {0.0f, +1.0f, 0.0f}, cubeColor},
//        Vertex{{-1.0f, +1.0f, +1.0f}, {0.0f, +1.0f, 0.0f}, cubeColor},
//        Vertex{{+1.0f, +1.0f, +1.0f}, {0.0f, +1.0f, 0.0f}, cubeColor},
//        Vertex{{+1.0f, +1.0f, -1.0f}, {0.0f, +1.0f, 0.0f}, cubeColor},
//
//        // Bottom (y = -1), normal (0,-1,0)
//        Vertex{{-1.0f, -1.0f, +1.0f}, {0.0f, -1.0f, 0.0f}, cubeColor},
//        Vertex{{-1.0f, -1.0f, -1.0f}, {0.0f, -1.0f, 0.0f}, cubeColor},
//        Vertex{{+1.0f, -1.0f, -1.0f}, {0.0f, -1.0f, 0.0f}, cubeColor},
//        Vertex{{+1.0f, -1.0f, +1.0f}, {0.0f, -1.0f, 0.0f}, cubeColor},
//    };
//
//    // 36 индексов (6 граней * 2 треугольника * 3)
//    std::array<std::uint16_t, 36> indices =
//    {
//        // front
//        0, 1, 2,   0, 2, 3,
//        // back
//        4, 5, 6,   4, 6, 7,
//        // left
//        8, 9,10,   8,10,11,
//        // right
//        12,13,14,  12,14,15,
//        // top
//        16,17,18,  16,18,19,
//        // bottom
//        20,21,22,  20,22,23
//    };
//
//    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
//    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);
//
//    mBoxGeo = std::make_unique<MeshGeometry>();
//    mBoxGeo->Name = "Kab4";
//
//    ThrowIfFailed(D3DCreateBlob(vbByteSize, &mBoxGeo->VertexBufferCPU));
//    CopyMemory(mBoxGeo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
//
//    ThrowIfFailed(D3DCreateBlob(ibByteSize, &mBoxGeo->IndexBufferCPU));
//    CopyMemory(mBoxGeo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);
//
//    mBoxGeo->VertexBufferGPU = Dx12Utils::CreateDefaultBuffer(
//        md3dDevice.Get(), mCommandList.Get(),
//        vertices.data(), vbByteSize,
//        mBoxGeo->VertexBufferUploader
//    );
//
//    mBoxGeo->IndexBufferGPU = Dx12Utils::CreateDefaultBuffer(
//        md3dDevice.Get(), mCommandList.Get(),
//        indices.data(), ibByteSize,
//        mBoxGeo->IndexBufferUploader
//    );
//
//    mBoxGeo->VertexByteStride = sizeof(Vertex);
//    mBoxGeo->VertexBufferByteSize = vbByteSize;
//    mBoxGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
//    mBoxGeo->IndexBufferByteSize = ibByteSize;
//
//    SubmeshGeometry submesh;
//    submesh.IndexCount = (UINT)indices.size();
//    submesh.StartIndexLocation = 0;
//    submesh.BaseVertexLocation = 0;
//
//    mBoxGeo->DrawArgs["box"] = submesh;
//}

void CubeApp::BuildSponzaGeometry()
{
    ObjMesh mesh;
    if (!ObjLoader::Load("content/models/sponza.obj", mesh))
    {
        throw std::runtime_error("Failed to load sponza.obj");
    }

    mSponzaGeo = std::make_unique<MeshGeometry>();
    mSponzaGeo->Name = "Sponza";

    const UINT vbByteSize = (UINT)mesh.Vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)mesh.Indices.size() * sizeof(std::uint16_t);

    // Создаем CPU буферы
    ThrowIfFailed(D3DCreateBlob(vbByteSize, &mSponzaGeo->VertexBufferCPU));
    CopyMemory(mSponzaGeo->VertexBufferCPU->GetBufferPointer(), mesh.Vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &mSponzaGeo->IndexBufferCPU));
    CopyMemory(mSponzaGeo->IndexBufferCPU->GetBufferPointer(), mesh.Indices.data(), ibByteSize);

    // Создаем GPU буферы
    mSponzaGeo->VertexBufferGPU = Dx12Utils::CreateDefaultBuffer(
        md3dDevice.Get(), mCommandList.Get(),
        mesh.Vertices.data(), vbByteSize,
        mSponzaGeo->VertexBufferUploader
    );

    mSponzaGeo->IndexBufferGPU = Dx12Utils::CreateDefaultBuffer(
        md3dDevice.Get(), mCommandList.Get(),
        mesh.Indices.data(), ibByteSize,
        mSponzaGeo->IndexBufferUploader
    );

    mSponzaGeo->VertexByteStride = sizeof(Vertex);
    mSponzaGeo->VertexBufferByteSize = vbByteSize;
    mSponzaGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
    mSponzaGeo->IndexBufferByteSize = ibByteSize;

    // Создаем подобласть для всей модели
    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)mesh.Indices.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;

    mSponzaGeo->DrawArgs["sponza"] = submesh;

    // Масштабируем модель если нужно
    XMMATRIX scale = XMMatrixScaling(0.01f, 0.01f, 0.01f); // Sponza обычно большая
    XMStoreFloat4x4(&mWorld, scale);
}

void CubeApp::BuildPSO()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
    psoDesc.pRootSignature = mRootSignature.Get();
    psoDesc.VS = 
	{ 
		reinterpret_cast<BYTE*>(mvsByteCode->GetBufferPointer()), 
		mvsByteCode->GetBufferSize() 
	};
    psoDesc.PS = 
	{ 
		reinterpret_cast<BYTE*>(mpsByteCode->GetBufferPointer()), 
		mpsByteCode->GetBufferSize() 
	};
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = mBackBufferFormat;
    psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    psoDesc.DSVFormat = mDepthStencilFormat;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));
}

LRESULT CubeApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Обработка клавиш WASD
    switch (msg)
    {
    case WM_KEYDOWN:
        switch (wParam)
        {
        case 'W':
        case 'w':
            mKeyW = true;
            return 0;
        case 'A':
        case 'a':
            mKeyA = true;
            return 0;
        case 'S':
        case 's':
            mKeyS = true;
            return 0;
        case 'D':
        case 'd':
            mKeyD = true;
            return 0;
        }
        break;
    case WM_KEYUP:
        switch (wParam)
        {
        case 'W':
        case 'w':
            mKeyW = false;
            return 0;
        case 'A':
        case 'a':
            mKeyA = false;
            return 0;
        case 'S':
        case 's':
            mKeyS = false;
            return 0;
        case 'D':
        case 'd':
            mKeyD = false;
            return 0;
        }
        break;
    }
    
    // Вызываем базовую обработку для остальных сообщений
    return AppBase::MsgProc(hwnd, msg, wParam, lParam);
}
