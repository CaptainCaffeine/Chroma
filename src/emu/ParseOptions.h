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

#pragma once

#include <string>
#include <vector>

#include "common/CommonTypes.h"

namespace Emu {

std::vector<std::string> GetTokens(char** begin, char** end);
bool ContainsOption(std::vector<std::string> tokens, std::string option);
std::string GetOptionParam(std::vector<std::string> tokens, std::string option);

void DisplayHelp();

std::vector<u8> LoadROM(std::string filename);

} // End namespace Emu