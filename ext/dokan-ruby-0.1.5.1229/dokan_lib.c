#include "ruby.h"
#include "dokan.h"
#include "list.h"
#include <locale.h>

#pragma comment(lib, "dokan.lib")

#define _UNICODE


#define CREATE_FILE_F		1
#define OPEN_DIRECTORY_F	2
#define CREATE_DIRECTORY_F	3
#define CLOSE_FILE_F		4
#define READ_FILE_F			5
#define WRITE_FILE_F		6
#define GET_FILE_INFO_F		7
#define FIND_FILES_F		8
#define SET_FILE_ATTR_F		9
#define SET_FILE_TIME_F		10
#define DELETE_FILE_F		11
#define DELETE_DIRECTORY_F	12
//#define SET_FILE_POINTER_F	13
#define MOVE_FILE_F			14
#define SET_END_OF_FILE_F	15
#define LOCK_FILE_F			16
#define UNLOCK_FILE_F		17
#define UNMOUNT_F			18
#define CLEANUP_F			19
#define FLUSH_F				20

static VALUE g_DokanRoot = Qnil;
static VALUE g_cDokan    = Qnil;
static VALUE g_cFileInfo = Qnil;

static LIST_ENTRY	g_EventQueue;
static HANDLE		g_DispatchEvent;
static HANDLE		g_DispatchMutex;

static BOOL g_Debug = FALSE;

/*
  open(path, fileinfo)

  create(path, fileinfo)

  truncate(path, offset, fileinfo)

  opendir(path, fileinfo)

  mkdir(path, fileinfo)
  
  close(path, fileinfo)  

  cleanup(path, fileinfo)

  read(path, offset, length, fileinfo)

  write(path, offset, data, fileinfo)

  flush(path, fileinfo)

  stat(path, fileinfo)
   [size, attr, ctime, atime, mtime]

  readdira(path, fileinfo)
    [ [name, size, attr, ctime, atime, mtime], [], ]

  readdir(path, fileinfo)
    [name1, name2, name3...]

  setattr(path, attr, fileinfo)

  utime(path, ctime, atime, mtime, fileinfo)

  remove(path, fileinfo)

  rename(path, newpath, fileinfo)

  rmdir(path, fileinfo)

  lock(path, offset, length, fileinfo)

  unlock(path, offset, length, fileinfo)

  unmount(path, fileinfo)
*/


static int
DR_DebugPrint(
	const wchar_t *format, ...)
{
	int r = 0;
	va_list va;
	va_start(va, format);
	if (g_Debug)
		r = vfwprintf(stderr, format, va);
	va_end(va);
	return r;
}


static VALUE
DR_FileInfo_directory_p(
	VALUE self)
{
	return rb_iv_get(self, "@is_directory");
}


static VALUE
DR_FileInfo_process_id(
	VALUE self)
{
	return rb_iv_get(self, "@process_id");
}

static VALUE
DR_FileInfo_context(
	VALUE self)
{
	return rb_iv_get(self, "@context");
}

static VALUE
DR_FileInfo_set_context(
	VALUE	self,
	VALUE	context)
{
	VALUE ary;
	VALUE old = rb_iv_get(self, "@context");
	if (old != Qnil)
		rb_gc_unregister_address(&old);

	rb_iv_set(self, "@context", context);

	// to prevent from gc, register the context as global object
	rb_gc_register_address(&context);
	//ary = rb_gv_get("$dokan_context");
	//rb_ary_push(ary, context);

	return self;
}


static VOID
DR_SetFileContext(
	PDOKAN_FILE_INFO	DokanFileInfo,
	VALUE				FileInfo)
{
	VALUE context = rb_iv_get(FileInfo, "@context");
	if (context != Qnil)
		DokanFileInfo->Context = (ULONG64)context;
}


static VOID
DR_ReleaseFileContext(
	VALUE		FileInfo)
{
	VALUE context = rb_iv_get(FileInfo, "@context");
	if (context != Qnil)
		rb_gc_unregister_address(&context);
}


static VOID
DR_DefineFileInfoClass()
{
	g_cFileInfo = rb_define_class_under(g_cDokan, "FileInfo", rb_cObject);

	rb_define_method(g_cFileInfo, "directory?", DR_FileInfo_directory_p, 0);
	rb_define_method(g_cFileInfo, "process_id", DR_FileInfo_process_id, 0);
	rb_define_method(g_cFileInfo, "context", DR_FileInfo_context, 0);
	rb_define_method(g_cFileInfo, "context=", DR_FileInfo_set_context, 1);
}


static VALUE
DR_GetFileInfo(
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	VALUE info = rb_class_new_instance(0, NULL, g_cFileInfo);

	rb_iv_set(info, "@is_directory", DokanFileInfo->IsDirectory ? Qtrue : Qfalse);
	rb_iv_set(info, "@process_id", ULONG2NUM(DokanFileInfo->ProcessId));

	rb_iv_set(info, "@context",
				DokanFileInfo->Context ? (VALUE)DokanFileInfo->Context : Qnil);
	return info;
}



static VALUE
DR_GetFileName(
	LPCWSTR	FileName)
{
	wchar_t name0[MAX_PATH];
	char name[MAX_PATH];
	VALUE file;
	int i;

	wcscpy(name0, FileName);
	for (i=0; i<wcslen(name0); ++i) {
		if (name0[i] == L'\\')
			name0[i] = L'/';
	}

	wcstombs(name, name0, sizeof(name));
	file = rb_str_new2(name);
	return file;
}



// TODO: this is not good error handling
//       CreateFile always return FILE_NOT_FOUND when error occured

static int
DR_CreateFile(
	LPCWSTR					FileName,
	DWORD					AccessMode,
	DWORD					ShareMode,
	DWORD					CreationDisposition,
	DWORD					FlagsAndAttributes,
	PDOKAN_FILE_INFO		DokanFileInfo)
{
	VALUE file = DR_GetFileName(FileName);
	VALUE fileinfo = DR_GetFileInfo(DokanFileInfo);
	VALUE ret;

	DR_DebugPrint(L"CreateFile %ws\n", FileName);
	

	switch (CreationDisposition) {
	case CREATE_NEW:
		// Creates a new file. The function fails if the specified
		// file already exists.
		ret = rb_funcall(g_DokanRoot, rb_intern("create"), 2, file, fileinfo);
		DR_SetFileContext(DokanFileInfo, fileinfo);
		return RTEST(ret) ? 0 : -ERROR_FILE_NOT_FOUND;

	case CREATE_ALWAYS:
		// Creates a new file. If the file exists, the function overwrites 
		// the file and clears the existing attributes.
		ret = rb_funcall(g_DokanRoot, rb_intern("create"), 2, file, fileinfo);
		DR_SetFileContext(DokanFileInfo, fileinfo);
		return RTEST(ret) ? 0 : -ERROR_FILE_NOT_FOUND;		

	case OPEN_EXISTING:
		// Opens the file. The function fails if the file does not exist.
		ret = rb_funcall(g_DokanRoot, rb_intern("open"), 2, file, fileinfo);
		DR_SetFileContext(DokanFileInfo, fileinfo);
		return RTEST(ret) ? 0 : -ERROR_FILE_NOT_FOUND;
	
	case OPEN_ALWAYS:
		// Opens the file, if it exists. If the file does not exist,
		// the function creates the file as if dwCreationDisposition were CREATE_NEW.
		ret = rb_funcall(g_DokanRoot, rb_intern("create"), 2, file, fileinfo);
		DR_SetFileContext(DokanFileInfo, fileinfo);
		return RTEST(ret) ? 0 : -ERROR_FILE_NOT_FOUND;

	case TRUNCATE_EXISTING:
		// Opens the file. Once opened, the file is truncated so that its size
		// is zero bytes. The calling process must open the file with at least
		// GENERIC_WRITE access. The function fails if the file does not exist.
		ret = rb_funcall(g_DokanRoot, rb_intern("open"), 2, file, fileinfo);
		DR_SetFileContext(DokanFileInfo, fileinfo);

		if (RTEST(ret))
			ret = rb_funcall(g_DokanRoot, rb_intern("truncate"), 3, file, INT2FIX(0), fileinfo);

		DR_SetFileContext(DokanFileInfo, fileinfo);
		return RTEST(ret) ? 0 : -ERROR_FILE_NOT_FOUND;
	default:
		fprintf(stderr, "unknown CreationDisposition %d\n", CreationDisposition); 
	}
	return -ERROR_FILE_NOT_FOUND;
}




