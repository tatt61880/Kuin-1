#include "file.h"

typedef struct SStream
{
	SClass Class;
	FILE* Handle;
	S64 DelimiterNum;
	Char* Delimiters;
} SStream;

static const U8 Newline[2] = { 0x0d, 0x0a };

static Char ReadUtf8(SStream* me_, Bool replace_delimiter);
static void WriteUtf8(SStream* me_, Char data);
static void NormPath(Char* path, Bool dir);
static void NormPathBackSlash(Char* path, Bool dir);
static Bool ForeachDirRecursion(const Char* path, Bool recursive, void* func);
static Bool DelDirRecursion(const Char* path);
static Bool CopyDirRecursion(const Char* dst, const Char* src);

EXPORT SClass* _makeReader(SClass* me_, const U8* path)
{
	THROWDBG(path == NULL, 0xc0000005);
	SStream* me2 = (SStream*)me_;
	FILE* file_ptr = _wfopen((Char*)(path + 0x10), L"rb");
	if (file_ptr == NULL)
		return NULL;
	me2->Handle = file_ptr;
	me2->DelimiterNum = 2;
	me2->Delimiters = (Char*)AllocMem(sizeof(Char) * 2);
	me2->Delimiters[0] = L' ';
	me2->Delimiters[1] = L',';
	return me_;
}

EXPORT SClass* _makeWriter(SClass* me_, const U8* path, Bool append)
{
	THROWDBG(path == NULL, 0xc0000005);
	SStream* me2 = (SStream*)me_;
	FILE* file_ptr = _wfopen((Char*)(path + 0x10), append ? L"ab" : L"wb");
	if (file_ptr == NULL)
		return NULL;
	me2->Handle = file_ptr;
	me2->DelimiterNum = 0;
	me2->Delimiters = NULL;
	return me_;
}

EXPORT void _streamDtor(SClass* me_)
{
	SStream* me2 = (SStream*)me_;
	if (me2->Handle != NULL)
		fclose(me2->Handle);
	if (me2->Delimiters != NULL)
		FreeMem(me2->Delimiters);
}

EXPORT void _streamFin(SClass* me_)
{
	SStream* me2 = (SStream*)me_;
#if defined(DBG)
	if (me2->Handle == NULL)
		THROW(0x1000, L"");
#endif
	fclose(me2->Handle);
	me2->Handle = NULL;
	if (me2->Delimiters != NULL)
	{
		FreeMem(me2->Delimiters);
		me2->Delimiters = NULL;
	}
}

EXPORT void _streamSetPos(SClass* me_, S64 origin, S64 pos)
{
	THROWDBG(origin < 0 || 2 < origin, 0xe9170006);
	SStream* me2 = (SStream*)me_;
#if defined(DBG)
	if (me2->Handle == NULL)
		THROW(0x1000, L"");
#endif
	if (_fseeki64(me2->Handle, pos, (int)origin))
		THROW(0x1000, L"");
}

EXPORT S64 _streamGetPos(SClass* me_)
{
	SStream* me2 = (SStream*)me_;
#if defined(DBG)
	if (me2->Handle == NULL)
		THROW(0x1000, L"");
#endif
	return _ftelli64(me2->Handle);
}

EXPORT void _streamDelimiter(SClass* me_, const U8* delimiters)
{
	SStream* me2 = (SStream*)me_;
	S64 len = *(S64*)(delimiters + 0x08);
	S64 i;
	const Char* ptr = (const Char*)(delimiters + 0x10);
	FreeMem(me2->Delimiters);
	me2->DelimiterNum = len;
	me2->Delimiters = (Char*)AllocMem(sizeof(Char) * (size_t)len);
	for (i = 0; i < len; i++)
		me2->Delimiters[i] = ptr[i];
}

EXPORT void* _streamRead(SClass* me_, S64 size)
{
	SStream* me2 = (SStream*)me_;
#if defined(DBG)
	if (me2->Handle == NULL)
		THROW(0x1000, L"");
#endif
	{
		U8* result = (U8*)AllocMem(0x10 + (size_t)size);
		size_t size2;
		((S64*)result)[0] = DefaultRefCntFunc;
		((S64*)result)[1] = size;
		size2 = fread(result + 0x10, 1, (size_t)size, me2->Handle);
		if (size2 != (size_t)size)
		{
			FreeMem(result);
			return NULL;
		}
		return result;
	}
}

EXPORT Char _streamReadLetter(SClass* me_)
{
	return ReadUtf8((SStream*)me_, False);
}

