#include "ImwWindowManagerDX11.h"
#include "ImwPlatformWindowDX11.h"

#include <imgui_impl_dx11.h>

using namespace ImWindow;

ImwWindowManagerDX11::ImwWindowManagerDX11()
{
	ImwPlatformWindowDX11::InitDX11();
}

ImwWindowManagerDX11::~ImwWindowManagerDX11()
{
	ImwPlatformWindowDX11::ShutdownDX11();
	//ImGui_ImplDX11_Shutdown();
}

void ImwWindowManagerDX11::InternalRun()
{
	MSG msg;
	if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	else
	{
		Update();
	}
}

ImwPlatformWindow* ImwWindowManagerDX11::CreatePlatformWindow(bool bMain, ImwPlatformWindow* pParent)
{
	ImwPlatformWindowDX11* pWindow = new ImwPlatformWindowDX11(bMain);
	ImwTest(pWindow->Init(pParent));
	return (ImwPlatformWindow*)pWindow;
}

ImVec2 ImwWindowManagerDX11::GetCursorPos()
{
	POINT oPoint;
	::GetCursorPos(&oPoint);
	return ImVec2(oPoint.x, oPoint.y);
}