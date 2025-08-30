#!/usr/bin/env python3
import struct
import sys

Import("env")

def parse_log_file(project_dir, filename):

    parsed_output_file = project_dir + "/parsed_logs.txt"

    try:
        with open(filename, 'rb') as f:
            data = f.read()
    except FileNotFoundError:
        print(f"Error: File '{filename}' not found.")
        return

    with open(parsed_output_file, 'w') as out_f:
        out_f.write(f"File size: {len(data)} bytes\n")
        out_f.write("--- Parsing Log Entries ---\n")

        # Define the structure format string (must match your C struct)
        # 'I' is unsigned int (4 bytes), '64s' is 64-character string
        struct_format = 'I64s'  # timestamp (4 bytes) + message (64 bytes)
        entry_size = struct.calcsize(struct_format)
        entry_count = 0
        
        offset = 0
        while offset + entry_size <= len(data):
            # Unpack one log entry from the binary data
            timestamp, message_bytes = struct.unpack_from(struct_format, data, offset)
            
            # Check for erased/unwritten flash (usually 0xFF or 0x00)
            if timestamp == 0xFFFFFFFF or timestamp == 0:
                # print(f"End of logs at offset 0x{offset:X}")
                offset += entry_size
                continue
                
            # Convert the bytes to a string, stopping at the first null byte
            try:
                message = message_bytes.decode('utf-8').split('\x00')[0]
            except UnicodeDecodeError:
                message = "[BINARY DATA]"

            out_f.write(f"[Entry {entry_count} @ 0x{offset:06X}] [T+{timestamp:6} ms] {message}\n")

            offset += entry_size
            entry_count += 1
    
        # Also show the raw hex for the first part for debugging
        out_f.write("\n--- Raw Hex Preview (first 128 bytes) ---\n")
        for i in range(0, min(128, len(data)), 16):
            hex_part = ' '.join(f'{b:02X}' for b in data[i:i+16])
            ascii_part = ''.join(chr(b) if 32 <= b <= 126 else '.' for b in data[i:i+16])
            out_f.write(f"0x{i:04X}: {hex_part:<48} {ascii_part}\n")

# Read the log partition
def read_logs_cmd(target, source, env):
    project_dir = env["PROJECT_DIR"]
    
    # Get upload port and speed from environment with fallbacks
    upload_port = env.GetProjectOption("upload_port", "/dev/ttyACM0")  # Change default as needed
    upload_speed = env.GetProjectOption("upload_speed", "115200")
    
    # Get esptool.py path
    esptool_path = env.PioPlatform().get_package_dir("tool-esptoolpy") + "/esptool.py"
    
    # Partition details - EDIT THESE to match your partitions.csv!
    partition_offset = "0x41F000"
    partition_size = "0x20000"
    output_file = project_dir + "/log_dump.bin"
    
    # Build the command
    cmd = [
        env["PYTHONEXE"],  # Use the same Python interpreter that PlatformIO uses
        esptool_path,
        "--chip", "esp32s3",
        "--port", upload_port,
        "--baud", upload_speed,
        "read_flash",
        partition_offset,
        partition_size,
        output_file
    ]

    ## At this point cmd is a list of command parts, we need to make it a single string but there are numbers and strings in it
    cmd = [str(part) for part in cmd]

    print("Running command: " + "".join(cmd))
    try:
        # Execute the command
        result = env.Execute(" ".join(cmd))
        if result == 0:
            print(f"\n✅ Success! Logs dumped to: {output_file}")
            parse_log_file(project_dir, output_file)
        else:
            print(f"\n❌ Command failed with exit code: {result}")
    except Exception as e:
        print(f"\n❌ Error executing command: {e}")

# Erase the log partition  
def erase_logs_cmd(target, source, env):
    project_dir = env["PROJECT_DIR"]
    upload_port = env.GetProjectOption("upload_port", "/dev/ttyACM0")  # Change default as needed
    upload_speed = env.GetProjectOption("upload_speed", "115200")
    esptool_path = env.PioPlatform().get_package_dir("tool-esptoolpy") + "/esptool.py"
    
    # Partition details - EDIT THESE!
    partition_offset = "0x41F000"
    partition_size = "0x20000"

    cmd = [
        env["PYTHONEXE"],
        esptool_path,
        "--chip", "esp32s3",
        "--port", upload_port,
        "--baud", upload_speed,
        "erase_region",
        partition_offset,
        partition_size
    ]
    
    cmd = [str(part) for part in cmd]

    print("Running command: " + " ".join(cmd))
    try:
        result = env.Execute(" ".join(cmd))
        if result == 0:
            print(f"\n✅ Success! Log partition erased.")
        else:
            print(f"\n❌ Command failed with exit code: {result}")
    except Exception as e:
        print(f"\n❌ Error executing command: {e}")

# Add the custom targets to SCons
read_logs_action = env.AlwaysBuild(env.Alias("read_logs", None, read_logs_cmd))
erase_logs_action = env.AlwaysBuild(env.Alias("erase_logs", None, erase_logs_cmd))