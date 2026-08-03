import sys
from capstone import *
from capstone.x86 import *
import pefile

def analyze(filepath):
    pe = pefile.PE(filepath)
    print("Imports:")
    if hasattr(pe, 'DIRECTORY_ENTRY_IMPORT'):
        for entry in pe.DIRECTORY_ENTRY_IMPORT:
            dll_name = entry.dll.decode('utf-8')
            for imp in entry.imports:
                if imp.name:
                    func_name = imp.name.decode('utf-8')
                    if func_name in ('WriteFile', 'CreateFileA', 'CreateFileW', 'SendDriverMessage', 'CallNamedPipeA', 'CallNamedPipeW', 'SendMessageA', 'SendMessageW'):
                        print(f"  {dll_name}: {func_name}")

if __name__ == '__main__':
    analyze(sys.argv[1])
