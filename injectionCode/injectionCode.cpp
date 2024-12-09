/**
  injectionCode.cpp : このファイルには 'main' 関数が含まれています。プログラム実行の開始と終了がそこで行われます。


  プログラムの実行: Ctrl + F5 または [デバッグ] > [デバッグなしで開始] メニュー
  プログラムのデバッグ: F5 または [デバッグ] > [デバッグの開始] メニュー
 */

// 作業を開始するためのヒント: 
//   1. ソリューション エクスプローラー ウィンドウを使用してファイルを追加/管理します 
//   2. チーム エクスプローラー ウィンドウを使用してソース管理に接続します
//   3. 出力ウィンドウを使用して、ビルド出力とその他のメッセージを表示します
//   4. エラー一覧ウィンドウを使用してエラーを表示します
//   5. [プロジェクト] > [新しい項目の追加] と移動して新しいコード ファイルを作成するか、[プロジェクト] > [既存の項目の追加] と移動して既存のコード ファイルをプロジェクトに追加します
//   6. 後ほどこのプロジェクトを再び開く場合、[ファイル] > [開く] > [プロジェクト] と移動して .sln ファイルを選択します


#if !defined(WIN32_LEAN_AND_MEAN)               
/* コンパイル速度の向上のために、
   Windows ヘッダーからほとんど使用されていない部分を除外する
   事を指示するマクロ */
#define WIN32_LEAN_AND_MEAN 1
#endif /* !defined(WIN32_LEAN_AND_MEAN) */


#include <iostream>
#include <locale>
#include <type_traits>
#include <limits>

#include <string>
#include <tuple>
#include <memory>
#include <array>
#include <set>
#include <cassert>
#include <initializer_list>

#include <tchar.h>
#include <windows.h>
#include <objbase.h>

#include <shellapi.h>
#include <pathcch.h>
#include <commctrl.h>

#include "../inject-data.h"

#pragma comment (lib , "Pathcch.lib")
#pragma comment (lib , "Comctl32.lib" )

#define APP_VISIBLE_WINDOW_CLASS       ( L"JUCE_" )
#define APP_COMMUNICATION_WINDOW_CLASS ( L"SACRIFICE_C" )

enum{
  WM_PRIVATE_HEAD = WM_APP+1,
  WM_PRIVATE_INJECT_BEGIN ,
  WM_PRIVATE_INJECT_UPLINK,
  WM_PRIVATE_INJECT_DOWNLINK
};

