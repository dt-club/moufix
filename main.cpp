#include <Windows.h>
#include <WinNT.h>
#include <intrin.h>
#include "detours/detours.h"
#include <ImageHlp.h>
#include <math.h>

#pragma comment(lib,"ImageHlp.lib")


HMODULE g_hClientDLL;
DWORD g_dwClientDLLSize;

HMODULE g_hEngineDLL;
DWORD g_dwEngineDLLSize;

typedef BOOL( WINAPI *fnGetCursorPos )(LPPOINT lpPoint);
typedef BOOL( WINAPI *fnSetCursorPos )(int X, int Y);


fnGetCursorPos g_pfnGetCursorPos;
fnSetCursorPos g_pfnSetCursorPos;

HWND g_hWnd;
RAWINPUTDEVICE g_rawDevice;
ULONG g_uButtons;
POINT g_ptCursorPos;

#ifndef HID_USAGE_PAGE_GENERIC
#define HID_USAGE_PAGE_GENERIC         ((USHORT) 0x01)
#endif
#ifndef HID_USAGE_GENERIC_MOUSE
#define HID_USAGE_GENERIC_MOUSE        ((USHORT) 0x02)
#endif


BOOL WINAPI newGetCursorPos( __out LPPOINT lpPoint )
{
	ULONG_PTR ReturnAddress = (ULONG_PTR)_ReturnAddress( );

	if( (ULONG_PTR)g_hClientDLL < ReturnAddress && ReturnAddress < ((ULONG_PTR)g_hClientDLL + g_dwClientDLLSize) )
	{
		lpPoint->x = g_ptCursorPos.x;
		lpPoint->y = g_ptCursorPos.y;
		return TRUE;
	}

	if( (ULONG_PTR)g_hEngineDLL < ReturnAddress && ReturnAddress < ((ULONG_PTR)g_hEngineDLL + g_dwEngineDLLSize) )
	{
		lpPoint->x = g_ptCursorPos.x;
		lpPoint->y = g_ptCursorPos.y;
		return TRUE;
	}
	return g_pfnGetCursorPos( lpPoint );
}

BOOL WINAPI newSetCursorPos( int X, int Y )
{
	ULONG_PTR ReturnAddress = (ULONG_PTR)_ReturnAddress( );

	if( (ULONG_PTR)g_hClientDLL < ReturnAddress && ReturnAddress < ((ULONG_PTR)g_hClientDLL + g_dwClientDLLSize) )
	{
		g_ptCursorPos.x = X;
		g_ptCursorPos.y = Y;
	}

	if( (ULONG_PTR)g_hEngineDLL < ReturnAddress && ReturnAddress < ((ULONG_PTR)g_hEngineDLL + g_dwEngineDLLSize) )
	{
		g_ptCursorPos.x = X;
		g_ptCursorPos.y = Y;
	}
	return g_pfnSetCursorPos( X, Y );
}


static int
GetScaledMouseDelta( float scale, int value, float *accum )
{
	if( scale != 1.0f ) {
		*accum += scale * value;
		if( *accum >= 0.0f ) {
			value = (int)floor( *accum );
		}
		else {
			value = (int)ceil( *accum );
		}
		*accum -= value;
	}
	return value;
}

LRESULT CALLBACK WindowProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	UINT dwSize;
	PRAWINPUT pRawData;

	if( uMsg == WM_CREATE )
	{
		g_rawDevice.usUsagePage = HID_USAGE_PAGE_GENERIC;
		g_rawDevice.usUsage = HID_USAGE_GENERIC_MOUSE;
		g_rawDevice.dwFlags = RIDEV_INPUTSINK;
		g_rawDevice.hwndTarget = hWnd;
		RegisterRawInputDevices( &g_rawDevice, 1, sizeof g_rawDevice );
		return DefWindowProc( hWnd, uMsg, wParam, lParam );
	}

	if( uMsg == WM_INPUT )
	{
		if( GetRawInputData( (HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof( RAWINPUTHEADER ) ) == -1 )
			return DefWindowProc( hWnd, uMsg, wParam, lParam );

		pRawData = (PRAWINPUT)alloca( dwSize );
		if( GetRawInputData( (HRAWINPUT)lParam, RID_INPUT, pRawData, &dwSize, sizeof( RAWINPUTHEADER ) ) == -1 )
			return DefWindowProc( hWnd, uMsg, wParam, lParam );

		if( pRawData->header.dwType != RIM_TYPEMOUSE )
			return DefWindowProc( hWnd, uMsg, wParam, lParam );

		if( pRawData->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_1_DOWN )
			g_uButtons |= MK_LBUTTON;

		if( pRawData->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_1_UP )
			g_uButtons &= ~MK_LBUTTON;

		if( pRawData->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_2_DOWN )
			g_uButtons |= MK_RBUTTON;

		if( pRawData->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_2_UP )
			g_uButtons &= ~MK_RBUTTON;

		if( pRawData->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_3_DOWN )
			g_uButtons |= MK_MBUTTON;

		if( pRawData->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_3_UP )
			g_uButtons &= ~MK_MBUTTON;

		if( pRawData->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_4_DOWN )
			g_uButtons |= MK_XBUTTON1;

		if( pRawData->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_4_UP )
			g_uButtons &= ~MK_XBUTTON1;

		if( pRawData->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_5_DOWN )
			g_uButtons |= MK_XBUTTON2;

		if( pRawData->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_5_UP )
			g_uButtons &= ~MK_XBUTTON2;

		if( pRawData->data.mouse.usFlags == MOUSE_MOVE_RELATIVE )
		{
			static float scale_accum_x = 0;
			static float scale_accum_y = 0;

			int xrel = GetScaledMouseDelta( 1.0f, pRawData->data.mouse.lLastX, &scale_accum_x );
			int yrel = GetScaledMouseDelta( 1.0f, pRawData->data.mouse.lLastY, &scale_accum_y );

			g_ptCursorPos.x += xrel;
			g_ptCursorPos.y += yrel;

			if( g_ptCursorPos.x < 0 )
				g_ptCursorPos.x = 0;

			if( g_ptCursorPos.y < 0 )
				g_ptCursorPos.y = 0;

			int right = GetSystemMetrics( SM_CXSCREEN );
			int bottom = GetSystemMetrics( SM_CYSCREEN );

			if( g_ptCursorPos.x > right )
				g_ptCursorPos.x = right;

			if( g_ptCursorPos.y > bottom )
				g_ptCursorPos.y = bottom;
		}

		if( pRawData->data.mouse.usFlags == MOUSE_MOVE_ABSOLUTE )
		{
			g_ptCursorPos.x = pRawData->data.mouse.lLastX;
			g_ptCursorPos.y = pRawData->data.mouse.lLastY;

			if( g_ptCursorPos.x < 0 )
				g_ptCursorPos.x = 0;

			if( g_ptCursorPos.y < 0 )
				g_ptCursorPos.y = 0;
		}

		return DefWindowProc( hWnd, uMsg, wParam, lParam );
	}
	return DefWindowProc( hWnd, uMsg, wParam, lParam );
}

