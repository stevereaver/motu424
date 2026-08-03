import sys
import pefile

def analyze(filepath):
    pe = pefile.PE(filepath)
    if hasattr(pe, 'DIRECTORY_ENTRY_EXPORT'):
        print("Exports:")
        for exp in pe.DIRECTORY_ENTRY_EXPORT.symbols:
            name = exp.name.decode('utf-8') if exp.name else "ordinal_{}".format(exp.ordinal)
            print(f"  {name} at {hex(exp.address)}")
            
    print("\nInteresting Strings:")
    for section in pe.sections:
        if b'.data' in section.Name or b'.rdata' in section.Name:
            data = section.get_data()
            # Simple string extraction
            pass # Too noisy, use strings command

if __name__ == '__main__':
    analyze(sys.argv[1])