static int
DR_CreateDirectory(
	LPCWSTR					FileName,
	PDOKAN_FILE_INFO		DokanFileInfo)
{
	VALUE file = DR_GetFileName(FileName);
	VALUE fileinfo = DR_GetFileInfo(DokanFileInfo);
	VALUE ret;

	DR_DebugPrint(L"CreateDirectory %ws\n", FileName);

	ret = rb_funcall(g_DokanRoot, rb_intern("mkdir"), 2, file, fileinfo);

	DR_SetFileContext(DokanFileInfo, fileinfo);

	return RTEST(ret) ? 0 : -ERROR_FILE_NOT_FOUND;
}



static int
DR_OpenDirectory(
	LPCWSTR					FileName,
	PDOKAN_FILE_INFO		DokanFileInfo)
{
	VALUE file = DR_GetFileName(FileName);
	VALUE fileinfo = DR_GetFileInfo(DokanFileInfo);
	VALUE ret;

	DR_DebugPrint(L"OpenDirectory %ws\n", FileName);

	ret = rb_funcall(g_DokanRoot, rb_intern("opendir"), 2, file, fileinfo);

	DR_SetFileContext(DokanFileInfo, fileinfo);

	return RTEST(ret) ? 0 : -ERROR_FILE_NOT_FOUND;
}


static int
DR_CloseFile(
	LPCWSTR					FileName,
	PDOKAN_FILE_INFO		DokanFileInfo)
{
	VALUE file = DR_GetFileName(FileName);
	VALUE fileinfo = DR_GetFileInfo(DokanFileInfo);
	VALUE ret;

	DR_DebugPrint(L"CloseFile %ws\n", FileName);	

	ret = rb_funcall(g_DokanRoot, rb_intern("close"), 2, file, fileinfo);

	DR_ReleaseFileContext(fileinfo);

	return RTEST(ret) ? 0 : -1;
}


static int
DR_Cleanup(
	LPCWSTR					FileName,
	PDOKAN_FILE_INFO		DokanFileInfo)
{
	VALUE file = DR_GetFileName(FileName);
	VALUE fileinfo = DR_GetFileInfo(DokanFileInfo);
	VALUE ret;

	DR_DebugPrint(L"Cleanup %ws\n", FileName);	

	ret = rb_funcall(g_DokanRoot, rb_intern("cleanup"), 2, file, fileinfo);

	//DR_SetFileContext(DokanFileInfo, fileinfo);

	return RTEST(ret) ? 0 : -1;
}


static int
DR_ReadFile(
	LPCWSTR				FileName,
	LPVOID				Buffer,
	DWORD				BufferLength,
	LPDWORD				ReadLength,
	LONGLONG			Offset,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	VALUE	file, fileinfo, ret;
	ULONG	len;
	ULONG	size;
	char*	str;

	DR_DebugPrint(L"ReadFile %ws\n", FileName);

	file = DR_GetFileName(FileName);
	fileinfo = DR_GetFileInfo(DokanFileInfo);

	ret = rb_funcall(g_DokanRoot,
					rb_intern("read"),
					4,
					file,
					ULONG2NUM((ULONG)Offset),
					ULONG2NUM((ULONG)BufferLength),
					fileinfo);

	DR_SetFileContext(DokanFileInfo, fileinfo);

	if (!RTEST(ret))
		return -1;

	StringValue(ret);

	len = RSTRING(ret)->len;
	str = RSTRING(ret)->ptr;

	size = BufferLength < len ? BufferLength : len;

	*ReadLength = size;
	CopyMemory(Buffer, str, size);

	return 0;
}


static int
DR_WriteFile(
	LPCWSTR		FileName,
	LPCVOID		Buffer,
	DWORD		NumberOfBytesToWrite,
	LPDWORD		NumberOfBytesWritten,
	LONGLONG			Offset,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	VALUE fileinfo, file;
	VALUE str, ret;

	DR_DebugPrint(L"WriteFile %ws\n", FileName);

	file = DR_GetFileName(FileName);
	fileinfo = DR_GetFileInfo(DokanFileInfo);
	
	str = rb_str_new(Buffer, NumberOfBytesToWrite);

	ret = rb_funcall(g_DokanRoot,
			rb_intern("write"),
			4,
			file,
			ULONG2NUM((ULONG)Offset),
			str,
			fileinfo);
	
	DR_SetFileContext(DokanFileInfo, fileinfo);

	if (!RTEST(ret))
		return -1;

	*NumberOfBytesWritten = NumberOfBytesToWrite;

	return 0;
}


static int
DR_Flush(
	LPCWSTR				FileName,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	VALUE file, fileinfo;
	VALUE ret;

	DR_DebugPrint(L"Flush %ws\n", FileName);

	file = DR_GetFileName(FileName);
	fileinfo = DR_GetFileInfo(DokanFileInfo);

	ret = rb_funcall(g_DokanRoot, rb_intern("flush"), 2, file, fileinfo);

	DR_SetFileContext(DokanFileInfo, fileinfo);


	if (!RTEST(ret))
		return -1;
	else
		return 0;
}



static VOID
DR_UnixTimeToFileTime(
	int			Time,
	LPFILETIME	FileTime)
{
     LONGLONG ll;
     ll = Int32x32To64(Time, 10000000) + 116444736000000000;
     FileTime->dwLowDateTime = (DWORD)ll;
     FileTime->dwHighDateTime = ll >> 32;
}


