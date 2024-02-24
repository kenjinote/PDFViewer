// Description: PDFの描画テスト

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
#include <string>
#include "resource.h"

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
	static ATL::CWndClassInfo& GetWndClassInfo()
	{
		static ATL::CWndClassInfo wc = 
		{ 
			{
				sizeof(WNDCLASSEX), 0, StartWindowProc, 0, 0, NULL, LoadIcon(_AtlBaseModule.GetResourceInstance(), MAKEINTRESOURCE(IDI_ICON1)), NULL, 0, NULL, L"PDF Viewer", NULL
			},
			NULL, NULL, IDC_ARROW, TRUE, 0, _T("")
		};
		return wc;
	}

	BEGIN_MSG_MAP(TestWindow)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WM_DROPFILES, OnDropFiles)
		MESSAGE_HANDLER(WM_KEYDOWN, OnKeyDown)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
		MESSAGE_HANDLER(WM_MOUSEWHEEL, OnMouseWheel)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
	END_MSG_MAP()

private:
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

	LRESULT OnPaint(UINT, WPARAM, LPARAM, BOOL&)
	{
		OnRender();
		ValidateRect(nullptr);
		return 0;
	}

	LRESULT OnDropFiles(UINT, WPARAM wParam, LPARAM, BOOL&)
	{
		HDROP hDrop = reinterpret_cast<HDROP>(wParam);
		UINT nFiles = DragQueryFile(hDrop, 0xFFFFFFFF, nullptr, 0);
		if (nFiles > 0)
		{
			WCHAR szFilePath[MAX_PATH];
			DragQueryFile(hDrop, 0, szFilePath, MAX_PATH);
			LoadPdf(szFilePath);
		}
		DragFinish(hDrop);
		SetForegroundWindow(m_hWnd);
		return 0;
	}

	LRESULT OnMouseWheel(UINT, WPARAM wParam, LPARAM, BOOL&)
	{
		bool pageChanged = false;
		if (GET_WHEEL_DELTA_WPARAM(wParam) > 0)
		{
			if (m_page > 0) {
				--m_page;
				pageChanged = true;
			}
		}
		else
		{
			if (m_page < m_pageCount - 1) {
				++m_page;
				pageChanged = true;
			}
		}
		if (pageChanged)
		{
			auto s = std::to_wstring(m_page + 1) + L"/" + std::to_wstring(m_pageCount);
			SetWindowText(s.c_str());
			Invalidate();
		}
		return 0;
	}

	LRESULT OnLButtonDown(UINT, WPARAM wParam, LPARAM, BOOL&)
	{
		if (m_document == nullptr) {
			WCHAR szFilePath[MAX_PATH] = { 0 };
			OPENFILENAME ofn = { 0 };
			ofn.lStructSize = sizeof(OPENFILENAME);
			ofn.hwndOwner = m_hWnd;
			ofn.lpstrFilter = TEXT("ﾕｰｻﾞｰ指定\0*.*\0PDFﾌｧｲﾙ (*.pdf)\0*.pdf\0すべてのﾌｧｲﾙ (*.*)\0*.*\0\0");
			ofn.lpstrFile = szFilePath;
			ofn.nMaxFile = MAX_PATH;
			ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
			ofn.lpstrTitle = TEXT("ファイルを開く");
			if (GetOpenFileName(&ofn))
			{
				LoadPdf(szFilePath);
			}
			return 0;
		}
		RECT rcLeft;
		RECT rcRight;
		GetClientRect(&rcLeft);
		GetClientRect(&rcRight);
		rcLeft.right = rcLeft.right / 2;
		rcRight.left = rcRight.right / 2;
		POINT pt;
		GetCursorPos(&pt);
		ScreenToClient(&pt);
		bool pageChanged = false;
		if (PtInRect(&rcLeft, pt))
		{
			if (m_page > 0) {
				--m_page;
				pageChanged = true;
			}
		}
		else if (PtInRect(&rcRight, pt)) {
			if (m_page < m_pageCount - 1) {
				++m_page;
				pageChanged = true;
			}
		}
		if (pageChanged)
		{
			auto s = std::to_wstring(m_page + 1) + L"/" + std::to_wstring(m_pageCount);
			SetWindowText(s.c_str());
			Invalidate();
		}
		return 0;
	}

	LRESULT OnTimer(UINT, WPARAM wParam, LPARAM, BOOL&)
	{
		KillTimer(0x1234);
		if (m_input.size() > 0)
		{
			int page = _wtoi(m_input.c_str()) - 1;
			if (page < 1) page = 0;
			if (page >= m_pageCount) page = m_pageCount - 1;
			m_page = page;
			auto s = std::to_wstring(m_page + 1) + L"/" + std::to_wstring(m_pageCount);
			SetWindowText(s.c_str());
			Invalidate();
			m_input = std::wstring();
		}
		return 0;
	}

	LRESULT OnKeyDown(UINT, WPARAM wParam, LPARAM, BOOL&)
	{
		bool pageChanged = false;
		switch (wParam)
		{
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		case VK_NUMPAD0:
		case VK_NUMPAD1:
		case VK_NUMPAD2:
		case VK_NUMPAD3:
		case VK_NUMPAD4:
		case VK_NUMPAD5:
		case VK_NUMPAD6:
		case VK_NUMPAD7:
		case VK_NUMPAD8:
		case VK_NUMPAD9:
			KillTimer(0x1234);
			if (VK_NUMPAD0 <= wParam && wParam <= VK_NUMPAD9)
				m_input += static_cast<wchar_t>(wParam - VK_NUMPAD0 + L'0');
			else
				m_input += static_cast<wchar_t>(wParam);
			SetTimer(0x1234, 500, nullptr);			
			break;
		case VK_PRIOR:
		case VK_UP:
		case VK_LEFT:
			if (m_page > 0) {
				--m_page;
				pageChanged = true;
			}
			break;
		case VK_NEXT:
		case VK_DOWN:
		case VK_RIGHT:
			if (m_page < m_pageCount - 1) {
				++m_page;
				pageChanged = true;
			}
			break;
		case VK_HOME:
			if (m_page > 0) {
				m_page = 0;
				pageChanged = true;
			}
			break;
		case VK_END:
			if (m_page < m_pageCount - 1) {
				m_page = m_pageCount - 1;
				pageChanged = true;
			}
			break;
		case VK_ESCAPE:
			if (m_fullScreen)
			{
				SendMessage(WM_KEYDOWN, VK_F11, 0);
			}
			else
			{
				PostMessage(WM_CLOSE);
			}
			break;
		case VK_F11:
			if (m_fullScreen)
			{
				SetWindowLong(GWL_STYLE, m_dwStyle);
				SetWindowPos(HWND_NOTOPMOST, &m_rcNormal, SWP_SHOWWINDOW);
				m_fullScreen = false;
			}
			else
			{
				MONITORINFO monitor_info;
				monitor_info.cbSize = sizeof(monitor_info);
				GetMonitorInfo(MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST),
					&monitor_info);
				
				m_dwStyle = GetWindowLong(GWL_STYLE);
				SetWindowLong(GWL_STYLE, WS_POPUP);
				GetWindowRect(&m_rcNormal);
				SetWindowPos(HWND_TOPMOST, &monitor_info.rcMonitor, SWP_SHOWWINDOW);
				m_fullScreen = true;
			}
			break;
		}
		if (pageChanged)
		{
			auto s = std::to_wstring(m_page + 1) + L"/" + std::to_wstring(m_pageCount);
			SetWindowText(s.c_str());
			Invalidate();
		}
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
			D2D1_SIZE_F size = resource.m_d2dDeviceContext->GetSize();

			auto pdfPage = m_document->GetPage(m_page);

			auto pdf_size = pdfPage->Size;
			float scale = min(size.width / pdf_size.Width, size.height / pdf_size.Height);
			D2D1_RECT_F destRect = D2D1::RectF(
				(size.width - pdf_size.Width * scale) / 2,
				(size.height - pdf_size.Height * scale) / 2,
				(size.width + pdf_size.Width * scale) / 2,
				(size.height + pdf_size.Height * scale) / 2);

			auto params = PdfRenderParams();

			resource.m_d2dDeviceContext->SetTransform(
				D2D1::Matrix3x2F::Scale(scale, scale) * D2D1::Matrix3x2F::Translation(destRect.left, destRect.top));

			resource.m_pdfRenderer->RenderPageToDeviceContext(
				reinterpret_cast<IUnknown*>(pdfPage),
				resource.m_d2dDeviceContext.Get(),
				&params);
		}
	}

	LRESULT OnCreate(UINT, WPARAM, LPARAM, BOOL&)
	{
		m_fullScreen = false;

		if (FAILED(CreateDeviceIndependentResources()))
			return -1;
		if (FAILED(CreateDeviceResources()))
			return -1;
		DragAcceptFiles(TRUE);
		return 0;
	}

	void HandleDeviceLost()
	{
		OutputDebugStringW(L"デバイスロストが発生しました。\r\n");

		ReleaseDeviceResources();
		CreateDeviceResources();
		// いったんメッセージループに戻したくて、OnRenderを直接呼ばず、こうしている。
		Invalidate();
	}

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

	HRESULT LoadPdf(LPCWSTR lpszFilePath)
	{
		IRandomAccessStream^ s;
		auto hr = CreateRandomAccessStreamOnFile(
			lpszFilePath,
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
				m_input = std::wstring();
				m_page = 0;
				m_pageCount = doc->PageCount;
				m_document = doc;
				auto s = std::to_wstring(m_page + 1) + L"/" + std::to_wstring(m_pageCount);
				SetWindowText(s.c_str());
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
	int m_page;
	int m_pageCount;
	std::wstring m_input;
	bool m_fullScreen;
	RECT m_rcNormal;
	DWORD m_dwStyle;
};

[STAThread]
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nShowCmd)
{
	(void)CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

	TestWindow wnd;
	if (!wnd.Create(nullptr, ATL::CWindow::rcDefault, L"PDF Viewer", WS_OVERLAPPEDWINDOW))
	{
		return 0;
	}

	wnd.ShowWindow(nShowCmd);
	wnd.UpdateWindow();

	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return 0;
}