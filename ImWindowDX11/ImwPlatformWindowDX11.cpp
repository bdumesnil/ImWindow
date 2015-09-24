#include "ImwPlatformWindowDX11.h"

#include "ImwWindowManager.h"

#include <windowsx.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <d3dx10.h>
#include <DxErr.h>

#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "d3dx11.lib")
#pragma comment (lib, "d3dx10.lib")
#pragma comment (lib, "d3dcompiler.lib")
#pragma comment (lib, "dxgi.lib")
#pragma comment (lib, "dxerr.lib")

using namespace ImWindow;

ImwPlatformWindowDX11::InstanceMap		ImwPlatformWindowDX11::s_mInstances;
bool									ImwPlatformWindowDX11::s_bClassInitialized = false;
WNDCLASSEX								ImwPlatformWindowDX11::s_oWndClassEx;

IDXGIFactory*							ImwPlatformWindowDX11::s_pFactory = NULL;
ID3D11Device*							ImwPlatformWindowDX11::s_pDevice = NULL;
ID3D11DeviceContext*					ImwPlatformWindowDX11::s_pDeviceContext = NULL;

ImwPlatformWindowDX11::ImwPlatformWindowDX11( bool bMain )
	: ImwPlatformWindow( bMain )
{
	m_pSwapChain = NULL;
	m_pRenderTargetView = NULL;
	m_bDrag = false; 
}

ImwPlatformWindowDX11::~ImwPlatformWindowDX11()
{
	s_mInstances.erase(m_hWnd);

	//ImwSafeRelease(m_pDevice);
	//ImwSafeRelease(m_pDeviceContext);

	ImwSafeRelease(m_pSwapChain);
	ImwSafeRelease(m_pRenderTargetView);

	DestroyWindow(m_hWnd);
}

bool ImwPlatformWindowDX11::Init(ImwPlatformWindow* pMain)
{
	InitWndClassEx();

	HRESULT hr;

	DWORD iWindowStyle = (pMain != NULL) ? WS_POPUP | WS_VISIBLE | WS_THICKFRAME : WS_OVERLAPPEDWINDOW;
	RECT wr = { 0, 0, 800, 600 };
	AdjustWindowRect(&wr, iWindowStyle, FALSE);

	m_hWnd = CreateWindowEx(NULL,
		"ImwPlatformWindowDX11",
		"ImwWindow",
		iWindowStyle,
		300,
		300,
		wr.right - wr.left,
		wr.bottom - wr.top,
		(pMain != NULL) ? ((ImwPlatformWindowDX11*)pMain)->GetHWnd() : NULL,
		NULL,
		GetModuleHandle(NULL),
		NULL);

	s_mInstances.insert(std::pair<HWND, ImwPlatformWindowDX11*>(m_hWnd, this));

	DXGI_SWAP_CHAIN_DESC scd;
	ZeroMemory(&scd, sizeof(DXGI_SWAP_CHAIN_DESC));
	scd.BufferCount = 1;
	scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scd.OutputWindow = m_hWnd;
	scd.SampleDesc.Count = 4;
	scd.Windowed = true;

	hr = s_pFactory->CreateSwapChain( s_pDevice, &scd, &m_pSwapChain );

	if (FAILED(hr))
	{
		MessageBox(NULL, DXGetErrorDescription(hr), TEXT("D3D11CreateDeviceAndSwapChain"), MB_OK);
		return false;
	}

	hr = s_pFactory->MakeWindowAssociation(m_hWnd, DXGI_MWA_NO_ALT_ENTER);

	if (FAILED(hr))
	{
		MessageBox(NULL, DXGetErrorDescription(hr), TEXT("m_pFactory->MakeWindowAssociation"), MB_OK);
		return false;
	}

	//Create our BackBuffer
	ID3D11Texture2D* pBackBuffer;
	hr = m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
	if (FAILED(hr))
	{
		MessageBox(NULL, DXGetErrorDescription(hr), TEXT("m_pSwapChain->GetBuffer"), MB_OK);
		return false;
	}

	//Create our Render Target
	hr = s_pDevice->CreateRenderTargetView(pBackBuffer, NULL, &m_pRenderTargetView);
	pBackBuffer->Release();
	if (FAILED(hr))
	{
		MessageBox(NULL, DXGetErrorDescription(hr), TEXT("m_pDevice->CreateRenderTargetView"), MB_OK);
		return false;
	}

	//Set our Render Target
	s_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, NULL);

	SetState();
	ImGui_ImplDX11_Init(m_hWnd, s_pDevice, s_pDeviceContext);
	RestoreState();
	return true;
}