static int
DR_GetFileInformation(
	LPCWSTR							FileName,
	LPBY_HANDLE_FILE_INFORMATION	HandleFileInformation,
	PDOKAN_FILE_INFO				DokanFileInfo)
{
	VALUE file, fileinfo;
	VALUE ret;

	DR_DebugPrint(L"GetFileInfo %ws\n", FileName);

	file = DR_GetFileName(FileName);
	fileinfo = DR_GetFileInfo(DokanFileInfo);

	ret = rb_funcall(g_DokanRoot, rb_intern("stat"), 2, file, fileinfo);

	DR_SetFileContext(DokanFileInfo, fileinfo);

	if (!RTEST(ret))
		return -1;

	Check_Type(ret, T_ARRAY);

	if (RARRAY(ret)->len != 5) {
		fprintf(stderr, "array size should be 5");
		return -1;
	}

	HandleFileInformation->nFileSizeLow = NUM2ULONG(rb_ary_entry(ret,0));
	HandleFileInformation->dwFileAttributes = NUM2ULONG(rb_ary_entry(ret,1));

	DR_UnixTimeToFileTime(NUM2LONG(rb_ary_entry(ret,2)),
							&HandleFileInformation->ftCreationTime);
	DR_UnixTimeToFileTime(NUM2LONG(rb_ary_entry(ret,3)),
							&HandleFileInformation->ftLastAccessTime);
	DR_UnixTimeToFileTime(NUM2LONG(rb_ary_entry(ret,4)),
							&HandleFileInformation->ftLastWriteTime);

	return 0;
}


static int
DR_FindFiles(
	LPCWSTR				FileName,
	PFillFindData		FillFindData,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	VALUE file, fileinfo;
	VALUE ret;
	int i;

	DR_DebugPrint(L"FindFiles %ws\n", FileName);

	/*
	   not implemented:
		readdira(path, fileinfo)
		[ [name, size, attr, ctime, atime, mtime], [], ]
	*/


	file = DR_GetFileName(FileName);
	fileinfo = DR_GetFileInfo(DokanFileInfo);

	//rb_gc_disable();
	ret = rb_funcall(g_DokanRoot, rb_intern("readdir"), 2, file, fileinfo);
	
	DR_SetFileContext(DokanFileInfo, fileinfo);

	if (!RTEST(ret))
		return -1;
	
	Check_Type(ret, T_ARRAY);

	for (i = 0; i<RARRAY(ret)->len; ++i) {
		WCHAR						filename[MAX_PATH];
		WIN32_FIND_DATAW			find;
		BY_HANDLE_FILE_INFORMATION	info;
		size_t	count;

		VALUE	entry = rb_ary_entry(ret,i);

		StringValue(entry);

		ZeroMemory(&find, sizeof(WIN32_FIND_DATAW));
		ZeroMemory(&info, sizeof(BY_HANDLE_FILE_INFORMATION));

		count = mbstowcs(find.cFileName, RSTRING(entry)->ptr, RSTRING(entry)->len);

		find.dwFileAttributes |= FILE_ATTRIBUTE_NORMAL;

		ZeroMemory(filename, sizeof(filename));
		wcscat(filename, FileName);
		wcscat(filename, find.cFileName);
		
		if (DR_GetFileInformation(filename, &info, DokanFileInfo) == 0) {

			find.dwFileAttributes	= info.dwFileAttributes;
			find.ftCreationTime		= info.ftCreationTime;
			find.ftLastAccessTime	= info.ftLastAccessTime;
			find.ftLastWriteTime	= info.ftLastWriteTime;
			find.nFileSizeLow		= info.nFileSizeLow;
		}

		FillFindData(&find, DokanFileInfo);
	}
	//rb_gc_enable();
	return 0;
}


static int
DR_SetFileAttributes(
	LPCWSTR				FileName,
	DWORD				FileAttributes,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	VALUE file, fileinfo, attr;
	VALUE ret;

	DR_DebugPrint(L"SetFileAttributes %ws\n", FileName);

	file = DR_GetFileName(FileName);
	fileinfo = DR_GetFileInfo(DokanFileInfo);

	attr = ULONG2NUM(FileAttributes);
	ret = rb_funcall(g_DokanRoot, rb_intern("setattr"), 3, file, attr, fileinfo);

	DR_SetFileContext(DokanFileInfo, fileinfo);

	return RTEST(ret) ? 0 : -1;
}

static int
DR_FileTimeToUnixTime(
	CONST FILETIME*	Time)
{
	LONGLONG ll = (LONGLONG)Time->dwLowDateTime + Time->dwHighDateTime << 32;
	return (int)(( ll - 0x19DB1DED53E8000 ) / 10000000);
}


static int
DR_SetFileTime(
   	LPCWSTR				FileName,
	CONST FILETIME*		CreationTime,
	CONST FILETIME*		LastAccessTime,
	CONST FILETIME*		LastWriteTime,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	VALUE file, fileinfo;
	VALUE ctime, atime, mtime;
	VALUE ret;

	DR_DebugPrint(L"SetFileTime %ws\n", FileName);

	file = DR_GetFileName(FileName);
	fileinfo = DR_GetFileInfo(DokanFileInfo);

	ctime = INT2NUM(DR_FileTimeToUnixTime(CreationTime));
	atime = INT2NUM(DR_FileTimeToUnixTime(LastAccessTime));
	mtime = INT2NUM(DR_FileTimeToUnixTime(LastWriteTime));

	ret = rb_funcall(g_DokanRoot, rb_intern("utime"), 5, file, ctime, atime, mtime, fileinfo);

	DR_SetFileContext(DokanFileInfo, fileinfo);

	return RTEST(ret) ? 0 : -1;
}


static int
DR_DeleteFile(
	LPCWSTR				FileName,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	VALUE file, fileinfo;
	VALUE ret;

	DR_DebugPrint(L"DeleteFile %ws\n", FileName);

	file = DR_GetFileName(FileName);
	fileinfo = DR_GetFileInfo(DokanFileInfo);

	ret = rb_funcall(g_DokanRoot, rb_intern("remove"), 2, file, fileinfo);

	DR_SetFileContext(DokanFileInfo, fileinfo);

	return RTEST(ret) ? 0 : -1;
}


static int
DR_DeleteDirectory(
	LPCWSTR				FileName,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	VALUE file, fileinfo;
	VALUE ret;

	DR_DebugPrint(L"DeleteDirectory %ws\n", FileName);

	file = DR_GetFileName(FileName);
	fileinfo = DR_GetFileInfo(DokanFileInfo);

	ret = rb_funcall(g_DokanRoot, rb_intern("rmdir"), 2, file, fileinfo);

	DR_SetFileContext(DokanFileInfo, fileinfo);
	
	return RTEST(ret) ? 0 : -1;
}


static int
DR_MoveFile(
	LPCWSTR				FileName,
	LPCWSTR				NewFileName,
	BOOL				ReplaceIfExisting,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	VALUE	file, newfile, fileinfo;
	VALUE	ret;
	ULONG	size;

	DR_DebugPrint(L"MoveFile %ws => %ws\n", FileName, NewFileName);

	file = DR_GetFileName(FileName);
	newfile = DR_GetFileName(NewFileName);
	fileinfo = DR_GetFileInfo(DokanFileInfo);

	ret = rb_funcall(g_DokanRoot, rb_intern("rename"), 3, file, newfile, fileinfo);

	DR_SetFileContext(DokanFileInfo, fileinfo);

	return RTEST(ret) ? 0 : -1;
}



