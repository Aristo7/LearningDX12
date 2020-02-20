#include "DX12App.h"

// DirectX 12 specific headers.
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <exception>

// D3D12 extension library.
#include <algorithm>

#include "d3dx12.h"

namespace Olex
{
    using namespace Microsoft::WRL;

    DX12App::~DX12App()
    {
        if ( IsInitialized() )
        {
            if ( m_currentGame )
            {
                m_currentGame->UnloadResources();
                m_currentGame.reset();
            }

            m_CommandQueue->Flush();
            m_CommandQueue.reset();
        }
    }

    void DX12App::Init( HWND windowsHandle )
    {
        m_hWnd = windowsHandle;

        EnableDebugLayer();

        m_TearingSupported = CheckTearingSupport();

        // Initialize the global window rect variable.
        ::GetWindowRect( m_hWnd, &m_WindowRect );

        ComPtr<IDXGIAdapter4> dxgiAdapter4 = GetAdapter( m_UseWarp );

        m_Device = CreateDevice( dxgiAdapter4 );

        m_CommandQueue = std::make_unique<CommandQueue>( *this, D3D12_COMMAND_LIST_TYPE_DIRECT );

        m_SwapChain = CreateSwapChain( m_hWnd, m_CommandQueue->GetD3D12CommandQueue(), m_ClientWidth, m_ClientHeight, m_NumFrames );
        m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();
        m_RTVDescriptorHeap = CreateDescriptorHeap( D3D12_DESCRIPTOR_HEAP_TYPE_RTV, m_NumFrames );
        m_RTVDescriptorSize = m_Device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_RTV );
        UpdateRenderTargetViews( m_SwapChain, m_RTVDescriptorHeap );

