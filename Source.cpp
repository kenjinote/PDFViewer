// Description: PDFの描画テスト

#define UNICODE
#define _UNICODE

#include <cstdlib>

#include <ppltasks.h>

#include <windows.h>
#include <shcore.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <d2d1_2.h>

#include <windows.data.pdf.interop.h>
#include <windows.storage.streams.h>

#include <wrl/client.h>
#include <wrl/event.h>

#include <atlbase.h>
#include <atlwin.h>
#include <atltypes.h>

#pragma comment(lib, "shcore.lib")
#pragma comment(lib, "runtimeobject.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "windows.data.pdf.lib")

using namespace Platform;
using namespace Windows::Data::Pdf;
using namespace Windows::Foundation;
using namespace Windows::Storage;
using namespace Windows::Storage::Streams;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

class TestWindow
    : public ATL::CWindowImpl<TestWindow>
{
public:
    DECLARE_WND_CLASS_EX(L"Test Window Class", 0, -1);

    BEGIN_MSG_MAP(TestWindow)
        MESSAGE_HANDLER(WM_SIZE, OnSize) //MSG_WM_SIZE(OnSize)
        MESSAGE_HANDLER(WM_PAINT, OnPaint) //MSG_WM_PAINT(OnPaint)
        MESSAGE_HANDLER(WM_CREATE, OnCreate) //MSG_WM_CREATE(OnCreate)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy) //MSG_WM_DESTROY(OnDestroy)
    END_MSG_MAP()

private:
    //void OnSize(UINT type, SIZE size)
    LRESULT OnSize(UINT, WPARAM type, LPARAM lp, BOOL&)
    {
        SIZE size{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        if (type == SIZE_MINIMIZED || type == SIZE_MAXHIDE)
            return 0;

        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        auto hr = resource.m_dxgiSwapChain->GetDesc1(&swapChainDesc);
        if (SUCCEEDED(hr)
            && swapChainDesc.Width == size.cx && swapChainDesc.Height == size.cy)
        {
            return 0;
        }
        resource.m_d2dDeviceContext->SetTarget(nullptr);

        hr = resource.m_dxgiSwapChain->ResizeBuffers(
            swapChainDesc.BufferCount, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
        {
            HandleDeviceLost();
        }
        InitializeRenderTarget();

        BOOL b{};
        OnPaint(0, 0, 0, b); // WTL使用時はOnPaint(nullptr);
        return 0;
    }

    //void OnPaint(HDC)
    LRESULT OnPaint(UINT, WPARAM, LPARAM, BOOL&)
    {
        OnRender();
        ValidateRect(nullptr);
        return 0;
    }

    void OnRender()
    {
        resource.m_d2dDeviceContext->BeginDraw();
        OnRenderCore();
        auto hr = resource.m_d2dDeviceContext->EndDraw();
        if (hr == D2DERR_RECREATE_TARGET)
        {
            HandleDeviceLost();
        }
        else if (SUCCEEDED(hr))
        {
            hr = resource.m_dxgiSwapChain->Present(1, 0);
            if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
            {
                HandleDeviceLost();
            }
        }
    }

    void OnRenderCore()
    {
        resource.m_d2dDeviceContext->Clear(D2D1::ColorF(D2D1::ColorF::White));
        if (m_document != nullptr)
        {
            auto pdfPage = m_document->GetPage(0);
            auto params = PdfRenderParams();
            resource.m_pdfRenderer->RenderPageToDeviceContext(
                reinterpret_cast<IUnknown*>(pdfPage),
                resource.m_d2dDeviceContext.Get(),
                &params);
        }
    }

    //LRESULT OnCreate(const CREATESTRUCT*)
    LRESULT OnCreate(UINT, WPARAM, LPARAM, BOOL&)
    {
        if (FAILED(CreateDeviceIndependentResources()))
            return -1;
        if (FAILED(CreateDeviceResources()))
            return -1;
        LoadPdf();
        return 0;
    }

    void HandleDeviceLost()
    {
        //MessageBox(L"デバイスロストが発生しました。");
        OutputDebugStringW(L"デバイスロストが発生しました。\r\n");

        ReleaseDeviceResources();
        CreateDeviceResources();
        // いったんメッセージループに戻したくて、OnRenderを直接呼ばず、こうしている。
        Invalidate();
    }

    //void OnDestroy()
    LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL&)
    {
        PostQuitMessage(0);
        RevokeDragDrop(m_hWnd);
        ReleaseDeviceResources();
        return 0;
    }

    void ReleaseDeviceResources()
    {
        resource = {};
    }

    HRESULT Direct3DCreateDevice(
        D3D_DRIVER_TYPE driverType, _COM_Outptr_ ID3D11Device** ppDevice)
    {
        UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
        //flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        const D3D_FEATURE_LEVEL featureLevels[] =
        {
          D3D_FEATURE_LEVEL_11_1,
          D3D_FEATURE_LEVEL_11_0,
          D3D_FEATURE_LEVEL_10_1,
          D3D_FEATURE_LEVEL_10_0,
          D3D_FEATURE_LEVEL_9_3,
          D3D_FEATURE_LEVEL_9_2,
          D3D_FEATURE_LEVEL_9_1,
        };
        return D3D11CreateDevice(nullptr,
            driverType,
            nullptr,
            flags,
            featureLevels,
            ARRAYSIZE(featureLevels),
            D3D11_SDK_VERSION,
            ppDevice,
            nullptr,
            nullptr);
    }

    HRESULT Direct3DCreateDevice(_COM_Outptr_ ID3D11Device** ppDevice)
    {
        auto hr = Direct3DCreateDevice(D3D_DRIVER_TYPE_HARDWARE, ppDevice);
        if (SUCCEEDED(hr)) return hr;
        return Direct3DCreateDevice(D3D_DRIVER_TYPE_WARP, ppDevice);
    }

    HRESULT CreateDeviceResources()
    {
        ComPtr<ID3D11Device> device;

        auto hr = Direct3DCreateDevice(&device);
        if (FAILED(hr)) return hr;
        ComPtr<IDXGIDevice2> dxgiDevice;
        hr = device.As(&dxgiDevice);
        if (FAILED(hr)) return hr;

        ComPtr<ID2D1Device> d2d1Device;
        hr = m_d2dFactory->CreateDevice(dxgiDevice.Get(), &d2d1Device);
        if (FAILED(hr)) return hr;

        hr = d2d1Device->CreateDeviceContext(
            D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
            &resource.m_d2dDeviceContext);
        if (FAILED(hr)) return hr;
        ComPtr<IDXGIAdapter> dxgiAdapter;
        hr = dxgiDevice->GetAdapter(&dxgiAdapter);
        if (FAILED(hr)) return hr;
        ComPtr<IDXGIFactory2> dxgiFactory;
        hr = dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory));
        if (FAILED(hr)) return hr;

        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.Width = 0;
        swapChainDesc.Height = 0;
        swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        swapChainDesc.Stereo = FALSE;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = 2;
        swapChainDesc.Scaling = DXGI_SCALING_NONE;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        hr = dxgiFactory->CreateSwapChainForHwnd(
            device.Get(),
            m_hWnd,
            &swapChainDesc,
            nullptr,
            nullptr,
            &resource.m_dxgiSwapChain);
        if (FAILED(hr)) return hr;
        (void)dxgiDevice->SetMaximumFrameLatency(1);
        hr = InitializeRenderTarget();
        if (FAILED(hr)) return hr;
        hr = PdfCreateRenderer(dxgiDevice.Get(), &resource.m_pdfRenderer);
        if (FAILED(hr)) return hr;
        return S_OK;
    }

    HRESULT InitializeRenderTarget()
    {
        ComPtr<ID2D1Bitmap1> d2d1RenderTarget;
        ComPtr<IDXGISurface2> dxgiSurface;
        auto hr = resource.m_dxgiSwapChain->GetBuffer(
            0, IID_PPV_ARGS(&dxgiSurface));
        if (FAILED(hr)) return hr;

        float dpiX, dpiY;
        m_d2dFactory->GetDesktopDpi(&dpiX, &dpiY);
        hr = resource.m_d2dDeviceContext->CreateBitmapFromDxgiSurface(
            dxgiSurface.Get(),
            D2D1::BitmapProperties1(
                D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                D2D1::PixelFormat(
                    DXGI_FORMAT_B8G8R8A8_UNORM,
                    D2D1_ALPHA_MODE_IGNORE)),
            &d2d1RenderTarget);
        if (FAILED(hr)) return hr;
        resource.m_d2dDeviceContext->SetTarget(d2d1RenderTarget.Get());
        return S_OK;
    }

    HRESULT LoadPdf()
    {
        IRandomAccessStream^ s;
        auto hr = CreateRandomAccessStreamOnFile(
            L"sample.pdf",
            static_cast<DWORD>(FileAccessMode::Read),
            IID_PPV_ARGS(reinterpret_cast<
                ABI::Windows::Storage::Streams::IRandomAccessStream**>(&s)));
        if (FAILED(hr))
            return hr;
        return LoadPdf(s);
    }

    HRESULT LoadPdf(_In_ IRandomAccessStream^ s)
    {
        auto asyncTask = concurrency::create_task(
            PdfDocument::LoadFromStreamAsync(s));
        asyncTask.then([this](PdfDocument^ doc)
            {
                m_document = doc;
                Invalidate();
            });
        return S_OK;
    }

    HRESULT CreateDeviceIndependentResources()
    {
        D2D1_FACTORY_OPTIONS options = {};
#ifdef _DEBUG
        options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
        auto hr = D2D1CreateFactory(
            D2D1_FACTORY_TYPE_MULTI_THREADED,
            options,
            m_d2dFactory.GetAddressOf());
        if (FAILED(hr)) return hr;
        return S_OK;
    }

    // デバイス非依存リソース
    ComPtr<ID2D1Factory1> m_d2dFactory;
    // デバイス依存リソース
    struct DeviceResources
    {
        ComPtr<IDXGISwapChain1> m_dxgiSwapChain;
        ComPtr<ID2D1DeviceContext> m_d2dDeviceContext;
        ComPtr<IPdfRendererNative> m_pdfRenderer;
    } resource;
    // その他
    PdfDocument^ m_document;
};

[STAThread]
int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int cmdShow)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    TestWindow wnd;
    if (!wnd.Create(nullptr, ATL::CWindow::rcDefault,
        TEXT("Hello, world"), WS_OVERLAPPEDWINDOW))
    {
        return 0;
    }

    wnd.ShowWindow(cmdShow);
    wnd.UpdateWindow();

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}