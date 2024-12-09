/**
   Windows Console Program
   
   (C) 2010-2023 TOGURO Mikito, 
*/

#ifndef WINVER
#define WINVER _WIN32_WINNT_WIN10 
#endif /* WINVER */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT WINVER
#endif /* _WIN32_WINNT */

/*  マクロ値のチェック  */
#if (WINVER != _WIN32_WINNT )
#error it was set different value between WINVER and _WIN32_WINNT. 
#endif 

#include <iostream>
#include <type_traits>
#include <tuple>
#include <locale>
#include <vector>
#include <array>
#include <deque>

/* Windows SDK */
#include <tchar.h>
#include <windows.h>

/* CRT */
#include <process.h>

#if defined(__cplusplus )
# include <cassert>
#else /* defined(__cplusplus ) */
# include <assert.h>
#endif /* defined(__cplusplus ) */

#if !defined( VERIFY )
# if defined( NDEBUG )
#  define VERIFY( exp ) (void)(exp)
# else 
#  define VERIFY( exp ) assert( (exp) )
# endif /* defined( NDEBUG ) */
#endif /* !defined( VERIFY ) */

#pragma comment( lib , "user32.lib" )
#pragma comment( lib , "gdi32.lib" )
#pragma comment( lib , "ole32.lib" )

static HANDLE currentThreadHandle(void);
static BOOL CtrlHandler( DWORD fdwCtrlType );
static SHORT entry( int argc , char** argv);

struct ReadInputArgument{
  HANDLE in;
};

static unsigned readInputThread( void * argument );

static unsigned readInputThread( void * const argument )
{
  ReadInputArgument* arg = static_cast< ReadInputArgument* > ( argument );
  assert( arg );
  if( arg ){
    if( INVALID_HANDLE_VALUE != arg->in ){
      std::unique_ptr<std::array<char,1024>> buf = std::make_unique<std::array<char,1024>>();
      for(;;){
        DWORD numberOfBytesRead = 0;
        SetLastError( ERROR_SUCCESS );
        if( ReadFile( arg->in , buf->data() , DWORD( buf->size() ), &numberOfBytesRead , nullptr ) ){
          if(! numberOfBytesRead ){
            std::cout << "ReadFile read 0 byte" << std::endl;
            break;
          }
          std::cout << std::string( buf->data() , numberOfBytesRead ) << std::endl;
        }else{
          DWORD const lastError = GetLastError();
          switch( lastError ){
          case ERROR_BROKEN_PIPE:
            break;
          case ERROR_OPERATION_ABORTED: // CancelSynchronousIo で停止させられた
            break;
          default:
            std::cout << "ReadFile fail " << lastError << std::endl;
          }
          break;
        }
      }
      VERIFY( CloseHandle( arg->in ) );
    }
  }
  return 0;
}

namespace wh{

  template<typename type_t>
  inline LRESULT
  windowProc( HWND const hWnd , UINT const msg , WPARAM const wParam , LPARAM const lParam )
  {
    if( WM_NCCREATE == msg ){
      SetWindowLongPtr( hWnd, 0 , (LONG_PTR)(new type_t::ContextType()) );
    }
    LONG_PTR const longptr = GetWindowLongPtr( hWnd, 0 );
    if( longptr ){
      auto ptr = reinterpret_cast<type_t::ContextType *>( longptr );
      LRESULT const lResult = ptr->wndProc( hWnd , msg , wParam , lParam );
      if( WM_DESTROY == msg ){
        SetWindowLongPtr( hWnd , 0 , 0 );
        delete ptr;
      }
      return lResult;
    }else{
      return ::DefWindowProc( hWnd , msg, wParam, lParam );
    }
  }

  template<typename context_type>
  struct WindowingContext{
    using ContextType = context_type;
    ATOM atom;
    WindowingContext()
      :atom(NULL){
    }
    
    ~WindowingContext()
    {
      if( atom ){
        VERIFY( UnregisterClass( reinterpret_cast<LPCTSTR>( atom ) ,
                                 GetModuleHandle( NULL )));
      }
    }
  };

