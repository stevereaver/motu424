import pefile
pe = pefile.PE('windows-drv/motuaw.sys')
# RVA to string
def get_string_at_rva(rva):
    data = pe.get_data(rva, 64)
    # try ascii
    try:
        s = b""
        for b in data:
            if b == 0: break
            s += bytes([b])
        return s.decode('ascii')
    except:
        return "<binary>"

print("String at 34960:", get_string_at_rva(0x34960))
