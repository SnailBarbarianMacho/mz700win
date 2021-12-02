#ifndef mz700win_common_h
#define mz700win_common_h

//
#ifndef NDEBUG
#define D3D_DEBUG_INFO
#define _CRTDBG_MAP_ALLOC
#endif //_DEBUG

//
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <stdio.h>
#include <string.h>
#ifndef NDEBUG
#include <crtdbg.h>
#endif //NDEBUG

#define	TRUE		1
#define	FALSE		0

#define FD_DRIVES	2		// FDD ÉhÉâÉCÉuêî
/*
#define FASTCALL	__fastcall
#ifndef INLINE
#define INLINE		static __inline
#endif
 */
void Error(const char* s);

#endif //mz700win_common_h
