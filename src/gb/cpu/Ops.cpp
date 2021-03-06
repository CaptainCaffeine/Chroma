// This file is a part of Chroma.
// Copyright (C) 2016-2017 Matthew Murray
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <stdexcept>

#include "gb/cpu/Cpu.h"
#include "gb/memory/Memory.h"
#include "gb/core/GameBoy.h"

namespace Gb {

// 8-bit Load operations
void Cpu::Load8Immediate(Reg8Addr r, u8 val) {
    regs.reg8[r] = val;
}

void Cpu::Load8(Reg8Addr r1, Reg8Addr r2) {
    regs.reg8[r1] = regs.reg8[r2];
}

void Cpu::Load8FromMem(Reg8Addr r, u16 addr) {
    regs.reg8[r] = ReadMemAndTick(addr);
}

void Cpu::Load8IntoMemImmediate(u16 addr, u8 val) {
    WriteMemAndTick(addr, val);
}

void Cpu::Load8IntoMem(u16 addr, Reg8Addr r) {
    WriteMemAndTick(addr, regs.reg8[r]);
}

// 16-bit Load operations
void Cpu::Load16Immediate(Reg16Addr r, u16 val) {
    regs.reg16[r] = val;
}

void Cpu::LoadHLIntoSP() {
    regs.reg16[SP] = regs.reg16[HL];
    gameboy.HardwareTick(4);
}

void Cpu::LoadSPnIntoHL(s8 val) {
    // The half carry & carry flags for this instruction are set by adding the value as an *unsigned* byte to the
    // lower byte of sp. The addition itself is done with the value as a signed byte.
    SetZero(false);
    SetSub(false);
    SetHalf(((regs.reg16[SP] & 0x000F) + (static_cast<u8>(val) & 0x0F)) & 0x0010);
    SetCarry(((regs.reg16[SP] & 0x00FF) + static_cast<u8>(val)) & 0x0100);

    // val will be sign-extended.
    regs.reg16[HL] = regs.reg16[SP] + val;
    
    // Internal delay
    gameboy.HardwareTick(4);
}

void Cpu::LoadSPIntoMem(u16 addr) {
    WriteMemAndTick(addr, regs.reg8[ToReg8AddrLo(SP)]);
    WriteMemAndTick(addr + 1, regs.reg8[ToReg8AddrHi(SP)]);
}

void Cpu::Push(Reg16Addr r) {
    // Internal delay
    gameboy.HardwareTick(4);

    WriteMemAndTick(--regs.reg16[SP], regs.reg8[ToReg8AddrHi(r)]);
    WriteMemAndTick(--regs.reg16[SP], regs.reg8[ToReg8AddrLo(r)]);
}

void Cpu::Pop(Reg16Addr r) {
    regs.reg8[ToReg8AddrLo(r)] = ReadMemAndTick(regs.reg16[SP]++);
    regs.reg8[ToReg8AddrHi(r)] = ReadMemAndTick(regs.reg16[SP]++);

    if (r == AF) {
        // The low nybble of the flags register is always 0.
        regs.reg8[F] &= 0xF0;
    }
}

// 8-bit Add operations
void Cpu::AddImmediate(u8 val) {
    u16 tmp16 = regs.reg8[A] + val;
    SetHalf(((regs.reg8[A] & 0x0F) + (val & 0x0F)) & 0x10);
    SetCarry(tmp16 & 0x0100);
    SetZero(!(tmp16 & 0x00FF));
    SetSub(false);

    regs.reg8[A] = static_cast<u8>(tmp16);
}

void Cpu::Add(Reg8Addr r) {
    AddImmediate(regs.reg8[r]);
}

void Cpu::AddFromMemAtHL() {
    AddImmediate(ReadMemAndTick(regs.reg16[HL]));
}

void Cpu::AddImmediateWithCarry(u8 val) {
    u16 tmp16 = regs.reg8[A] + val + Carry();
    SetHalf(((regs.reg8[A] & 0x0F) + (val & 0x0F) + Carry()) & 0x10);
    SetCarry(tmp16 & 0x0100);
    SetZero(!(tmp16 & 0x00FF));
    SetSub(false);

    regs.reg8[A] = static_cast<u8>(tmp16);
}

void Cpu::AddWithCarry(Reg8Addr r) {
    AddImmediateWithCarry(regs.reg8[r]);
}

void Cpu::AddFromMemAtHLWithCarry() {
    AddImmediateWithCarry(ReadMemAndTick(regs.reg16[HL]));
}

// 8-bit Subtract operations
void Cpu::SubImmediate(u8 val) {
    SetHalf((regs.reg8[A] & 0x0F) < (val & 0x0F));
    SetCarry(regs.reg8[A] < val);
    SetSub(true);

    regs.reg8[A] -= val;
    SetZero(!regs.reg8[A]);
}

void Cpu::Sub(Reg8Addr r) {
    SubImmediate(regs.reg8[r]);
}

void Cpu::SubFromMemAtHL() {
    SubImmediate(ReadMemAndTick(regs.reg16[HL]));
}

void Cpu::SubImmediateWithCarry(u8 val) {
    u8 carry_val = Carry();
    SetHalf((regs.reg8[A] & 0x0F) < (val & 0x0F) + carry_val);
    SetCarry(regs.reg8[A] < val + carry_val);
    SetSub(true);

    regs.reg8[A] -= val + carry_val;
    SetZero(!regs.reg8[A]);
}

void Cpu::SubWithCarry(Reg8Addr r) {
    SubImmediateWithCarry(regs.reg8[r]);
}

void Cpu::SubFromMemAtHLWithCarry() {
    SubImmediateWithCarry(ReadMemAndTick(regs.reg16[HL]));
}

void Cpu::IncReg8(Reg8Addr r) {
    SetHalf((regs.reg8[r] & 0x0F) == 0x0F);
    ++regs.reg8[r];
    SetZero(!regs.reg8[r]);
    SetSub(false);
}

void Cpu::IncMemAtHL() {
    u8 val = ReadMemAndTick(regs.reg16[HL]);

    SetHalf((val & 0x0F) == 0x0F);
    ++val;
    SetZero(!val);
    SetSub(false);

    WriteMemAndTick(regs.reg16[HL], val);
}

void Cpu::DecReg8(Reg8Addr r) {
    SetHalf((regs.reg8[r] & 0x0F) == 0x00);
    --regs.reg8[r];
    SetZero(!regs.reg8[r]);
    SetSub(true);
}

void Cpu::DecMemAtHL() {
    u8 val = ReadMemAndTick(regs.reg16[HL]);

    SetHalf((val & 0x0F) == 0x00);
    --val;
    SetZero(!val);
    SetSub(true);

    WriteMemAndTick(regs.reg16[HL], val);
}

// Logical operations
void Cpu::AndImmediate(u8 val) {
    regs.reg8[A] &= val;

    SetZero(!regs.reg8[A]);
    SetSub(false);
    SetHalf(true);
    SetCarry(false);
}

void Cpu::And(Reg8Addr r) {
    AndImmediate(regs.reg8[r]);
}

void Cpu::AndFromMemAtHL() {
    AndImmediate(ReadMemAndTick(regs.reg16[HL]));
}

// Bitwise Or operations
void Cpu::OrImmediate(u8 val) {
    regs.reg8[A] |= val;

    SetZero(!regs.reg8[A]);
    SetSub(false);
    SetHalf(false);
    SetCarry(false);
}

void Cpu::Or(Reg8Addr r) {
    OrImmediate(regs.reg8[r]);
}

void Cpu::OrFromMemAtHL() {
    OrImmediate(ReadMemAndTick(regs.reg16[HL]));
}

// Bitwise Xor operations
void Cpu::XorImmediate(u8 val) {
    regs.reg8[A] ^= val;

    SetZero(!regs.reg8[A]);
    SetSub(false);
    SetHalf(false);
    SetCarry(false);
}

void Cpu::Xor(Reg8Addr r) {
    XorImmediate(regs.reg8[r]);
}

void Cpu::XorFromMemAtHL() {
    XorImmediate(ReadMemAndTick(regs.reg16[HL]));
}

// Compare operations
void Cpu::CompareImmediate(u8 val) {
    SetZero(!(regs.reg8[A] - val));
    SetSub(true);
    SetHalf((regs.reg8[A] & 0x0F) < (val & 0x0F));
    SetCarry(regs.reg8[A] < val);
}

void Cpu::Compare(Reg8Addr r) {
    CompareImmediate(regs.reg8[r]);
}

void Cpu::CompareFromMemAtHL() {
    CompareImmediate(ReadMemAndTick(regs.reg16[HL]));
}

// 16-bit Arithmetic operations
void Cpu::AddHL(Reg16Addr r) {
    SetSub(false);
    SetHalf(((regs.reg16[HL] & 0x0FFF) + (regs.reg16[r] & 0x0FFF)) & 0x1000);
    SetCarry((regs.reg16[HL] + regs.reg16[r]) & 0x10000);
    regs.reg16[HL] += regs.reg16[r];

    gameboy.HardwareTick(4);
}

void Cpu::AddSP(s8 val) {
    // The half carry & carry flags for this instruction are set by adding the value as an *unsigned* byte to the
    // lower byte of sp. The addition itself is done with the value as a signed byte.
    SetZero(false);
    SetSub(false);

    SetHalf(((regs.reg16[SP] & 0x000F) + (static_cast<u8>(val) & 0x0F)) & 0x0010);
    SetCarry(((regs.reg16[SP] & 0x00FF) + static_cast<u8>(val)) & 0x0100);

    regs.reg16[SP] += val;

    // Two internal delays.
    gameboy.HardwareTick(8);
}

void Cpu::IncReg16(Reg16Addr r) {
    ++regs.reg16[r];
    gameboy.HardwareTick(4);
}

void Cpu::DecReg16(Reg16Addr r) {
    --regs.reg16[r];
    gameboy.HardwareTick(4);
}

// Miscellaneous arithmetic
void Cpu::DecimalAdjustA() {
    if (Sub()) {
        if (Carry()) {
            regs.reg8[A] -= 0x60;
        }
        if (Half()) {
            regs.reg8[A] -= 0x06;
        }
    } else {
        if (Carry() || regs.reg8[A] > 0x99) {
            regs.reg8[A] += 0x60;
            SetCarry(true);
        }
        if (Half() || (regs.reg8[A] & 0x0F) > 0x09) {
            regs.reg8[A] += 0x06;
        }
    }
    SetZero(!regs.reg8[A]);
    SetHalf(false);
}

void Cpu::ComplementA() {
    regs.reg8[A] = ~regs.reg8[A];
    SetSub(true);
    SetHalf(true);
}

void Cpu::SetCarry() {
    SetCarry(true);
    SetSub(false);
    SetHalf(false);
}

void Cpu::ComplementCarry() {
    SetCarry(!Carry());
    SetSub(false);
    SetHalf(false);
}

// Rotates and Shifts
void Cpu::RotateLeft(Reg8Addr r) {
    SetCarry(regs.reg8[r] & 0x80);

    regs.reg8[r] = (regs.reg8[r] << 1) | (regs.reg8[r] >> 7);

    SetZero(!regs.reg8[r]);
    SetSub(false);
    SetHalf(false);
}

void Cpu::RotateLeftMemAtHL() {
    u8 val = ReadMemAndTick(regs.reg16[HL]);
    SetCarry(val & 0x80);

    val = (val << 1) | (val >> 7);

    SetZero(!val);
    SetSub(false);
    SetHalf(false);

    WriteMemAndTick(regs.reg16[HL], val);
}

void Cpu::RotateLeftThroughCarry(Reg8Addr r) {
    u8 carry_val = regs.reg8[r] & 0x80;
    regs.reg8[r] = (regs.reg8[r] << 1) | Carry();

    SetZero(!regs.reg8[r]);
    SetSub(false);
    SetHalf(false);
    SetCarry(carry_val);
}

void Cpu::RotateLeftMemAtHLThroughCarry() {
    u8 val = ReadMemAndTick(regs.reg16[HL]);
    u8 carry_val = val & 0x80;

    val = (val << 1) | Carry();

    SetZero(!val);
    SetSub(false);
    SetHalf(false);
    SetCarry(carry_val);

    WriteMemAndTick(regs.reg16[HL], val);
}

void Cpu::RotateRight(Reg8Addr r) {
    SetCarry(regs.reg8[r] & 0x01);

    regs.reg8[r] = (regs.reg8[r] >> 1) | (regs.reg8[r] << 7);

    SetZero(!regs.reg8[r]);
    SetSub(false);
    SetHalf(false);
}

void Cpu::RotateRightMemAtHL() {
    u8 val = ReadMemAndTick(regs.reg16[HL]);
    SetCarry(val & 0x01);

    val = (val >> 1) | (val << 7);

    SetZero(!val);
    SetSub(false);
    SetHalf(false);

    WriteMemAndTick(regs.reg16[HL], val);
}

void Cpu::RotateRightThroughCarry(Reg8Addr r) {
    u8 carry_val = regs.reg8[r] & 0x01;
    regs.reg8[r] = (regs.reg8[r] >> 1) | (Carry() << 7);

    SetZero(!regs.reg8[r]);
    SetSub(false);
    SetHalf(false);
    SetCarry(carry_val);
}

void Cpu::RotateRightMemAtHLThroughCarry() {
    u8 val = ReadMemAndTick(regs.reg16[HL]);
    u8 carry_val = val & 0x01;

    val = (val >> 1) | (Carry() << 7);

    SetZero(!val);
    SetSub(false);
    SetHalf(false);
    SetCarry(carry_val);

    WriteMemAndTick(regs.reg16[HL], val);
}

void Cpu::ShiftLeft(Reg8Addr r) {
    SetCarry(regs.reg8[r] & 0x80);

    regs.reg8[r] <<= 1;

    SetZero(!regs.reg8[r]);
    SetSub(false);
    SetHalf(false);
}

void Cpu::ShiftLeftMemAtHL() {
    u8 val = ReadMemAndTick(regs.reg16[HL]);
    SetCarry(val & 0x80);

    val <<= 1;

    SetZero(!val);
    SetSub(false);
    SetHalf(false);

    WriteMemAndTick(regs.reg16[HL], val);
}

void Cpu::ShiftRightArithmetic(Reg8Addr r) {
    SetCarry(regs.reg8[r] & 0x01);

    regs.reg8[r] >>= 1;
    regs.reg8[r] |= (regs.reg8[r] & 0x40) << 1;

    SetZero(!regs.reg8[r]);
    SetSub(false);
    SetHalf(false);
}

void Cpu::ShiftRightArithmeticMemAtHL() {
    u8 val = ReadMemAndTick(regs.reg16[HL]);
    SetCarry(val & 0x01);

    val >>= 1;
    val |= (val & 0x40) << 1;

    SetZero(!val);
    SetSub(false);
    SetHalf(false);

    WriteMemAndTick(regs.reg16[HL], val);
}

void Cpu::ShiftRightLogical(Reg8Addr r) {
    SetCarry(regs.reg8[r] & 0x01);

    regs.reg8[r] >>= 1;

    SetZero(!regs.reg8[r]);
    SetSub(false);
    SetHalf(false);
}

void Cpu::ShiftRightLogicalMemAtHL() {
    u8 val = ReadMemAndTick(regs.reg16[HL]);
    SetCarry(val & 0x01);

    val >>= 1;

    SetZero(!val);
    SetSub(false);
    SetHalf(false);

    WriteMemAndTick(regs.reg16[HL], val);
}

void Cpu::SwapNybbles(Reg8Addr r) {
    regs.reg8[r] = (regs.reg8[r] << 4) | (regs.reg8[r] >> 4);

    SetZero(!regs.reg8[r]);
    SetSub(false);
    SetHalf(false);
    SetCarry(false);
}

void Cpu::SwapMemAtHL() {
    u8 val = ReadMemAndTick(regs.reg16[HL]);

    val = (val << 4) | (val >> 4);

    SetZero(!val);
    SetSub(false);
    SetHalf(false);
    SetCarry(false);

    WriteMemAndTick(regs.reg16[HL], val);
}

// Bit manipulation
void Cpu::TestBit(unsigned int bit, Reg8Addr r) {
    SetZero(!(regs.reg8[r] & (0x01 << bit)));
    SetSub(false);
    SetHalf(true);
}

void Cpu::TestBitOfMemAtHL(unsigned int bit) {
    SetZero(!(ReadMemAndTick(regs.reg16[HL]) & (0x01 << bit)));
    SetSub(false);
    SetHalf(true);
}

void Cpu::ResetBit(unsigned int bit, Reg8Addr r) {
    regs.reg8[r] &= ~(0x01 << bit);
}

void Cpu::ResetBitOfMemAtHL(unsigned int bit) {
    u8 val = ReadMemAndTick(regs.reg16[HL]);

    val &= ~(0x01 << bit);

    WriteMemAndTick(regs.reg16[HL], val);
}

void Cpu::SetBit(unsigned int bit, Reg8Addr r) {
    regs.reg8[r] |= (0x01 << bit);
}

void Cpu::SetBitOfMemAtHL(unsigned int bit) {
    u8 val = ReadMemAndTick(regs.reg16[HL]);

    val |= (0x01 << bit);

    WriteMemAndTick(regs.reg16[HL], val);
}

// Jumps
void Cpu::Jump(u16 addr) {
    // Internal delay
    gameboy.HardwareTick(4);

    pc = addr;
}

void Cpu::JumpToHL() {
    pc = regs.reg16[HL];
}

void Cpu::RelativeJump(s8 val) {
    // Internal delay
    gameboy.HardwareTick(4);

    pc += val;
}

// Calls and Returns
void Cpu::Call(u16 addr) {
    // Internal delay
    gameboy.HardwareTick(4);

    WriteMemAndTick(--regs.reg16[SP], static_cast<u8>(pc >> 8));
    WriteMemAndTick(--regs.reg16[SP], static_cast<u8>(pc));

    pc = addr;
}

void Cpu::Return() {
    u8 byte_lo = ReadMemAndTick(regs.reg16[SP]++);
    u8 byte_hi = ReadMemAndTick(regs.reg16[SP]++);

    pc = (static_cast<u16>(byte_hi) << 8) | static_cast<u16>(byte_lo);

    // Internal delay
    gameboy.HardwareTick(4);
}

// System Control
void Cpu::Halt() {
    if (!interrupt_master_enable && mem.RequestedEnabledInterrupts()) {
        // If interrupts are disabled and there are requested, enabled interrupts pending when HALT is executed,
        // the GB will not enter halt mode. Instead, the GB will fail to increase the PC when executing the next 
        // instruction, thus executing it twice.
        cpu_mode = CpuMode::HaltBug;
    } else {
        cpu_mode = CpuMode::Halted;
    }
}

void Cpu::Stop() {
    // STOP is a two-byte long opcode. If the opcode following STOP is not 0x00, the LCD supposedly turns on?
    ++pc;
    gameboy.HaltedTick(4);

    // Turn off the LCD.
    gameboy.StopLcd();

    // During STOP mode, the clock increases as usual, but normal interrupts are not serviced or checked. Regardless
    // if the joypad interrupt is enabled in the IE register, a stopped Game Boy will intercept any joypad presses
    // if the corresponding input lines in the P1 register are enabled.

    // Check if we should begin a speed switch.
    if (gameboy.GameModeCgb() && (mem.ReadMem(0xFF4D) & 0x01)) {
        // A speed switch takes 128*1024-80=130992 cycles to complete, plus 4 cycles to decode the STOP instruction.
        speed_switch_cycles = 130992;
    } else if ((mem.ReadMem(0xFF00) & 0x30) == 0x30) {
        throw std::runtime_error("The CPU has hung. Reason: STOP mode was entered with all joypad inputs disabled.");
    }

    cpu_mode = CpuMode::Stopped;
}

} // End namespace Gb
