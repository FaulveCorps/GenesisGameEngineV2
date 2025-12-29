from pathlib import Path
p = Path(r"C:\Users\jpfau\Desktop\Project\GenesisGameEngine\build-vs\tests\Debug\UnitTests.exe")
b = p.read_bytes()
for s in [b"Token::~Token", b"Token::~Token: unregistering host", b"g_callbacks", b"mark holder inactive", b"Token::~Token: about to acquire g_callbacksMutex"]:
    idx = b.find(s)
    print(s.decode(errors='replace'), 'found at', idx)
