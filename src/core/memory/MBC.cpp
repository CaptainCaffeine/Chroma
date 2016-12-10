// This file is a part of Chroma.
// Copyright (C) 2016 Matthew Murray
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

#include "core/memory/Memory.h"

namespace Core {

u8 Memory::ReadExternalRAM(const u16 addr) const {
    if (ext_ram_enabled) {
        u16 adjusted_addr = addr - 0xA000 + 0x2000*ram_bank_num;
        switch (mbc_mode) {
        case MBC::MBC1:
            // Out of bounds reads return 0xFF.
            if (adjusted_addr < ext_ram.size()) {
                return ext_ram[adjusted_addr];
            } else {
                return 0xFF;
            }
        case MBC::MBC2:
            // MBC2 RAM range is only A000-A1FF.
            if (adjusted_addr < ext_ram.size()) {
                return ext_ram[adjusted_addr] & 0xF0;
            } else {
                return 0xFF;
            }
        case MBC::MBC3:
            // RAM bank or RTC register?
            if (ram_bank_num & 0x08) {
                // Any address in the range will work to write the RTC registers.
                switch (ram_bank_num) {
                case 0x08:
                    return rtc_seconds;
                case 0x09:
                    return rtc_minutes;
                case 0x0A:
                    return rtc_hours;
                case 0x0B:
                    return rtc_day;
                case 0x0C:
                    return rtc_flags | 0x3E;
                default:
                    // I'm assuming an invalid register value (0x0D-0x0F) returns 0xFF, needs confirmation though.
                    return 0xFF;
                }
            } else {
                // Out of bounds reads return 0xFF.
                if (adjusted_addr < ext_ram.size()) {
                    return ext_ram[adjusted_addr];
                } else {
                    return 0xFF;
                }
            }
        case MBC::MBC5:
            if (rumble_present) {
                // Carts with rumble cannot use bit 4 of the RAM bank register for bank selection.
                adjusted_addr = addr - 0xA000 + 0x2000 * (ram_bank_num & 0x07);
            }

            // Out of bounds reads return 0xFF.
            if (adjusted_addr < ext_ram.size()) {
                return ext_ram[adjusted_addr];
            } else {
                return 0xFF;
            }

        default:
            return 0xFF;
        }
    } else {
        // Reads from this region when the RAM banks are disabled or not present return 0xFF.
        return 0xFF;
    }
}

void Memory::WriteExternalRAM(const u16 addr, const u8 data) {
    // Writes are ignored if external RAM is disabled or not present.
    if (ext_ram_enabled) {
        u16 adjusted_addr = addr - 0xA000 + 0x2000*ram_bank_num;
        switch (mbc_mode) {
        case MBC::MBC1:
            // Ignore out-of-bounds writes.
            if (adjusted_addr < ext_ram.size()) {
                ext_ram[adjusted_addr] = data;
            }
            break;
        case MBC::MBC2:
            // MBC2 RAM range is only A000-A1FF. Only the lower nibble of the bytes in this region are used.
            if (adjusted_addr < ext_ram.size()) {
                ext_ram[adjusted_addr] = data & 0x0F;
            }
            break;
        case MBC::MBC3:
            // RAM bank or RTC register?
            if (ram_bank_num & 0x08) {
                // Any address in the range will work to write the RTC registers.
                switch (ram_bank_num) {
                case 0x08:
                    rtc_seconds = data % 60;
                    break;
                case 0x09:
                    rtc_minutes = data % 60;
                    break;
                case 0x0A:
                    rtc_hours = data % 24;
                    break;
                case 0x0B:
                    rtc_day = data;
                    break;
                case 0x0C:
                    rtc_flags = data & 0xC1;
                    break;
                default:
                    // I'm assuming an invalid register value (0x0D-0x0F) is just ignored.
                    break;
                }
            } else {
                // Ignore out-of-bounds writes.
                if (adjusted_addr < ext_ram.size()) {
                    ext_ram[adjusted_addr] = data;
                }
            }
            break;
        case MBC::MBC5:
            if (rumble_present) {
                // Carts with rumble cannot use bit 4 of the RAM bank register for bank selection.
                adjusted_addr = addr - 0xA000 + 0x2000 * (ram_bank_num & 0x07);
            }

            // Ignore out-of-bounds writes.
            if (adjusted_addr < ext_ram.size()) {
                ext_ram[adjusted_addr] = data;
            }
            break;

        default:
            break;
        }
    }
}

void Memory::WriteMBCControlRegisters(const u16 addr, const u8 data) {
    switch (mbc_mode) {
    case MBC::MBC1:
        if (addr < 0x2000) {
            // RAM enable register -- RAM banking is enabled if a byte with lower nibble 0xA is written
            if (ext_ram_present && (data & 0x0F) == 0x0A) {
                ext_ram_enabled = true;
            } else {
                ext_ram_enabled = false;
            }
        } else if (addr < 0x4000) {
            // ROM bank register
            // Only the lower 5 bits of the written value are considered -- preserve the upper bits.
            rom_bank_num = (rom_bank_num & 0x60) | (data & 0x1F);

            // 0x00, 0x20, 0x40, 0x60 all map to 0x01, 0x21, 0x41, 0x61 respectively.
            if (rom_bank_num == 0x00 || rom_bank_num == 0x20 || rom_bank_num == 0x40 || rom_bank_num == 0x60) {
                ++rom_bank_num;
            }
        } else if (addr < 0x6000) {
            // RAM bank register (or upper bits ROM bank)
            // Only the lower 2 bits of the written value are considered.
            if (ram_bank_mode) {
                ram_bank_num = data & 0x03;
            } else {
                rom_bank_num = (rom_bank_num & 0x1F) | (data & 0x03) << 5;
            }
        } else if (addr < 0x8000) {
            // Memory mode -- selects whether the two bits in the above register act as the RAM bank number or 
            // the upper bits of the ROM bank number.
            ram_bank_mode = data & 0x01;
            if (ram_bank_mode) {
                // The 5th and 6th bits of the ROM bank number become the RAM bank number.
                ram_bank_num = (rom_bank_num & 0x60) >> 5;
                rom_bank_num &= 0x1F;
            } else {
                // The RAM bank number becomes the 5th and 6th bits of the ROM bank number.
                rom_bank_num |= ram_bank_num << 5;
                ram_bank_num = 0x00;
            }
        }
        break;
    case MBC::MBC2:
        if (addr < 0x2000) {
            // RAM enable register -- RAM banking is enabled if a byte with lower nibble 0xA is written
            // The least significant bit of the upper address byte must be zero to enable or disable external ram.
            if (!(addr & 0x0100)) {
                if (ext_ram_present && (data & 0x0F) == 0x0A) {
                    ext_ram_enabled = true;
                } else {
                    ext_ram_enabled = false;
                }
            }
        } else if (addr < 0x4000) {
            // ROM bank register -- The least significant bit of the upper address byte must be 1 to switch ROM banks.
            if (addr & 0x0100) {
                // Only the lower 4 bits of the written value are considered.
                rom_bank_num = data & 0x0F;
                if (rom_bank_num == 0) {
                    ++rom_bank_num;
                }
            }
        }
        // MBC2 does not have RAM banking.
        break;
    case MBC::MBC3:
        if (addr < 0x2000) {
            // RAM banking and RTC registers enable register -- enabled if a byte with lower nibble 0xA is written.
            if (ext_ram_present && (data & 0x0F) == 0x0A) {
                ext_ram_enabled = true;
            } else {
                ext_ram_enabled = false;
            }
        } else if (addr < 0x4000) {
            // ROM bank register
            // The 7 lower bits of the written value select the ROM bank to be used at 0x4000-0x7FFF.
            rom_bank_num = data & 0x7F;

            // Selecting 0x00 will select bank 0x01. Unlike MBC1, the banks 0x20, 0x40, and 0x60 can all be selected.
            if (rom_bank_num == 0x00) {
                ++rom_bank_num;
            }
        } else if (addr < 0x6000) {
            // RAM bank selection or RTC register selection register
            // Values 0x00-0x07 select one of the RAM banks, and values 0x08-0x0C select one of the RTC registers.
            ram_bank_num = data & 0x0F;
        } else if (addr < 0x8000) {
            // Latch RTC data.
            // Writing a 0x00 then a 0x01 latches the current time into the RTC registers. Some games don't always
            // write 0x00 before writing 0x01, and other games write 0x00 before and after writing a 0x01.
            // TODO: RTC unimplemented.
        }
        break;
    case MBC::MBC5:
        if (addr < 0x2000) {
            // RAM banking enable register -- enabled if a byte with lower nibble 0xA is written.
            if (ext_ram_present && (data & 0x0F) == 0x0A) {
                ext_ram_enabled = true;
            } else {
                ext_ram_enabled = false;
            }
        } else if (addr < 0x3000) {
            // Low byte ROM bank register
            // This register selects the low 8 bits of the ROM bank to be used at 0x4000-0x7FFF.
            // Unlike both MBC1 and MBC3, ROM bank 0 can be mapped here.
            rom_bank_num = (rom_bank_num & 0xFF00) | data;
        } else if (addr < 0x4000) {
            // High byte ROM bank register
            // This register selects the high 8 bits of the ROM bank to be used at 0x4000-0x7FFF.
            // There is only one official game known to use more than 256 ROM banks (Densha de Go! 2), and it only 
            // uses bit 0 of this register.
            rom_bank_num = (rom_bank_num & 0x00FF) | (static_cast<unsigned int>(data) << 8);
        } else if (addr < 0x6000) {
            // RAM bank selection.
            // Can have as many as 16 RAM banks. Carts with rumble activate it by writing 0x08 to this register, so
            // they cannot have more than 8 RAM banks.
            ram_bank_num = data & 0x0F;
        }
        break;

    default:
        // Carts with no MBC ignore writes here.
        break;
    }
}

} // End namespace Core