EXPORT S64 _streamReadInt(SClass* me_)
{
	Char buf[33];
	int ptr = 0;
	buf[0] = L'\0';
	for (; ; )
	{
		Char c = ReadUtf8((SStream*)me_, True);
		if (c == WEOF)
		{
			if (buf[0] == L'\0')
				THROWDBG(True);
			break;
		}
		if (c == L'\0')
		{
			if (buf[0] == L'\0')
				continue;
			break;
		}
		if (ptr == 32)
			THROWDBG(True);
		buf[ptr] = c;
		ptr++;
	}
	for (; ; )
	{
		Char c = ReadUtf8((SStream*)me_, True);
		if (c == WEOF)
			break;
		if (c != L'\0')
		{
			_fseeki64(((SStream*)me_)->Handle, -1, SEEK_CUR);
			break;
		}
	}
	buf[ptr] = L'\0';
	{
		S64 result;
		errno = 0;
		result = _wtoi64(buf);
		if (errno != 0)
			THROW(0x1000, L"");
		return result;
	}
}

EXPORT double _streamReadFloat(SClass* me_)
{
	Char buf[33];
	int ptr = 0;
	buf[0] = L'\0';
	for (; ; )
	{
		Char c = ReadUtf8((SStream*)me_, True);
		if (c == WEOF)
		{
			if (buf[0] == L'\0')
				THROWDBG(True);
			break;
		}
		if (c == L'\0')
		{
			if (buf[0] == L'\0')
				continue;
			break;
		}
		if (ptr == 32)
			THROWDBG(True);
		buf[ptr] = c;
		ptr++;
	}
	for (; ; )
	{
		Char c = ReadUtf8((SStream*)me_, True);
		if (c == WEOF)
			break;
		if (c != L'\0')
		{
			_fseeki64(((SStream*)me_)->Handle, -1, SEEK_CUR);
			break;
		}
	}
	buf[ptr] = L'\0';
	{
		double result;
		errno = 0;
		result = _wtof(buf);
		if (errno != 0)
			THROW(0x1000, L"");
		return result;
	}
}

EXPORT Char _streamReadChar(SClass* me_)
{
	Char c = L'\0';
	for (; ; )
	{
		c = ReadUtf8((SStream*)me_, True);
		if (c == WEOF)
			THROWDBG(True);
		if (c != L'\0')
			break;
	}
	for (; ; )
	{
		Char c2 = ReadUtf8((SStream*)me_, True);
		if (c2 == WEOF)
			break;
		if (c2 != L'\0')
		{
			_fseeki64(((SStream*)me_)->Handle, -1, SEEK_CUR);
			break;
		}
	}
	return c;
}

EXPORT void* _streamReadStr(SClass* me_)
{
	Char buf[1025];
	int ptr = 0;
	buf[0] = L'\0';
	for (; ; )
	{
		Char c = ReadUtf8((SStream*)me_, True);
		if (c == WEOF)
		{
			if (buf[0] == L'\0')
				THROWDBG(True);
			break;
		}
		if (c == L'\0')
		{
			if (buf[0] == L'\0')
				continue;
			break;
		}
		buf[ptr] = c;
		ptr++;
		if (ptr == 1024)
			break;
	}
	for (; ; )
	{
		Char c = ReadUtf8((SStream*)me_, True);
		if (c == WEOF)
			break;
		if (c != L'\0')
		{
			_fseeki64(((SStream*)me_)->Handle, -1, SEEK_CUR);
			break;
		}
	}
	buf[ptr] = L'\0';
	{
		U8* result = (U8*)AllocMem(0x10 + sizeof(Char) * ((size_t)ptr + 1));
		*(S64*)(result + 0x00) = DefaultRefCntFunc;
		*(S64*)(result + 0x08) = (S64)ptr;
		wcscpy((Char*)(result + 0x10), buf);
		return result;
	}
}

EXPORT void* _streamReadLine(SClass* me_)
{
	Char buf[1025];
	int ptr = 0;
	buf[0] = L'\0';
	for (; ; )
	{
		Char c = ReadUtf8((SStream*)me_, False);
		if (c == L'\r')
			continue;
		if (c == WEOF || c == L'\n')
			break;
		buf[ptr] = c;
		ptr++;
		if (ptr == 1024)
			break;
	}
	buf[ptr] = L'\0';
	{
		U8* result = (U8*)AllocMem(0x10 + sizeof(Char) * ((size_t)ptr + 1));
		*(S64*)(result + 0x00) = DefaultRefCntFunc;
		*(S64*)(result + 0x08) = (S64)ptr;
		wcscpy((Char*)(result + 0x10), buf);
		return result;
	}
}

