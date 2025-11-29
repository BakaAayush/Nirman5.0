import sys
import struct
import os

print("--- Converter Script Started ---")

# RP2350 RISC-V Family ID
FAMILY_ID = 0xe48bff58 

def convert_bin_to_uf2(bin_filename, uf2_filename):
    print(f"Reading input: {bin_filename}")
    
    try:
        with open(bin_filename, "rb") as f:
            data = f.read()
    except FileNotFoundError:
        print(f"ERROR: Could not open file: {bin_filename}")
        print("Check if the build folder exists and has the .bin file!")
        return

    print(f"File size: {len(data)} bytes")
    
    with open(uf2_filename, "wb") as f:
        num_blocks = (len(data) + 255) // 256
        for i in range(num_blocks):
            chunk = data[i * 256 : (i + 1) * 256]
            if len(chunk) < 256:
                chunk += b"\x00" * (256 - len(chunk))
            
            header = struct.pack(
                "<IIIIIIII",
                0x0A324655, 0x9E5D5157, 0x00002000, 
                0x10000000 + (i * 256), 256, i, num_blocks, FAMILY_ID
            )
            f.write(header + chunk + b"\x00" * 220 + struct.pack("<I", 0x0AB16F30))

    print(f"SUCCESS! Output written to: {uf2_filename}")
    print(">> GO FLUSH IT NOW! <<")

if __name__ == "__main__":
    # Get the folder where THIS script lives
    script_dir = os.path.dirname(os.path.abspath(__file__))
    
    # Construct the full paths safely
    input_path = os.path.join(script_dir, "build", "secure_iot.bin")
    output_path = os.path.join(script_dir, "build", "secure_iot.uf2")
    
    print(f"Looking for .bin file at: {input_path}")
    convert_bin_to_uf2(input_path, output_path)