#pragma once

#ifndef HEADER_GUARD_APP_INJECTION_H 
#define HEADER_GUARD_APP_INJECTION_H 1

#if defined( __cplusplus)

namespace app{
  /**
     エラーコード このアプリケーションが使用するexitCode
     
   */
  enum{
    /* 分からないもの */
    ERROR_UNKNOWN = 2,
    /* CoInitializeEx に失敗した */
    ERROR_CoInitializeEx  = 3,
    /* SetDllDirectory("") に失敗した */
    ERROR_SetDllDirectory = 4,
    /* RegisterWindowMessageW に失敗しました。 */
    ERROR_REGISTER_WINDOW_MESSAGE_FAIL = 5,

    ERROR_LAST_CODE 
  };
  
  static_assert( ERROR_LAST_CODE < 256 ); // 一応 0 から 255 までが扱えるコード 
};


#endif /* defined( __cplusplus ) */
#endif /* !defined( HEADER_GUARD_APP_INJECTION_H ) */