EXPORT void _streamWrite(SClass* me_, void* bin)
{
	SStream* me2 = (SStream*)me_;
#if defined(DBG)
	if (me2->Handle == NULL)
		THROW(0x1000, L"");
#endif
	{
		U8* bin2 = (U8*)bin;
		fwrite(bin2 + 0x10, 1, (size_t)*(S64*)(bin2 + 0x08), me2->Handle);
	}
}

EXPORT void _streamWriteInt(SClass* me_, S64 n)
{
	Char str[33];
	int len = swprintf(str, 33, L"%I64d", n);
	int i;
	for (i = 0; i < len; i++)
		WriteUtf8((SStream*)me_, str[i]);
}

EXPORT void _streamWriteFloat(SClass* me_, double n)
{
	Char str[33];
	int len = swprintf(str, 33, L"%g", n);
	int i;
	for (i = 0; i < len; i++)
		WriteUtf8((SStream*)me_, str[i]);
}

EXPORT void _streamWriteChar(SClass* me_, Char n)
{
	WriteUtf8((SStream*)me_, n);
}

EXPORT void _streamWriteStr(SClass* me_, const U8* n)
{
	const Char* ptr = (const Char*)(n + 0x10);
	while (*ptr != L'\0')
	{
		WriteUtf8((SStream*)me_, *ptr);
		ptr++;
	}
}

EXPORT S64 _streamFileSize(SClass* me_)
{
	SStream* me2 = (SStream*)me_;
#if defined(DBG)
	if (me2->Handle == NULL)
		THROW(0x1000, L"");
#endif
	{
		S64 current = _ftelli64(me2->Handle);
		S64 result;
		_fseeki64(me2->Handle, 0, SEEK_END);
		result = _ftelli64(me2->Handle);
		_fseeki64(me2->Handle, current, SEEK_SET);
		return result;
	}
}

EXPORT Bool _streamTerm(SClass* me_)
{
	SStream* me2 = (SStream*)me_;
#if defined(DBG)
	if (me2->Handle == NULL)
		THROW(0x1000, L"");
#endif
	{
		Bool result = fgetc(me2->Handle) == EOF;
		if (!result)
			_fseeki64(me2->Handle, -1, SEEK_CUR);
		return result;
	}
}

EXPORT Bool _makeDir(const U8* path)
{
	THROWDBG(path == NULL, 0xc0000005);
	THROWDBG(*(S64*)(path + 0x08) > MAX_PATH, 0xe9170006);
	if (!DelDirRecursion((const Char*)(path + 0x10)))
		return False;
	{
		Char path2[MAX_PATH + 2];
		wcscpy(path2, (const Char*)(path + 0x10));
		NormPathBackSlash(path2, True);
		return SHCreateDirectory(NULL, path2) == ERROR_SUCCESS;
	}
}

EXPORT void _foreachDir(const U8* path, Bool recursive, void* func)
{
	if (!ForeachDirRecursion((const Char*)(path + 0x10), recursive, func))
		THROW(0x1000, L"");
}

EXPORT Bool _existPath(const U8* path)
{
	THROWDBG(path == NULL, 0xc0000005);
	return PathFileExists((const Char*)(path + 0x10)) != 0;
}

EXPORT Bool _delDir(const U8* path)
{
	THROWDBG(path == NULL, 0xc0000005);
	return DelDirRecursion((const Char*)(path + 0x10)) != 0;
}

EXPORT Bool _delFile(const U8* path)
{
	THROWDBG(path == NULL, 0xc0000005);
	if (!PathFileExists(path))
		return True;
	return DeleteFile((const Char*)(path + 0x10)) != 0;
}

EXPORT Bool _copyDir(const U8* dst, const U8* src)
{
	THROWDBG(dst == NULL, 0xc0000005);
	THROWDBG(src == NULL, 0xc0000005);
	return CopyDirRecursion((const Char*)(dst + 0x10), (const Char*)(src + 0x10)) != 0;
}

EXPORT Bool _copyFile(const U8* dst, const U8* src)
{
	THROWDBG(dst == NULL, 0xc0000005);
	THROWDBG(src == NULL, 0xc0000005);
	return CopyFile((const Char*)(src + 0x10), (const Char*)(dst + 0x10), FALSE) != 0;
}

EXPORT Bool _moveDir(const U8* dst, const U8* src)
{
	// TODO:
}