  template<typename WindowingContext_type>
  static ATOM simpleWindow( WindowingContext_type &context ){
    WNDCLASSEX wndClassEx = {};
    wndClassEx.cbSize = sizeof( wndClassEx );
    wndClassEx.style = CS_HREDRAW | CS_VREDRAW;
    wndClassEx.lpfnWndProc = &windowProc<WindowingContext_type>;
    wndClassEx.cbClsExtra = 0;
    static_assert( sizeof( void* ) <= sizeof( LONG_PTR ) , ""); // LONG_PTR のサイズは、void* を保持するのに十分な大きさを持っている。
    wndClassEx.cbWndExtra = sizeof( LONG_PTR ) * 1 ;
    wndClassEx.hInstance = GetModuleHandle( NULL );
    wndClassEx.hIcon = LoadIcon(NULL , IDI_APPLICATION);
    wndClassEx.hCursor = LoadCursor(NULL , IDC_ARROW);
    wndClassEx.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wndClassEx.lpszMenuName = NULL;
    wndClassEx.lpszClassName = TEXT("SIMPLIFIEDWINDOW");
    wndClassEx.hIconSm = LoadIcon(NULL , IDI_APPLICATION);
    return ( context.atom = RegisterClassEx( &wndClassEx ) );
  }

  template<typename WindowingContext_type>
  static HWND createWindow( WindowingContext_type &context ){
    return CreateWindowEx( WS_EX_OVERLAPPEDWINDOW | WS_EX_APPWINDOW ,
                           (PCTSTR)context.atom ,
                           TEXT("Window"),
                           WS_OVERLAPPEDWINDOW ,
                           CW_USEDEFAULT, 0, CW_USEDEFAULT, 0,
                           NULL,
                           NULL ,
                           GetModuleHandle( NULL ) ,
                           new WindowingContext_type::ContextType() );
  }
};