        m_IsInitialized = true;
    }

    void DX12App::SetGame( std::unique_ptr<BaseGameInterface> game )
    {
        if ( IsInitialized() )
        {
            m_currentGame = std::move( game );
            m_currentGame->LoadResources();
        }
    }

    void DX12App::OnPaintEvent()
    {
        Update();
        Render();
    }

    void DX12App::OnKeyEvent( WPARAM wParam )
    {
        bool alt = ( ::GetAsyncKeyState( VK_MENU ) & 0x8000 ) != 0;

        switch ( wParam )
        {
        case 'V':
            m_VSync = !m_VSync;
            break;
        case VK_ESCAPE:
            ::PostQuitMessage( 0 );
            break;
        case VK_RETURN:
            if ( alt )
            {
        case VK_F11:
            SetFullscreen( !m_Fullscreen );
            }
            break;
        }
    }

    void DX12App::OnResize()
    {
        RECT clientRect = {};
        ::GetClientRect( m_hWnd, &clientRect );

        int width = clientRect.right - clientRect.left;
        int height = clientRect.bottom - clientRect.top;

        Resize( width, height );
    }

    void DX12App::EnableDebugLayer()
    {
#if defined(_DEBUG)
        // Always enable the debug layer before doing anything DX12 related
        // so all possible errors generated while creating DX12 objects
        // are caught by the debug layer.
        Microsoft::WRL::ComPtr<ID3D12Debug> debugInterface;
        ThrowIfFailed( D3D12GetDebugInterface( IID_PPV_ARGS( &debugInterface ) ) );
        debugInterface->EnableDebugLayer();
#endif
    }

    void DX12App::ThrowIfFailed( HRESULT hr )
    {
        if ( FAILED( hr ) )
        {
            throw std::exception();
        }
    }

    Microsoft::WRL::ComPtr<IDXGIAdapter4> DX12App::GetAdapter( bool useWarp )
    {
        using namespace Microsoft::WRL;

        ComPtr<IDXGIFactory4> dxgiFactory;
        UINT createFactoryFlags = 0;
#if defined(_DEBUG)
        createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

        ThrowIfFailed( CreateDXGIFactory2( createFactoryFlags, IID_PPV_ARGS( &dxgiFactory ) ) );

        ComPtr<IDXGIAdapter1> dxgiAdapter1;
        ComPtr<IDXGIAdapter4> dxgiAdapter4;

        if ( useWarp )
        {
            ThrowIfFailed( dxgiFactory->EnumWarpAdapter( IID_PPV_ARGS( &dxgiAdapter1 ) ) );
            ThrowIfFailed( dxgiAdapter1.As( &dxgiAdapter4 ) );
        }
        else
        {
            SIZE_T maxDedicatedVideoMemory = 0;
            for ( UINT i = 0; dxgiFactory->EnumAdapters1( i, &dxgiAdapter1 ) != DXGI_ERROR_NOT_FOUND; ++i )
            {
                DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
                dxgiAdapter1->GetDesc1( &dxgiAdapterDesc1 );

                // Check to see if the adapter can create a D3D12 device without actually
                // creating it. The adapter with the largest dedicated video memory
                // is favored.
                if ( ( dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE ) == 0 ) // looking only for hardware interfaces
                {
                    if ( SUCCEEDED( D3D12CreateDevice( dxgiAdapter1.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof( ID3D12Device ), nullptr ) ) &&
                        dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory )
                    {
                        maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
                        ThrowIfFailed( dxgiAdapter1.As( &dxgiAdapter4 ) );
                    }
                }
            }
        }

        return dxgiAdapter4;
    }

    Microsoft::WRL::ComPtr<ID3D12Device2> DX12App::CreateDevice( Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter )
    {

        ComPtr<ID3D12Device2> d3d12Device2;
        ThrowIfFailed( D3D12CreateDevice( adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS( &d3d12Device2 ) ) );

        // Enable debug messages in debug mode.
#if defined(_DEBUG)
        ComPtr<ID3D12InfoQueue> pInfoQueue;
        if ( SUCCEEDED( d3d12Device2.As( &pInfoQueue ) ) )
        {
            pInfoQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE );
            pInfoQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_ERROR, TRUE );
            pInfoQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_WARNING, TRUE );

            // Suppress whole categories of messages
            //D3D12_MESSAGE_CATEGORY Categories[] = {};

            // Suppress messages based on their severity level
            D3D12_MESSAGE_SEVERITY Severities[] =
            {
                D3D12_MESSAGE_SEVERITY_INFO
            };

            // Suppress individual messages by their ID
            D3D12_MESSAGE_ID DenyIds[] = {
                D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
                D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
                D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
            };

            D3D12_INFO_QUEUE_FILTER NewFilter = {};
            //NewFilter.DenyList.NumCategories = _countof(Categories);
            //NewFilter.DenyList.pCategoryList = Categories;
            NewFilter.DenyList.NumSeverities = _countof( Severities );
            NewFilter.DenyList.pSeverityList = Severities;
            NewFilter.DenyList.NumIDs = _countof( DenyIds );
            NewFilter.DenyList.pIDList = DenyIds;

            ThrowIfFailed( pInfoQueue->PushStorageFilter( &NewFilter ) );
        }
