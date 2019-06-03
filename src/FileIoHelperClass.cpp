/**
 * @file    FileIoHelperClass
 * @brief
 *
 * This file contains test code for `FileIoHelper` class.
 *
 * @author  Yonhgwhan, Roh (fixbrain@gmail.com)
 * @date    2011.10.13 created.
 * @copyright All rights reserved by Yonghwan, Roh.
**/
#include "stdafx.h"

#include "Win32Utils.h"
#include "FileIoHelperClass.h"

FileIoHelper::FileIoHelper()
	:
	_do_not_close_handle(true),
	_read_only(true),
	_file_handle(INVALID_HANDLE_VALUE),
	_file_size(0),
	_map_handle(nullptr),
	_file_view(nullptr)
{
}

FileIoHelper::~FileIoHelper()
{
	this->close();
}

/// @brief	I/O �� ����ȭ�� ������� �����Ѵ�. 
///			��Ȯ�� ��ġ�� �𸣰����� SYSTEM_INFO.dwAllocationGranularity(64k) * 8 ������ ���� 
///			������ ������ �����°� ����. (win7, win10 ���� �׽�Ʈ)
//static
uint32_t FileIoHelper::GetOptimizedBlockSize()
{
	static DWORD AllocationGranularity = 0;
	if (0 == AllocationGranularity)
	{
		SYSTEM_INFO si = { 0 };
		GetSystemInfo(&si);
		AllocationGranularity = si.dwAllocationGranularity;
	}

	if (0 == AllocationGranularity)
	{
		return (64 * 1024) * 8;
	}
	else
	{
		return AllocationGranularity * 8;
	}
}

/// @brief	������ �б���� �����Ѵ�. 
bool
FileIoHelper::OpenForRead(_In_ const wchar_t* file_path)
{
	_ASSERTE(nullptr != file_path);
	if (nullptr == file_path) return false;

	if (true != is_file_existsW(file_path))
	{
		log_err "no file exists. file=%ws",
			file_path
			log_end;
		return false;
	}

	if (true == Initialized()) { close(); }

	HANDLE file_handle = CreateFileW(file_path,
									 GENERIC_READ,
									 0,
									 nullptr,
									 OPEN_EXISTING,
									 FILE_ATTRIBUTE_NORMAL,
									 nullptr);
	if (INVALID_HANDLE_VALUE == file_handle)
	{
		log_err
			"CreateFile() failed, file=%ws, gle=%u",
			file_path,
			GetLastError()
			log_end
			return false;
	}

	_do_not_close_handle = false; //<!

	if (true != OpenForRead(file_handle))
	{
		CloseHandle(file_handle);
		return false;
	}

	_ASSERTE(_file_handle == file_handle);
	_ASSERTE(_read_only == true);
	_ASSERTE(_map_handle != nullptr);
	return true;
}

/// @brief	������ �б���� �����Ѵ�. 
bool
FileIoHelper::OpenForRead(
	_In_ const HANDLE file_handle
)
{
	_ASSERTE(INVALID_HANDLE_VALUE != file_handle);
	if (INVALID_HANDLE_VALUE == file_handle)
	{
		return false;
	}

	if (true == Initialized())
	{
		close();
	}

	uint64_t file_size = 0;
	HANDLE file_map = nullptr;

#pragma warning(disable: 4127)
	bool ret = false;
	do
	{
		// check file size 
		// 
		if (!GetFileSizeEx(file_handle, (PLARGE_INTEGER)&file_size))
		{
			log_err
				"GetFileSizeEx() failed. file_handle=0x%p, gle=%u",
				file_handle,
				GetLastError()
				log_end
				break;
		}

		if (file_size == 0)
		{
			log_err "Empty file specified." log_end;
			break;
		}

		file_map = CreateFileMapping(file_handle,
									 nullptr,
									 PAGE_READONLY,
									 0,
									 0,
									 nullptr);
		if (nullptr == file_map)
		{
			log_err
				"CreateFileMapping() failed, file_handle=0x%p, gle=%u",
				file_handle,
				GetLastError()
				log_end
				break;
		}

		ret = true;
	} while (false);
#pragma warning(default: 4127)

	if (true != ret)
	{
		if (nullptr != file_map)
		{
			CloseHandle(file_map);
		}
	}
	else
	{
		_read_only = true;
		_file_size = file_size;
		_file_handle = file_handle;
		_map_handle = file_map;
	}
	return ret;
}

