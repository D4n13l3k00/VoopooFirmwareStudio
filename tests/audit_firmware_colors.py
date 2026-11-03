"""Compare preview fixtures against actual ARM draw-call arguments in packed 3103.

Generate fixtures with VOOPOO_COLOR_AUDIT_DIR set while running --smoke-test.
Run: python tests/audit_firmware_colors.py <fixture directory>
Development-only dependencies: unicorn, capstone. No USB or device access.
"""

import argparse
import json
import struct
from pathlib import Path

from capstone import CS_ARCH_ARM, CS_MODE_THUMB, Cs
from unicorn import UC_ARCH_ARM, UC_HOOK_CODE, UC_MODE_THUMB, Uc
from unicorn.arm_const import (
    UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_R0, UC_ARM_REG_R1,
    UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_SP,
)

BASE = 0x08005800
STOP = 0x0801F000
BITMAP = 0x08009EDE
DIGIT = 0x08009A88
COLOR = 0x0800AC58
COPY = 0x08005B36
DECODED_CALLS = {}
REGS = (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3)
FONT = {6: (0x080130D2, 10), 7: (0x08013136, 13),
        11: (0x080149F6, 40), 19: (0x08013206, 120)}


def draw_calls(image, entry, args, mode, first_only=False):
    cpu = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
    cpu.mem_map(0x08000000, 0x40000)
    cpu.mem_write(BASE, image)
    cpu.mem_map(0x20000000, 0x10000)
    cpu.mem_map(0x1FFF0000, 0x10000)
    cpu.mem_write(0x200018B0, struct.pack('<I', 2 if mode else 0))
    cpu.mem_write(0x200018AC, struct.pack('<I', 1))  # ACTIVATED is visible
    cpu.reg_write(UC_ARM_REG_SP, 0x2000E000)
    cpu.reg_write(UC_ARM_REG_LR, STOP | 1)
    for reg, value in zip(REGS, args):
        cpu.reg_write(reg, value)
    decoder = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
    calls = []
    allocations = [0x20003000]

    def hook(uc, address, size, _):
        if address in (BITMAP, DIGIT, COPY, 0x0800EEBC):
            regs = [uc.reg_read(r) for r in REGS]
            sp = uc.reg_read(UC_ARM_REG_SP)
        if address in (BITMAP, DIGIT):
            words = struct.unpack('<III', uc.mem_read(sp, 12))
            if address == BITMAP:
                pointer = words[2]
                dimensions = (regs[2], regs[3])
            else:
                font_base, stride = FONT[regs[3]]
                pointer = font_base + min(regs[2], 15) * stride
                dimensions = None
            calls.append((pointer, words[0] & 0xFFFF, words[1] & 0xFFFF, dimensions))
            if first_only:
                uc.emu_stop()
            else:
                uc.reg_write(UC_ARM_REG_PC, uc.reg_read(UC_ARM_REG_LR))
            return
        if address == COPY:
            uc.mem_write(regs[0], bytes(uc.mem_read(regs[1], regs[2])))
            uc.reg_write(UC_ARM_REG_PC, uc.reg_read(UC_ARM_REG_LR))
            return
        if address == 0x0800EEBC:
            uc.mem_write(regs[0], struct.pack('<III', regs[1], regs[1], allocations[0]))
            allocations[0] += 0x100
            uc.reg_write(UC_ARM_REG_PC, uc.reg_read(UC_ARM_REG_LR))
            return
        # Run real caller instructions and the real RGB888->565 conversion.
        # Other callees are peripheral/delay/setup helpers, outside this audit.
        key = (address, bytes(uc.mem_read(address, size)))
        if key not in DECODED_CALLS:
            insn = next(decoder.disasm(key[1], address))
            DECODED_CALLS[key] = int(insn.op_str.lstrip('#'), 0) if insn.mnemonic in ('bl', 'blx') else None
        target = DECODED_CALLS[key]
        if target is not None and target not in (BITMAP, DIGIT, COLOR, COPY, 0x0800EEBC):
            uc.reg_write(UC_ARM_REG_R0, 0)
            uc.reg_write(UC_ARM_REG_PC, (address + size) | 1)

    cpu.hook_add(UC_HOOK_CODE, hook)
    try:
        cpu.emu_start(entry | 1, STOP, count=50000)
    except Exception as error:
        raise RuntimeError(f'Caller {entry:#x}, PC={cpu.reg_read(UC_ARM_REG_PC):#x}: {error}') from error
    if not first_only and cpu.reg_read(UC_ARM_REG_PC) != STOP:
        raise AssertionError(f'Caller did not return: {entry:#x}')
    if not calls:
        raise AssertionError(f'Caller made no draw calls: {entry:#x}')
    return calls


