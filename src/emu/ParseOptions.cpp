// This file is a part of Chroma.
// Copyright (C) 2016-2021 Matthew Murray
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

#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <filesystem>
#include <fmt/format.h>

#include "gb/memory/CartridgeHeader.h"
#include "gba/memory/Memory.h"
#include "emu/ParseOptions.h"

namespace Emu {

std::vector<std::string> GetTokens(char** begin, char** end) {
    std::vector<std::string> tokens;
    for (; begin != end; ++begin) {
        tokens.emplace_back(*begin);
    }

    return tokens;
}

bool ContainsOption(const std::vector<std::string>& tokens, const std::string& option) {
    return std::find(tokens.cbegin(), tokens.cend(), option) != tokens.cend();
}

std::string GetOptionParam(const std::vector<std::string>& tokens, const std::string& option) {
    auto itr = std::find(tokens.cbegin(), tokens.cend(), option);
    if (itr != tokens.cend() && ++itr != tokens.cend()) {
        return *itr;
    }

    return "";
}

void DisplayHelp() {
    fmt::print("Usage: chroma [options] <path/to/rom>\n\n");
    fmt::print("Options:\n");
    fmt::print("  -h                           display help\n");
    fmt::print("  -m [dmg, cgb, agb]           specify device to emulate\n");
    fmt::print("  -l [trace, regs]             specify log level (default: none)\n");
    fmt::print("  -s [1-15]                    specify resolution scale (default: 2)\n");
    fmt::print("  -f                           activate fullscreen mode\n");
    fmt::print("  --filter [iir, nearest]      choose audio filtering method (default: iir)\n");
    fmt::print("                                   IIR (slow, better quality)\n");
    fmt::print("                                   nearest-neighbour (fast, lesser quality)\n");
    fmt::print("  --multicart                  emulate this game using an MBC1M\n");
}

Gb::Console GetGameBoyType(const std::vector<std::string>& tokens) {
    const std::string gb_string = Emu::GetOptionParam(tokens, "-m");
    if (!gb_string.empty()) {
        if (gb_string == "dmg") {
            return Gb::Console::DMG;
        } else if (gb_string == "cgb") {
            return Gb::Console::CGB;
        } else if (gb_string == "agb") {
            return Gb::Console::AGB;
        } else {
            throw std::invalid_argument("Invalid console specified: " + gb_string);
        }
    } else {
        // If no console specified, the console type will default to the cart type.
        return Gb::Console::Default;
    }
}

LogLevel GetLogLevel(const std::vector<std::string>& tokens) {
    const std::string log_string = Emu::GetOptionParam(tokens, "-l");
    if (!log_string.empty()) {
        if (log_string == "trace") {
            return LogLevel::Trace;
        } else if (log_string == "regs" || log_string == "registers") {
            return LogLevel::Registers;
        } else {
            // Passing the "-l" argument by itself defaults to instruction trace logging.
            return LogLevel::Trace;
        }
    } else {
        // If no log level specified, then no logging by default.
        return LogLevel::None;
    }
}

unsigned int GetPixelScale(const std::vector<std::string>& tokens) {
    const std::string scale_string = Emu::GetOptionParam(tokens, "-s");
    if (!scale_string.empty()) {
        unsigned int scale = std::stoi(scale_string);
        if (scale > 15) {
            throw std::invalid_argument("Invalid scale value specified: " + scale_string);
        }

        return scale;
    } else {
        // If no resolution scale specified, default to 2x native resolution.
        return 2;
    }
}

bool GetFilterEnable(const std::vector<std::string>& tokens) {
    const std::string filter_string = Emu::GetOptionParam(tokens, "--filter");
    if (!filter_string.empty()) {
        if (filter_string == "iir") {
            return true;
        } else if (filter_string == "nearest") {
            return false;
        } else {
            throw std::invalid_argument("Invalid filter method specified: " + filter_string);
        }
    } else {
        // If no filter specified, default to using IIR filter.
        return true;
    }
}

Gb::Console CheckRomFile(const std::string& rom_path) {
    std::ifstream rom_file(rom_path);
    if (!rom_file) {
        throw std::runtime_error("Error when attempting to open " + rom_path);
    }

    CheckPathIsRegularFile(rom_path);

    const auto rom_size = std::filesystem::file_size(rom_path);

    if (rom_size > 0x2000000) {
        // 32MB is the largest possible GBA game.
        throw std::runtime_error("Rom size of " + std::to_string(rom_size)
                                 + " bytes is too large to be a GB or GBA game.");
    } else if (rom_size < 0x134) {
        // Provided file is not large enough to contain a DMG Nintendo logo.
        throw std::runtime_error("Rom size of " + std::to_string(rom_size)
                                 + " bytes is too small to be a GB or GBA game.");
    }

    // Read the first 0x134 bytes to check for the Nintendo logos.
    std::vector<u8> rom_header(0x134);
    rom_file.read(reinterpret_cast<char*>(rom_header.data()), rom_header.size());

    if (Gba::Memory::CheckNintendoLogo(rom_header)) {
        return Gb::Console::AGB;
    } else if (Gb::CartridgeHeader::CheckNintendoLogo(Gb::Console::CGB, rom_header)) {
        if (rom_size < 0x8000) {
            // 32KB is the smallest possible GB game.
            throw std::runtime_error("Rom size of " + std::to_string(rom_size)
                                     + " bytes is too small to be a GB game.");
        }

        return Gb::Console::CGB;
    } else {
        throw std::runtime_error("Provided ROM is neither a GB or GBA game. No valid Nintendo logo found.");
    }

    return Gb::Console::CGB;
}

template<typename T>
std::vector<T> LoadRom(const std::string& rom_path) {
    std::ifstream rom_file(rom_path);
    if (!rom_file) {
        throw std::runtime_error("Error when attempting to open " + rom_path);
    }

    const auto rom_size = std::filesystem::file_size(rom_path);

    std::vector<T> rom_contents(rom_size / sizeof(T));
    rom_file.read(reinterpret_cast<char*>(rom_contents.data()), rom_size);

    return rom_contents;
}

template std::vector<u8> LoadRom<u8>(const std::string& rom_path);
template std::vector<u16> LoadRom<u16>(const std::string& rom_path);

std::string SaveGamePath(const std::string& rom_path) {
    std::size_t last_dot = rom_path.rfind('.');

    if (last_dot == std::string::npos) {
        throw std::runtime_error("No file extension found.");
    }

    if (rom_path.substr(last_dot, rom_path.size()) == ".sav") {
        throw std::runtime_error("You tried to run a save file instead of a ROM.");
    }

    return rom_path.substr(0, last_dot) + ".sav";
}

std::vector<u32> LoadGbaBios() {
    std::string bios_path = "gba_bios.bin";
    std::ifstream bios_file(bios_path);
    for (int i = 0; i < 2; ++i) {
        if (bios_file) {
            break;
        }

        bios_file.close();
        bios_path = "../" + bios_path;
        bios_file.open(bios_path);
    }

    if (!bios_file) {
        throw std::runtime_error("Error when attempting to open gba_bios.bin");
    }

    CheckPathIsRegularFile(bios_path);

    const auto bios_size = std::filesystem::file_size(bios_path);

    if (bios_size != 0x4000) {
        throw std::runtime_error("GBA BIOS must be 16KB. Provided file is " + std::to_string(bios_size) + " bytes.");
    }

    std::vector<u32> bios_contents(bios_size / sizeof(u32));
    bios_file.read(reinterpret_cast<char*>(bios_contents.data()), bios_size);

    return bios_contents;
}

void CheckPathIsRegularFile(const std::string& filename) {
    // Check that the provided path points to a regular file.
    if (std::filesystem::is_directory(filename)) {
        throw std::runtime_error("Provided path is a directory: " + filename);
    } else if (!std::filesystem::is_regular_file(filename)) {
        throw std::runtime_error("Provided path is not a regular file: " + filename);
    }
}

} // End namespace Emu