/// @brief	file_size ����Ʈ ¥�� ������ �����Ѵ�.
bool
FileIoHelper::OpenForWrite(
	_In_ const wchar_t* file_path,
	_In_ uint64_t file_size
)
{
	if (true == Initialized()) { close(); }
	if (file_size == 0) return false;

	_read_only = false;

#pragma warning(disable: 4127)
	bool ret = false;
	do
	{
		_file_size = file_size;
		_file_handle = CreateFileW(file_path,
								   GENERIC_READ | GENERIC_WRITE,
								   FILE_SHARE_READ,		// write ���� �ٸ� ���μ������� �бⰡ ����
								   nullptr,
								   CREATE_ALWAYS,
								   FILE_ATTRIBUTE_NORMAL,
								   nullptr);
		if (INVALID_HANDLE_VALUE == _file_handle)
		{
			log_err
				"CreateFile() failed, file=%ws, gle=%u",
				file_path,
				GetLastError()
				log_end
				break;
		}

		_do_not_close_handle = false;	//<!

		//
		//	��û�� ũ�⸸ŭ ���ϻ���� �ø���.
		// 
		if (!SetFilePointerEx(_file_handle,
							  *(PLARGE_INTEGER)&_file_size,
							  nullptr,
							  FILE_BEGIN))
		{
			log_err
				"SetFilePointerEx() failed, file=%ws, size=%llu, gle=%u",
				file_path,
				file_size,
				GetLastError()
				log_end
				break;
		}

		if (!SetEndOfFile(_file_handle))
		{
			log_err "SetEndOfFile() failed, file=%ws, gle=%u",
				file_path,
				GetLastError()
				log_end
				break;
		}

		_map_handle = CreateFileMapping(_file_handle,
										nullptr,
										PAGE_READWRITE,
										0,
										0,
										nullptr);
		if (nullptr == _map_handle)
		{
			log_err
				"CreateFileMapping() failed, file=%ws, gle=%u",
				file_path,
				GetLastError()
				log_end
				break;
		}

		ret = true;
	} while (false);
#pragma warning(default: 4127)

	if (true != ret)
	{
		if (nullptr != _map_handle) CloseHandle(_map_handle);

		if (INVALID_HANDLE_VALUE != _file_handle)
		{
			CloseHandle(_file_handle);
			_file_handle = INVALID_HANDLE_VALUE;
		}

	}

	return ret;
}

/// @brief	������ �б�/������� �����Ѵ�. 
bool FileIoHelper::OpenForReadWrite(_In_ const wchar_t* file_path)
{
	_ASSERTE(nullptr != file_path);
	if (nullptr == file_path) return false;

	if (true != is_file_existsW(file_path))
	{
		log_err "no file exists. file=%ws",
			file_path
			log_end;
		return false;
	}

	if (true == Initialized()) { close(); }

	HANDLE file_handle = CreateFileW(file_path,
									 GENERIC_READ | GENERIC_WRITE,
									 0,
									 nullptr,
									 OPEN_EXISTING,
									 FILE_ATTRIBUTE_NORMAL,
									 nullptr);
	if (INVALID_HANDLE_VALUE == file_handle)
	{
		log_err
			"CreateFile() failed, file=%ws, gle=%u",
			file_path,
			GetLastError()
			log_end
			return false;
	}

	_do_not_close_handle = false;	//<!

	if (true != OpenForReadWrite(file_handle))
	{
		CloseHandle(file_handle);
		return false;
	}

	_ASSERTE(_file_handle == file_handle);
	_ASSERTE(_read_only == false);
	_ASSERTE(_map_handle != nullptr);
	return true;
}

/// @brief	������ �б�/������� �����Ѵ�. 
bool
FileIoHelper::OpenForReadWrite(
	_In_ const HANDLE file_handle
)
{
	_ASSERTE(INVALID_HANDLE_VALUE != file_handle);
	if (INVALID_HANDLE_VALUE == file_handle)
	{
		return false;
	}

	if (true == Initialized()) { close(); }

	uint64_t file_size = 0;
	HANDLE file_map = nullptr;

#pragma warning(disable: 4127)
	bool ret = false;
	do
	{
		// check file size 
		// 
		if (!GetFileSizeEx(file_handle, (PLARGE_INTEGER)&file_size))
		{
			log_err
				"GetFileSizeEx() failed. file_handle=0x%p, gle=%u",
				file_handle,
				GetLastError()
				log_end
				break;
		}

		if (file_size == 0)
		{
			log_err "Empty file specified." log_end;
			break;
		}

		file_map = CreateFileMapping(file_handle,
									 nullptr,
									 PAGE_READWRITE,
									 0,
									 0,
									 nullptr);
		if (nullptr == file_map)
		{
			log_err
				"CreateFileMapping() failed, file_handle=0x%p, gle=%u",
				file_handle,
				GetLastError()
				log_end
				break;
		}

		ret = true;
	} while (false);
#pragma warning(default: 4127)

	if (true != ret)
	{
		if (nullptr != file_map)
		{
			CloseHandle(file_map);
		}
	}
	else
	{
		_read_only = false;
		_file_size = file_size;
		_file_handle = file_handle;
		_map_handle = file_map;
	}
	return ret;
}