static int
DR_SetEndOfFile(
	LPCWSTR				FileName,
	LONGLONG			ByteOffset,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	VALUE	file, length, fileinfo;
	VALUE	ret;

	DR_DebugPrint(L"SetEndOfFile %ws\n", FileName);
	
	file = DR_GetFileName(FileName);
	fileinfo = DR_GetFileInfo(DokanFileInfo);

	length = ULONG2NUM((ULONG)ByteOffset);

	ret = rb_funcall(g_DokanRoot, rb_intern("truncate"), 3, file, length, fileinfo);

	DR_SetFileContext(DokanFileInfo, fileinfo);

	return RTEST(ret) ? 0 : -1;
}


static int
DR_LockFile(
	LPCWSTR				FileName,
	LONGLONG			ByteOffset,
	LONGLONG			Length,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	VALUE	file, offset, length, fileinfo;
	VALUE	ret;

	DR_DebugPrint(L"LockFile %ws\n", FileName);

	file = DR_GetFileName(FileName);
	fileinfo = DR_GetFileInfo(DokanFileInfo);

	offset = ULONG2NUM((ULONG)ByteOffset);
	length = ULONG2NUM((ULONG)Length);

	ret = rb_funcall(g_DokanRoot, rb_intern("lock"), 4, file, offset, length, fileinfo);

	DR_SetFileContext(DokanFileInfo, fileinfo);

	return RTEST(ret) ? 0 : -1;
}


static int
DR_UnlockFile(
	LPCWSTR				FileName,
	LONGLONG			ByteOffset,
	LONGLONG			Length,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	VALUE	file, offset, length, fileinfo;
	VALUE	ret;

	DR_DebugPrint(L"UnlockFile %ws\n", FileName);

	file = DR_GetFileName(FileName);
	fileinfo = DR_GetFileInfo(DokanFileInfo);

	offset = ULONG2NUM((ULONG)ByteOffset);
	length = ULONG2NUM((ULONG)Length);

	ret = rb_funcall(g_DokanRoot, rb_intern("unlock"), 4, file, offset, length, fileinfo);

	DR_SetFileContext(DokanFileInfo, fileinfo);

	return RTEST(ret) ? 0 : -1;
}


static int
DR_Unmount(
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	VALUE fileinfo;

	DR_DebugPrint(L"Unmount\n");

	fileinfo = DR_GetFileInfo(DokanFileInfo);
	rb_funcall(g_DokanRoot, rb_intern("unmount"), 1, fileinfo);

	return 0;
}


typedef struct _EVENT_ENTRY
{
	ULONG		Function;
	PVOID		Args;
	HANDLE		Event;
	LIST_ENTRY	ListEntry;
} EVENT_ENTRY, *PEVENT_ENTRY;


static VOID
DR_DispatchAndWait(
	ULONG	Function,
	PVOID	Args)
{
	PEVENT_ENTRY entry = (PEVENT_ENTRY)malloc(sizeof(EVENT_ENTRY));
	entry->Function = Function;
	entry->Args = Args;
	entry->Event = CreateEvent(NULL, TRUE, FALSE, NULL);
	
	WaitForSingleObject(g_DispatchMutex, INFINITE);
	InsertTailList(&g_EventQueue, &entry->ListEntry);
	SetEvent(g_DispatchEvent);
	ReleaseMutex(g_DispatchMutex);

	WaitForSingleObject(entry->Event, INFINITE);

	CloseHandle(entry->Event);
	free(entry);
}




typedef struct _CREATE_FILE
{
	LPCWSTR					FileName;
	DWORD					AccessMode;
	DWORD					ShareMode;
	DWORD					CreationDisposition;
	DWORD					FlagsAndAttributes;
	PDOKAN_FILE_INFO		DokanFileInfo;
	int						Return;
} CREATE_FILE, *PCREATE_FILE;


static int DOKAN_CALLBACK
DR_CreateFile0(
	LPCWSTR					FileName,
	DWORD					AccessMode,
	DWORD					ShareMode,
	DWORD					CreationDisposition,
	DWORD					FlagsAndAttributes,
	PDOKAN_FILE_INFO		DokanFileInfo)
{
	PCREATE_FILE args = (PCREATE_FILE)malloc(sizeof(CREATE_FILE));
	int ret;

	//DR_DebugPrint(L"CreateFile0 %ws\n", FileName);

	args->FileName = FileName;
	args->AccessMode = AccessMode;
	args->ShareMode = ShareMode;
	args->CreationDisposition = CreationDisposition;
	args->FlagsAndAttributes = FlagsAndAttributes;
	args->DokanFileInfo = DokanFileInfo;
	args->Return = -1;

	DR_DispatchAndWait(CREATE_FILE_F, args);
	ret = args->Return;
	free(args);
	
	return ret;
}


typedef struct _CREATE_DIRECTORY
{
	LPCWSTR					FileName;
	PDOKAN_FILE_INFO		DokanFileInfo;
	int						Return;
} CREATE_DIRECTORY, *PCREATE_DIRECTORY;


static int DOKAN_CALLBACK
DR_CreateDirectory0(
	LPCWSTR					FileName,
	PDOKAN_FILE_INFO		DokanFileInfo)
{
	PCREATE_DIRECTORY args = (PCREATE_DIRECTORY)malloc(sizeof(CREATE_DIRECTORY));
	int ret;

	//DR_DebugPrint(L"CreateDirectory0 %ws\n", FileName);

	args->FileName = FileName;
	args->DokanFileInfo = DokanFileInfo;
	args->Return = -1;

	DR_DispatchAndWait(CREATE_DIRECTORY_F, args);
	ret = args->Return;
	free(args);

	return ret;
}


typedef struct _OPEN_DIRECTORY
{
	LPCWSTR					FileName;
	PDOKAN_FILE_INFO		DokanFileInfo;
	int						Return;
} OPEN_DIRECTORY, *POPEN_DIRECTORY;


static int DOKAN_CALLBACK
DR_OpenDirectory0(
	LPCWSTR					FileName,
	PDOKAN_FILE_INFO		DokanFileInfo)
{
	POPEN_DIRECTORY args = (POPEN_DIRECTORY)malloc(sizeof(OPEN_DIRECTORY));
	int ret;

	//DR_DebugPrint(L"OpenDirectory0 %ws\n", FileName);

	args->FileName = FileName;
	args->DokanFileInfo = DokanFileInfo;
	args->Return = -1;

	DR_DispatchAndWait(OPEN_DIRECTORY_F, args);
	ret = args->Return;
	free(args);
	
	return ret;
}


typedef struct _CLOSE_FILE
{
	LPCWSTR				FileName;
	PDOKAN_FILE_INFO	DokanFileInfo;
	int					Return;
} CLOSE_FILE, *PCLOSE_FILE;


static int DOKAN_CALLBACK
DR_CloseFile0(
	LPCWSTR					FileName,
	PDOKAN_FILE_INFO		DokanFileInfo)
{
	PCLOSE_FILE	args = (PCLOSE_FILE)malloc(sizeof(CLOSE_FILE));
	int ret;
	
	//DR_DebugPrint(L"CloseFile0 %ws\n", FileName);
	
	args->FileName = FileName;
	args->DokanFileInfo = DokanFileInfo;
	args->Return = -1;

	DR_DispatchAndWait(CLOSE_FILE_F, args);
	ret = args->Return;
	free(args);
	
	return ret;
}


typedef struct _CLEANUP
{
	LPCWSTR				FileName;
	PDOKAN_FILE_INFO	DokanFileInfo;
	int					Return;
} CLEANUP, *PCLEANUP;