int main( int argc , char* argv[] )
{
  std::locale::global( std::locale{""} );

  std::ignore = argc;
  std::ignore = argv;


  if( ! SetDllDirectory(TEXT("")) ){
    assert( false && "SetDllDirectory(TEXT(\"\"))");
    return 3;
  }
  
  HRESULT hr = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
  if( SUCCEEDED( hr ) ){
    const HMODULE hModule = GetModuleHandle(NULL);
    assert( hModule );

    UINT inject = RegisterWindowMessageW( WM_INJECT_STRING );
    std::wcout << "injection " << inject << std::endl;

    const WNDCLASSW comm_wndclass = {
      CS_HREDRAW | CS_VREDRAW ,
      DefWindowProc,
      0,0,
      hModule,
      LoadIcon(NULL , IDI_APPLICATION),
      LoadCursor(NULL , IDC_ARROW),
      (HBRUSH)GetStockObject(WHITE_BRUSH),
      NULL,
      L"injectcommunicationWindowClass"
    };

    ATOM wndClass = RegisterClassW(&comm_wndclass);
    assert( wndClass );
    HWND injection_code = 
      CreateWindowExW( 0,
                       PCTSTR( wndClass ),
                       L"InjectionCode",
                       WS_OVERLAPPEDWINDOW,
                       CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                       HWND_MESSAGE ,
                       NULL ,
                       hModule ,
                       nullptr );
    std::wcout << L"injectionHWND " << injection_code << std::endl;

    struct WindowPrivateData{
      UINT_PTR nIDEvent;
      UINT injectMessage;
      SUBCLASSPROC pfnSubClass;
      HWND remoteWindow;
    } windowPrivateData = {};
    
    auto subclassProc = []( HWND hWnd , UINT uMsg , WPARAM wParam , LPARAM lParam , UINT_PTR uIdSubclass , DWORD_PTR dwRefData ) -> LRESULT
    {
      struct WindowPrivateData* self = reinterpret_cast<struct WindowPrivateData*>( dwRefData );
      switch( uMsg ){
      case WM_TIMER:
        if( wParam == self->nIDEvent ){
          OutputDebugStringW( L"TIMEOUT\n" );
          if( self->remoteWindow ){
            std::wcout << "remotewindow " << self->remoteWindow
                       << " injectMessage " << self->injectMessage
                       << " InjectControl::WAKEUP "
                       << std::endl;

            if( !IsWindow( self->remoteWindow ) ){
              std::wcout << "remoteWindow is invalid" << std::endl;
              PostQuitMessage( 0 );
              return 0;
            }else{
              PostMessage( self->remoteWindow ,
                           WM_NULL, 0, 0 );
              
              PostMessageW( self->remoteWindow ,
                            self->injectMessage ,
                            static_cast<WPARAM>(InjectControl::WAKEUP),
                            reinterpret_cast<LPARAM>(hWnd) ) ;
              std::wcout << "SendMessageW return" << std::endl;
            }
          }
          return 0;
        }
        break;
      case WM_PRIVATE_INJECT_BEGIN:
        {
          self->remoteWindow = reinterpret_cast<HWND>( lParam );
          std::wcout <<"remoteWindow" <<  self->remoteWindow << std::endl;
          if( self->remoteWindow ){
            if( SetTimer( hWnd , self->nIDEvent , 100, NULL ) ){
              BOOL bRet;
              MSG msg = {0};
              while( (bRet = GetMessage( &msg, NULL, 0, 0 )) != 0){ 
                if (bRet == -1) {
                  // handle the error and possibly exit
                }else{
                  TranslateMessage(&msg); 
                  DispatchMessage(&msg); 
                }
              }
            }
            KillTimer( hWnd , self->nIDEvent );
          }
        }
        return 0;
      case WM_PRIVATE_INJECT_UPLINK:
        std::wcout << "UPLINK" << std::endl;
        PostQuitMessage( 0 );
        return 0;
      case WM_PRIVATE_INJECT_DOWNLINK:
        {
          DestroyWindow( hWnd );
        }
        return 0;
      case WM_DESTROY:
        {
          RemoveWindowSubclass( hWnd , self->pfnSubClass , uIdSubclass );
          PostQuitMessage( 0 );
        }
        break;
      default:
        if( self->injectMessage == uMsg ){
          switch( wParam ){
          case static_cast<WPARAM>(InjectControl::WAKEUP):
            PostMessage( hWnd , WM_PRIVATE_INJECT_UPLINK , 0 ,0 );
            return 0;
          case static_cast<WPARAM>( InjectControl::PAINT_MESSAGE ):
            std::wcout << "WM_PAINT" << std::endl;
            return 0;
          default:
            break;
          }
          return 0;
        }
        break;
      }
      return DefSubclassProc( hWnd, uMsg , wParam , lParam );
    };
    
    windowPrivateData = {1, inject ,subclassProc};
    
    SetWindowSubclass( injection_code ,
                       subclassProc,
                       0 ,
                       reinterpret_cast<DWORD_PTR>(&windowPrivateData) );
    

    std::unique_ptr<std::array<wchar_t, MAX_PATH>> path = std::make_unique<std::array<wchar_t, MAX_PATH>>();
    if( 0 < GetModuleFileNameW( hModule , (LPWSTR)(path->data()) , (DWORD)path->size() )){
      PathCchRemoveFileSpec( path->data() , path->size() );
      PathCchAppend( path->data() , path->size() , L"Sacrifice.exe" );

      std::wcout << path->data() << std::endl;
      
      SHELLEXECUTEINFOW shellExecInfo = {0};
      shellExecInfo.cbSize = sizeof( shellExecInfo );

      static_assert( 0 == SEE_MASK_DEFAULT, "0==SEE_MASK_DEFAULT" );
      shellExecInfo.fMask = SEE_MASK_DEFAULT | SEE_MASK_NOCLOSEPROCESS ;

      shellExecInfo.hwnd = NULL;
      shellExecInfo.lpVerb = L"open";
      shellExecInfo.lpFile = L"C:\\Program Files\\VOICEPEAK\\voicepeak.exe";//path->data();
      shellExecInfo.lpParameters = nullptr;
      shellExecInfo.lpDirectory = nullptr;
      shellExecInfo.nShow = SW_SHOWNORMAL;
      /*
        プロセスの立ち上げ
      */
      if( ShellExecuteExW( &shellExecInfo ) ){
        std::wcout << (void*)( shellExecInfo.hProcess ) << std::endl;
        
        assert( shellExecInfo.hProcess ); // ここでは、 hProcess は必ず プロセスハンドルを返す。
        
        /*
          プロセスが入力待ちの状態になるまで待つ。（ Window の作成が終わって GetMessage() や PeekMessage() で待機状態になるのを待つ
        */
        if( shellExecInfo.hProcess ){
          switch( WaitForInputIdle( shellExecInfo.hProcess , 10000 ) ){

          case 0:
            {
              Sleep( 5000 );
              /*
                フックをかけるために、
                リモートのプロセスに属しているスレッドのトップレベルウィンドウを列挙する。
              */
              struct EnumProcArg{
                const DWORD processId;
                std::set< std::tuple<HWND,DWORD> > whset;
              } enumProcArg = { GetProcessId( shellExecInfo.hProcess ), std::set<std::tuple<HWND,DWORD> >{}  };
            
              EnumWindows( []( HWND hWnd , LPARAM lParam )->BOOL{
                struct EnumProcArg* arg = reinterpret_cast< struct EnumProcArg*>( lParam );
              
                DWORD processId = 0;
                DWORD threadId = GetWindowThreadProcessId( hWnd ,&processId );
              
                if( 0 < threadId ){
                  if( processId == arg->processId ){
                    arg->whset.emplace( hWnd , threadId );
                  }
                }
                return TRUE;
              }, (LPARAM)&enumProcArg );
            
              std::set<DWORD> inject_threadid {};
            
              for( auto&& tup : enumProcArg.whset ){
                std::wcout << "HWND: " << std::get<0>( tup ) << std::endl;
                std::array<wchar_t, 128> className;
                static_assert( className.size() < (std::numeric_limits<int>::max)() ,""); // 左辺は constexpr , 右辺の(..max)()は、Windows の関数型マクロ max に対応するためのハック
                if( GetClassNameW( std::get<0>(tup) , className.data() , int(className.size()) ) ){
                  for( auto&& targetWndClassName : { APP_VISIBLE_WINDOW_CLASS,  APP_COMMUNICATION_WINDOW_CLASS  } ){
                    if( 0 == std::char_traits<wchar_t>::compare( className.data() , targetWndClassName ,std::char_traits<wchar_t>::length( targetWndClassName )) ){
                      std::array<wchar_t, 128> windowText{};
                      std::wcout << "HWND: " << std::get<0>(tup) << ", " 
                                 << '"' << className.data() << '"' << ","
                                 << "threadid: " << std::get<1>( tup );
                      int r = GetWindowTextW( std::get<0>(tup) , windowText.data() , (int)windowText.size() );
                      if( 0 < r ){
                        std::wcout << std::wstring{ windowText.data(), size_t( r ) };
                      }
                      std::wcout << std::endl;

                      inject_threadid.emplace( std::get<1>( tup ) );
                    }
                  }
                }
              }

              {
                PathCchRemoveFileSpec( path->data() , path->size() );
                PathCchAppend( path->data() , path->size() , L"WMPaintNotify.dll" );
              
                std::unique_ptr<InjectThreadArgument> data = std::make_unique<InjectThreadArgument>();
                data->paintNotifyPath = *path;
                data->controlWindowHandle = NULL; // TODO 
              
                HMODULE paintNotifyDll = LoadLibraryExW( path->data(), NULL , LOAD_LIBRARY_SEARCH_SYSTEM32 );
                assert( paintNotifyDll );
                if( paintNotifyDll ){
                  FARPROC hookProc = GetProcAddress( paintNotifyDll , "HookProc" );
                  assert( hookProc );
                  if( hookProc ){
                    for( auto&& threadid : inject_threadid ){
                      assert( 0 != threadid);
                      if( SetWindowsHookExW(WH_GETMESSAGE, reinterpret_cast<HOOKPROC>(hookProc) , paintNotifyDll , threadid) ){
                        std::wcout << "SetWindowsHookExW() " << std::endl;
                      }
                    }
                  }
                }
              }

              WaitForInputIdle( shellExecInfo.hProcess , 10000 );
              
              for( auto&& tup : enumProcArg.whset ){
                std::array<wchar_t, 128> className;
                static_assert( className.size() < (std::numeric_limits<int>::max)() ,""); // 左辺は constexpr , 右辺の(..max)()は、Windows の関数型マクロ max に対応するためのハック
                if( GetClassNameW( std::get<0>(tup) , className.data() , int(className.size()) ) ){
                  for( auto&& targetWndClassName : { APP_VISIBLE_WINDOW_CLASS,  APP_COMMUNICATION_WINDOW_CLASS  } ){
                    if( 0 == std::char_traits<wchar_t>::compare( className.data() , targetWndClassName ,std::char_traits<wchar_t>::length( targetWndClassName )) ){

                      WINDOWINFO windowInfo = { 0 };
                      windowInfo.cbSize = sizeof(WINDOWINFO);
                      if (GetWindowInfo(std::get<0>(tup), &windowInfo)) {
                        std::wcout << className.data() << "," <<  windowInfo.dwStyle << std::endl;
                        std::array<wchar_t, 128> windowText{};
                        int r = GetWindowTextW( std::get<0>( tup ) ,windowText.data(), (int)windowText.size() );
                        if( 0 < r ){
                          if( 0 == std::char_traits<wchar_t>::compare( windowText.data() , L"VOICEPEAK" , std::char_traits<wchar_t>::length( L"VOICEPEAK") )) {
                            std::wcout << "PostMessage WM_PRIVATE_INJECT_BEGIN" << std::endl;
                            PostMessage(injection_code, WM_PRIVATE_INJECT_BEGIN, 0, reinterpret_cast<LPARAM>(std::get<0>(tup)));
                          }
                        }
                      }
                    }
                  }
                }
              }

              {
                std::array<HANDLE,1> waitObjects = { shellExecInfo.hProcess };

                for(;;){
                  DWORD waitValue = MsgWaitForMultipleObjects( DWORD( waitObjects.size() ) , waitObjects.data() , FALSE , INFINITE , QS_ALLEVENTS  );
                  if( WAIT_OBJECT_0 <= waitValue && waitValue < WAIT_OBJECT_0 + waitObjects.size() ){
                    // オブジェクトがシグナル状態になりました。
                    if( WAIT_OBJECT_0 == waitValue ){
                      PostMessage( injection_code , WM_PRIVATE_INJECT_DOWNLINK , 0 , 0);
                    }
                  }
                  if( WAIT_OBJECT_0 <= waitValue &&  waitValue <= WAIT_OBJECT_0 + waitObjects.size() ){
                    // 新着メッセージがあるかもね。
                    MSG msg = {0};
                    while( PeekMessage( &msg , NULL , 0 , 0 , PM_REMOVE ) ){
                      TranslateMessage(&msg); 
                      DispatchMessage(&msg); 
                      if( WM_QUIT == msg.message ){
                        goto END_OF_MESSAGE_LOOP;
                      }
                    }
                    continue;
                  }else{
                    // なんかエラーが出たはず。

                  }
                }
              END_OF_MESSAGE_LOOP:
                ;
              }
            }
            break;
          case WAIT_TIMEOUT:
          case WAIT_FAILED:
          default:
            break;
          }
          WaitForSingleObject( shellExecInfo.hProcess , INFINITE );
          CloseHandle( shellExecInfo.hProcess );
        }
      }

    }
    CoUninitialize();
  }
  return EXIT_SUCCESS;
}

