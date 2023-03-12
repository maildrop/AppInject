// dllmain.cpp : DLL アプリケーションのエントリ ポイントを定義します。
#include "pch.h"
#include <string>

#include <cassert>

#include "../inject-data.h"


static UINT injectMessage = 0;
static HWND notifyWindow = 0;

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
      OutputDebugStringW( L"DLL_PROCESS_ATTACH\n" );
      break;
    case DLL_THREAD_ATTACH:
      break;
    case DLL_THREAD_DETACH:
      break;
    case DLL_PROCESS_DETACH:
      OutputDebugStringW( L"DLL_PROCESS_DETACH\n" );
      break;
    }
    return TRUE;
}


extern "C"{
  __declspec(dllexport) LRESULT CALLBACK HookProc( int nCode , WPARAM wParam , LPARAM lParam );
};


__declspec(dllexport) LRESULT CALLBACK HookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode < 0) {
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }

    if (HC_ACTION == nCode) {
        if (injectMessage == 0) {
            injectMessage = RegisterWindowMessageW(WM_INJECT_STRING);
            OutputDebugStringW(L"RegisterWindowMessageW");
        }
        
        MSG* msg = reinterpret_cast<MSG*>(lParam);
        OutputDebugStringW( (std::to_wstring( wParam ) +L" " + std::to_wstring( msg->message )).c_str() );
        
        switch (msg->message) {
        case WM_PAINT:
          if (notifyWindow) {
            OutputDebugStringW( L"WM_PAINT\n");
            PostMessage(notifyWindow, injectMessage, static_cast<WPARAM>(InjectControl::PAINT_MESSAGE), 0);
          }
          break;
        default:
          {
            if (injectMessage == msg->message) {
              OutputDebugString(TEXT("INJECT MESSAGE"));
              switch (msg->wParam) {
              case static_cast<WPARAM>(InjectControl::NOP):
                break;
              case static_cast<WPARAM>(InjectControl::SHUTDOWN):
                break;
              case static_cast<WPARAM>(InjectControl::WAKEUP):
                OutputDebugStringW(L"InjectControl::WAKEUP");
                if (msg->lParam) {
                  notifyWindow = reinterpret_cast<HWND>(msg->lParam);
                  OutputDebugStringW(std::to_wstring(msg->lParam).c_str());
                  LRESULT const lresult = PostMessage(reinterpret_cast<HWND>(msg->lParam), injectMessage, static_cast<WPARAM>(InjectControl::WAKEUP), (LPARAM)(msg->hwnd));
                  DWORD const lastError = GetLastError();
                  OutputDebugStringW((std::wstring{ L"LRESULT=" } + std::to_wstring(lresult) +
                                      std::wstring{ L" lastError=" } + std::to_wstring(lastError)).c_str());
                }
                break;
              default:
                break;
              }
            }
          }
          break;
        }
    }
    /*
      @see
       CallNextHookEx first paramter hhk is ignored.
       check https://learn.microsoft.com/ja-jp/windows/win32/api/winuser/nf-winuser-callnexthookex
    */
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

#if 0 
__declspec(dllexport) LRESULT CALLBACK HookProc( int nCode , WPARAM wParam , LPARAM lParam )
{
  if( nCode < 0 ){
    return CallNextHookEx( NULL , nCode , wParam , lParam );
  }

  if( HC_ACTION == nCode ){
    if( injectMessage == 0 ){
      injectMessage = RegisterWindowMessageW( WM_INJECT_STRING );
      OutputDebugStringW( L"RegisterWindowMessageW" );
    }
    
    CWPSTRUCT* cw = reinterpret_cast< CWPSTRUCT *>( lParam );
    assert( cw );
    switch( cw->message ){
    case WM_PAINT:

      if( notifyWindow ){
        PostMessage( notifyWindow , injectMessage , static_cast<WPARAM>( InjectControl::PAINT_MESSAGE ), 0 );
      }
      break;
    default:
      {
        if( injectMessage == cw->message ){
          OutputDebugString( TEXT("INJECT MESSAGE"));
          switch( cw->wParam ){
          case static_cast<WPARAM>(InjectControl::NOP) :
            break;
          case static_cast<WPARAM>(InjectControl::SHUTDOWN) :
            break;
          case static_cast<WPARAM>(InjectControl::WAKEUP):
            OutputDebugStringW( L"InjectControl::WAKEUP" );
            if( cw->lParam ){
              notifyWindow = reinterpret_cast<HWND>( cw->lParam );
              OutputDebugStringW( std::to_wstring( cw->lParam ).c_str() );
              LRESULT const lresult = PostMessage( reinterpret_cast<HWND>( cw->lParam ) , injectMessage , static_cast<WPARAM>( InjectControl::WAKEUP ) , (LPARAM)(cw->hwnd) );
              DWORD const lastError = GetLastError();
              OutputDebugStringW( (std::wstring{L"LRESULT="} + std::to_wstring( lresult ) + 
                                   std::wstring{L" lastError="} + std::to_wstring( lastError )).c_str() );
            }
            break;
          default:
            break;
          }
        }
      }
      break;
    }
  }
  /*
    @see
     CallNextHookEx first paramter hhk is ignored.
     check https://learn.microsoft.com/ja-jp/windows/win32/api/winuser/nf-winuser-callnexthookex
  */
  return CallNextHookEx( NULL , nCode , wParam , lParam );
}

#endif