/// @brief	��� ���ҽ��� �����Ѵ�.
void FileIoHelper::close()
{
	if (true != Initialized()) return;

	ReleaseFilePointer();
	if (nullptr != _map_handle)
	{
		CloseHandle(_map_handle);
		_map_handle = nullptr;
	}

	if (true != _do_not_close_handle && INVALID_HANDLE_VALUE != _file_handle)
	{
		CloseHandle(_file_handle);
	}
	_file_handle = INVALID_HANDLE_VALUE;
}

/// @beief	�޸� ���ε� ������ ��θ� ���Ѵ�. 
///			convert_to_dosname == false �� ��� 
///				\Device\harddiskVolume1\Windows\System32\notepad.exe ��� ����
///
///			convert_to_dosname == true �� ��� ����̽� ������ ���������� ����
///				c:\Windows\System32\notepad.exe ��� ����
bool FileIoHelper::GetMappedFileName(_In_ bool convet_to_dosname, _Out_ std::wstring& file_path)
{
	if (true != Initialized()) return false;
	if (nullptr != _file_view)
	{
		log_err "mapped file pointer is busy. " log_end;
		return false;
	}

	uint8_t* ptr = GetFilePointer(true, 0, 8);
	if (nullptr == ptr)
	{
		log_err "GetFilePointer() failed." log_end;
		return false;
	}


	bool ret = false;
	do
	{
		std::wstring nt_file_name;
		if (true != get_mapped_file_name(GetCurrentProcess(), ptr, nt_file_name))
		{
			log_err "get_mapped_file_name() failed." log_end;
			break;
		}

		if (true == convet_to_dosname)
		{
			std::wstring dos_file_name;
			if (!nt_name_to_dos_name(nt_file_name.c_str(), dos_file_name))
			{
				log_err "nt_name_to_dos_name() failed. nt_name=%ws",
					nt_file_name.c_str()
					log_end;
				break;
			}

			file_path = dos_file_name;
		}
		else
		{
			file_path = nt_file_name;
		}

		ret = true;
	} while (false);

	ReleaseFilePointer();
	return ret;

}

