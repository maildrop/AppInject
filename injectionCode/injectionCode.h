#pragma once 
#if !defined( INJECTION_CODE_H_HEADER_GUARD )
#define INJECTION_CODE_H_HEADER_GUARD 1

#if !defined( VERIFY )
# if defined( NDEBUG )
#  define VERIFY( exp ) (void)(exp)
# else 
#  define VERIFY( exp ) assert( (exp) )
# endif /* defined( NDEBUG ) */
#endif /* !defined( VERIFY ) */

# if defined( __cplusplus )

namespace wh{
  /**
     ウィンドウをサブクラス化するための型
     この 引数型には メンバー関数 LRESULT subClassProc(HWND,UINT,WPARAM,LPARAM) noexcept を
     要求する。
     これは、 SetWindowSubclass() によってサブクラス化をするので、DefSubclassProc() を使用すること。
     コンビニエンス関数 window_subclassify() から使用すると楽。
  */
#if 0
  // 例
  struct PrivateWindowData{
    LRESULT
    subClassProc( HWND hWnd , UINT uMsg , WPARAM wParam , LPARAM lParam ) noexcept
    {
      return ::DefSubclassProc( hWnd, uMsg , wParam , lParam );
    }
  };
#endif

  template<typename PrivateWindowData>
  struct WindowSubclassify_t{

    /**
       サブクラス化する。
       
       既に同じ uIdSubclass でサブクラス化がされている場合には、置き換えを行う
       置き換える場合には、 既に割り当てられている pwdRefData を delete する。
    */
    template<typename... Args>
    inline static BOOL
    setWindowSubclass( HWND hWnd , UINT_PTR uIdSubclass , Args&&... args ) noexcept
    {
      if( !hWnd ){
        return FALSE;
      }
      
      std::unique_ptr<PrivateWindowData> data =
        std::make_unique<PrivateWindowData>( std::forward<Args>( args )... );
      
      if( !data ){
        return FALSE;
      }

      /*
        既に設定されている場合があってそのときには、 PrivateWindowData を delete しないといけない
      */
      DWORD_PTR pwdRefData = NULL ;
      if( ::GetWindowSubclass( hWnd ,
                               windowSubclassProc ,
                               uIdSubclass ,
                               &pwdRefData ) ){
        /* 同じ windowSubclassProc と uIdSubclass で 既にサブクラス化されている
           pwdRefData は 任意の値(NULLも含まれる） である。 */
      }else{
        assert( !pwdRefData ); // NULL を指しているはず
        pwdRefData = NULL; // 後で unique_ptr にresetするので NULL 確実に NULL に落としておく 
      }
      
      if( ::SetWindowSubclass( hWnd ,
                               windowSubclassProc ,
                               uIdSubclass ,
                               DWORD_PTR( data.get() )) ){
        data.release(); // Window側に、所有権を渡した。
        data.reset( reinterpret_cast<PrivateWindowData*>(pwdRefData) ); // 所有権を渡された ので、uniq_ptr に delete させる。
        return TRUE;
      }
      
      return FALSE;
    }

    /**
      SetWindowSubclass() に渡す関数
      この関数は Win32 では __stdcall でなければならないため CALLBACKマクロ (Win64 では空白に置換) を使用している
     */
    static LRESULT CALLBACK
    windowSubclassProc( HWND hWnd, UINT uMsg , WPARAM wParam , LPARAM lParam ,
                        UINT_PTR uIdSubclass, DWORD_PTR dwRefData ) noexcept
    {
      if( WM_NCDESTROY == uMsg ){ // NonClient-area Destroy ( WM_DESTROY の"後"に送られるメッセージ )
        VERIFY( ::RemoveWindowSubclass( hWnd , windowSubclassProc , uIdSubclass ) );
        if( dwRefData ){
          PrivateWindowData* control = reinterpret_cast<PrivateWindowData*>( dwRefData );
          delete control;
        }
        return ::DefSubclassProc( hWnd, uMsg , wParam , lParam );
      }
      
      PrivateWindowData* const control = reinterpret_cast<PrivateWindowData*>( dwRefData );
      if( control ){
        return control->subClassProc( hWnd , uMsg , wParam , lParam );
      }
      return ::DefSubclassProc( hWnd, uMsg , wParam , lParam );
    }
  };


  /**
     ウィンドウをサブクラス化するためのコンビニエンス関数
     可変長引数部 の args は、 PrivateWindowData のコンストラクタに forward される。
     uIdSubclass のデフォルトは、 WindowSubclassify_t<PrivateWindowData>::windowSubclassProc への関数ポインタの UINT_PTR 値
  */
  template<typename PrivateWindowData, typename... Args >
  auto 
  window_subclassify( HWND hWnd ,
                      UINT_PTR uIdSubclass = UINT_PTR( WindowSubclassify_t<PrivateWindowData>::windowSubclassProc ),
                      Args&&... args ) noexcept
    -> decltype( WindowSubclassify_t<PrivateWindowData>::setWindowSubclass( hWnd , uIdSubclass , std::forward<Args>(args)... ) )
  {
    return WindowSubclassify_t<PrivateWindowData>::setWindowSubclass( hWnd , uIdSubclass , std::forward<Args>(args)... );
  }

}; // end of namespace wh