EXPORT Bool _moveFile(const U8* dst, const U8* src)
{
	THROWDBG(dst == NULL, 0xc0000005);
	THROWDBG(src == NULL, 0xc0000005);
	return MoveFileEx((const Char*)(src + 0x10), (const Char*)(dst + 0x10), MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH | MOVEFILE_REPLACE_EXISTING) != 0;
}

EXPORT void* _dir(const U8* path)
{
	THROWDBG(path == NULL, 0xc0000005);
	const Char* path2 = (const Char*)(path + 0x10);
	size_t len = wcslen(path2);
	U8* result;
	const Char* ptr = path2 + len;
	while (ptr != path2 && *ptr != L'\\' && *ptr != L'/')
		ptr--;
	if (ptr == path2)
	{
		result = (U8*)AllocMem(0x10 + sizeof(Char) * 3);
		*(S64*)(result + 0x00) = DefaultRefCntFunc;
		*(S64*)(result + 0x08) = 2;
		wcscpy((Char*)(result + 0x10), L"./");
	}
	else
	{
		size_t len2 = ptr - path2 + 1;
		size_t i;
		Char* str;
		result = (U8*)AllocMem(0x10 + sizeof(Char) * (len2 + 1));
		*(S64*)(result + 0x00) = DefaultRefCntFunc;
		*(S64*)(result + 0x08) = len2;
		str = (Char*)(result + 0x10);
		for (i = 0; i < len2; i++)
			str[i] = path2[i] == L'\\' ? L'/' : path2[i];
		str[i] = L'\0';
	}
	return result;
}

EXPORT void* _ext(const U8* path)
{
	// TODO:
	return NULL;
}

EXPORT void* _fileName(const U8* path)
{
	THROWDBG(path == NULL, 0xc0000005);
	const Char* path2 = (const Char*)(path + 0x10);
	size_t len = wcslen(path2);
	U8* result;
	const Char* ptr = path2 + len;
	const Char* ptr2 = ptr;
	while (ptr != path2 && *ptr != L'\\' && *ptr != L'/')
		ptr--;
	if (ptr == path2)
		return (void*)path;
	ptr++;
	{
		size_t len2 = ptr2 + 1 - ptr;
		size_t i;
		Char* str;
		result = (U8*)AllocMem(0x10 + sizeof(Char) * (len2 + 1));
		*(S64*)(result + 0x00) = DefaultRefCntFunc;
		*(S64*)(result + 0x08) = len2;
		str = (Char*)(result + 0x10);
		for (i = 0; i < len2; i++)
			str[i] = ptr[i];
		str[i] = L'\0';
	}
	return result;
}

EXPORT void* _fullPath(const U8* path)
{
	// TODO:
	return NULL;
}

EXPORT void* _delExt(const U8* path)
{
	THROWDBG(path == NULL, 0xc0000005);
	const Char* path2 = (const Char*)(path + 0x10);
	size_t len = wcslen(path2);
	U8* result;
	const Char* ptr = path2 + len;
	while (ptr != path2 && *ptr != L'\\' && *ptr != L'/' && *ptr != L'.')
		ptr--;
	if (ptr == path2 || *ptr != L'.')
		return (void*)path;
	{
		size_t len2 = ptr - path2;
		size_t i;
		Char* str;
		result = (U8*)AllocMem(0x10 + sizeof(Char) * (len2 + 1));
		*(S64*)(result + 0x00) = DefaultRefCntFunc;
		*(S64*)(result + 0x08) = len2;
		str = (Char*)(result + 0x10);
		for (i = 0; i < len2; i++)
			str[i] = path2[i] == L'\\' ? L'/' : path2[i];
		str[i] = L'\0';
	}
	return result;
}

EXPORT void* _tmpFile(void)
{
	// TODO:
	return NULL;
}

EXPORT void* _sysDir(S64 kind)
{
	Char path[MAX_PATH + 2];
	if (!SHGetSpecialFolderPath(NULL, path, (int)kind, TRUE))
		return NULL;
	NormPath(path, True);
	{
		size_t len = wcslen(path);
		U8* result = (U8*)AllocMem(0x10 + sizeof(Char) * (len + 1));
		*(S64*)(result + 0x00) = DefaultRefCntFunc;
		*(S64*)(result + 0x08) = (S64)len;
		wcscpy((Char*)(result + 0x10), path);
		return result;
	}
}