int ImwPlatformWindowDX11::GetWidth() const
{
	return m_iWidth;
}

int ImwPlatformWindowDX11::GetHeight() const
{
	return m_iHeight;
}

void ImwPlatformWindowDX11::Show()
{
	ShowWindow(m_hWnd, SW_SHOW);
}

void ImwPlatformWindowDX11::Hide()
{
	ShowWindow(m_hWnd, SW_HIDE);
}

void ImwPlatformWindowDX11::SetSize(int iWidth, int iHeight)
{
	SetWindowPos(m_hWnd, 0, 0, 0, iWidth, iHeight, SWP_NOMOVE);
}

void ImwPlatformWindowDX11::SetPos(int iX, int iY)
{
	SetWindowPos(m_hWnd, 0, iX, iY, 0, 0, SWP_NOSIZE);
}

void ImwPlatformWindowDX11::SetTitle(const char* pTtile)
{
	SetWindowText(m_hWnd, pTtile);
}

void ImwPlatformWindowDX11::Paint()
{
	if (m_bDrag)
	{
		//GetCursorPos()

		RECT oRect;
		GetWindowRect(m_hWnd, &oRect);

		POINT oCursorPoint;
		GetCursorPos(&oCursorPoint);

		int iX = m_iWindowPosStartDrag.x + oCursorPoint.x - m_iCursorPosStartDrag.x;
		int iY = m_iWindowPosStartDrag.y + oCursorPoint.y - m_iCursorPosStartDrag.y;
		SetWindowPos(m_hWnd, 0, iX, iY, 0, 0, SWP_NOSIZE);
	}

	if ( NULL != m_pSwapChain )
	{
		D3DXCOLOR bgColor(0.4f, 0.4f, 0.4f, 1.0f);

		s_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, NULL);

		ImwIsSafe(s_pDeviceContext)->ClearRenderTargetView(m_pRenderTargetView, bgColor);

		SetState();

		ImGui_ImplDX11_NewFrame(/*m_hWnd*/);
		
		RECT rect;
		GetClientRect(m_hWnd, &rect);
		ImGui::GetIO().DisplaySize = ImVec2((float)(rect.right - rect.left), (float)(rect.bottom - rect.top));

		ImwPlatformWindow::Paint();

		RestoreState();

		//Present the backbuffer to the screen
		ImwIsSafe(m_pSwapChain)->Present(0, 0);
	}
}

void ImwPlatformWindowDX11::StartDrag()
{
	m_bDrag = true;
	RECT oRect;
	GetWindowRect(m_hWnd, &oRect);
	m_iWindowPosStartDrag.x = oRect.left;
	m_iWindowPosStartDrag.y = oRect.top;

	POINT oCursorPoint;
	GetCursorPos(&oCursorPoint);
	m_iCursorPosStartDrag.x = oCursorPoint.x;
	m_iCursorPosStartDrag.y = oCursorPoint.y;
}

void ImwPlatformWindowDX11::StopDrag()
{
	m_bDrag = false;
}

bool ImwPlatformWindowDX11::IsDraging()
{
	return m_bDrag;
}


HWND ImwPlatformWindowDX11::GetHWnd()
{
	return m_hWnd;
}

