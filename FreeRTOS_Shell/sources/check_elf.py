#!/usr/bin/env python3
"""
Check STM32 ELF file for proper vector table
Usage: python3 check_elf.py stm32app.elf
"""

import sys
import struct

def check_elf(filename):
    try:
        with open(filename, 'rb') as f:
            # Read ELF header
            elf_header = f.read(52)
            if elf_header[:4] != b'\x7fELF':
                print("ERROR: Not a valid ELF file")
                return False
            
            # Get program header info
            e_phoff = struct.unpack('<I', elf_header[28:32])[0]
            e_phnum = struct.unpack('<H', elf_header[44:46])[0]
            
            print(f"ELF file: {filename}")
            print(f"Program headers: {e_phnum} at offset 0x{e_phoff:X}")
            print()
            
            # Read program headers
            f.seek(e_phoff)
            found_flash = False
            flash_offset = 0
            
            for i in range(e_phnum):
                ph = f.read(32)
                p_type = struct.unpack('<I', ph[0:4])[0]
                p_offset = struct.unpack('<I', ph[4:8])[0]
                p_vaddr = struct.unpack('<I', ph[8:12])[0]
                p_filesz = struct.unpack('<I', ph[16:20])[0]
                
                if p_type == 1:  # PT_LOAD
                    print(f"LOAD segment {i}:")
                    print(f"  File offset: 0x{p_offset:08X}")
                    print(f"  Virtual addr: 0x{p_vaddr:08X}")
                    print(f"  Size: {p_filesz} bytes")
                    
                    if p_vaddr == 0x08000000:
                        found_flash = True
                        flash_offset = p_offset
                        print("  *** This is the FLASH segment at 0x08000000 ***")
                    print()
            
            if not found_flash:
                print("ERROR: No segment found at 0x08000000 (flash start)")
                return False
            
            # Read vector table from flash
            f.seek(flash_offset)
            vector_data = f.read(16)
            
            sp = struct.unpack('<I', vector_data[0:4])[0]
            reset = struct.unpack('<I', vector_data[4:8])[0]
            nmi = struct.unpack('<I', vector_data[8:12])[0]
            hardfault = struct.unpack('<I', vector_data[12:16])[0]
            
            print("=== VECTOR TABLE ===")
            print(f"Initial SP:    0x{sp:08X}")
            print(f"Reset Handler: 0x{reset:08X}")
            print(f"NMI Handler:   0x{nmi:08X}")
            print(f"HardFault:     0x{hardfault:08X}")
            print()
            
            # Validate
            errors = []
            if sp == 0:
                errors.append("ERROR: Stack Pointer is 0!")
            elif sp < 0x20000000 or sp > 0x20020000:
                errors.append(f"WARNING: SP 0x{sp:08X} outside expected RAM range")
            
            if reset == 0:
                errors.append("ERROR: Reset Handler is 0!")
            elif reset < 0x08000000 or reset > 0x08080000:
                errors.append(f"ERROR: Reset Handler 0x{reset:08X} outside flash")
            elif (reset & 1) == 0:
                errors.append(f"ERROR: Reset Handler 0x{reset:08X} missing Thumb bit!")
            
            if errors:
                print("PROBLEMS FOUND:")
                for err in errors:
                    print(f"  {err}")
                return False
            else:
                print("✓ Vector table looks good!")
                return True
                
    except FileNotFoundError:
        print(f"ERROR: File not found: {filename}")
        return False
    except Exception as e:
        print(f"ERROR: {e}")
        return False

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python3 check_elf.py <elf_file>")
        sys.exit(1)
    
    success = check_elf(sys.argv[1])
    sys.exit(0 if success else 1)