/**
   実行時オプション
 */
struct RuntimeOption{
  bool isDebugMode;
};

/**
   引数 process で与えられた プロセスが input idle 状態になるのを待機する。
   duration: 一度の待機時間 単位 msec. default: 500 (in injectionCode.h)
   num: 待機回数 default: 120 (in injectionCode.h)
   全体で、最大 duration * num 回数 msec 待機する。
 */
DWORD wait_for_input_idle_of_execute_process( const HANDLE& process ,
                                              const int duration = 500 , const int num = 120 ) noexcept;


/**
   関数オブジェクトのフィルタ関数を呼び出すラッパー関数
 */
template<typename conditionOperator>
static inline BOOL 
do_filter_injection_target_window( const RuntimeOption& runtimeOption , 
                                   const DWORD & thread_id,
                                   HWND &hWnd ,
                                   const DWORD & processId )
  noexcept( noexcept( std::declval<conditionOperator>( runtimeOption )( std::declval<HWND>() )))
{
  return conditionOperator{runtimeOption}( hWnd );
}

/**
   @param out フィルタ条件に適応する値を格納するセット
   @param in フィルタの入力セット
   @param processId ヒントのプロセスID 
 */
template<typename conditionOperator>
static inline BOOL 
do_filter_injection_target_window( const RuntimeOption& runtimeOption ,
                                   std::set<std::tuple<DWORD,HWND> > &out ,
                                   const std::set<std::tuple<DWORD,HWND> > &in ,
                                   const DWORD &processID )
  noexcept( noexcept( do_filter_injection_target_window<conditionOperator>( runtimeOption,
                                                                            std::declval<const DWORD&>(),
                                                                            std::declval<HWND&>(),
                                                                            processID )))
{
  for( auto&& tup : in ){
    if( do_filter_injection_target_window<conditionOperator>( runtimeOption ,
                                                              std::get<0>(tup),
                                                              std::get<1>(tup),
                                                              processID )) {
      out.emplace( tup );
    }
  }
  return TRUE;
}

/**
   processID に属する window を列挙する
 */
static BOOL
enumulate_window_of_the_process( const RuntimeOption& runtimeOption ,
                                 std::set<std::tuple<DWORD,HWND>> &out ,
                                 const DWORD &processID ) noexcept
{
  std::ignore = runtimeOption;

  struct EnumProcArg{
    const DWORD &processID;
    std::set<std::tuple<DWORD,HWND>> &out;
  } const enumProcArg = { processID , out };
  
  if( ::EnumWindows( []( HWND hWnd , LPARAM lParam )->BOOL{
    const struct EnumProcArg* const arg = reinterpret_cast<const struct EnumProcArg*>( lParam );
    assert( arg );

    DWORD window_process = 0;
    const DWORD threadId = ::GetWindowThreadProcessId( hWnd , &window_process );
    if( 0 < threadId ){
      assert( 0 != window_process );
      if( arg->processID == window_process ){
        arg->out.emplace( threadId , hWnd );
      }
    }
    return TRUE;
  } , LPARAM(&enumProcArg) ) ){
    return TRUE;
  }else{
    return FALSE;
  }
}

/**
   現在のデスクトップで、processID に属するトップレベルウィンドウのうち conditionOperator でフィルタされた threadid と HWND のセットを
   window_set_of_the_process に入れてもどす

   @return 処理に成功した場合には TRUE が、何らかの理由で失敗した場合には FALSE が返る。
   処理が成功していても、window_set_of_the_process のサイズが0 の場合は、当該のプロセスが、
   条件に対応するトップレベルウィンドウを持っていない場合には window_set_of_the_process.size() は 0 を示す。
 */
template<typename conditionOperator>
static BOOL
find_target_window( const RuntimeOption& runtimeOption ,
                    std::set<std::tuple<DWORD,HWND>> &window_set_of_the_process, 
                    const DWORD &processID ) noexcept
{
  std::ignore = runtimeOption;
  std::ignore = processID;

  if( enumulate_window_of_the_process( runtimeOption ,
                                       window_set_of_the_process ,
                                       processID ) ){
    std::set< std::tuple<DWORD, HWND> > window_of_injection_target{};
    do_filter_injection_target_window<conditionOperator>( runtimeOption ,
                                                          window_of_injection_target ,
                                                          window_set_of_the_process ,
                                                          processID );
    return TRUE;
  }
  return FALSE;
}

/**
   極めてシンプルなウィンドウクラスを登録する
 */
ATOM registerSimpleWindowClass() noexcept;

/**
   極めてシンプルなウィンドウクラスのATOMを取得する
 */
const ATOM& getSimpleWindowClass() noexcept;


# endif /* defined( __cplusplus ) */

#endif /* INJECTION_CODE_H_HEADER_GUARD */
