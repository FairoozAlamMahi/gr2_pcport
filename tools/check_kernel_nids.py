import re

with open('src/kernel/libkernel.h', 'r') as f:
    hdr = f.read()

hdr_funcs = set(re.findall(r'PS4_SYSV_ABI\s+(?:[\w\*]+)\s+(\w+)\s*\(', hdr))
print(f'libkernel.h has {len(hdr_funcs)} declared functions')

with open('tools/generated_host_nids.cpp', 'r') as f:
    lines = f.readlines()

in_kernel = False
kernel_calls = []
for l in lines:
    if 'libkernel_web.sprx.json' in l:
        in_kernel = True
        continue
    elif l.startswith('// =====') and in_kernel:
        in_kernel = False
        break
    if in_kernel and l.startswith('RegisterSymbol('):
        parts = l.split('"')
        if len(parts) >= 4:
            nid = parts[1]
            fn = parts[3]
            kernel_calls.append((nid, fn, fn in hdr_funcs))

print(f'Kernel NID calls: {len(kernel_calls)}')
matched = [x for x in kernel_calls if x[2]]
missing = [x for x in kernel_calls if not x[2]]
print(f'Declared in libkernel.h: {len(matched)}')
print(f'Missing from libkernel.h: {len(missing)}')
for nid, fn, _ in missing:
    print(f'  missing: {fn} ({nid})')
