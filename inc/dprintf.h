#ifndef _DPRINTF_H_
#define _DPRINTF_H_

#include "COMMON.H"
#ifndef _DEBUG
#define dprintf	//dprintf
#else
#ifdef __cplusplus
extern "C" {
#endif //__cplusplus
void dprintf( char *fmt, ... );
#ifdef __cplusplus
}
#endif //__CPLUSPLUS

#endif //__DEBUG

#endif // _DPRINTF_H_