#endif

        return d3d12Device2;
    }

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> DX12App::CreateCommandQueue(
        D3D12_COMMAND_LIST_TYPE type )
    {
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> d3d12CommandQueue;

        D3D12_COMMAND_QUEUE_DESC desc = {};
        desc.Type = type;
        desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.NodeMask = 0;

        ThrowIfFailed( m_Device->CreateCommandQueue( &desc, IID_PPV_ARGS( &d3d12CommandQueue ) ) );

        return d3d12CommandQueue;
    }

    bool DX12App::CheckTearingSupport()
    {
        BOOL allowTearing = FALSE;

        // Rather than create the DXGI 1.5 factory interface directly, we create the
        // DXGI 1.4 interface and query for the 1.5 interface. This is to enable the
        // graphics debugging tools which will not support the 1.5 factory interface
        // until a future update.
        Microsoft::WRL::ComPtr<IDXGIFactory4> factory4;
        if ( SUCCEEDED( CreateDXGIFactory1( IID_PPV_ARGS( &factory4 ) ) ) )
        {
            Microsoft::WRL::ComPtr<IDXGIFactory5> factory5;
            if ( SUCCEEDED( factory4.As( &factory5 ) ) )
            {
                if ( FAILED( factory5->CheckFeatureSupport(
                    DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                    &allowTearing, sizeof( allowTearing ) ) ) )
                {
                    allowTearing = FALSE;
                }
            }
        }

        return allowTearing == TRUE;
    }

    Microsoft::WRL::ComPtr<IDXGISwapChain4> DX12App::CreateSwapChain( HWND hWnd,
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue,
        uint32_t width,
        uint32_t height,
        uint32_t bufferCount )
    {
        ComPtr<IDXGISwapChain4> dxgiSwapChain4;
        Microsoft::WRL::ComPtr<IDXGIFactory4> dxgiFactory4;
        UINT createFactoryFlags = 0;
#if defined(_DEBUG)
        createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

        ThrowIfFailed( CreateDXGIFactory2( createFactoryFlags, IID_PPV_ARGS( &dxgiFactory4 ) ) );

        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.Width = width;
        swapChainDesc.Height = height;
        swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.Stereo = FALSE;
        swapChainDesc.SampleDesc = { 1, 0 };
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = bufferCount;
        swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        // It is recommended to always allow tearing if tearing support is available.
        swapChainDesc.Flags = CheckTearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

        ComPtr<IDXGISwapChain1> swapChain1;
        ThrowIfFailed( dxgiFactory4->CreateSwapChainForHwnd(
            commandQueue.Get(),
            hWnd,
            &swapChainDesc,
            nullptr,
            nullptr,
            &swapChain1 ) );

        // Disable the Alt+Enter fullscreen toggle feature. Switching to fullscreen
        // will be handled manually.
        ThrowIfFailed( dxgiFactory4->MakeWindowAssociation( hWnd, DXGI_MWA_NO_ALT_ENTER ) );

        ThrowIfFailed( swapChain1.As( &dxgiSwapChain4 ) );

        return dxgiSwapChain4;
    }

    ComPtr<ID3D12DescriptorHeap> DX12App::CreateDescriptorHeap(
        D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors )
    {
        ComPtr<ID3D12DescriptorHeap> descriptorHeap;

        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.NumDescriptors = numDescriptors;
        desc.Type = type;

        ThrowIfFailed( m_Device->CreateDescriptorHeap( &desc, IID_PPV_ARGS( &descriptorHeap ) ) );

        return descriptorHeap;
    }

    void DX12App::UpdateRenderTargetViews(
        ComPtr<IDXGISwapChain4> swapChain, ComPtr<ID3D12DescriptorHeap> descriptorHeap )
    {
        m_RTVDescriptorSize = m_Device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_RTV );

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle( descriptorHeap->GetCPUDescriptorHandleForHeapStart() );

        for ( int i = 0; i < m_NumFrames; ++i )
        {
            ComPtr<ID3D12Resource> backBuffer;
            ThrowIfFailed( swapChain->GetBuffer( i, IID_PPV_ARGS( &backBuffer ) ) );

            m_Device->CreateRenderTargetView( backBuffer.Get(), nullptr, rtvHandle );

            m_BackBuffers[i] = backBuffer;

            rtvHandle.Offset( m_RTVDescriptorSize );
        }
    }

    ComPtr<ID3D12CommandAllocator> DX12App::CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE type )
    {
        ComPtr<ID3D12CommandAllocator> commandAllocator;
        ThrowIfFailed( m_Device->CreateCommandAllocator( type, IID_PPV_ARGS( &commandAllocator ) ) );

        return commandAllocator;
    }

    ComPtr<ID3D12GraphicsCommandList> DX12App::CreateCommandList(
        ComPtr<ID3D12CommandAllocator> commandAllocator, D3D12_COMMAND_LIST_TYPE type )
    {
        ComPtr<ID3D12GraphicsCommandList> commandList;
        ThrowIfFailed( m_Device->CreateCommandList( 0, type, commandAllocator.Get(), nullptr, IID_PPV_ARGS( &commandList ) ) );

        return commandList;
    }

    ComPtr<ID3D12GraphicsCommandList2> DX12App::CreateCommandList2(
        ComPtr<ID3D12CommandAllocator> commandAllocator, D3D12_COMMAND_LIST_TYPE type )
    {
        ComPtr<ID3D12GraphicsCommandList2> commandList;
        ThrowIfFailed( m_Device->CreateCommandList( 0, type, commandAllocator.Get(), nullptr, IID_PPV_ARGS( &commandList ) ) );

        return commandList;
    }

    ComPtr<ID3D12Fence> DX12App::CreateFence( UINT64 initialValue )
    {
        ComPtr<ID3D12Fence> fence;

        ThrowIfFailed( m_Device->CreateFence( initialValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS( &fence ) ) );

        return fence;
    }

    HANDLE DX12App::CreateEventHandle()
    {
        HANDLE fenceEvent;

        fenceEvent = ::CreateEvent( NULL, FALSE, FALSE, NULL );
        assert( fenceEvent && "Failed to create fence event." );

        return fenceEvent;
    }

    uint64_t DX12App::Signal( ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence> fence,
        uint64_t& fenceValue )
    {
        const uint64_t fenceValueForSignal = ++fenceValue;
        ThrowIfFailed( commandQueue->Signal( fence.Get(), fenceValueForSignal ) );

        return fenceValueForSignal;
    }

    void DX12App::WaitForFenceValue( ComPtr<ID3D12Fence> fence, uint64_t fenceValue, HANDLE fenceEvent, std::chrono::milliseconds duration )
    {
        if ( fence->GetCompletedValue() < fenceValue )
        {
            ThrowIfFailed( fence->SetEventOnCompletion( fenceValue, fenceEvent ) );
            ::WaitForSingleObject( fenceEvent, static_cast<DWORD>( duration.count() ) );
        }
    }

    void DX12App::Flush( ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence> fence,
        uint64_t& fenceValue, HANDLE fenceEvent )
    {
        uint64_t fenceValueForSignal = Signal( commandQueue, fence, fenceValue );
        WaitForFenceValue( fence, fenceValueForSignal, fenceEvent );
    }

    void DX12App::Update()
    {
        static uint64_t frameCounter = 0;
        static double totalSeconds = 0.0;
        static double elapsedSeconds = 0.0;
        static std::chrono::high_resolution_clock clock;
        static auto t0 = clock.now();

        frameCounter++;
        const auto t1 = clock.now();
        const auto deltaTime = t1 - t0;
        t0 = t1;

        const double frameTime = deltaTime.count() * 1e-9;

        totalSeconds += frameTime;
        elapsedSeconds += frameTime;
        if ( elapsedSeconds > 1.0 )
        {
            wchar_t buffer[500];
            const double fps = frameCounter / elapsedSeconds;
            swprintf_s( buffer, 500, L"FPS: %f\n", fps );
            OutputDebugString( buffer );

            frameCounter = 0;
            elapsedSeconds = 0.0;
        }

        if ( m_currentGame )
        {
            m_currentGame->Update( { frameTime, totalSeconds } );
        }
    }

    void DX12App::Render()
    {
        auto backBuffer = m_BackBuffers[m_CurrentBackBufferIndex];

        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList = m_CommandQueue->GetCommandList();

        // Clear the render target.
        {
            CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                backBuffer.Get(),
                D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET );

            commandList->ResourceBarrier( 1, &barrier );

            FLOAT clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };
            CD3DX12_CPU_DESCRIPTOR_HANDLE rtv( m_RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), m_CurrentBackBufferIndex, m_RTVDescriptorSize );

            commandList->ClearRenderTargetView( rtv, clearColor, 0, nullptr );
        }

        // Present
        {
            CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                backBuffer.Get(),
                D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT );
            commandList->ResourceBarrier( 1, &barrier );

            const uint64_t fenceValueToWaitOn = m_CommandQueue->ExecuteCommandList( commandList );

            const UINT syncInterval = m_VSync ? 1 : 0;
            const UINT presentFlags = m_TearingSupported && !m_VSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
            ThrowIfFailed( m_SwapChain->Present( syncInterval, presentFlags ) );

            m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();

            m_CommandQueue->WaitForFenceValue( fenceValueToWaitOn );
        }
    }

    void DX12App::Resize( uint32_t width, uint32_t height )
    {
        if ( m_ClientWidth != width || m_ClientHeight != height )
        {
            // Don't allow 0 size swap chain back buffers.
            m_ClientWidth = std::max( 1u, width );
            m_ClientHeight = std::max( 1u, height );

            // Flush the GPU queue to make sure the swap chain's back buffers
            // are not being referenced by an in-flight command list.
            m_CommandQueue->Flush();

            for ( auto& backBuffer : m_BackBuffers )
            {
                // Any references to the back buffers must be released
                // before the swap chain can be resized.
                backBuffer.Reset();
            }

            DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
            ThrowIfFailed( m_SwapChain->GetDesc( &swapChainDesc ) );
            ThrowIfFailed( m_SwapChain->ResizeBuffers( m_NumFrames, m_ClientWidth, m_ClientHeight,
                swapChainDesc.BufferDesc.Format, swapChainDesc.Flags ) );

            m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();

            UpdateRenderTargetViews( m_SwapChain, m_RTVDescriptorHeap );
        }
    }

    void DX12App::SetFullscreen( bool fullscreen )
    {
        if ( m_Fullscreen != fullscreen )
        {
            m_Fullscreen = fullscreen;

            if ( m_Fullscreen ) // Switching to fullscreen.
            {
                // Store the current window dimensions so they can be restored
                // when switching out of fullscreen state.
                ::GetWindowRect( m_hWnd, &m_WindowRect );

                // Set the window style to a borderless window so the client area fills the entire screen.
                UINT windowStyle = WS_OVERLAPPEDWINDOW & ~( WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX );

                ::SetWindowLongW( m_hWnd, GWL_STYLE, windowStyle );

                // Query the name of the nearest display device for the window.
                // This is required to set the fullscreen dimensions of the window
                // when using a multi-monitor setup.
                HMONITOR hMonitor = ::MonitorFromWindow( m_hWnd, MONITOR_DEFAULTTONEAREST );
                MONITORINFOEX monitorInfo = {};
                monitorInfo.cbSize = sizeof( MONITORINFOEX );
                ::GetMonitorInfo( hMonitor, &monitorInfo );

                ::SetWindowPos( m_hWnd, HWND_TOP,
                    monitorInfo.rcMonitor.left,
                    monitorInfo.rcMonitor.top,
                    monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
                    monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
                    SWP_FRAMECHANGED | SWP_NOACTIVATE );

                ::ShowWindow( m_hWnd, SW_MAXIMIZE );
            }
            else
            {
                // Restore all the window decorators.
                ::SetWindowLong( m_hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW );

                ::SetWindowPos( m_hWnd, HWND_NOTOPMOST,
                    m_WindowRect.left,
                    m_WindowRect.top,
                    m_WindowRect.right - m_WindowRect.left,
                    m_WindowRect.bottom - m_WindowRect.top,
                    SWP_FRAMECHANGED | SWP_NOACTIVATE );

                ::ShowWindow( m_hWnd, SW_NORMAL );
            }
        }
    }
}
