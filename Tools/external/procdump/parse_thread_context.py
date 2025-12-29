from minidump.minidumpfile import MinidumpFile
import sys
md = MinidumpFile.parse(sys.argv[1])
rec = md.exception.exception_records[0]
print('rec.ThreadContext.Rva:', rec.ThreadContext.Rva, 'DataSize:', rec.ThreadContext.DataSize)
# try to use private parser
parser = md._MinidumpFile__parse_thread_context
ctx = parser(rec.ThreadContext)
print('parsed ctx type:', type(ctx))
print('\nctx attrs:')
for n in dir(ctx):
    if not n.startswith('_'):
        try:
            print(n, getattr(ctx, n))
        except Exception as e:
            print('error reading', n, e)

# try common register names
for r in ['Rax','Rcx','Rdx','Rbx','Rsp','Rbp','Rsi','Rdi','Rip']:
    if hasattr(ctx, r):
        print(r, hex(getattr(ctx, r)))
    else:
        print(r, 'not present')
