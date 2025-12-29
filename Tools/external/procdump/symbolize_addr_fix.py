import sys
import ctypes
from ctypes import wintypes

if len(sys.argv) < 4:
    print('Usage: symbolize_addr_fix.py <module_path> <base_addr_hex> <addr_hex>')
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
SYMOPT_LOAD_LINES = 0x00000010
# Request deferred loads, human-readable names, and load line info
SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME | SYMOPT_LOAD_LINES)
if not SymInitialize(hProcess, None, False):
    print('SymInitialize failed')
    sys.exit(1)

# Set symbol search path to module folder and Microsoft's public symbol server
import os
sym_dir = os.path.dirname(module_path)
# Use a local cache directory for SRV to avoid repeated downloads
sym_server = r"SRV*C:\symbols*https://msdl.microsoft.com/download/symbols"
full_sym_path = sym_dir + ";" + sym_server
SymSetSearchPath = dbghelp.SymSetSearchPathW
SymSetSearchPath.argtypes = [wintypes.HANDLE, wintypes.LPCWSTR]
SymSetSearchPath.restype = wintypes.DWORD
rv = SymSetSearchPath(hProcess, full_sym_path)
print('SymSetSearchPath returned', rv)

# Load module (pass file size to assist symbol loader)
file_size = os.path.getsize(module_path)
modbase = SymLoadModuleEx(hProcess, None, module_path, None, base_addr, file_size, None, 0)
if modbase == 0:
    print('SymLoadModuleEx failed')
    sys.exit(1)
print('Module loaded at base', hex(modbase), 'size=', file_size)

# Try to get module info (LoadedPdbName etc.) using SymGetModuleInfo64
try:
    class IMAGEHLP_MODULE64(ctypes.Structure):
        _fields_ = [
            ('SizeOfStruct', DWORD64),
            ('BaseOfImage', DWORD64),
            ('ImageSize', DWORD64),
            ('TimeDateStamp', DWORD),
            ('CheckSum', DWORD),
            ('NumSyms', DWORD),
            ('SymType', ctypes.c_uint),
            ('ModuleName', ctypes.c_char * 32),
            ('ImageName', ctypes.c_char * 256),
            ('LoadedImageName', ctypes.c_char * 256),
            ('LoadedPdbName', ctypes.c_char * 256),
            ('CVSig', DWORD),
            ('CVData', ctypes.c_char * 64),
        ]
    SymGetModuleInfo64 = dbghelp.SymGetModuleInfo64
    SymGetModuleInfo64.argtypes = [wintypes.HANDLE, DWORD64, ctypes.POINTER(IMAGEHLP_MODULE64)]
    SymGetModuleInfo64.restype = wintypes.BOOL
    modinfo = IMAGEHLP_MODULE64()
    modinfo.SizeOfStruct = ctypes.sizeof(IMAGEHLP_MODULE64)
    if SymGetModuleInfo64(hProcess, modbase, ctypes.byref(modinfo)):
        try:
            img_name = modinfo.ImageName.decode(errors='replace')
            pdb_name = modinfo.LoadedPdbName.decode(errors='replace')
        except Exception:
            img_name = repr(modinfo.ImageName)
            pdb_name = repr(modinfo.LoadedPdbName)
        print('Module info: ImageName=', img_name)
        print('LoadedPdbName=', pdb_name)
    else:
        print('SymGetModuleInfo64 failed, GetLastError=', ctypes.get_last_error())
except Exception as e:
    print('SymGetModuleInfo64 not available or failed:', e)

# Prepare symbol buffer
buffsize = ctypes.sizeof(SYMBOL_INFO) + MAX_SYM_NAME
class SYMBOL_INFO_BUFF(ctypes.Structure):
    _fields_ = [('si', SYMBOL_INFO), ('name', ctypes.c_char * (MAX_SYM_NAME - 1))]

sib = SYMBOL_INFO_BUFF()
sib.si.SizeOfStruct = ctypes.sizeof(SYMBOL_INFO)
sib.si.MaxNameLen = MAX_SYM_NAME

# Call SymFromAddr

# Using a raw buffer read to extract name safely
from ctypes import addressof, sizeof, string_at

displacement = DWORD64(0)
res = SymFromAddr(hProcess, addr, ctypes.byref(displacement), ctypes.byref(sib.si))
if not res:
    err = ctypes.get_last_error()
    print('SymFromAddr failed, GetLastError=', err)
else:
    # Read raw bytes from the buffer area and extract the null-terminated name
    raw = string_at(addressof(sib), sizeof(sib))
    # Name is located at offset sizeof(SYMBOL_INFO)-1
    name_offset = sizeof(SYMBOL_INFO) - 1
    name_bytes = raw[name_offset:]
    name = name_bytes.split(b'\x00', 1)[0]
    try:
        name_str = name.decode(errors='replace')
    except Exception:
        name_str = repr(name)
    print('Symbol:', name_str, 'Address:', hex(sib.si.Address), 'Displacement:', hex(displacement.value))

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