static SHORT entry( int argc , char** argv)
{
  std::ignore = argc ;
  std::ignore = argv ;

  enum{
    WM_PRIVATE = (WM_APP+1),
    WM_PRIVATE_NULL ,
    WM_PRIVATE_BEGIN,
    WM_PRIVATE_PRCESS,
    WM_PRIVATE_STOP,
    WM_PRIVATE_SHUTDOWN,
    WM_PRIVATE_END,
  };
  
  struct WindowThis{
    std::deque<std::wstring> queue;
    uintptr_t readthread;

    WindowThis()
      : queue{},
        readthread(NULL) {
    }
    
    
    inline LRESULT wndProc( HWND hWnd , UINT msg , WPARAM wParam , LPARAM lParam )
    {
      switch( msg ){
      case WM_CREATE:
        std::cout << "WM_CREATE" << std::endl;
        break;
      case WM_DESTROY:
        std::cout << "WM_DESTROY" << std::endl;
        PostQuitMessage( 0 );
        break;
      case WM_CLOSE:
        std::cout << "WM_CLOSE" << std::endl;
        break;
      case WM_PAINT:
        {
          PAINTSTRUCT ps = {};
          HDC hDC = BeginPaint( hWnd , &ps );
          {
            const std::wstring str = std::wstring{L"HEllo world"};
            
            int y = 0;
            for( auto&& text : this->queue ){
              y += 20;
              TextOutW( hDC , 10 , y , text.c_str() , int( text.size()));
              std::wcout << text.c_str() << std::endl;
            }
          }
          EndPaint( hWnd, &ps );
        }
        break;
      case WM_PRIVATE_BEGIN:
        {
          this->queue.emplace_back( L"begin" );
          SetWindowText( hWnd , TEXT("Window - BEGIN") );
          assert( 0 == this->readthread );
          InvalidateRect( hWnd , nullptr,  TRUE );
          PostMessage( hWnd , WM_PRIVATE_PRCESS , 0 , 0 );
        }
        return 0;
      case WM_PRIVATE_PRCESS:
        {
          this->queue.emplace_back( L"process" );
          SetWindowText( hWnd, TEXT("Window - PROCESSING") );
          InvalidateRect( hWnd , nullptr , TRUE );
        }
        return 0;
      default:
        break;
      }
      return ::DefWindowProc( hWnd , msg , wParam , lParam );
    }
  };
  
  wh::WindowingContext<WindowThis> windowingContext{};
  wh::simpleWindow( windowingContext );
  HWND hWnd = wh::createWindow( windowingContext);
  ShowWindow( hWnd , SW_SHOW );
  
  ReadInputArgument readInputArgument = {};
  if( DuplicateHandle( GetCurrentProcess() , GetStdHandle( STD_INPUT_HANDLE ) ,
                       GetCurrentProcess() , &readInputArgument.in ,
                       NULL , FALSE ,
                       DUPLICATE_SAME_ACCESS ) ){

  }else{
    std::cout << "Duplicate Handle fail" << std::endl;
  }

  uintptr_t readthread = _beginthreadex( nullptr , 0 ,
                                         readInputThread , &readInputArgument ,
                                         0 , nullptr );

#if 0
  HANDLE waitableTimer = CreateWaitableTimer( NULL,  TRUE , NULL );
  assert( waitableTimer );
  std::tuple< HANDLE , HANDLE > timerArgs = { waitableTimer , HANDLE( readthread )};
  LARGE_INTEGER interval;
  interval.QuadPart = -10LL * 1000LL * 1000LL * 1LL; // - (10)micro sec, (*1000)mili sec ,(*1000) sec 
  VERIFY( SetWaitableTimer( waitableTimer ,   &interval , 1000 ,
                            []( LPVOID lpArgToCompletionRoutine , DWORD dwTimerLowValue, DWORD dwTimerHighValue ){
                              std::ignore = dwTimerHighValue ;
                              std::ignore = dwTimerLowValue;
                              std::tuple< HANDLE , HANDLE > *arg = reinterpret_cast<std::tuple<HANDLE,HANDLE>*>( lpArgToCompletionRoutine );
                              VERIFY( CancelSynchronousIo( std::get<1>(*arg) ) );
                              VERIFY( CloseHandle( std::get<0>( *arg ) ) );
                            },
                            reinterpret_cast<void*>( &timerArgs ), 
                            FALSE ) );
#endif

  PostMessage( hWnd, WM_PRIVATE_BEGIN , 0 , 0 );
  
  std::vector<HANDLE> waitList{};
  if( readthread ){
    waitList.emplace_back( HANDLE(readthread) );

    auto consumeMessage = []()->BOOL{
      MSG msg = {0};
      for(;msg.message!=WM_QUIT;){
        if( !PeekMessage( &msg , NULL , 0 ,0 , PM_REMOVE ) ){
          break;
        }
        (void)(TranslateMessage( &msg ));
        (void)(DispatchMessage( &msg ));
      }
      if( msg.message == WM_QUIT ){
        return FALSE;
      }
      return TRUE;
    };
    
    for(;;){
      assert( waitList.size() < (std::numeric_limits<DWORD>::max)()  );
      assert( waitList.size() < 64 );
      DWORD const waitValue = MsgWaitForMultipleObjectsEx( (DWORD)waitList.size() , waitList.data() , INFINITE , QS_ALLEVENTS , MWMO_ALERTABLE);
      if( WAIT_OBJECT_0 <= waitValue && waitValue < (WAIT_OBJECT_0+waitList.size()) ){
        if( waitList.at( waitValue - WAIT_OBJECT_0 ) == reinterpret_cast<HANDLE>( readthread )){
          waitList.erase( waitList.begin() + ( waitValue - WAIT_OBJECT_0 ));
          VERIFY( CloseHandle( reinterpret_cast<HANDLE>(readthread) ) );
          DestroyWindow( hWnd );
        }
        if( !consumeMessage() ){
          break;
        }
      }else if( (WAIT_OBJECT_0 + waitList.size() ) == waitValue ){
        if( !consumeMessage() ){
          break;
        }
      }else if( WAIT_ABANDONED_0 <= waitValue && waitValue < (WAIT_ABANDONED_0 + waitList.size() )){

      }else if( WAIT_TIMEOUT == waitValue ){

      }else if( WAIT_FAILED == waitValue  ){

      }else if( WAIT_IO_COMPLETION == waitValue ){
        std::cout << "WAIT_IO_COMPLETION" << std::endl;
      }
    }

    std::cout << "end of message loop" << std::endl;
  }
  return EXIT_SUCCESS;
}

static CRITICAL_SECTION primary_thread_handle_section = {};
static HANDLE primary_thread_handle = NULL;

