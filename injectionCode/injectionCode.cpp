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

/*  マクロ値のチェック  */
#if (WINVER != _WIN32_WINNT )
#error it was set different value between WINVER and _WIN32_WINNT. 
#endif 

#ifndef WINVER
#define WINVER _WIN32_WINNT_WIN10 
#endif /* WINVER */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT WINVER
#endif /* _WIN32_WINNT */

#if !defined(WIN32_LEAN_AND_MEAN)               
/* コンパイル速度の向上のために、
   Windows ヘッダーからほとんど使用されていない部分を除外する
   事を指示するマクロ */
#error WIN32_LEAN_AND_MEAN 
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
#include <objbase.h>  // Ole32 

#include <pathcch.h>  // パス操作関数
#include <commctrl.h> // コモンコントロール
#include <shellapi.h> // shell操作関数

#include "../inject-data.h"

#pragma comment (lib , "Pathcch.lib")
#pragma comment (lib , "Comctl32.lib" )
#pragma comment (lib , "ole32.lib" )

#if !defined( VERIFY )
# if defined( NDEBUG )
#  define VERIFY( exp ) (void)(exp)
# else 
#  define VERIFY( exp ) assert( (exp) )
# endif /* defined( NDEBUG ) */
#endif /* !defined( VERIFY ) */

/**
  アプリケーションでデスクトップに表示されるウィンドウクラス
  JUCE は JUCE_ に適当な文字列が追加されたクラス名を使う
 */
#define APP_VISIBLE_WINDOW_CLASS       ( L"JUCE_" )

/**
   アプリケーションと通信するためのウィンドウクラス
 */
#define APP_COMMUNICATION_WINDOW_CLASS ( L"SACRIFICE_C" )

/**
   プライベートなウィンドウメッセージ
 */
enum{
  WM_PRIVATE_HEAD = (WM_APP+1),
  WM_PRIVATE_INJECT_BEGIN ,
  WM_PRIVATE_INJECT_UPLINK,
  WM_PRIVATE_INJECT_DOWNLINK
};

/**
   プログラムのエントリーポイント
 */
