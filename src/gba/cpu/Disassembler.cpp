// This file is a part of Chroma.
// Copyright (C) 2018 Matthew Murray
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

#include <bitset>
#include <stdexcept>

#include "gba/cpu/Disassembler.h"
#include "gba/cpu/Cpu.h"
#include "gba/cpu/Instruction.h"

namespace Gba {

Disassembler::Disassembler(LogLevel level, Core& _core)
        : core(_core)
        , thumb_instructions(GetThumbInstructionTable<Disassembler>())
        , arm_instructions(GetArmInstructionTable<Disassembler>())
        , alt_level(level) {
    // Leave log_stream unopened if logging disabled.
    if (level != LogLevel::None) {
        log_stream = std::ofstream("log.txt");

        if (!log_stream) {
            throw std::runtime_error("Error when attempting to open ./log.txt for writing.");
        }
    }
}

// Needed to declare std::vector with forward-declared type in the header file.
Disassembler::~Disassembler() = default;

void Disassembler::DisassembleThumb(Thumb opcode, const std::array<u32, 16>& regs, u32 cpsr) {
    if (log_level == LogLevel::None) {
        return;
    }

    for (const auto& instr : thumb_instructions) {
        if (instr.Match(opcode)) {
            fmt::print(log_stream, "0x{:0>8X}, T: {}\n", regs[pc], instr.impl_func(*this, opcode));
            break;
        }
    }

    if (log_level == LogLevel::Registers) {
        LogRegisters(regs, cpsr);
    }
}

void Disassembler::DisassembleArm(Arm opcode, const std::array<u32, 16>& regs, u32 cpsr) {
    if (log_level == LogLevel::None) {
        return;
    }

    for (const auto& instr : arm_instructions) {
        if (instr.Match(opcode)) {
            fmt::print(log_stream, "0x{:0>8X}, A: {}\n", regs[pc], instr.impl_func(*this, opcode));
            break;
        }
    }

    if (log_level == LogLevel::Registers) {
        LogRegisters(regs, cpsr);
    }
}

void Disassembler::LogRegisters(const std::array<u32, 16>& regs, u32 cpsr) {
    for (int i = 0; i < 13; ++i) {
        fmt::print(log_stream, "R{:X}=0x{:0>8X}, ", i, regs[i]);
        if (i == 4 || i == 9) {
            fmt::print(log_stream, "\n");
        }
    }

    fmt::print(log_stream, "SP=0x{:0>8X}, ", regs[sp]);
    fmt::print(log_stream, "LR=0x{:0>8X}, ", regs[lr]);
    fmt::print(log_stream, "{}", (cpsr & 0x8000'0000) ? "N" : "");
    fmt::print(log_stream, "{}", (cpsr & 0x4000'0000) ? "Z" : "");
    fmt::print(log_stream, "{}", (cpsr & 0x2000'0000) ? "C" : "");
    fmt::print(log_stream, "{}\n\n", (cpsr & 0x1000'0000) ? "V" : "");
}

void Disassembler::LogHalt() {
    if (log_level != LogLevel::None) {
        fmt::print(log_stream, "Halted for {} cycles\n", halt_cycles);
    }

    halt_cycles = 0;
}

void Disassembler::SwitchLogLevel() {
    if (log_level == alt_level) {
        return;
    }

    std::swap(log_level, alt_level);

    auto LogLevelString = [](LogLevel level) {
        switch (level) {
        case LogLevel::None:
            return "None";
        case LogLevel::Trace:
            return "Trace";
        case LogLevel::Registers:
            return "Registers";
        default:
            return "";
        }
    };

    fmt::print(log_stream, "Log level changed to {}\n", LogLevelString(log_level));
    fmt::print("Log level changed to {}\n", LogLevelString(log_level));
}

std::string Disassembler::RegStr(Reg r) {
    switch (r) {
    case sp:
        return "SP";
    case lr:
        return "LR";
    case pc:
        return "PC";
    default:
        return fmt::format("R{}", r);
    }
}

std::string Disassembler::ShiftStr(ImmediateShift shift) {
    if (shift.imm == 0) {
        return "";
    } else {
        return fmt::format(", {} #0x{:X}", shift.type, shift.imm);
    }
}

std::string Disassembler::ListStr(u32 reg_list) {
    const std::bitset<16> rlist{reg_list};
    Reg highest_reg = HighestSetBit(reg_list);
    std::string list_str{"{"};

    for (Reg i = 0; i < highest_reg + 1; ++i) {
        if (rlist[i]) {
            if (i == highest_reg) {
                list_str += RegStr(i) + "}";
            } else {
                list_str += RegStr(i) + ", ";
            }
        }
    }

    return list_str;
}

std::string Disassembler::AddrOffset(bool pre_indexed, bool add, bool wb, u32 imm) {
    if (pre_indexed) {
        if (imm == 0 && !wb) {
            return "]";
        } else {
            return fmt::format(", #{}0x{:X}]{}", (add) ? "+" : "-", imm, (wb) ? "!" : "");
        }
    } else {
        return fmt::format("], #{}0x{:X}", (add) ? "+" : "-", imm);
    }
}

std::string Disassembler::StatusReg(bool spsr, u32 mask) {
    std::string psr;

    if (spsr) {
        psr = "SPSR_";
    } else {
        psr = "CPSR_";
    }

    if (mask & 0x1) {
        psr += "c";
    }

    if (mask & 0x8){
        psr += "f";
    }

    return psr;
}

} // End namespace Gba
