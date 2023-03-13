/**
   
   include from pch.h 
 */
#pragma once

#if !defined( FRAMEWORDK_H_HEADER_GUARD )
# define FRAMEWORDK_H_HEADER_GUARD 1 

# define WIN32_LEAN_AND_MEAN             // Windows ヘッダーからほとんど使用されていない部分を除外する
// Windows ヘッダー ファイル
# include <tchar.h>
# include <windows.h>

# if defined( __cplusplus )
#  include <string>
#  include <cassert>
# endif /* defined( __cplusplus ) */

#endif /* !defined( FRAMEWORDK_H_HEADER_GUARD ) */
