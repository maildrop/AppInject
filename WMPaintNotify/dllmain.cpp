/*
  dllmain.cpp : DLL アプリケーションのエントリ ポイントを定義します。
  
*/
#include "pch.h"

#include "../inject-data.h"

extern "C"{
  BOOL APIENTRY DllMain( HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved );
  __declspec(dllexport) LRESULT CALLBACK HookProc( int nCode , WPARAM wParam , LPARAM lParam );
};

static UINT injectMessage = 0;
static HWND notifyWindow = 0;
static LRESULT doHookProc( int nCode , WPARAM wParam , LPARAM lParam ) noexcept;

/**
   DLL エントリーポイント
 */
BOOL APIENTRY DllMain( HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved )
{
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
      break;
      
    case DLL_THREAD_ATTACH:
      break;
      
    case DLL_THREAD_DETACH:
      break;
      
    case DLL_PROCESS_DETACH:
      break;
    }

    return TRUE;
}


/**
   HookProc は、C の関数として export
   C++ の関数を呼び出すためのラッパーとして機能する。 noexcept 修飾を消去するため。
 */

__declspec(dllexport) LRESULT CALLBACK HookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
  return doHookProc( nCode , wParam , lParam );
}

/**
   HookProc の実装
 */
static LRESULT doHookProc( int nCode , WPARAM wParam , LPARAM lParam ) noexcept
{
  if (nCode < 0) {
    return CallNextHookEx(NULL, nCode, wParam, lParam);
  }

  if (HC_ACTION == nCode) {

    if (injectMessage == 0) {
      // OutputDebugStringW(L"RegisterWindowMessageW");
      injectMessage = RegisterWindowMessageW(WM_INJECT_STRING);
    }

    const MSG* msg = reinterpret_cast<const MSG*>(lParam);

    // OutputDebugStringW( (std::to_wstring( wParam ) +L" " + std::to_wstring( msg->message )).c_str() );

    switch (msg->message) {
    case WM_PAINT:
      {
        //OutputDebugStringW( L"WM_PAINT\n");
        if (notifyWindow) {
          PostMessage(notifyWindow, injectMessage, static_cast<WPARAM>(InjectControl::PAINT_MESSAGE), 0);
        }
      }
      break;
      
    default:
      {
        if ( msg->message == injectMessage ) {
          OutputDebugString(TEXT("INJECT MESSAGE"));
          switch (msg->wParam) {
          case static_cast<WPARAM>(InjectControl::NOP):
            break;
          case static_cast<WPARAM>(InjectControl::SHUTDOWN):
            break;
          case static_cast<WPARAM>(InjectControl::WAKEUP):
            {
              OutputDebugStringW(L"InjectControl::WAKEUP");
              if (msg->lParam) {
                notifyWindow = reinterpret_cast<HWND>(msg->lParam);
                assert( notifyWindow );
                
#if !defined( NDEBUG )
                OutputDebugStringW(std::to_wstring(msg->lParam).c_str());
#endif /* !defined( NDEBUG ) */
                /* WAKEUP メッセージに対応して、notifyするウィンドウに通知する。 */
                LRESULT const lresult =
                  PostMessage(reinterpret_cast<HWND>(msg->lParam),
                              injectMessage,
                              static_cast<WPARAM>(InjectControl::WAKEUP),
                              (LPARAM)(msg->hwnd));

#if !defined( NDEBUG )
                DWORD const lastError = GetLastError();
                OutputDebugStringW((std::wstring{ L"LRESULT=" } + std::to_wstring(lresult) +
                                    std::wstring{ L" lastError=" } + std::to_wstring(lastError)).c_str());
#endif /* !defined( NDEBUG ) */
              }
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


