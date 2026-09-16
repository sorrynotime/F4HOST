"""Compile project C sources for Cortex-M4 and run the portable parser test.

Requires arm-none-eabi-gcc on PATH and a native clang (or pass --cc).
Outputs stay in .build/gamepad. This checks C compilation, not the Keil link/flash.
"""
import argparse
import os
import pathlib
import shutil
import subprocess
import xml.etree.ElementTree as ET

ROOT = pathlib.Path(__file__).resolve().parents[1]
PROJECT = ROOT / "Msp/MDK-ARM/F4HOST.uvprojx"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cc", default=shutil.which("clang"))
    ap.add_argument("--sanitize", action="store_true")
    args = ap.parse_args()
    out = ROOT / ".build/gamepad"
    out.mkdir(parents=True, exist_ok=True)
    tree = ET.parse(PROJECT)
    cads = tree.find(".//TargetOption/TargetArmAds/Cads")
    includes = ["-I" + str((PROJECT.parent / p.replace("\\", "/")).resolve())
                for p in cads.findtext("VariousControls/IncludePath").split(";") if p]
    defines = ["-D" + p for p in cads.findtext("VariousControls/Define").split(",") if p]
    compiler = shutil.which("arm-none-eabi-gcc")
    if not compiler:
        raise SystemExit("arm-none-eabi-gcc not found")
    sources = []
    for node in tree.findall(".//Groups/Group/Files/File"):
        path = (PROJECT.parent / node.findtext("FilePath").replace("\\", "/")).resolve()
        if path.suffix.lower() == ".c" and node.findtext("FileOption/CommonProperty/IncludeInBuild") != "0":
            sources.append(path)
    for index, path in enumerate(sources):
        command = [compiler, "-mcpu=cortex-m4", "-mthumb", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard",
                   "-std=c99", "-Wall", "-Wextra", "-ffunction-sections", "-fdata-sections"]
        if "host_gamepad" in path.parts:
            command += ["-Werror"]
        subprocess.run(command + defines + includes + ["-c", str(path), "-o", str(out / f"{index}_{path.stem}.o")],
                       check=True, cwd=ROOT)
    print(f"Cortex-M4 C compilation passed: {len(sources)} source files", flush=True)
    if not args.cc:
        raise SystemExit("Native clang not found; pass --cc PATH to run the parser tests")
    exe = out / "test_gamepad_hid.exe"
    command = [args.cc, "-std=c99", "-Wall", "-Wextra", "-Werror", "-I", str(ROOT / "Lib/host_gamepad")]
    if args.sanitize:
        command += ["-fsanitize=address,undefined", "-g"]
    test_env = os.environ.copy()
    if args.sanitize:
        resource = subprocess.check_output([args.cc, "-print-resource-dir"], text=True).strip()
        runtime = pathlib.Path(resource) / "lib/windows"
        test_env["PATH"] = str(runtime) + os.pathsep + test_env.get("PATH", "")
    subprocess.run(command + [str(ROOT / "tests/test_gamepad_hid.c"),
                             str(ROOT / "Lib/host_gamepad/gamepad_hid.c"), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True, env=test_env)
    host_exe = out / "test_gamepad_host.exe"
    subprocess.run(command + ["-I", str(ROOT / "tests/stubs"),
                             str(ROOT / "tests/test_gamepad_host.c"),
                             str(ROOT / "Lib/host_gamepad/host_gamepad.c"),
                             str(ROOT / "Lib/host_gamepad/gamepad_hid.c"),
                             "-o", str(host_exe)], check=True)
    subprocess.run([str(host_exe)], check=True, env=test_env)


if __name__ == "__main__":
    main()
