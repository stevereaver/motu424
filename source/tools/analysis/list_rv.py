import pefile

pe = pefile.PE("windows-drv/motuaw.sys")
for s in pe.sections:
    print(s.Name.decode().strip('\x00'), hex(s.VirtualAddress))