def audit(directory):
    total = 0
    for fixture in sorted(directory.glob('theme_*.json')):
        cases = json.loads(fixture.read_text())
        expected = {(c['address'], c['mode'], c['rank']): c for c in cases}
        image = fixture.with_suffix('.bin').read_bytes()
        covered = set()
        roots = [
            (0x08008024, ()), (0x08008564, ()), (0x080085D4, (0, 57, 0)),
            (0x080088D8, ()), (0x08008964, ()), (0x080089CC, ()),
            (0x08008A3C, ()), (0x08008AAC, ()), (0x08008B14, ()),
            (0x08008B7C, ()), (0x08008BE4, ()), (0x08008C54, ()),
            (0x08008CB8, ()), (0x0800AD58, (0, 0, 1)),
            (0x0800AD58, (1, 1, 1)), (0x0800AE78, (59, 1)),
            (0x0800AEF8, (12345, 123, 0, 1)),
            (0x0800AEF8, (12345, 123, 1, 1)),
            (0x0800B2F4, (123, 1)), (0x0800B42C, (1234, 1)),
            (0x0800BA80, (123, 1)), (0x0800DF88, (0x20001000,)),
        ]

        def compare(calls, mode, rank=0):
            nonlocal total
            for pointer, fg, bg, dimensions in calls:
                case = expected.get((pointer, mode, rank)) or expected.get((pointer, mode, 0))
                if case is None:
                    continue  # Firmware resources not exposed in the catalog.
                assert (fg, bg) == (case['fg'], case['bg']), (
                    f"{fixture.stem} {case['id']} mode={mode} rank={rank}: "
                    f"firmware=({fg:04x},{bg:04x}), preview=({case['fg']:04x},{case['bg']:04x})")
                if dimensions:
                    assert dimensions == (case['width'], case['height']), (
                        f"{case['id']}: firmware dimensions {dimensions} != catalog")
                covered.add(case['id'])
                total += 1

        for mode in (0, 1):
            for entry, args in roots:
                compare(draw_calls(image, entry, args, mode), mode)
            compare(draw_calls(image, 0x08007E94, (), mode, first_only=True), mode)
            # First score in each rank; all animation frames are additionally
            # covered by samples throughout the rank's score interval.
            thresholds = (0, 478, 2250, 6000, 11800, 21400, 31900, 46550, 99999)
            for rank in range(8):
                low, high = thresholds[rank:rank + 2]
                for score in range(low, high, max(1, (high - low) // 80)):
                    compare(draw_calls(image, 0x0800B5CC, (score * 1000, 1), mode), mode, rank)
        required = {c['id'] for c in cases}
        # A glyph family's color is identical for each digit/hex value. The
        # firmware calls above choose representative values from every family.
        font_families = {i.rsplit('_', 1)[0] for i in covered if i.startswith('font_')}
        missing = {i for i in required - covered if i.rsplit('_', 1)[0] not in font_families}
        assert not missing, f'{fixture.stem}: unverified resources: {sorted(missing)}'
        print(f'{fixture.stem}: ARM draw colors and dimensions match; {len(required)} mask assets')
    assert total, 'No fixtures found'
    print(f'ARM COLOR AUDIT OK: {total} draw calls')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    audit(parser.parse_args().directory)
