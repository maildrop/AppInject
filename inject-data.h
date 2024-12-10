/**
   グローバルな定数値
   WMPaintNotify と通信するためのウィンドウメッセージ等 
 */
#pragma once 
#if !defined( INJECT_DATA_H_HEADER_GUARD )
#define INJECT_DATA_H_HEADER_GUARD 1

/** RegisterWindowMessageW で登録するグローバルなウィンドウメッセージの文字列 */
#define WM_INJECT_STRING (L"InjectionWindowMessage-bdf54460-b28b-44cb-baf2-4175162860e3")


#if defined( __cplusplus ) 

#include <array>

struct InjectThreadArgument{
  std::array<wchar_t,MAX_PATH> paintNotifyPath;
  intptr_t controlWindowHandle;
};

enum class InjectControl : WPARAM{
  NOP = 0,
  SHUTDOWN,
  WAKEUP,
  PAINT_MESSAGE
};

#endif /* defined( __cplusplus ) */

#endif /* !defined( INJECT_DATA_H_HEADER_GUARD ) */