/// @brief	������ ������ Offset ��ġ�� Size ��ŭ �����ϰ�, �ش� �޸� ������ �����Ѵ�.
///			Offset �� SYSTEM_INFO.dwAllocationGranularity �� ����� �����ؾ� �Ѵ�. 
///			�׷��� ���� ��� �ڵ����� SYSTEM_INFO.dwAllocationGranularity ������ �����ؼ�
///			������ �����ϰ�, pointer �� ������ �����ؼ� �����Ѵ�.
uint8_t*
FileIoHelper::GetFilePointer(
	_In_ bool read_only,
	_In_ uint64_t Offset,
	_In_ uint32_t Size
)
{
	_ASSERTE(nullptr == _file_view);
	if (nullptr != _file_view)
	{
		log_err "ReleaseFilePointer() first!" log_end;
		return nullptr;
	}

	if (true != Initialized()) return false;
	if (IsReadOnly() && !read_only)
	{
		log_err "file mapped read only." log_end;
		return nullptr;
	}

	// 
	//	��û�� offset �� ���� ������� ũ�ٸ� ������ �����Ѵ�.
	// 
	if (Offset >= _file_size)
	{
		log_err "Req offset > File size. req offset=0x%llx, file size=0x%llx",
			Offset,
			_file_size
			log_end;
		return nullptr;
	}

	//
	//	��û�� ����� ���ϻ������ ũ�ٸ� ���ϻ����ŭ�� �����Ѵ�. 
	//
	uint32_t adjusted_size = Size;
	if (Offset + Size > _file_size)
	{
		adjusted_size = (uint32_t)(_file_size - Offset);
	}

	//
	//	MapViewOfFile() �Լ��� dwFileOffsetLow �Ķ���ʹ� 
	//	SYSTEM_INFO::dwAllocationGranularity ���� ����̾�� �Ѵ�.
	//	Ȥ�ö� ������ ���� 64k �� �����Ѵ�. 
	// 
	static DWORD AllocationGranularity = 0;
	if (0 == AllocationGranularity)
	{
		SYSTEM_INFO si = { 0 };
		GetSystemInfo(&si);
		AllocationGranularity = si.dwAllocationGranularity;
	}

	_ASSERTE(0 != AllocationGranularity);
	if (0 == AllocationGranularity)
	{
		AllocationGranularity = (64 * 1024);
	}

	//
	//	AllocationGranularity ������ ���� ������. 
	//	�ᱹ �����ؾ� �� ������� ������ ������ ��ŭ Ŀ���� �Ѵ�.
	// 	
	uint64_t AdjustMask = (uint64_t)(AllocationGranularity - 1);
	uint64_t adjusted_offset = Offset & ~AdjustMask;
	adjusted_size = (DWORD)(Offset & AdjustMask) + adjusted_size;

	_file_view = (PUCHAR)MapViewOfFile(_map_handle,
		(true == read_only) ? FILE_MAP_READ : FILE_MAP_READ | FILE_MAP_WRITE,
									   ((PLARGE_INTEGER)&adjusted_offset)->HighPart,
									   ((PLARGE_INTEGER)&adjusted_offset)->LowPart,
									   adjusted_size);
	if (nullptr == _file_view)
	{
		log_err
			"MapViewOfFile(high=0x%08x, low=0x%08x, bytes to map=%u) failed, gle=%u",
			((PLARGE_INTEGER)&adjusted_offset)->HighPart,
			((PLARGE_INTEGER)&adjusted_offset)->LowPart,
			adjusted_size,
			GetLastError()
			log_end;
		return nullptr;
	}

	//
	//	������ adjust offset ���� ������ �����ϴ� �޸� �����ʹ� 
	//	��û�� ��� �������־�� �Ѵ�.
	// 
	return &_file_view[Offset & AdjustMask];
}


/// @brief	���ε� ���������͸� �������Ѵ�. 
void FileIoHelper::ReleaseFilePointer()
{
	if (nullptr != _file_view)
	{
		UnmapViewOfFile(_file_view);
		_file_view = nullptr;
	}
}

/// @brief	������ Offset ���� Size ��ŭ �о Buffer �� �����Ѵ�.
bool
FileIoHelper::ReadFromFile(
	_In_ uint64_t Offset,
	_In_ DWORD Size,
	_Inout_updates_bytes_(Size) PUCHAR Buffer
)
{
	_ASSERTE(nullptr != Buffer);
	if (nullptr == Buffer) return false;

	uint8_t* src_ptr = GetFilePointer(true, Offset, Size);
	if (nullptr == src_ptr)
	{
		log_err "GetFilePointer() failed. offset=0x%llx, size=%u",
			Offset,
			Size
			log_end;
		return false;
	}

	bool ret = false;
	__try
	{
		RtlCopyMemory(Buffer, src_ptr, Size);
		ret = true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		log_err
			"exception. offset=0x%llx, size=%u, code=0x%08x",
			Offset,
			Size,
			GetExceptionCode()
			log_end
	}

	ReleaseFilePointer();
	return ret;

}

/// @brief	Buffer �� ������ Offset �� Size ��ŭ ����.
bool
FileIoHelper::WriteToFile(
	_In_ uint64_t Offset,
	_In_ DWORD Size,
	_In_reads_bytes_(Size) PUCHAR Buffer
)
{
	_ASSERTE(nullptr != Buffer);
	_ASSERTE(0 != Size);
	_ASSERTE(nullptr != Buffer);
	if (nullptr == Buffer || 0 == Size || nullptr == Buffer) return false;

	uint8_t* dst_ptr = GetFilePointer(false, Offset, Size);
	if (nullptr == dst_ptr)
	{
		log_err "GetFilePointer() failed. offset=0x%llx, size=%u",
			Offset,
			Size
			log_end;
		return false;
	}

	bool ret = false;
	__try
	{
		RtlCopyMemory(dst_ptr, Buffer, Size);
		ret = true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		log_err
			"exception. offset=0x%llx, size=%u, code=0x%08x",
			Offset,
			Size,
			GetExceptionCode()
			log_end
	}

	ReleaseFilePointer();
	return ret;
}
