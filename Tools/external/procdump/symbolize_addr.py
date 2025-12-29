import sys
import ctypes
from ctypes import wintypes

if len(sys.argv) < 4:
    print('Usage: symbolize_addr.py <module_path> <base_addr_hex> <addr_hex>')
    sys.exit(1)

module_path = sys.argv[1]
base_addr = int(sys.argv[2], 16)
addr = int(sys.argv[3], 16)

# Load DbgHelp
dbghelp = ctypes.WinDLL('DbgHelp')
kernel32 = ctypes.WinDLL('kernel32')
GetCurrentProcess = kernel32.GetCurrentProcess

# Types
DWORD = ctypes.c_ulong
DWORD64 = ctypes.c_ulonglong
LPVOID = ctypes.c_void_p

SYMOPT_DEFERRED_LOADS = 0x00000004
SYMOPT_UNDNAME = 0x00000002

# SYMBOL_INFO structure
MAX_SYM_NAME = 1024
class SYMBOL_INFO(ctypes.Structure):
    _fields_ = [
        ('SizeOfStruct', ctypes.c_ulong),
        ('TypeIndex', ctypes.c_ulong),
        ('Reserved', ctypes.c_ulonglong * 2),
        ('Index', ctypes.c_ulong),
        ('Size', ctypes.c_ulong),
        ('ModBase', ctypes.c_ulonglong),
        ('Flags', ctypes.c_ulong),
        ('Value', ctypes.c_ulonglong),
        ('Address', ctypes.c_ulonglong),
        ('Register', ctypes.c_ulong),
        ('Scope', ctypes.c_ulong),
        ('Tag', ctypes.c_ulong),
        ('NameLen', ctypes.c_ulong),
        ('MaxNameLen', ctypes.c_ulong),
        ('Name', ctypes.c_char * 1),
    ]

# SymInitialize
SymInitialize = dbghelp.SymInitializeW
SymInitialize.argtypes = [wintypes.HANDLE, wintypes.LPCWSTR, wintypes.BOOL]
SymInitialize.restype = wintypes.BOOL

SymSetOptions = dbghelp.SymSetOptions
SymSetOptions.argtypes = [DWORD]
SymSetOptions.restype = DWORD

SymLoadModuleEx = dbghelp.SymLoadModuleExW
SymLoadModuleEx.argtypes = [wintypes.HANDLE, wintypes.HANDLE, wintypes.LPCWSTR, wintypes.LPCWSTR, DWORD64, DWORD, wintypes.LPVOID, DWORD]
SymLoadModuleEx.restype = DWORD64

SymFromAddr = dbghelp.SymFromAddr
SymFromAddr.argtypes = [wintypes.HANDLE, DWORD64, ctypes.POINTER(DWORD64), ctypes.POINTER(SYMBOL_INFO)]
SymFromAddr.restype = wintypes.BOOL

SymGetLineFromAddr64 = dbghelp.SymGetLineFromAddr64
SymGetLineFromAddr64.argtypes = [wintypes.HANDLE, DWORD64, ctypes.POINTER(DWORD64), ctypes.c_void_p]
SymGetLineFromAddr64.restype = wintypes.BOOL

# Initialize
hProcess = GetCurrentProcess()
SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME)
if not SymInitialize(hProcess, None, False):
    print('SymInitialize failed')
    sys.exit(1)

# Set symbol search path to module folder
import os
sym_dir = os.path.dirname(module_path)
SymSetSearchPath = dbghelp.SymSetSearchPathW
SymSetSearchPath.argtypes = [wintypes.HANDLE, wintypes.LPCWSTR]
SymSetSearchPath.restype = wintypes.DWORD
rv = SymSetSearchPath(hProcess, sym_dir)
print('SymSetSearchPath returned', rv)

# Load module
modbase = SymLoadModuleEx(hProcess, None, module_path, None, base_addr, 0, None, 0)
if modbase == 0:
    print('SymLoadModuleEx failed')
    sys.exit(1)
print('Module loaded at base', hex(modbase))

# Prepare symbol buffer
buffsize = ctypes.sizeof(SYMBOL_INFO) + MAX_SYM_NAME
class SYMBOL_INFO_BUFF(ctypes.Structure):
    _fields_ = [('si', SYMBOL_INFO), ('name', ctypes.c_char * (MAX_SYM_NAME - 1))]

sib = SYMBOL_INFO_BUFF()
sib.si.SizeOfStruct = ctypes.sizeof(SYMBOL_INFO)
sib.si.MaxNameLen = MAX_SYM_NAME

displacement = DWORD64(0)
res = SymFromAddr(hProcess, addr, ctypes.byref(displacement), ctypes.byref(sib.si))
if not res:
    err = ctypes.get_last_error()
    print('SymFromAddr failed, GetLastError=', err)
else:
    name = ctypes.string_at(ctypes.addressof(sib.name), sib.si.NameLen)
    print('Symbol:', name.decode(errors='replace'), 'Address:', hex(sib.si.Address), 'Displacement:', hex(displacement.value))

# Try to get source line
class IMAGEHLP_LINE64(ctypes.Structure):
    _fields_ = [('SizeOfStruct', DWORD), ('Key', LPVOID), ('LineNumber', DWORD), ('FileName', ctypes.c_wchar_p), ('Address', DWORD64)]

line = IMAGEHLP_LINE64()
line.SizeOfStruct = ctypes.sizeof(IMAGEHLP_LINE64)
displacement_line = DWORD64(0)
if SymGetLineFromAddr64(hProcess, addr, ctypes.byref(displacement_line), ctypes.byref(line)):
    print('File:', line.FileName, 'Line:', line.LineNumber)
else:
    print('No line info available')

# Cleanup
# There's no SymCleanup call here; process exit will clean up.