static int DOKAN_CALLBACK
DR_Cleanup0(
	LPCWSTR					FileName,
	PDOKAN_FILE_INFO		DokanFileInfo)
{
	PCLOSE_FILE	args = (PCLOSE_FILE)malloc(sizeof(CLOSE_FILE));
	int ret;
	
	//DR_DebugPrint(L"Cleanup0 %ws\n", FileName);
	
	args->FileName = FileName;
	args->DokanFileInfo = DokanFileInfo;
	args->Return = -1;

	DR_DispatchAndWait(CLEANUP_F, args);
	ret = args->Return;
	free(args);
	
	return ret;
}


typedef struct _READ_FILE
{
	LPCWSTR				FileName;
	LPVOID				Buffer;
	DWORD				BufferLength;
	LPDWORD				ReadLength;
	LONGLONG			Offset;
	PDOKAN_FILE_INFO	DokanFileInfo;
	int					Return;
} READ_FILE, *PREAD_FILE;


static int DOKAN_CALLBACK
DR_ReadFile0(
	LPCWSTR				FileName,
	LPVOID				Buffer,
	DWORD				BufferLength,
	LPDWORD				ReadLength,
	LONGLONG			Offset,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	PREAD_FILE args  = (PREAD_FILE)malloc(sizeof(READ_FILE));
	int ret;

	//DR_DebugPrint(L"ReadFile0 %ws\n", FileName);

	args->FileName = FileName;
	args->Buffer	= Buffer;
	args->BufferLength = BufferLength;
	args->ReadLength = ReadLength;
	args->Offset = Offset;
	args->DokanFileInfo = DokanFileInfo;
	args->Return = -1;

	DR_DispatchAndWait(READ_FILE_F, args);
	ret = args->Return;
	free(args);
	
	return ret;
}


typedef struct _WRITE_FILE
{
	LPCWSTR				FileName;
	LPCVOID				Buffer;
	DWORD				NumberOfBytesToWrite;
	LPDWORD				NumberOfBytesWritten;
	LONGLONG			Offset;
	PDOKAN_FILE_INFO	DokanFileInfo;
	int					Return;
} WRITE_FILE, *PWRITE_FILE;

static int DOKAN_CALLBACK
DR_WriteFile0(
	LPCWSTR		FileName,
	LPCVOID		Buffer,
	DWORD		NumberOfBytesToWrite,
	LPDWORD		NumberOfBytesWritten,
	LONGLONG			Offset,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	PWRITE_FILE	args = (PWRITE_FILE)malloc(sizeof(WRITE_FILE));
	int ret;

	//DR_DebugPrint(L"WriteFile0 %ws\n", FileName);

	args->FileName = FileName;
	args->Buffer = Buffer;
	args->NumberOfBytesToWrite = NumberOfBytesToWrite;
	args->NumberOfBytesWritten = NumberOfBytesWritten;
	args->Offset = Offset;
	args->DokanFileInfo = DokanFileInfo;
	args->Return = -1;

	DR_DispatchAndWait(WRITE_FILE_F, args);
	ret = args->Return;
	free(args);
	
	return ret;
}


typedef struct _GET_FILE_INFO
{
	LPCWSTR							FileName;
	LPBY_HANDLE_FILE_INFORMATION	HandleFileInformation;
	PDOKAN_FILE_INFO				DokanFileInfo;
	int								Return;
} GET_FILE_INFO, *PGET_FILE_INFO;


static int DOKAN_CALLBACK
DR_GetFileInformation0(
	LPCWSTR							FileName,
	LPBY_HANDLE_FILE_INFORMATION	HandleFileInformation,
	PDOKAN_FILE_INFO				DokanFileInfo)
{
	PGET_FILE_INFO args = (PGET_FILE_INFO)malloc(sizeof(GET_FILE_INFO));
	int ret;

	//DR_DebugPrint(L"GetFileInfo0 %ws\n", FileName);

	args->FileName = FileName;
	args->HandleFileInformation = HandleFileInformation;
	args->DokanFileInfo = DokanFileInfo;
	args->Return = -1;

	DR_DispatchAndWait(GET_FILE_INFO_F, args);
	ret = args->Return;
	free(args);
	
	return ret;
}


typedef struct _FIND_FILES
{
	LPCWSTR				FileName;
	PFillFindData		FillFindData;
	PDOKAN_FILE_INFO	DokanFileInfo;
	int					Return;
} FIND_FILES, *PFIND_FILES;


static int DOKAN_CALLBACK
DR_FindFiles0(
	LPCWSTR				FileName,
	PFillFindData		FillFindData,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	PFIND_FILES args = (PFIND_FILES)malloc(sizeof(FIND_FILES));
	int ret;

	//DR_DebugPrint(L"FindFiles0 %ws\n", FileName);

	args->FileName = FileName;
	args->FillFindData = FillFindData;
	args->DokanFileInfo = DokanFileInfo;
	args->Return = -1;

	DR_DispatchAndWait(FIND_FILES_F, args);
	ret = args->Return;
	free(args);
	
	return ret;
}

typedef struct _DELETE_FILE
{
	LPCWSTR				FileName;
	PDOKAN_FILE_INFO	DokanFileInfo;
	HANDLE				Event;
	int					Return;
} DELETE_FILE, *PDELETE_FILE;

static int DOKAN_CALLBACK
DR_DeleteFile0(
	LPCWSTR				FileName,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	PDELETE_FILE args = (PDELETE_FILE)malloc(sizeof(DELETE_FILE));
	int ret;

	//DR_DebugPrint(L"DeleteFile0 %ws\n", FileName);

	args->FileName = FileName;
	args->DokanFileInfo = DokanFileInfo;
	args->Return = -1;

	DR_DispatchAndWait(DELETE_FILE_F, args);
	ret = args->Return;
	free(args);

	return ret;
}


typedef struct _DELETE_DIRECTORY
{
	LPCWSTR				FileName;
	PDOKAN_FILE_INFO	DokanFileInfo;
	HANDLE				Event;
	int					Return;
} DELETE_DIRECTORY, *PDELETE_DIRECTORY;


static int DOKAN_CALLBACK
DR_DeleteDirectory0(
	LPCWSTR				FileName,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	PDELETE_DIRECTORY args = (PDELETE_DIRECTORY)malloc(sizeof(DELETE_DIRECTORY));
	int ret;

	//DR_DebugPrint(L"DeleteDirectory0 %ws\n", FileName);

	args->FileName = FileName;
	args->DokanFileInfo = DokanFileInfo;
	args->Return = -1;

	DR_DispatchAndWait(DELETE_DIRECTORY_F, args);
	ret = args->Return;
	free(args);
	
	return ret;
}


typedef struct _MOVE_FILE
{
	LPCWSTR				FileName;
	LPCWSTR				NewFileName;
	BOOL				ReplaceIfExisting;
	PDOKAN_FILE_INFO	DokanFileInfo;
	HANDLE				Event;
	int					Return;
} MOVE_FILE, *PMOVE_FILE;


