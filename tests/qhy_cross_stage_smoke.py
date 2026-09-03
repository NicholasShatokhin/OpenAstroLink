#!/usr/bin/env python3
from pathlib import Path
import os
import subprocess
import tarfile
import tempfile

root = Path(__file__).resolve().parents[1]
stage = root / "scripts" / "stage_qhy_cross_sdk.sh"


def elf_header(bits: int, machine: int) -> bytes:
    size = 64 if bits == 64 else 52
    elf = bytearray(size)
    elf[0:4] = b"\x7fELF"
    elf[4] = 2 if bits == 64 else 1
    elf[5] = 1
    elf[6] = 1
    elf[16:18] = (3).to_bytes(2, "little")  # ET_DYN
    elf[18:20] = machine.to_bytes(2, "little")
    elf[20:24] = (1).to_bytes(4, "little")
    return bytes(elf)


def make_archive(repo: Path, name: str, bits: int, machine: int, libname: str) -> Path:
    pkg = repo.parent / (name + "-pkg")
    (pkg / "include").mkdir(parents=True)
    (pkg / "lib").mkdir(parents=True)
    (pkg / "include" / "qhyccd.h").write_text("// synthetic qhy header\n")
    versioned = pkg / "lib" / libname
    versioned.write_bytes(elf_header(bits, machine))
    archive = repo / name
    with tarfile.open(archive, "w:gz", dereference=False) as tf:
        tf.add(pkg, arcname="qhyccdsdk-test")
    return archive


with tempfile.TemporaryDirectory(prefix="oal-qhy-stage-") as td_s:
    td = Path(td_s)
    repo = td / "repo"
    repo.mkdir()

    # Reproduce the real legacy QHYCCD_Linux_New surprise: an archive called
    # armv8 contains a 32-bit ARM EABI library (EM_ARM=40), not AArch64.
    legacy = make_archive(
        repo,
        "qhyccdsdk-v2.0.11-Linux-Debian-Ubuntu-armv8.tar.gz",
        32,
        40,
        "libqhyccd.so.2.0.11",
    )

    arm64_dest = td / "arm64-dest"
    failed = subprocess.run(
        [str(stage), "arm64", str(repo), str(arm64_dest)],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    assert failed.returncode != 0
    assert "32-bit ARM EABI" in failed.stdout or "ELF 32-bit" in failed.stdout
    assert "not AArch64" in failed.stdout

    # The same misleading legacy archive is valid for an ARMHF target because
    # actual ELF architecture, not filename, is the authority.
    armhf_dest = td / "armhf-dest"
    subprocess.run([str(stage), "armhf", str(repo), str(armhf_dest)], check=True)
    assert (armhf_dest / "include" / "qhyccd.h").is_file()
    armhf_desc = subprocess.check_output(
        ["file", "-Lb", str(armhf_dest / "lib" / "libqhy.so")], text=True
    )
    assert "ELF 32-bit" in armhf_desc and "ARM" in armhf_desc

    # A genuine AArch64 archive should then stage cleanly for ARM64.  Test a
    # direct archive path as well as directory discovery.
    genuine = make_archive(
        repo,
        "sdk_arm64_26.06.04.tgz",
        64,
        183,  # EM_AARCH64
        "libqhyccd.so.26.6.4",
    )
    arm64_dest2 = td / "arm64-dest2"
    subprocess.run([str(stage), "arm64", str(genuine), str(arm64_dest2)], check=True)
    assert (arm64_dest2 / "include" / "qhyccd.h").is_file()
    desc = subprocess.check_output(
        ["file", "-Lb", str(arm64_dest2 / "lib" / "libqhy.so")], text=True
    )
    assert "ARM aarch64" in desc or "aarch64" in desc.lower(), desc
    meta = (arm64_dest2 / "OAL_QHY_SDK.txt").read_text()
    assert "target_arch=arm64" in meta
    assert str(genuine) in meta

    # The new official QHY packaging may expose a static libqhyccd.a.  The
    # stager must inspect an object inside the archive instead of trusting the
    # .a filename.
    static_pkg = td / "static-pkg"
    (static_pkg / "usr" / "local" / "include").mkdir(parents=True)
    (static_pkg / "usr" / "local" / "lib").mkdir(parents=True)
    (static_pkg / "usr" / "local" / "include" / "qhyccd.h").write_text("// static qhy header\n")
    obj = td / "qhy_aarch64.o"
    rel = bytearray(elf_header(64, 183))
    rel[16:18] = (1).to_bytes(2, "little")  # ET_REL
    obj.write_bytes(bytes(rel))
    static_lib = static_pkg / "usr" / "local" / "lib" / "libqhyccd.a"
    subprocess.run(["ar", "rcs", str(static_lib), str(obj)], check=True)
    static_archive = repo / "sdk_linux_arm64_26.06.04.tar.gz"
    with tarfile.open(static_archive, "w:gz") as tf:
        tf.add(static_pkg, arcname="sdk_linux_arm64_26.06.04")
    static_dest = td / "arm64-static-dest"
    subprocess.run([str(stage), "arm64", str(static_archive), str(static_dest)], check=True)
    assert (static_dest / "include" / "qhyccd.h").is_file()
    assert (static_dest / "lib" / "libqhyccd.a").is_file()
    static_meta = (static_dest / "OAL_QHY_SDK.txt").read_text()
    assert "libqhyccd.a" in static_meta

print("qhy cross stage smoke: PASS")
