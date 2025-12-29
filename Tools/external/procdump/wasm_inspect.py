import sys

# Minimal wasm decoder to inspect imports and function bodies for small test modules
# Usage: python wasm_inspect.py <hexstring_file_or_hex>

from io import BytesIO

# LEB decode little-endian
def read_u32_leb(f):
    result = 0
    shift = 0
    while True:
        b = f.read(1)
        if not b:
            raise EOFError('Unexpected EOF in LEB')
        v = b[0]
        result |= (v & 0x7F) << shift
        if (v & 0x80) == 0:
            break
        shift += 7
    return result

opcode_names = {
    0x00: 'unreachable',
    0x01: 'nop',
    0x02: 'block',
    0x03: 'loop',
    0x04: 'if',
    0x05: 'else',
    0x0b: 'end',
    0x10: 'call',
    0x20: 'local.get',
    0x21: 'local.set',
    0x41: 'i32.const',
    0x6a: 'i32.add',
}


def parse_wasm_bytes(data):
    f = BytesIO(data)
    magic = f.read(4)
    if magic != b"\x00asm":
        print('Not wasm magic:', magic)
        return
    version = f.read(4)
    print('WASM version bytes:', version.hex())
    imports = []
    func_types = []
    types = []
    functions = []
    exports = []
    # iterate sections
    while True:
        b = f.read(1)
        if not b:
            break
        sec_id = b[0]
        sec_size = read_u32_leb(f)
        sec_start = f.tell()
        sec_bytes = f.read(sec_size)
        # process section
        if sec_id == 1:
            # type
            tf = BytesIO(sec_bytes)
            count = read_u32_leb(tf)
            for _ in range(count):
                form = tf.read(1)[0]
                if form != 0x60:
                    print('unknown form', form)
                param_count = read_u32_leb(tf)
                params = [tf.read(1)[0] for __ in range(param_count)]
                ret_count = read_u32_leb(tf)
                rets = [tf.read(1)[0] for __ in range(ret_count)]
                types.append((params, rets))
            print('Types:', types)
        elif sec_id == 2:
            # import
            tf = BytesIO(sec_bytes)
            count = read_u32_leb(tf)
            for _ in range(count):
                mod_len = read_u32_leb(tf)
                module = tf.read(mod_len).decode('utf-8')
                fld_len = read_u32_leb(tf)
                field = tf.read(fld_len).decode('utf-8')
                kind = tf.read(1)[0]
                if kind == 0: # func
                    type_index = read_u32_leb(tf)
                    imports.append(('func', module, field, type_index))
                else:
                    # skip other import kinds minimally
                    if kind == 2: # memory
                        pass
                    print('import other kind', kind)
            print('Imports:', imports)
        elif sec_id == 3:
            tf = BytesIO(sec_bytes)
            count = read_u32_leb(tf)
            for _ in range(count):
                type_idx = read_u32_leb(tf)
                func_types.append(type_idx)
            print('Function types:', func_types)
        elif sec_id == 7:
            # exports
            tf = BytesIO(sec_bytes)
            count = read_u32_leb(tf)
            for _ in range(count):
                l = read_u32_leb(tf)
                name = tf.read(l).decode('utf-8')
                kind = tf.read(1)[0]
                index = read_u32_leb(tf)
                exports.append((name, kind, index))
            print('Exports:', exports)
        elif sec_id == 10:
            # code
            tf = BytesIO(sec_bytes)
            count = read_u32_leb(tf)
            bodies = []
            for i in range(count):
                body_size = read_u32_leb(tf)
                body = tf.read(body_size)
                bodies.append(body)
            print('Found', len(bodies), 'function bodies')
            # disassemble first few bodies
            for bi, body in enumerate(bodies):
                print('\nFunction body', bi)
                bf = BytesIO(body)
                # locals
                local_count = read_u32_leb(bf)
                for li in range(local_count):
                    n = read_u32_leb(bf)
                    t = bf.read(1)[0]
                    print('  local decl count=', n, 'type=', hex(t))
                # opcodes until end
                ops = []
                while True:
                    b = bf.read(1)
                    if not b:
                        break
                    op = b[0]
                    if op == 0x0b:
                        ops.append(('end', None))
                        break
                    if op == 0x10: # call
                        idx = read_u32_leb(bf)
                        ops.append(('call', idx))
                    elif op == 0x41:
                        val = read_u32_leb(bf)
                        ops.append(('i32.const', val))
                    else:
                        ops.append((hex(op), None))
                print('  ops:', ops)
        else:
            pass
    return

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print('Usage: wasm_inspect.py <hexstring or file>')
        sys.exit(1)
    arg = sys.argv[1]
    if len(arg) > 0 and all(c in '0123456789abcdefABCDEF' for c in arg.strip()):
        hexs = arg.strip()
    else:
        with open(arg, 'r') as fh:
            hexs = fh.read().strip()
    # remove non-hex chars
    import re
    hexs = re.sub('[^0-9a-fA-F]', '', hexs)
    data = bytes.fromhex(hexs)
    parse_wasm_bytes(data)