static int DOKAN_CALLBACK
DR_MoveFile0(
	LPCWSTR				FileName,
	LPCWSTR				NewFileName,
	BOOL				ReplaceIfExisting,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	PMOVE_FILE args = (PMOVE_FILE)malloc(sizeof(MOVE_FILE));
	int ret;

	//DR_DebugPrint(L"MoveFile0 %ws\n", FileName);

	args->FileName = FileName;
	args->NewFileName = NewFileName;
	args->ReplaceIfExisting = ReplaceIfExisting;
	args->DokanFileInfo = DokanFileInfo;
	args->Return = -1;

	DR_DispatchAndWait(MOVE_FILE_F, args);
	ret = args->Return;
	free(args);
	
	return ret;
}


typedef struct _SET_FILE_ATTR
{
	LPCWSTR				FileName;
	DWORD				FileAttributes;
	PDOKAN_FILE_INFO	DokanFileInfo;
	HANDLE				Event;
	int					Return;
} SET_FILE_ATTR, *PSET_FILE_ATTR;


static int DOKAN_CALLBACK
DR_SetFileAttributes0(
	LPCWSTR				FileName,
	DWORD				FileAttributes,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	PSET_FILE_ATTR args = (PSET_FILE_ATTR)malloc(sizeof(SET_FILE_ATTR));
	int ret;

	//DR_DebugPrint(L"SetFileAttributes0 %ws\n", FileName);

	args->FileName = FileName;
	args->FileAttributes = FileAttributes;
	args->DokanFileInfo = DokanFileInfo;
	args->Return = -1;

	DR_DispatchAndWait(SET_FILE_ATTR_F, args);
	ret = args->Return;
	free(args);

	return ret;
}


typedef struct _SET_FILE_TIME
{
	LPCWSTR				FileName;
	CONST FILETIME*		CreationTime;
	CONST FILETIME*		LastAccessTime;
	CONST FILETIME*		LastWriteTime;
	PDOKAN_FILE_INFO	DokanFileInfo;
	HANDLE				Event;
	int					Return;
} SET_FILE_TIME, *PSET_FILE_TIME;

static int DOKAN_CALLBACK
DR_SetFileTime0(
   	LPCWSTR				FileName,
	CONST FILETIME*		CreationTime,
	CONST FILETIME*		LastAccessTime,
	CONST FILETIME*		LastWriteTime,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	PSET_FILE_TIME args = (PSET_FILE_TIME)malloc(sizeof(SET_FILE_TIME));
	int ret;

	//DR_DebugPrint(L"SetFileTime0 %ws\n", FileName);

	args->FileName = FileName;
	args->CreationTime = CreationTime;
	args->LastAccessTime = LastAccessTime;
	args->LastWriteTime = LastWriteTime;
	args->Return = -1;

	DR_DispatchAndWait(SET_FILE_TIME_F, args);
	ret = args->Return;
	free(args);

	return ret;
}


typedef struct _SET_END_OF_FILE
{
	LPCWSTR				FileName;
	LONGLONG			ByteOffset;
	PDOKAN_FILE_INFO	DokanFileInfo;
	int					Return;
} SET_END_OF_FILE, *PSET_END_OF_FILE;

static int DOKAN_CALLBACK
DR_SetEndOfFile0(
	LPCWSTR				FileName,
	LONGLONG			ByteOffset,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	PSET_END_OF_FILE args = (PSET_END_OF_FILE)malloc(sizeof(SET_END_OF_FILE));
	int ret;

	//DR_DebugPrint(L"SetEndOfFile0 %ws\n", FileName);

	args->FileName = FileName;
	args->ByteOffset = ByteOffset;
	args->DokanFileInfo = DokanFileInfo;
	args->Return = -1;

	DR_DispatchAndWait(SET_END_OF_FILE_F, args);
	ret = args->Return;
	free(args);

	return ret;
}



typedef struct _LOCK_FILE
{
	LPCWSTR				FileName;
	LONGLONG			ByteOffset;
	LONGLONG			Length;
	PDOKAN_FILE_INFO	DokanFileInfo;
	int					Return;
} LOCK_FILE, *PLOCK_FILE;

static int DOKAN_CALLBACK
DR_LockFile0(
	LPCWSTR				FileName,
	LONGLONG			ByteOffset,
	LONGLONG			Length,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	PLOCK_FILE	args = (PLOCK_FILE)malloc(sizeof(LOCK_FILE));
	int ret;

	//DR_DebugPrint(L"LockFile0 %ws\n", FileName);

	args->FileName = FileName;
	args->ByteOffset = ByteOffset;
	args->Length	= Length;
	args->DokanFileInfo = DokanFileInfo;
	args->Return	= -1;

	DR_DispatchAndWait(LOCK_FILE_F, args);
	ret = args->Return;
	free(args);

	return ret;
}



typedef struct _UNLOCK_FILE
{
	LPCWSTR				FileName;
	LONGLONG			ByteOffset;
	LONGLONG			Length;
	PDOKAN_FILE_INFO	DokanFileInfo;
	int					Return;
} UNLOCK_FILE, *PUNLOCK_FILE;

static int DOKAN_CALLBACK
DR_UnlockFile0(
	LPCWSTR				FileName,
	LONGLONG			ByteOffset,
	LONGLONG			Length,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	PUNLOCK_FILE	args = (PUNLOCK_FILE)malloc(sizeof(UNLOCK_FILE));
	int ret;

	//DR_DebugPrint(L"UnlockFile0 %ws\n", FileName);

	args->FileName = FileName;
	args->ByteOffset = ByteOffset;
	args->Length	= Length;
	args->DokanFileInfo = DokanFileInfo;
	args->Return	= -1;

	DR_DispatchAndWait(UNLOCK_FILE_F, args);
	ret = args->Return;
	free(args);

	return ret;
}



typedef struct _UNMOUNT
{
	PDOKAN_FILE_INFO	DokanFileInfo;
	int					Return;
} UNMOUNT, *PUNMOUNT;

static int DOKAN_CALLBACK
DR_Unmount0(
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	PUNMOUNT args = (PUNMOUNT)malloc(sizeof(UNMOUNT));
	int ret;

	//DR_DebugPrint(L"Unmount0\n");

	args->DokanFileInfo = DokanFileInfo;
	args->Return = -1;

	DR_DispatchAndWait(UNMOUNT_F, args);
	ret = args->Return;
	free(args);

	return ret;
}


typedef struct _FLUSH
{
	LPCWSTR				FileName;
	PDOKAN_FILE_INFO	DokanFileInfo;
	int					Return;
} FLUSH, *PFLUSH;

static int DOKAN_CALLBACK
DR_FlushFileBuffers0(
	LPCWSTR				FileName,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	PFLUSH args = (PFLUSH)malloc(sizeof(FLUSH));
	int ret;

	args->FileName = FileName;
	args->DokanFileInfo = DokanFileInfo;
	args->Return = -1;

	DR_DispatchAndWait(FLUSH_F, args);
	ret = args->Return;
	free(args);

	return ret;
}