static BOOL CtrlHandler( DWORD fdwCtrlType )
{
  switch( fdwCtrlType ){
  case CTRL_C_EVENT:
    {
      EnterCriticalSection( &primary_thread_handle_section );
      if( primary_thread_handle ){
        if( !QueueUserAPC( []( ULONG_PTR )->void{
          return ;
        }, primary_thread_handle , 0 ) ){
          DWORD lastError = GetLastError();
          assert( !(lastError == ERROR_ACCESS_DENIED) || !"DuplicateHandle() require ACCESS flag. use dwOptions as DUPLICATE_SAME_ACCESS" );
          return FALSE;
        }
      }
      LeaveCriticalSection( &primary_thread_handle_section );
    }
    return TRUE;
  default:
    break;
  }
  return FALSE;
}

static HANDLE currentThreadHandle(void)
{
  HANDLE h = NULL;
  if( DuplicateHandle( GetCurrentProcess() , GetCurrentThread() ,
                       GetCurrentProcess() , &h ,
                       0 ,
                       FALSE ,
                       DUPLICATE_SAME_ACCESS ) ){
    return h;
  }
  return NULL;
}


#if defined( __cplusplus )
namespace wh{
  namespace util{

    /*
      今、 SFINAE を使って CRTの機能である、_get_heap_handle() が使えるかどうかを判定したいのだが、
      decltype( _get_heap_handle() ) とすると この部分は、template（の型引数）に依存しなくなるので、
      即時コンパイルエラーになる。
      このために、ダミーのパラメータパック引数 type_t を追加して、_get_heap_handle() をテンプレート型依存にする。
      
      使うときは
      decltype( enable_get_heap_handle(nullptr) ) と、nullptrを引数に渡す
      
      _get_heap_handle() が存在しない環境の場合、このテンプレートはSFINAEで除去される。
    */
    template<typename ...type_t >
    static auto enable_get_heap_handle( std::nullptr_t&&, type_t ...args )->
      decltype( _get_heap_handle( args... ) , std::true_type{} );
    static auto enable_get_heap_handle( ... )->std::false_type;

    template <typename type_t>
    struct CRTHeapEnableTerminateionOnCorruption{
      inline
      void operator()(){
# if !defined( NDEBUG )
        OutputDebugString(TEXT("disable HeapEnableTerminationOnCorruption\n"));
# endif
        return;
      }
    };
    
    template<>
    struct CRTHeapEnableTerminateionOnCorruption<std::true_type>
    {
      inline
      void operator()(){
# if !defined( NDEBUG )
        OutputDebugString(TEXT("enable HeapEnableTerminationOnCorruption\n"));
# endif /* !defined( NDEBUG ) */
        HANDLE const crtHeapHandle = reinterpret_cast<HANDLE>( _get_heap_handle()); 
        VERIFY( crtHeapHandle != NULL );
        if( crtHeapHandle ){
          if( GetProcessHeap() == crtHeapHandle ){// CRT のバージョンによって ProcessHeap を直接使っていることがある。
            // MSVC 201? あたりで変更されている。
# if !defined( NDEBUG )
            OutputDebugString(TEXT(" CRT uses ProcessHeap directly.\n" ));
#endif /* !defined( NDEBUG ) */
          }else{
            VERIFY( HeapSetInformation(crtHeapHandle, HeapEnableTerminationOnCorruption, NULL, 0) );
            
            // 追加でLow-Fragmentation-Heap を使用するように指示する。
            ULONG heapInformaiton(2L); // Low Fragmention Heap を指示
            VERIFY( HeapSetInformation(crtHeapHandle, HeapCompatibilityInformation, 
                                       &heapInformaiton, sizeof( heapInformaiton)));
          }
        }
        return;
      }
    };
  }
}

#endif /* defined( __cplusplus ) */

