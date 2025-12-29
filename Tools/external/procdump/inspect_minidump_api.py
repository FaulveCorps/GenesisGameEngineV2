import minidump
print('minidump package:', minidump)
print('\nAvailable names:')
print('\n'.join([n for n in dir(minidump) if not n.startswith('_')]))
