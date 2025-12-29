from minidump.minidumpfile import MinidumpFile
import sys
path = sys.argv[1]
md = MinidumpFile.parse(path)
print('md.threads dir:', dir(md.threads))
try:
    tlist = md.threads.to_table()
    print('thread table len:', len(tlist))
    print(tlist[:6])
except Exception as e:
    print('to_table failed', e)
recs = getattr(md.exception, 'exception_records', None)
if recs and len(recs)>0:
    tid = recs[0].ThreadId
    print('crash tid:', tid)
    try:
        for t in tlist:
            # t[0] is 'ThreadId' like '0x2fa4'
            try:
                if int(t[0],16) == tid:
                    print('found thread row for crash tid:', t)
            except:
                pass
    except Exception as e:
        print('pair load failed', e)
