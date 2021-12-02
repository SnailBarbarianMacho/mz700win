typedef HANDLE FILE_HDL;

#define FILE_VAL_ERROR 	INVALID_HANDLE_VALUE	// Win32:Error
// SEEK—p
#define FILE_SEEK_SET	FILE_BEGIN
#define FILE_SEEK_CUR	FILE_CURRENT
#define FILE_SEEK_END	FILE_END

//
FILE_HDL FILE_OPEN(LPCSTR filename);
FILE_HDL FILE_ROPEN(LPCSTR filename);
FILE_HDL FILE_COPEN(LPCSTR filename);
FILE_HDL FILE_AOPEN(LPCSTR filename);
int FILE_READ(FILE_HDL fh, void * ptr, int sz);
int FILE_WRITE(FILE_HDL fh, void * ptr, int sz);
BOOL FILE_CLOSE(FILE_HDL fh);
int FILE_SEEK(FILE_HDL fh, long pointer, int mode);

//
BOOL FileExists(LPCSTR filename);
short FILE_ATTR(LPCSTR filename);

