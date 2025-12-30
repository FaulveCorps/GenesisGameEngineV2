$py = @'
from minidump.minidumpfile import MinidumpFile
import inspect
sig = inspect.signature(MinidumpFile._MinidumpFile__parse_thread_context)
print('signature:', sig)
print(inspect.getsource(MinidumpFile._MinidumpFile__parse_thread_context))
'@
python - << $py
