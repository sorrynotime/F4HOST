"""Standalone Arm Compiler 6 compile/link of the F4HOST project source list.

Uses the project's include paths, defines, IROM/IRAM and checked-in startup.
Does not run uVision, flash hardware, or modify the project. Outputs: .build/armclang.
"""
import argparse
import pathlib
import subprocess
import xml.etree.ElementTree as ET

ROOT = pathlib.Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--bin", default="C:/Keil_v5/ARM/ARMCLANG/bin")
    args = parser.parse_args()
    bindir = pathlib.Path(args.bin)
    project = ROOT / "Msp/MDK-ARM/F4HOST.uvprojx"
    tree = ET.parse(project)
    cads = tree.find(".//TargetOption/TargetArmAds/Cads")
    incs = ["-I" + str((project.parent / p.replace("\\", "/")).resolve())
            for p in cads.findtext("VariousControls/IncludePath").split(";") if p]
    defines = ["-D" + p for p in cads.findtext("VariousControls/Define").split(",") if p]
    output = ROOT / ".build/armclang"
    output.mkdir(parents=True, exist_ok=True)
    objects = []
    for node in tree.findall(".//Groups/Group/Files/File"):
        if node.findtext("FileOption/CommonProperty/IncludeInBuild") == "0":
            continue
        source = (project.parent / node.findtext("FilePath").replace("\\", "/")).resolve()
        obj = output / (source.stem + ".o")
        if source.suffix.lower() == ".c":
            cmd = [str(bindir / "armclang.exe"), "--target=arm-arm-none-eabi", "-mcpu=cortex-m4",
                   "-mfloat-abi=hard", "-mfpu=fpv4-sp-d16", "-std=c99", "-O1", "-g",
                   "-ffunction-sections", "-fdata-sections", "-Wall"] + incs + defines
            if "host_gamepad" in source.parts:
                cmd += ["-Wextra", "-Werror"]
            cmd += ["-c", str(source), "-o", str(obj)]
        elif source.suffix.lower() == ".s":
            cmd = [str(bindir / "armasm.exe"), "--cpu=Cortex-M4.fp.sp", str(source), "-o", str(obj)]
        else:
            continue
        subprocess.run(cmd, check=True, cwd=ROOT)
        objects.append(str(obj))
    memory = tree.find(".//OnChipMemories")
    rom = memory.findtext("IROM/StartAddress")
    rom_size = memory.findtext("IROM/Size")
    ram = memory.findtext("IRAM/StartAddress")
    ram_size = memory.findtext("IRAM/Size")
    scatter = output / "F4HOST.sct"
    scatter.write_text(f"""LR_IROM1 {rom} {rom_size} {{
  ER_IROM1 {rom} {rom_size} {{
    startup_stm32f401xc.o (RESET, +First)
    *(InRoot$$Sections)
    .ANY (+RO)
  }}
  RW_IRAM1 {ram} {ram_size} {{ .ANY (+RW +ZI) }}
}}
""", encoding="ascii")
    axf = output / "F4HOST.axf"
    subprocess.run([str(bindir / "armlink.exe"), "--cpu=Cortex-M4.fp.sp",
                    "--scatter=" + str(scatter), "--entry=Reset_Handler", "--map", "--info=sizes",
                    "--list=" + str(output / "F4HOST.map"), "--output=" + str(axf)] + objects,
                   check=True, cwd=ROOT)
    subprocess.run([str(bindir / "fromelf.exe"), "--i32combined", "--output=" + str(output / "F4HOST.hex"),
                    str(axf)], check=True)
    print(f"Arm Compiler 6 build passed: {axf}")


if __name__ == "__main__":
    main()
