import sys
import ctypes
from ctypes import wintypes

if len(sys.argv) < 3:
    print('Usage: create_minidump.py <pid> <out.dmp>')
    sys.exit(1)

pid = int(sys.argv[1], 0)
out = sys.argv[2]

kernel32 = ctypes.WinDLL('kernel32')
dbghelp = ctypes.WinDLL('Dbghelp')

OpenProcess = kernel32.OpenProcess
OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
OpenProcess.restype = wintypes.HANDLE

CreateFileW = kernel32.CreateFileW
CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, wintypes.LPVOID, wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
CreateFileW.restype = wintypes.HANDLE

CloseHandle = kernel32.CloseHandle
CloseHandle.argtypes = [wintypes.HANDLE]
CloseHandle.restype = wintypes.BOOL

MiniDumpWriteDump = dbghelp.MiniDumpWriteDump
MiniDumpWriteDump.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.HANDLE, ctypes.c_int, wintypes.LPVOID, wintypes.LPVOID, wintypes.LPVOID]
MiniDumpWriteDump.restype = wintypes.BOOL

# Constants
PROCESS_ALL_ACCESS = 0x1F0FFF
GENERIC_WRITE = 0x40000000
CREATE_ALWAYS = 2
FILE_ATTRIBUTE_NORMAL = 0x80
FILE_SHARE_READ = 1
FILE_SHARE_WRITE = 2

# MiniDump flags from dbghelp.h (subset)
MiniDumpWithFullMemory = 0x00000002
MiniDumpWithDataSegs = 0x00000001
MiniDumpWithHandleData = 0x00000004
MiniDumpWithThreadInfo = 0x00001000
MiniDumpWithFullMemoryInfo = 0x00000800
MiniDumpWithUnloadedModules = 0x00002000
MiniDumpWithProcessThreadData = 0x00040000

flags = MiniDumpWithFullMemory | MiniDumpWithHandleData | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules | MiniDumpWithFullMemoryInfo

hProcess = OpenProcess(PROCESS_ALL_ACCESS, False, pid)
if not hProcess:
    print('OpenProcess failed for pid', pid)
    sys.exit(2)

hFile = CreateFileW(out, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, None, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, None)
if hFile == wintypes.HANDLE(-1).value:
    print('CreateFileW failed for', out)
    CloseHandle(hProcess)
    sys.exit(3)

res = MiniDumpWriteDump(hProcess, pid, hFile, flags, None, None, None)
if not res:
    err = kernel32.GetLastError()
    print('MiniDumpWriteDump failed, GetLastError=', err)
else:
    print('Wrote dump', out)

CloseHandle(hFile)
CloseHandle(hProcess)
