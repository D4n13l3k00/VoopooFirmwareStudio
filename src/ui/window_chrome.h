#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
namespace WindowChrome {
void Initialize(HWND);
bool InitializeGraphics(ID3D11Device*, ID3D11DeviceContext*);
void ReleaseGraphics();
float Height();
bool HandleMessage(HWND, UINT, WPARAM, LPARAM, LRESULT&);
void Render(HWND);
}