static DOKAN_OPERATIONS
dokanOperations = {
	DR_CreateFile0,
	DR_OpenDirectory0,
	DR_CreateDirectory0,
	DR_Cleanup0,
	DR_CloseFile0,
	DR_ReadFile0,
	DR_WriteFile0,
	DR_FlushFileBuffers0,
	DR_GetFileInformation0,
	DR_FindFiles0,
	NULL, // FindFilesWithPattern
	DR_SetFileAttributes0,
	DR_SetFileTime0,
	DR_DeleteFile0,
	DR_DeleteDirectory0,
	DR_MoveFile0,
	DR_SetEndOfFile0,
	DR_LockFile0,
	DR_UnlockFile0,
	NULL, // GetDiskFreeSpace
	NULL, // GetVolumeInformation
	DR_Unmount0
};


static PEVENT_ENTRY CurrentEvent;

static VALUE
DR_catch_exception(
	VALUE	arg)
{
	PEVENT_ENTRY entry = CurrentEvent;

	switch (entry->Function) {
	case CREATE_FILE_F:
		{
			PCREATE_FILE args = (PCREATE_FILE)entry->Args;
			args->Return = DR_CreateFile(
								args->FileName,
								args->AccessMode,
								args->ShareMode,
								args->CreationDisposition,
								args->FlagsAndAttributes,
								args->DokanFileInfo);
			}
			break;
				
	case OPEN_DIRECTORY_F:
		{
			POPEN_DIRECTORY args = (POPEN_DIRECTORY)entry->Args;
			args->Return = DR_OpenDirectory(
								args->FileName,
								args->DokanFileInfo);
		}
		break;
				
	case CREATE_DIRECTORY_F:
		{
			PCREATE_DIRECTORY args = (PCREATE_DIRECTORY)entry->Args;
			args->Return = DR_CreateDirectory(
								args->FileName,
								args->DokanFileInfo);
		}
		break;

	case CLOSE_FILE_F:
		{
			PCLOSE_FILE args = (PCLOSE_FILE)entry->Args;
			args->Return = DR_CloseFile(
								args->FileName,
								args->DokanFileInfo);
		}
		break;
				
	case READ_FILE_F:
		{
			PREAD_FILE args = (PREAD_FILE)entry->Args;
			args->Return = DR_ReadFile(
								args->FileName,
								args->Buffer,
								args->BufferLength,
								args->ReadLength,
								args->Offset,
								args->DokanFileInfo);
		}
		break;
				
	case WRITE_FILE_F:
		{
			PWRITE_FILE args = (PWRITE_FILE)entry->Args;
			args->Return = DR_WriteFile(
								args->FileName,
								args->Buffer,
								args->NumberOfBytesToWrite,
								args->NumberOfBytesWritten,
								args->Offset,
								args->DokanFileInfo);
			}
			break;
				
	case GET_FILE_INFO_F:
		{
			PGET_FILE_INFO args = (PGET_FILE_INFO)entry->Args;
			args->Return = DR_GetFileInformation(
								args->FileName,
								args->HandleFileInformation,
								args->DokanFileInfo);
		}
		break;
				
	case FIND_FILES_F:
		{
			PFIND_FILES args = (PFIND_FILES)entry->Args;
			args->Return = DR_FindFiles(
								args->FileName,
								args->FillFindData,
								args->DokanFileInfo);
		}
		break;
				
	case SET_FILE_ATTR_F:
		{
			PSET_FILE_ATTR args = (PSET_FILE_ATTR)entry->Args;
			args->Return = DR_SetFileAttributes(
								args->FileName,
								args->FileAttributes,
								args->DokanFileInfo);
		}
		break;
				
	case SET_FILE_TIME_F:
		{
			PSET_FILE_TIME args = (PSET_FILE_TIME)entry->Args;
			args->Return = DR_SetFileTime(
								args->FileName,
								args->CreationTime,
								args->LastAccessTime,
								args->LastWriteTime,
								args->DokanFileInfo);
		}
		break;
				
	case DELETE_FILE_F:
		{
			PDELETE_FILE args = (PDELETE_FILE)entry->Args;
			args->Return = DR_DeleteFile(
								args->FileName,
								args->DokanFileInfo);
		}
		break;
				
	case DELETE_DIRECTORY_F:
		{
			PDELETE_DIRECTORY args = (PDELETE_DIRECTORY)entry->Args;
			args->Return = DR_DeleteDirectory(
								args->FileName,
								args->DokanFileInfo);
		}
		break;
		
	case MOVE_FILE_F:
		{
			PMOVE_FILE args = (PMOVE_FILE)entry->Args;
			args->Return = DR_MoveFile(
								args->FileName,
								args->NewFileName,
								args->ReplaceIfExisting,
								args->DokanFileInfo);
		}
		break;
				
	case SET_END_OF_FILE_F:
		{
			PSET_END_OF_FILE args = (PSET_END_OF_FILE)entry->Args;
			args->Return = DR_SetEndOfFile(
								args->FileName,
								args->ByteOffset,
								args->DokanFileInfo);
		}
		break;

	case LOCK_FILE_F:
		{
			PLOCK_FILE args = (PLOCK_FILE)entry->Args;
			args->Return = DR_LockFile(
								args->FileName,
								args->ByteOffset,
								args->Length,
								args->DokanFileInfo);
		}
		break;

	case UNLOCK_FILE_F:
		{
			PUNLOCK_FILE args = (PUNLOCK_FILE)entry->Args;
			args->Return = DR_UnlockFile(
								args->FileName,
								args->ByteOffset,
								args->Length,
								args->DokanFileInfo);
		}
		break;

	case UNMOUNT_F:
		{
			PUNMOUNT args = (PUNMOUNT)entry->Args;
			args->Return = DR_Unmount(
								args->DokanFileInfo);
		}
		break;

	case CLEANUP_F:
		{
			PCLEANUP args = (PCLEANUP)entry->Args;
			args->Return = DR_Cleanup(
								args->FileName,
								args->DokanFileInfo);
		}
		break;

	case FLUSH_F:
		{
			PFLUSH args = (PFLUSH)entry->Args;
			args->Return = DR_Flush(
								args->FileName,
								args->DokanFileInfo);
		}
		break;

	default:
		fprintf(stderr, "unknown dispatch\n");
		break;
	}

	SetEvent(entry->Event);
	return Qnil;
}

static int DOKAN_CALLBACK
DR_Unmount_dummy(
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	//fprintf(stderr, "Unmount dummy\n");
	return 0;
}

static VALUE
DR_dokan_unmount(
	 VALUE self)
{
	wchar_t letter;

	VALUE	drive = rb_iv_get(self, "@drive");

	StringValue(drive);

	mbtowc(&letter, RSTRING(drive)->ptr, 1);

	DokanUnmount(letter);

	return Qtrue;
}


static VALUE
DR_rescue(
	VALUE arg)
{
	PEVENT_ENTRY entry = CurrentEvent;

	SetEvent(entry->Event);

	// do not dispatch 'unmount'
	dokanOperations.Unmount = DR_Unmount_dummy;
	DR_dokan_unmount(g_cDokan);

	// save ruby exception
	// rethrow it after unmounting
	rb_gv_set("$dokan_exception", rb_gv_get("$!"));
	return Qtrue;
}


static VALUE
DR_rescue_unmount(
	VALUE arg)
{
	PEVENT_ENTRY entry = CurrentEvent;
	SetEvent(entry->Event);

	// save ruby exceptiton
	rb_gv_set("$dokan_exception", rb_gv_get("$!"));
	return Qtrue;
}


