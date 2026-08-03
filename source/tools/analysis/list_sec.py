import pefile
pe = pefile.PE("windows-drv/motuaw.sys")
for s in pe.sections:
    print("%-10s 0x%08x 0x%08x" % (s.Name.decode().strip('\x00'), s.VirtualAddress, s.VirtualAddress + s.Misc_VirtualSize))
