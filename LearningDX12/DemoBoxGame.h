#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include <memory>
#include <wrl/client.h>


#include "BaseGameInterface.h"
#include "CommandQueue.h"

namespace Olex
{
    class DX12App;

    class DemoBoxGame final
        : public BaseGameInterface
    {
    public:
        DemoBoxGame( DX12App& app, UINT width, UINT height );

        void LoadResources() override;
        void ResizeDepthBuffer( int width, int height );
        void UnloadResources() override;

        int GetClientWidth() const
        {
            return m_Width;
        }

        int GetClientHeight() const
        {
            return m_Height;
        }

        void Update( UpdateEventArgs args ) override;
        void TransitionResource( Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
            Microsoft::WRL::ComPtr<ID3D12Resource> resource,
            D3D12_RESOURCE_STATES beforeState,
            D3D12_RESOURCE_STATES afterState );
        void ClearRTV( Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
            D3D12_CPU_DESCRIPTOR_HANDLE rtv,
            FLOAT* clearColor );
        void ClearDepth( Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
            D3D12_CPU_DESCRIPTOR_HANDLE dsv,
            FLOAT depth = 1.0f );
        void Render( RenderEventArgs args ) override;
        void Resize( ResizeEventArgs args ) override;

        void UpdateBufferResource( Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
            ID3D12Resource** pDestinationResource,
            ID3D12Resource** pIntermediateResource,
            size_t numElements,
            size_t elementSize,
            const void* bufferData,
            D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE );

    private:
        DX12App& m_app;

        // Vertex buffer for the cube.
        Microsoft::WRL::ComPtr<ID3D12Resource> m_VertexBuffer;
        D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView;
        // Index buffer for the cube.
        Microsoft::WRL::ComPtr<ID3D12Resource> m_IndexBuffer;
        D3D12_INDEX_BUFFER_VIEW m_IndexBufferView;

        // Vertex data for a colored cube.
        struct VertexPosColor
        {
            DirectX::XMFLOAT3 Position;
            DirectX::XMFLOAT3 Color;
        };

        VertexPosColor m_Vertices[8] = {
        { DirectX::XMFLOAT3( -1.0f, -1.0f, -1.0f ), DirectX::XMFLOAT3( 0.0f, 0.0f, 0.0f ) }, // 0
        { DirectX::XMFLOAT3( -1.0f,  1.0f, -1.0f ), DirectX::XMFLOAT3( 0.0f, 1.0f, 0.0f ) }, // 1
        { DirectX::XMFLOAT3( 1.0f,  1.0f, -1.0f ), DirectX::XMFLOAT3( 1.0f, 1.0f, 0.0f ) }, // 2
        { DirectX::XMFLOAT3( 1.0f, -1.0f, -1.0f ), DirectX::XMFLOAT3( 1.0f, 0.0f, 0.0f ) }, // 3
        { DirectX::XMFLOAT3( -1.0f, -1.0f,  1.0f ), DirectX::XMFLOAT3( 0.0f, 0.0f, 1.0f ) }, // 4
        { DirectX::XMFLOAT3( -1.0f,  1.0f,  1.0f ), DirectX::XMFLOAT3( 0.0f, 1.0f, 1.0f ) }, // 5
        { DirectX::XMFLOAT3( 1.0f,  1.0f,  1.0f ), DirectX::XMFLOAT3( 1.0f, 1.0f, 1.0f ) }, // 6
        { DirectX::XMFLOAT3( 1.0f, -1.0f,  1.0f ), DirectX::XMFLOAT3( 1.0f, 0.0f, 1.0f ) }  // 7
        };

        WORD m_Indicies[36] =
        {
            0, 1, 2, 0, 2, 3,
            4, 6, 5, 4, 7, 6,
            4, 5, 1, 4, 1, 0,
            3, 2, 6, 3, 6, 7,
            1, 5, 6, 1, 6, 2,
            4, 0, 3, 4, 3, 7
        };

        // Depth buffer.
        Microsoft::WRL::ComPtr<ID3D12Resource> m_DepthBuffer;
        // Descriptor heap for depth buffer.
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_DSVHeap;

        // Root signature
        Microsoft::WRL::ComPtr<ID3D12RootSignature> m_RootSignature;

        // Pipeline state object.
        Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;

        D3D12_VIEWPORT m_Viewport;
        D3D12_RECT m_ScissorRect;

        float m_FoV;

        DirectX::XMMATRIX m_ModelMatrix;
        DirectX::XMMATRIX m_ViewMatrix;
        DirectX::XMMATRIX m_ProjectionMatrix;

        bool m_ContentLoaded;

        void ThrowIfFailed( HRESULT hr );

        int m_Width;
        int m_Height;
    };
}