int main( int argc , char* argv[] )
{
  std::locale::global( std::locale{""} );

  std::ignore = argc;
  std::ignore = argv;

  // LoadLibrary のディレクトリを制限
  if( ! SetDllDirectory(TEXT("")) ){
    assert( !"SetDllDirectory(TEXT(\"\"))");
    std::wcerr << "SetDllDirectory(\"\") failed." << std::endl;
    return 2;
  }
  
  {
    // COMのスレッドアパートメントの宣言
    HRESULT hr = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if(! SUCCEEDED( hr ) ){
      assert( !"CoInitializeEx()" );
      std::wcerr << "CoInitializeEx() failed"  << std::endl;
      return 3;
    }
  }
  
  const HMODULE hModule = GetModuleHandle(NULL);
  assert( hModule );

  UINT const inject = RegisterWindowMessageW( WM_INJECT_STRING );

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

  ATOM const wndClass = RegisterClassW(&comm_wndclass);
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
#if !defined( NDEBUG )
  std::wcout << L"injectionHWND " << injection_code << std::endl;
#endif
  
  assert( injection_code );
  
  struct WindowPrivateData{
    UINT_PTR nIDEvent; // タイマーのID 
    UINT injectMessage;
    SUBCLASSPROC pfnSubClass;
    HWND remoteWindow;
    bool initializing;
  };
    
  auto subclassProc =
    []( HWND hWnd , UINT uMsg , WPARAM wParam , LPARAM lParam ,
        UINT_PTR uIdSubclass , DWORD_PTR dwRefData ) -> LRESULT  {

      struct WindowPrivateData* const self =
        reinterpret_cast<struct WindowPrivateData*>( dwRefData );
      
      if( ! self ){
        
        assert( self );
        
        switch( uMsg ){
        case WM_DESTROY:
          {
            PostQuitMessage( 1 );
            return DefSubclassProc( hWnd, uMsg , wParam , lParam );
          }
        default:
          return DefSubclassProc( hWnd, uMsg , wParam , lParam );
        }
      }
      
      switch( uMsg ){

      case WM_TIMER:
        if( wParam == self->nIDEvent ){
          if( self->remoteWindow ){
            if( !IsWindow( self->remoteWindow ) ){
              std::wcout << "remoteWindow is invalid" << std::endl;
              VERIFY( DestroyWindow( hWnd ) );
              return 0;
            }else{
              PostMessageW( self->remoteWindow ,
                            self->injectMessage ,
                            static_cast<WPARAM>(InjectControl::WAKEUP),
                            reinterpret_cast<LPARAM>(hWnd) ) ;
              SetTimer( hWnd , self->nIDEvent , 1000 , NULL );
              return 0;
            }
          }
          
          return 0;
        }
        break;
      case WM_PRIVATE_INJECT_BEGIN:
        {
          self->remoteWindow = reinterpret_cast<HWND>( lParam );
          std::wcout << "remoteWindow " <<  self->remoteWindow << std::endl;
          if( self->remoteWindow ){
            if( self->initializing ){
              // 再投入されている場合がある
              std::wcout << L"再投入？" << std::endl;
            }else{
              if( self->nIDEvent == SetTimer( hWnd , self->nIDEvent , 1000 , NULL ) ){
                SendMessageW( self->remoteWindow ,
                              WM_NULL, 0, 0 );
                PostMessageW( self->remoteWindow ,
                              self->injectMessage ,
                              static_cast<WPARAM>(InjectControl::WAKEUP),
                              reinterpret_cast<LPARAM>(hWnd) ) ;
                BOOL bRet;
                MSG msg = {0};
                while( (bRet = GetMessage( &msg, NULL, 0, 0 )) != 0){ 
                  if (bRet == -1) {
                    // handle the error and possibly exit
                    break;
                  }else{
                    TranslateMessage(&msg); 
                    DispatchMessage(&msg); 
                  }
                } // end of while
                KillTimer( hWnd , self->nIDEvent );
                self->initializing = false;
              }
            }
          }
        }
        return 0;
      case WM_PRIVATE_INJECT_UPLINK:
        {
          std::wcout << "UPLINK" << std::endl;
          PostQuitMessage( 0 ); // WM_PRIVATE_INJECT_BEGIN の中のループから脱出する
        }
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
        case static_cast<WPARAM>( InjectControl::WAKEUP ):
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
  
  struct WindowPrivateData windowPrivateData{ 1, inject ,subclassProc , NULL , false};
    
  VERIFY( SetWindowSubclass( injection_code ,
                             subclassProc,
                             0 ,
                             reinterpret_cast<DWORD_PTR>(&windowPrivateData) ) );

  /* この実行可能ファイルのパス */
  std::unique_ptr<std::array<wchar_t, MAX_PATH>> path =
    std::make_unique<std::array<wchar_t, MAX_PATH>>();

  if( 0 < GetModuleFileNameW( hModule , (LPWSTR)(path->data()) , (DWORD)path->size() )){

    {
      HRESULT hr = PathCchRemoveFileSpec( path->data() , path->size() );
      std::ignore = hr;
      assert( SUCCEEDED( hr ) || !"PathCchRemoveFileSpec( path->data() , path->size() )");
    }
    {
      HRESULT hr = PathCchAppend( path->data() , path->size() , L"Sacrifice.exe" );
      std::ignore = hr;
      assert( SUCCEEDED( hr ) || !"PathCchAppend( path->data() , path->size() , L\"Sacrifice.exe\" )");
    }
    
    // std::wcout << path->data() << std::endl;
      
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
    std::wcout << L"Invoke \"" << shellExecInfo.lpFile << L"\"" << std::endl;;
    if( ShellExecuteExW( &shellExecInfo ) ){
      assert( shellExecInfo.hProcess ); // ここでは、 hProcess は必ず プロセスハンドルを返す。
      std::wcout << L" ProcessHandleValue: " << (void*)( shellExecInfo.hProcess ) << std::endl;

      if( shellExecInfo.hProcess ){
        /*
          プロセスが入力待ちの状態になるまで待つ。（ Window の作成が終わって GetMessage() や PeekMessage() で待機状態になるのを待つ
        */
        std::wcout << "waiting idle.." ;
        DWORD const waitForInputIdleResult = WaitForInputIdle( shellExecInfo.hProcess , 10000 );
        std::wcout << "done." << std::endl;
        
        if( 0 == waitForInputIdleResult ){
          /*
            フックをかけるために、
            リモートのプロセスに属しているスレッドのトップレベルウィンドウを列挙する。
          */
          std::set< std::tuple<DWORD,HWND> > inject_threadid {} ;
          {
            struct EnumProcArg{
              const DWORD processId;
              std::set< std::tuple<DWORD,HWND> > whset;
            } enumProcArg = { GetProcessId( shellExecInfo.hProcess ), std::set<std::tuple<DWORD,HWND> >{}  };

            for( size_t numberOfTry{0};
                 inject_threadid.empty() && numberOfTry < 100 /* 100 * Sleep(100) = 10 sec  */ ;
                 enumProcArg.whset.clear() , ++numberOfTry ){
              // TODO ここでプロセスがまだ生きている事をチェックしないといけない
              
              EnumWindows( []( HWND hWnd , LPARAM lParam )->BOOL{
                struct EnumProcArg* arg = reinterpret_cast< struct EnumProcArg*>( lParam );
                
                DWORD processId = 0;
                DWORD threadId = GetWindowThreadProcessId( hWnd ,&processId );
                
                if( 0 < threadId ){
                  if( processId == arg->processId ){
                    arg->whset.emplace( threadId , hWnd  );
                  }
                }
                return TRUE;
              }, (LPARAM)&enumProcArg );
              
              for( auto&& tup : enumProcArg.whset ){
                std::array<wchar_t, 128> className;
                // 左辺は constexpr , 右辺の(..max)()は、Windows の関数型マクロ max に対応するためのハック
                static_assert( className.size() < (std::numeric_limits<decltype( className.size() )>::max)() ,""); 
                if( GetClassNameW( std::get<1>(tup) , className.data() , int(className.size()) ) ){
                  for( auto&& targetWndClassName : { APP_VISIBLE_WINDOW_CLASS,  APP_COMMUNICATION_WINDOW_CLASS  } ){
                    if( 0 == std::char_traits<wchar_t>::compare( className.data() , targetWndClassName ,std::char_traits<wchar_t>::length( targetWndClassName )) ){

                      std::array<wchar_t, 128> windowText{};
                      int r = GetWindowTextW( std::get<1>(tup) , windowText.data() , (int)windowText.size() );

                      if( true ){ // デバッグ用の表示
                        std::wcout << L"{ \"HWND\" : " << std::get<1>(tup) << L", " 
                                   << L"\"wndClassName\" : \"" << className.data() << L'"'
                                   << L",";
                        std::wcout << "\"windowText\" : ";

                        if( 0 < r ){
                          std::wcout << L'"' << std::wstring{windowText.data(), size_t(r)} << L'"';
                        }else{
                          assert( 0 == r );
                          std::wcout << L"\"\"" << std::endl;
                        }
                        std::wcout << "}" << std::endl;
                      }
                      
                      constexpr wchar_t window_suffix[] = L"VOICEPEAK";
                      if( ((::IsWindowVisible(std::get<1>(tup))) /* 表示されていて */ && 
                           NULL == ::GetWindowLongPtr(std::get<1>(tup), GWLP_HWNDPARENT) /* 親ウィンドウを持たない */) ||
                          ( 0 < r &&
                            0 == std::char_traits<wchar_t>::compare( windowText.data() ,
                                                                     window_suffix ,
                                                                     std::char_traits<wchar_t>::length( window_suffix ) ) )){
                          inject_threadid.emplace( tup );
                      }
                    }
                  }
                }
              }// end of for enumProcArg

              if( inject_threadid.empty() ){
                (void)Sleep( 100 );
              }

            } // end of numberOfTry 
          }

          // ここまで来ると inject_threadid が正しいスレッドを選択する

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
                  assert( 0 != std::get<0>(threadid) );
                  if( !SetWindowsHookExW(WH_GETMESSAGE, reinterpret_cast<HOOKPROC>(hookProc) , paintNotifyDll ,std::get<0>( threadid )) ){
                    std::wcerr << "SetWindowsHookExW() failed" << std::endl;
                  }else{
                    for( auto&& tup : inject_threadid ){
                      if( false ){
                        std::array<wchar_t, 128> className;
                        // 左辺は constexpr , 右辺の(..max)()は、Windows の関数型マクロ max に対応するためのハック
                        static_assert( className.size() < (std::numeric_limits<decltype( className.size() )>::max)() ,"");
                        if( GetClassNameW( std::get<1>(tup) , className.data() , int(className.size()) ) ){
                          for( auto&& targetWndClassName : { APP_VISIBLE_WINDOW_CLASS,  APP_COMMUNICATION_WINDOW_CLASS  } ){
                            if( 0 == std::char_traits<wchar_t>::compare( className.data() , targetWndClassName ,std::char_traits<wchar_t>::length( targetWndClassName )) ){
                              WINDOWINFO windowInfo = { 0 };
                              windowInfo.cbSize = sizeof(WINDOWINFO);
                              if (GetWindowInfo(std::get<1>(tup), &windowInfo)) {
                                std::wcout << className.data() << "," <<  windowInfo.dwStyle << std::endl;
                                std::array<wchar_t, 128> windowText{};
                                int r = GetWindowTextW( std::get<1>( tup ) ,windowText.data(), (int)windowText.size() );
                                if( 0 < r ){
                                  constexpr wchar_t window_suffix[] = L"VOICEPEAK";
                                  if( 0 == std::char_traits<wchar_t>::compare( windowText.data() , window_suffix , std::char_traits<wchar_t>::length( window_suffix ) )) {
                                  }
                                }
                              }
                            }
                          }
                        }
                      }
                      PostMessage(injection_code, WM_PRIVATE_INJECT_BEGIN, 0, reinterpret_cast<LPARAM>(std::get<1>(tup)));
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
                }
              }
            }
          }
        }else if( WAIT_TIMEOUT == waitForInputIdleResult ){
          
        }else if( WAIT_FAILED == waitForInputIdleResult ){
          
        }
        WaitForSingleObject( shellExecInfo.hProcess , INFINITE );
        VERIFY( CloseHandle( shellExecInfo.hProcess ) );
      }
    }
  }

  (void) ::CoUninitialize();
  return EXIT_SUCCESS;
}