LRESULT ImwPlatformWindowDX11::OnMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
	SetState();
	ImGuiIO& io = ImGui::GetIO();
	RestoreState();

	switch (message)
	{
	case WM_CLOSE:
		OnClose();
		break;
	//case WM_ENTERSIZEMOVE:
	//case WM_EXITSIZEMOVE:
	case WM_SIZE:
		{
			//RECT wr = { 0, 0, LOWORD(lParam), HIWORD(lParam) };
			//AdjustWindowRect(&wr, GetWindowLong(m_hWnd, GWL_STYLE), FALSE);
			if (NULL != m_pSwapChain)
			{
				s_pDeviceContext->OMSetRenderTargets(0, 0, 0);

				// Release all outstanding references to the swap chain's buffers.
				m_pRenderTargetView->Release();

				HRESULT hr;
				// Preserve the existing buffer count and format.
				// Automatically choose the width and height to match the client rect for HWNDs.
				hr = m_pSwapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);

				// Perform error handling here!

				// Get buffer and create a render-target-view.
				ID3D11Texture2D* pBuffer;
				hr = m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBuffer);
				// Perform error handling here!

				hr = s_pDevice->CreateRenderTargetView(pBuffer, NULL, &m_pRenderTargetView);
				// Perform error handling here!
				pBuffer->Release();

				s_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, NULL);

				m_iWidth = LOWORD(lParam);
				m_iHeight = HIWORD(lParam);
				// Set up the viewport.
				D3D11_VIEWPORT vp;
				vp.Width = m_iWidth;
				vp.Height = m_iHeight;
				vp.MinDepth = 0.0f;
				vp.MaxDepth = 1.0f;
				vp.TopLeftX = 0;
				vp.TopLeftY = 0;
				s_pDeviceContext->RSSetViewports(1, &vp);

				Paint();
				return 1;
			}
		}
		break;
	case WM_DESTROY:
		//OutputDebugString("WM_DESTROY\n");
		//PostQuitMessage(0);
		break;
	case WM_LBUTTONDOWN:
		io.MouseDown[0] = true;
		return 1;
	case WM_LBUTTONUP:
		io.MouseDown[0] = false; 
		return 1;
	case WM_RBUTTONDOWN:
		io.MouseDown[1] = true; 
		return 1;
	case WM_RBUTTONUP:
		io.MouseDown[1] = false; 
		return 1;
	case WM_MBUTTONDOWN:
		io.MouseDown[2] = true; 
		return 1;
	case WM_MBUTTONUP:
		io.MouseDown[2] = false; 
		return 1;
	case WM_MOUSEWHEEL:
		io.MouseWheel += GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? +1.0f : -1.0f;
		return 1;
	case WM_MOUSEMOVE:
		io.MousePos.x = (signed short)(lParam);
		io.MousePos.y = (signed short)(lParam >> 16); 
		return 1;
	case WM_KEYDOWN:
		if (wParam < 256)
			io.KeysDown[wParam] = 1;
		return 1;
	case WM_KEYUP:
		if (wParam < 256)
			io.KeysDown[wParam] = 0;
		return 1;
	case WM_CHAR:
		// You can also use ToAscii()+GetKeyboardState() to retrieve characters.
		if (wParam > 0 && wParam < 0x10000)
			io.AddInputCharacter((unsigned short)wParam);
		return 1;
	}

	return DefWindowProc(m_hWnd, message, wParam, lParam);
}

// Static

int ImwPlatformWindowDX11::GetInstanceCount()
{
	return s_mInstances.size();
}

void ImwPlatformWindowDX11::InitWndClassEx()
{
	if (!s_bClassInitialized)
	{
		WNDCLASSEX wc;

		ZeroMemory(&wc, sizeof(WNDCLASSEX));

		wc.cbSize = sizeof(WNDCLASSEX);
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = ImwPlatformWindowDX11Proc;
		//wc.hInstance = hInstance;
		wc.hInstance = GetModuleHandle(NULL);
		wc.hCursor = LoadCursor(NULL, IDC_ARROW);
		wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
		wc.lpszClassName = "ImwPlatformWindowDX11";

		RegisterClassEx(&wc);

		s_bClassInitialized = true;
	}
}

LRESULT ImwPlatformWindowDX11::ImwPlatformWindowDX11Proc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	std::map<HWND, ImwPlatformWindowDX11*>::iterator it = s_mInstances.find(hWnd);
	if (it != s_mInstances.end())
	{
		return it->second->OnMessage(message, wParam, lParam);
	}
	/*else
	{
		ImAssert(false, "HWND not found in ImwPlatformWindowDX11 instances");
	}
	*/

	return DefWindowProc(hWnd, message, wParam, lParam);
}

bool ImwPlatformWindowDX11::InitDX11()
{
	HRESULT hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)(&s_pFactory));

	if (FAILED(hr))
	{
		MessageBox(NULL, DXGetErrorDescription(hr), TEXT("CreateDXGIFactory"), MB_OK);
		return false;
	}

	//m_pFactory->CreateSwapChainForHwnd(

	hr = D3D11CreateDevice(NULL,
		D3D_DRIVER_TYPE_HARDWARE,
		NULL,
		NULL,
		NULL,
		NULL,
		D3D11_SDK_VERSION,
		&s_pDevice,
		NULL,
		&s_pDeviceContext);

	if (FAILED(hr))
	{
		MessageBox(NULL, DXGetErrorDescription(hr), TEXT("CreateDXGIFactory"), MB_OK);
		return false;
	}

	return true;
}

void ImwPlatformWindowDX11::ShutdownDX11()
{
	ImwSafeRelease(s_pDevice);
	ImwSafeRelease(s_pDeviceContext);
	ImwSafeRelease(s_pFactory);
}