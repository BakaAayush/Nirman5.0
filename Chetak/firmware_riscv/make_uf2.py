import sys
import struct
import os

# RP2350 RISC-V Family ID
FAMILY_ID = 0xe48bff56

def convert_bin_to_uf2(bin_filename, uf2_filename):
    with open(bin_filename, "rb") as f:
        data = f.read()

    with open(uf2_filename, "wb") as f:
        num_blocks = (len(data) + 255) // 256
        for i in range(num_blocks):
            chunk = data[i * 256 : (i + 1) * 256]
            # Pad chunk to 256 bytes
            if len(chunk) < 256:
                chunk += b"\x00" * (256 - len(chunk))
            
            # UF2 Header
            # Magic1, Magic2, Flags, Target Addr, Payload Size, Block No, Num Blocks, FamilyID
            header = struct.pack(
                "<IIIIIIII",
                0x0A324655, # Magic Start 1
                0x9E5D5157, # Magic Start 2
                0x00002000, # Flags (FamilyID present)
                0x10000000 + (i * 256), # Address (Flash Start)
                256,        # Payload Size
                i,          # Block Number
                num_blocks, # Total Blocks
                FAMILY_ID   # RP2350 Family
            )
            
            # UF2 Footer (Magic End)
            footer = struct.pack("<I", 0x0AB16F30)
            
            # Write 512 byte block: Header + Data + Padding + Footer
            f.write(header + chunk + b"\x00" * 220 + footer)

    print(f"Success! Created {uf2_filename}")

if __name__ == "__main__":
    # Check paths relative to where script is run
    input_path = "build/secure_iot.bin"
    output_path = "build/secure_iot.uf2"
    
    if not os.path.exists(input_path):
        print(f"Error: Could not find {input_path}")
        print("Make sure you have built the project successfully!")
    else:
        convert_bin_to_uf2(input_path, output_path)