EXPORT void* _exeDir(void)
{
	Char path[MAX_PATH + 1];
	if (!GetModuleFileName(NULL, path, MAX_PATH))
		THROW(0x1000, L"");
	{
		size_t len = wcslen(path);
		U8* result;
		const Char* ptr = path + len;
		while (ptr != path && *ptr != L'\\' && *ptr != L'/')
			ptr--;
		if (ptr == path)
			THROW(0x1000, L"");
		{
			size_t len2 = ptr - path + 1;
			size_t i;
			Char* str;
			result = (U8*)AllocMem(0x10 + sizeof(Char) * (len2 + 1));
			*(S64*)(result + 0x00) = DefaultRefCntFunc;
			*(S64*)(result + 0x08) = len2;
			str = (Char*)(result + 0x10);
			for (i = 0; i < len2; i++)
				str[i] = path[i] == L'\\' ? L'/' : path[i];
			str[i] = L'\0';
		}
		return result;
	}
}

EXPORT S64 _fileSize(const U8* path)
{
	THROWDBG(path == NULL, 0xc0000005);
	S64 result;
	FILE* file_ptr = _wfopen((const Char*)(path + 0x10), L"rb");
	if (file_ptr == NULL)
		THROW(0xe9170007);
	_fseeki64(file_ptr, 0, SEEK_END);
	result = _ftelli64(file_ptr);
	fclose(file_ptr);
	return result;
}

static Char ReadUtf8(SStream* me_, Bool replace_delimiter)
{
	U8 c;
	int len;
	U64 u;
	Char u2;
	for (; ; )
	{
		int c2 = fgetc(me_->Handle);
		if (c2 == EOF)
			return WEOF;
		c = (U8)c2;
		if ((c & 0xc0) == 0x80)
			continue;
		if ((c & 0x80) == 0x00)
			len = 0;
		else if ((c & 0xe0) == 0xc0)
		{
			len = 1;
			c &= 0x1f;
		}
		else if ((c & 0xf0) == 0xe0)
		{
			len = 2;
			c &= 0x0f;
		}
		else if ((c & 0xf8) == 0xf0)
		{
			len = 3;
			c &= 0x07;
		}
		else if ((c & 0xfc) == 0xf8)
		{
			len = 4;
			c &= 0x03;
		}
		else if ((c & 0xfe) == 0xfc)
		{
			len = 5;
			c &= 0x01;
		}
		else
			continue; // TODO:
		break;
	}
	u = (U64)c;
	{
		int i;
		for (i = 0; i < len; i++)
		{
			if (fread(&c, 1, 1, me_->Handle) != 1 || (c & 0xc0) != 0x80)
				THROWDBG(True);
			u = (u << 6) | (c & 0x3f);
		}
	}
	if (0x00010000 <= u && u <= 0x0010ffff)
		u = 0x20;
	u2 = (Char)u;
	if (!replace_delimiter)
		return u2;
	if (u2 == L'\0' || u2 == L'\r' || u2 == L'\n')
		return L'\0';
	{
		S64 i;
		for (i = 0; i < me_->DelimiterNum; i++)
		{
			if (u2 == me_->Delimiters[i])
				return L'\0';
		}
	}
	return u2;
}

static void WriteUtf8(SStream* me_, Char data)
{
	U64 u;
	size_t size;
	if ((data >> 7) == 0)
	{
		u = data;
		size = 1;
	}
	else
	{
		u = (0x80 | (data & 0x3f)) << 8;
		data >>= 6;
		if ((data >> 5) == 0)
		{
			u |= 0xc0 | data;
			size = 2;
		}
		else
		{
			u = (u | 0x80 | (data & 0x3f)) << 8;
			data >>= 6;
			if ((data >> 4) == 0)
			{
				u |= 0xe0 | data;
				size = 3;
			}
			else
			{
				u = (u | 0x80 | (data & 0x3f)) << 8;
				data >>= 6;
				if ((data >> 3) == 0)
				{
					u |= 0xf0 | data;
					size = 4;
				}
				else
				{
					u = (u | 0x80 | (data & 0x3f)) << 8;
					data >>= 6;
					if ((data >> 2) == 0)
					{
						u |= 0xf8 | data;
						size = 5;
					}
					else
					{
						u = (u | 0x80 | (data & 0x3f)) << 8;
						data >>= 6;
						if ((data >> 1) == 0)
						{
							u |= 0xfc | data;
							size = 6;
						}
						else
							return; // TODO:
					}
				}
			}
		}
	}
	if (size == 1 && u == 0x0a)
		fwrite(&Newline, 1, sizeof(Newline), me_->Handle);
	else
		fwrite(&u, 1, size, me_->Handle);
}