int main(int argc,char* argv[])
{
  /**********************************
   // まず、ヒープ破壊の検出で直ぐ死ぬように設定
   ***********************************/
  // HeapSetInformation() は Kernel32.dll 
  { // プロセスヒープの破壊をチェック Kernel32.dll依存だが、これは KnownDLL
    HANDLE processHeapHandle = ::GetProcessHeap(); // こちらの関数は、HANDLE が戻り値
    VERIFY( processHeapHandle != NULL );
    if( processHeapHandle ){
      VERIFY( HeapSetInformation(processHeapHandle, HeapEnableTerminationOnCorruption, NULL, 0) );
    }
  }
  
  // 可能であれば、CRT Heap に対しても HeapEnableTerminationOnCorruption を設定する。
  wh::util::CRTHeapEnableTerminateionOnCorruption<decltype(wh::util::enable_get_heap_handle(nullptr))>{}();
  
  /***************************************************
  DLL の読み込み先から、カレントワーキングディレクトリの排除 
  ***************************************************/
  { 
    /* DLL探索リストからカレントディレクトリの削除
       http://codezine.jp/article/detail/5442?p=2  */
    HMODULE kernel32ModuleHandle = GetModuleHandle( _T("kernel32")); // kernel32 は 実行中アンロードされないからこれでよし
    if( NULL == kernel32ModuleHandle){
      return 1;
    }
    FARPROC pFarProc = GetProcAddress( kernel32ModuleHandle , "SetDllDirectoryW");
    if( NULL == pFarProc ){
      return 1;
    }
    decltype(SetDllDirectoryW) *pSetDllDirectoryW(reinterpret_cast<decltype(SetDllDirectoryW)*>(pFarProc) );
    VERIFY( 0 != pSetDllDirectoryW(L"") ); /* サーチパスからカレントワーキングディレクトリを排除する */
  }

  std::locale::global( std::locale(""));

  int return_value = 0;
  InitializeCriticalSection( &primary_thread_handle_section );
  struct section_raii_type{
    ~section_raii_type(){
      DeleteCriticalSection( &primary_thread_handle_section );
    }
  } section_raii;
  
  struct arg_type{
    int argc;
    char** argv;
  } arg = { argc , argv} ;

  const HRESULT hr = CoInitializeEx( nullptr , COINIT_MULTITHREADED );
  if( ! SUCCEEDED( hr ) ){
    return 3;
  }

  assert( S_OK == hr );
  
  struct com_raii_type{
    ~com_raii_type(){
      (void)CoUninitialize();
    }
  } const raii{};
  
  const uintptr_t ptr = 
    _beginthreadex( nullptr , 0 , []( void * arg_ptr ) -> unsigned{
      HRESULT hr = CoInitializeEx( nullptr , COINIT_APARTMENTTHREADED );
      if( SUCCEEDED(hr) ){
        
        assert( S_OK == hr );
        
        const com_raii_type raii{};
        
        struct arg_type &arg = *reinterpret_cast<arg_type*>( arg_ptr );
        return entry( arg.argc , arg.argv );
      }
      return EXIT_FAILURE;
    }, &arg , 0 , nullptr );
  
  if( ! ptr ){
    return EXIT_FAILURE;
  }
  
  assert( ptr );
  
  if( SetConsoleCtrlHandler( CtrlHandler , TRUE ) ){
    EnterCriticalSection( &primary_thread_handle_section );
    {
      primary_thread_handle = currentThreadHandle();
    }
    LeaveCriticalSection( &primary_thread_handle_section );
    
    for(;;){
      DWORD waitValue = WaitForSingleObjectEx( reinterpret_cast<HANDLE>( ptr ) , INFINITE , TRUE );
      if( WAIT_OBJECT_0 == waitValue ){
        break;
      }else if( WAIT_IO_COMPLETION == waitValue ){
        OutputDebugString( TEXT(" WAIT_IO_COMPLETION \n"));
        continue;
      }
      break;
    }
    EnterCriticalSection( &primary_thread_handle_section );
    {
      VERIFY( CloseHandle( primary_thread_handle ) );
      primary_thread_handle = NULL;
    }
    LeaveCriticalSection( &primary_thread_handle_section );
    
    VERIFY( SetConsoleCtrlHandler( nullptr , FALSE ) );
  }
  
  {
    DWORD exitCode = 0;
    if( ! GetExitCodeThread( reinterpret_cast<HANDLE>( ptr ) , &exitCode ) ){
      // failed GetExitCodeThread()
      DWORD lastError = GetLastError();
      assert( lastError );
    }else{
      return_value = exitCode;
    }
    VERIFY( CloseHandle( reinterpret_cast<HANDLE>(ptr) ) );
  }

  return return_value;
}