typedef NTSTATUS (NTAPI *fnRtlGetVersion)( _Out_ PRTL_OSVERSIONINFOW lpVersionInformation );

BOOL initialized;
void Initialize( void )
{
	fnRtlGetVersion RtlGetVersion;
	RTL_OSVERSIONINFOW osvi;
	osvi.dwOSVersionInfoSize = sizeof( osvi );
	RtlGetVersion = (fnRtlGetVersion)GetProcAddress( GetModuleHandle( TEXT( "ntdll.dll" ) ), "RtlGetVersion" );

	RtlGetVersion( (PRTL_OSVERSIONINFOW)&osvi );
	if( osvi.dwBuildNumber < 10240 )
		return;


	g_hClientDLL = GetModuleHandle( TEXT( "client.dll" ) );

	if( g_hClientDLL != NULL )
	{
		PIMAGE_NT_HEADERS NtHeader = ImageNtHeader( g_hClientDLL );
		g_dwClientDLLSize = NtHeader->OptionalHeader.SizeOfImage;
	}
	else
	{
		g_hClientDLL = (HMODULE)0x1900000;
		g_dwClientDLLSize = 0x400000;
	}

	g_hEngineDLL = GetModuleHandle( TEXT( "hw.dll" ) );
	if(g_hEngineDLL == NULL)
	{
		g_hEngineDLL = GetModuleHandle( TEXT( "sw.dll" ) );
	}
	if(g_hEngineDLL != NULL)
	{
		PIMAGE_NT_HEADERS NtHeader = ImageNtHeader( g_hEngineDLL );
		g_dwEngineDLLSize = NtHeader->OptionalHeader.SizeOfImage;
	}
	else
	{
		g_hEngineDLL = (HMODULE)0x1D00000;
		g_dwEngineDLLSize = 0x400000;
	}

	WNDCLASSEX WndClassEx = {0};

	WndClassEx.cbSize = sizeof( WndClassEx );
	WndClassEx.lpfnWndProc = WindowProc;
	WndClassEx.hInstance = NULL;
	WndClassEx.hIcon = LoadIcon( NULL, IDI_APPLICATION );
	WndClassEx.hIconSm = LoadIcon( NULL, IDI_APPLICATION );
	WndClassEx.hCursor = LoadCursor( nullptr, IDC_ARROW );
	WndClassEx.style = CS_VREDRAW | CS_HREDRAW;
	WndClassEx.hbrBackground = (HBRUSH)COLOR_WINDOW;
	WndClassEx.lpszClassName = TEXT( "RawMouse-HL" );
	RegisterClassEx( &WndClassEx );

	g_hWnd = CreateWindowEx( WS_EX_TOOLWINDOW | WS_EX_COMPOSITED | WS_EX_TRANSPARENT | WS_EX_LAYERED,
							 TEXT( "RawMouse-HL" ),
							 TEXT( "RawMouse-HL" ),
							 WS_POPUP,
							 CW_USEDEFAULT,
							 CW_USEDEFAULT,
							 1,
							 1,
							 NULL,
							 NULL,
							 NULL,
							 NULL
	);

	g_pfnGetCursorPos = (fnGetCursorPos)GetProcAddress( GetModuleHandleA( "user32.dll" ), "GetCursorPos" );
	g_pfnSetCursorPos = (fnSetCursorPos)GetProcAddress( GetModuleHandleA( "user32.dll" ), "SetCursorPos" );
	DetourTransactionBegin( );
	DetourAttach( (PVOID *)&g_pfnGetCursorPos, newGetCursorPos );
	DetourAttach( (PVOID *)&g_pfnSetCursorPos, newSetCursorPos );
	DetourTransactionCommit( );

	initialized = TRUE;

}

void Shutdown( void )
{
	if( initialized )
	{
		DetourTransactionBegin( );
		DetourDetach( (PVOID *)&g_pfnGetCursorPos, newGetCursorPos );
		DetourDetach( (PVOID *)&g_pfnSetCursorPos, newSetCursorPos );
		DetourTransactionCommit( );
	}
}

BOOL WINAPI DllMain( HMODULE hModule, DWORD dwReason, LPVOID lpReserved )
{
	UNREFERENCED_PARAMETER( lpReserved );

	if( dwReason == DLL_PROCESS_ATTACH )
	{
		Initialize( );
	}

	if( dwReason == DLL_PROCESS_DETACH )
	{
		Shutdown( );
	}

	return TRUE;
}