static void NormPath(Char* path, Bool dir)
{
	Char* ptr = path;
	if (*ptr == L'\0')
		return;
	do
	{
		if (*ptr == L'\\')
			*ptr = L'/';
		ptr++;
	} while (*ptr != L'\0');
	if (dir && ptr[-1] != L'/')
	{
		ptr[0] = L'/';
		ptr[1] = L'\0';
	}
}

static void NormPathBackSlash(Char* path, Bool dir)
{
	Char* ptr = path;
	if (*ptr == L'\0')
		return;
	do
	{
		if (*ptr == L'/')
			*ptr = L'\\';
		ptr++;
	} while (*ptr != L'\0');
	if (dir && ptr[-1] != L'\\')
	{
		ptr[0] = L'\\';
		ptr[1] = L'\0';
	}
}

static Bool ForeachDirRecursion(const Char* path, Bool recursive, void* func)
{
	Char path2[MAX_PATH + 1];
	if (wcslen(path) > MAX_PATH)
		return False;
	if (!PathFileExists(path))
		return False;
	wcscpy(path2, path);
	wcscat(path2, L"*");
	{
		WIN32_FIND_DATA find_data;
		HANDLE handle = FindFirstFile(path2, &find_data);
		if (handle == INVALID_HANDLE_VALUE)
			return False;
		do
		{
			if (wcscmp(find_data.cFileName, L".") == 0 || wcscmp(find_data.cFileName, L"..") == 0)
				continue;
			{
				wcscpy(path2, path);
				wcscat(path2, find_data.cFileName);
				if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
				{
					if (recursive)
					{
						wcscat(path2, L"/");
						if (!ForeachDirRecursion(path2, recursive, func))
						{
							FindClose(handle);
							return False;
						}
					}
				}
				else
				{
					// TODO: call
				}
			}
		} while (FindNextFile(handle, &find_data));
		FindClose(handle);
	}
	return True;
}

static Bool DelDirRecursion(const Char* path)
{
	Char path2[MAX_PATH + 1];
	if (wcslen(path) > MAX_PATH)
		return False;
	if (!PathFileExists(path))
		return True;
	wcscpy(path2, path);
	wcscat(path2, L"*");
	{
		WIN32_FIND_DATA find_data;
		HANDLE handle = FindFirstFile(path2, &find_data);
		if (handle == INVALID_HANDLE_VALUE)
			return False;
		do
		{
			if (wcscmp(find_data.cFileName, L".") == 0 || wcscmp(find_data.cFileName, L"..") == 0)
				continue;
			{
				wcscpy(path2, path);
				wcscat(path2, find_data.cFileName);
				if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
				{
					wcscat(path2, L"/");
					if (!DelDirRecursion(path2))
					{
						FindClose(handle);
						return False;
					}
				}
				else
				{
					if (DeleteFile(path2) == 0)
					{
						FindClose(handle);
						return False;
					}
				}
			}
		} while (FindNextFile(handle, &find_data));
		FindClose(handle);
	}
	return RemoveDirectory(path) != 0;
}

static Bool CopyDirRecursion(const Char* dst, const Char* src)
{
	Char src2[MAX_PATH + 1];
	Char dst2[MAX_PATH + 1];
	if (wcslen(src) > MAX_PATH)
		return False;
	if (!PathFileExists(src))
		return False;
	CreateDirectory(dst, NULL);
	wcscpy(src2, src);
	wcscat(src2, L"*");
	{
		WIN32_FIND_DATA find_data;
		HANDLE handle = FindFirstFile(src2, &find_data);
		if (handle == INVALID_HANDLE_VALUE)
			return False;
		do
		{
			if (wcscmp(find_data.cFileName, L".") == 0 || wcscmp(find_data.cFileName, L"..") == 0)
				continue;
			{
				wcscpy(src2, src);
				wcscat(src2, find_data.cFileName);
				wcscpy(dst2, dst);
				wcscat(dst2, find_data.cFileName);
				if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
				{
					wcscat(src2, L"/");
					wcscat(dst2, L"/");
					if (!CopyDirRecursion(dst2, src2))
					{
						FindClose(handle);
						return False;
					}
				}
				else
				{
					if (!CopyFile(src2, dst2, FALSE))
					{
						FindClose(handle);
						return False;
					}
				}
			}
		} while (FindNextFile(handle, &find_data));
		FindClose(handle);
	}
	return True;
}
