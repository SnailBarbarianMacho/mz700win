#include "dprintf.h"

#if _DEBUG
//----------------------------------------------------------------------
// debug—pprintf
//----------------------------------------------------------------------
void dprintf( char *fmt, ... )
{
	va_list		args;
	char		buf[512];

	va_start(args, fmt);
	vsprintf_s(buf, sizeof(buf), fmt, args);
	va_end(args);

	OutputDebugString(buf);
}
#endif