static int
DR_Dispatch()
{
	while(TRUE) {
		WaitForSingleObject(g_DispatchEvent, INFINITE);
		WaitForSingleObject(g_DispatchMutex, INFINITE);

		while(!IsListEmpty(&g_EventQueue)) {
			PLIST_ENTRY	listEntry = RemoveHeadList(&g_EventQueue);
			PEVENT_ENTRY entry = CONTAINING_RECORD(listEntry, EVENT_ENTRY, ListEntry);
			
			CurrentEvent = entry;

			if (entry->Function == UNMOUNT_F) {
				// when exception raised, DR_rescue is called
				// and DR_rescue returns Qtrue
				if (Qtrue == rb_rescue(DR_catch_exception, Qnil, DR_rescue_unmount, Qnil)) {
					//fprintf(stderr, "catch exception: unmount\n");
					ReleaseMutex(g_DispatchMutex);
					return -1;
				}
				ReleaseMutex(g_DispatchMutex);
				return 0;

			} else {	
				// when exception raised, DR_rescue is called and it returns Qtrue
				if (Qtrue == rb_rescue(DR_catch_exception, Qnil, DR_rescue, Qnil)) {
					//fprintf(stderr, "catch exception: method %u\n", entry->Function);
					ReleaseMutex(g_DispatchMutex);
					return -1;
				}
			}
		}
		ResetEvent(g_DispatchEvent);
		ReleaseMutex(g_DispatchMutex);
	}
}


static VALUE
DR_dokan_set_root(
	VALUE self,
	VALUE obj)
{
	g_DokanRoot = obj;

	rb_iv_set(self, "@root", obj);
	return Qtrue;
}


static VALUE
DR_dokan_mount_under(
	VALUE	self,
	VALUE	DriveLetter)
{
	StringValue(DriveLetter);

	rb_iv_set(self, "@drive", DriveLetter);

	return self;
}



static DWORD __stdcall
DR_Main(
	PVOID	Param)
{
	DokanMain((PDOKAN_OPTIONS)Param, &dokanOperations);
	return 0;
}



static VALUE
DR_dokan_run(
	VALUE self)
{
	wchar_t letter;
	HANDLE handle;
	ULONG	threadId;
	int status;
	PDOKAN_OPTIONS dokanOptions;

	VALUE	drive = rb_iv_get(self, "@drive");

	StringValue(drive);

	mbtowc(&letter, RSTRING(drive)->ptr, 1);

	if ( !(L'e' <= letter && letter <= L'z')
		&& !('E' <= letter && letter <= L'Z') ) {
		fprintf(stderr, "drive letter error");
		return Qfalse;
	}

	dokanOptions = (PDOKAN_OPTIONS)malloc(sizeof(DOKAN_OPTIONS));
	ZeroMemory(dokanOptions, sizeof(DOKAN_OPTIONS));

	dokanOptions->DriveLetter = letter;
	dokanOptions->ThreadCount = 0; // default
	dokanOptions->DebugMode = 0; // debug mode off

	rb_iv_set(self, "@@mounted", Qtrue);

	handle = (HANDLE)_beginthreadex(
				NULL,
				0,
				DR_Main,
				(PVOID)dokanOptions,
				0,
				&threadId);

	status = DR_Dispatch();

	Sleep(1000);
	//WaitForSingleObject(handle, INFINITE);

	if (status < 0) {
		//fprintf(stderr, "raise exception\n");
		//rb_gv_set("$!", rb_gv_get("$dokan_exception"));
		rb_exc_raise(rb_gv_get("$dokan_exception"));
	}

	free(dokanOptions);

	return Qtrue;
}


static VALUE
DR_dokan_mount(
	VALUE	self,
	VALUE	DriveLetter,
	VALUE	Root)
{
	StringValue(DriveLetter);

	rb_iv_set(self, "@drive", DriveLetter);
	rb_iv_set(self, "@root", Root);

	g_DokanRoot = Root;

	return DR_dokan_run(self);
}

static VALUE
DR_dokan_set_debug(
	VALUE	self,
	VALUE	isDebug)
{
	if(RTEST(isDebug))
		g_Debug = TRUE;
	else
		g_Debug = FALSE;

	return Qnil;
}

#define DR_define_const(klass, val) rb_define_const(klass, #val, INT2FIX(val))

void
Init_dokan_lib()
{
	InitializeListHead(&g_EventQueue);
	g_DispatchEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	g_DispatchMutex = CreateMutex(NULL, FALSE, NULL);

	setlocale(LC_ALL, "");

	g_cDokan = rb_define_module("Dokan");
	//rb_define_singleton_method(g_cDokan, "mount_under", DR_dokan_mount_under, 1);
	rb_define_singleton_method(g_cDokan, "mount", DR_dokan_mount, 2);
	//rb_define_singleton_method(g_cDokan, "run", DR_dokan_run, 0);
	//rb_define_singleton_method(g_cDokan, "set_root", DR_dokan_set_root, 1);
	//rb_define_singleton_method(g_cDokan, "root=", DR_dokan_set_root, 1);
	rb_define_singleton_method(g_cDokan, "unmount", DR_dokan_unmount, 0);
	
	rb_define_singleton_method(g_cDokan, "set_debug", DR_dokan_set_debug, 1);
	rb_define_singleton_method(g_cDokan, "debug=", DR_dokan_set_debug, 1);

	rb_define_const(g_cDokan, "READONLY", INT2FIX(FILE_ATTRIBUTE_READONLY));
	rb_define_const(g_cDokan, "HIDDEN", INT2FIX(FILE_ATTRIBUTE_HIDDEN));
	rb_define_const(g_cDokan, "SYSTEM", INT2FIX(FILE_ATTRIBUTE_SYSTEM));
	rb_define_const(g_cDokan, "DIRECTORY", INT2FIX(FILE_ATTRIBUTE_DIRECTORY));
	rb_define_const(g_cDokan, "ARCHIVE", INT2FIX(FILE_ATTRIBUTE_ARCHIVE));
	//DR_define_const(g_cDokan, FILE_ATTRIBUTE_DEVICE);
	rb_define_const(g_cDokan, "NORMAL", INT2FIX(FILE_ATTRIBUTE_NORMAL));

	rb_define_const(g_cDokan, "TEMPORARY", INT2FIX(FILE_ATTRIBUTE_TEMPORARY));
	rb_define_const(g_cDokan, "SPARSE_FILE", INT2FIX(FILE_ATTRIBUTE_SPARSE_FILE));
	rb_define_const(g_cDokan, "REPARSE_POINT", INT2FIX(FILE_ATTRIBUTE_REPARSE_POINT));
	rb_define_const(g_cDokan, "COMPRESSED", INT2FIX(FILE_ATTRIBUTE_COMPRESSED));

	rb_define_const(g_cDokan, "OFFLINE", INT2FIX(FILE_ATTRIBUTE_OFFLINE));
	rb_define_const(g_cDokan, "NOT_CONTENT_INDEXED", INT2FIX(FILE_ATTRIBUTE_NOT_CONTENT_INDEXED));
	rb_define_const(g_cDokan, "ENCRYPTED", INT2FIX(FILE_ATTRIBUTE_ENCRYPTED));

	DR_DefineFileInfoClass();

	rb_gv_set("$dokan_context", rb_ary_new());
}

