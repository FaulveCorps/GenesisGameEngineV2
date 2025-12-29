from minidump.minidumpfile import MinidumpFile
import inspect
print('parse_bytes sig:')
print(inspect.getsource(MinidumpFile.parse_bytes))
print('\nparse_buff sig:')
print(inspect.getsource(MinidumpFile.parse_buff))
