#if !defined( INJECT_DATA_H_HEADER_GUARD )
#define INJECT_DATA_H_HEADER_GUARD 1

#include <array>

struct InjectThreadArgument{
  std::array<wchar_t,MAX_PATH> paintNotifyPath;
  intptr_t controlWindowHandle;
};


#define WM_INJECT_STRING (L"InjectionWindowMessage")

enum class InjectControl : WPARAM{
  NOP,
  SHUTDOWN,
  WAKEUP,
  PAINT_MESSAGE
};

#endif /* !defined( INJECT_DATA_H_HEADER_GUARD ) */
