import os
import sys
import shutil
import subprocess
from pathlib import Path

bios = r"/opt/homebrew/share/qemu/edk2-aarch64-code.fd"

def main():
    build_dir = Path("build")
    if build_dir.exists():
        shutil.rmtree(build_dir)

    build_dir.mkdir()
    subprocess.run(["cmake", ".."], cwd=build_dir, text=True)
    subprocess.run(["make"], cwd=build_dir, text=True)

    subprocess.run(["aarch64-elf-objcopy", "-O", "binary", "kernel.elf", "kernel.bin"], cwd=build_dir, text=True)

    subprocess.run(["dd", "if=/dev/zero", "of=disk.img", "bs=1M", "count=128", "status=none"], cwd=build_dir, text=True)

    subprocess.run(["mformat", "-i", "disk.img", "-F", "::"], cwd=build_dir, text=True)

    subprocess.run(["curl", "-o", "BOOTAA64.EFI", "https://raw.githubusercontent.com/limine-bootloader/limine/v8.x-binary/BOOTAA64.EFI"], cwd=build_dir, text=True)

    subprocess.run(["mmd", "-i", "disk.img", "::/EFI"], cwd=build_dir, text=True)
    subprocess.run(["mmd", "-i", "disk.img", "::/EFI/BOOT"], cwd=build_dir, text=True)
    subprocess.run(["mcopy", "-i", "disk.img", "../limine.conf", "::/EFI/BOOT/limine.conf"], cwd=build_dir, text=True)
    subprocess.run(["mcopy", "-i", "disk.img", "BOOTAA64.EFI", "::/EFI/BOOT/BOOTAA64.EFI"], cwd=build_dir, text=True)
    subprocess.run(["mcopy", "-i", "disk.img", "kernel.bin", "::/"], cwd=build_dir, text=True)

if __name__ == "__main__":
    try:
        main()
        for arg in sys.argv:
            if arg == "--run":
                subprocess.run([
                    "qemu-system-aarch64", 
                    "-M", "virt", 
                    "-m", "2G", 
                    "-cpu", "cortex-a710", 
                    "-bios", bios, 
                    "-nographic", 
                    "-drive", "file=disk.img,if=virtio,format=raw",
                    "-smp", "cores=2", 
                    "-netdev", "user,id=net0",
                    "-device", "virtio-net-device,netdev=net0"
                ], cwd=Path("build"))

    except Exception as e:
        print(f"An error occurred: {e}")