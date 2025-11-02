#include "borderless_window.hpp"
#include <iostream>

static BorderlessWindow* g_instance = nullptr;

BorderlessWindow::BorderlessWindow(const std::string& title, int width, int height, int x, int y)
	: hwnd_(nullptr), width_(width), height_(height), x_(x), y_(y), title_(title), browser_(nullptr) {
}

BorderlessWindow::~BorderlessWindow() {
	if (hwnd_) {
		DestroyWindow(hwnd_);
	}
}

void BorderlessWindow::setBrowser(CefRefPtr<CefBrowser> browser) {
	browser_ = browser;
}

HWND BorderlessWindow::create() {
	WNDCLASSEXW wc = {};
	wc.cbSize = sizeof(WNDCLASSEXW);
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = GetModuleHandle(nullptr);
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszClassName = L"SIMILIBorderlessWindow";

	RegisterClassExW(&wc);

	DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
	
	std::wstring wtitle(title_.begin(), title_.end());

	hwnd_ = CreateWindowExW(
		0,
		L"SIMILIBorderlessWindow",
		wtitle.c_str(),
		style,
		x_, y_, width_, height_,
		nullptr,
		nullptr,
		GetModuleHandle(nullptr),
		this  
	);
	
	if (hwnd_) {
		SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
		ShowWindow(hwnd_, SW_SHOW);
		UpdateWindow(hwnd_);
	}

	return hwnd_;
}

HWND BorderlessWindow::getHandle() const {
	return hwnd_;
}

LRESULT CALLBACK BorderlessWindow::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) 
{
	BorderlessWindow* window = reinterpret_cast<BorderlessWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
	
	switch (uMsg) 
	{
		case WM_GETMINMAXINFO:
		{
			// Set minimum window size
			MINMAXINFO* mmi = (MINMAXINFO*)lParam;
			mmi->ptMinTrackSize.x = 800;
			mmi->ptMinTrackSize.y = 600;
			return 0;
		}
		case WM_SIZING:
			return DefWindowProcW(hwnd, uMsg, wParam, lParam);
		case WM_SIZE:
			if (window && window->browser_) 
			{
				HWND browser_hwnd = window->browser_->GetHost()->GetWindowHandle();
				if (browser_hwnd) {
					RECT rect;
					GetClientRect(hwnd, &rect);
					
					int width = rect.right - rect.left;
					int height = rect.bottom - rect.top;
					
					SetWindowPos(browser_hwnd, nullptr, 
							   0, 0, width, height,
							   SWP_NOZORDER | SWP_NOACTIVATE);
					
					// Notify CEF of resize
					window->browser_->GetHost()->WasResized();
					window->browser_->GetHost()->Invalidate(PET_VIEW);
					
					UpdateWindow(browser_hwnd);
				}
			}
			return 0;
		case WM_ENTERSIZEMOVE:
			std::cout << "[BorderlessWindow] WM_ENTERSIZEMOVE - Start resizing" << std::endl;
			break;
		case WM_EXITSIZEMOVE:
			std::cout << "[BorderlessWindow] WM_EXITSIZEMOVE - End resizing" << std::endl;
			if (window && window->browser_) 
			{
				HWND browser_hwnd = window->browser_->GetHost()->GetWindowHandle();
				if (browser_hwnd) {
					RECT rect;
					GetClientRect(hwnd, &rect);
					
					SetWindowPos(browser_hwnd, nullptr, 
							   rect.left, rect.top, 
							   rect.right - rect.left, rect.bottom - rect.top,
							   SWP_NOZORDER | SWP_NOACTIVATE);
					
					UpdateWindow(browser_hwnd);
					
					window->browser_->GetHost()->WasResized();
					window->browser_->GetHost()->Invalidate(PET_VIEW);
					
					RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
				}
			}
			break;
		case WM_TIMER:
			return 0;
		case WM_ERASEBKGND:
			if (window && window->browser_) {
				HDC hdc = (HDC)wParam;
				RECT rect;
				GetClientRect(hwnd, &rect);
				HBRUSH brush = CreateSolidBrush(RGB(30, 30, 30));
				FillRect(hdc, &rect, brush);
				DeleteObject(brush);
			}
			return 1;
		case WM_CLOSE:
			PostQuitMessage(0);
			return 0;
		case WM_DESTROY:
			return 0;
		default:
			return DefWindowProcW(hwnd, uMsg, wParam, lParam);
	}
	return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

BorderlessWindow* BorderlessWindow::GetInstance() 
{
	return g_instance;
}

void BorderlessWindow::SetInstance(BorderlessWindow* instance) 
{
	g_instance = instance;
}