import pefile
pe = pefile.PE("windows-drv/motuaw.sys")
for s in pe.sections:
    print("%-10s %08x %08x" % (s.Name.decode().strip('\x00'), s.PointerToRawData, s.SizeOfRawData))
