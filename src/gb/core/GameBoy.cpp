// This file is a part of Chroma.
// Copyright (C) 2016-2019 Matthew Murray
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

#include <chrono>

#include "gb/core/GameBoy.h"
#include "gb/cpu/Cpu.h"
#include "gb/memory/Memory.h"
#include "gb/memory/CartridgeHeader.h"
#include "gb/lcd/Lcd.h"
#include "gb/audio/Audio.h"
#include "gb/hardware/Timer.h"
#include "gb/hardware/Serial.h"
#include "gb/hardware/Joypad.h"
#include "gb/logging/Logging.h"
#include "emu/SdlContext.h"
#include "common/Screenshot.h"

namespace Gb {

GameBoy::GameBoy(const Console _console, const CartridgeHeader& header, Emu::SdlContext& context,
                 const std::string& save_path, const std::vector<u8>& rom, bool enable_iir, LogLevel log_level)
        : console(_console)
        , game_mode(header.game_mode)
        , timer(std::make_unique<Timer>(*this))
        , serial(std::make_unique<Serial>(*this))
        , lcd(std::make_unique<Lcd>(*this))
        , joypad(std::make_unique<Joypad>(*this))
        , audio(std::make_unique<Audio>(enable_iir, *this))
        , mem(std::make_unique<Memory>(header, rom, save_path, *this))
        , cpu(std::make_unique<Cpu>(*mem, *this))
        , logging(std::make_unique<Logging>(log_level, *this))
        , sdl_context(context)
        , front_buffer(160 * 144) {

    RegisterCallbacks();
}

// Needed to declare std::unique_ptr with forward-declared type in the header file.
GameBoy::~GameBoy() = default;

void GameBoy::EmulatorLoop() {
    // The Game Boy executes exactly 70224 cycles per frame. However, the display runs at a rate of ~59.7275Hz
    // instead of 60Hz, so on a 60Hz monitor we need to execute 69905 cycles per frame to run at the correct speed.
    // Unfortunately, the sample rate that gives us does not resample nicely to 800 samples per frame at all. So
    // instead we execute 69920 cycles per frame, which is very close to the correct speed and resamples much
    // better to our target sample rate.
    static constexpr int cycles_per_frame = 69920;
    int overspent_cycles = 0;

    sdl_context.UnpauseAudio();

    using namespace std::chrono;
    auto max_frame_time = 0us;
    auto avg_frame_time = 0us;
    int frame_count = 0;

    while (!quit) {
        const auto start_time = steady_clock::now();

        sdl_context.PollEvents();

        if (pause && !frame_advance) {
            SDL_Delay(48);
            sdl_context.RenderFrame(front_buffer.data());
            continue;
        }

        frame_advance = false;

        joypad->UpdateJoypad();

        // Overspent cycles is always zero or negative.
        int target_cycles = (cycles_per_frame << mem->double_speed) + overspent_cycles;
        overspent_cycles = cpu->RunFor(target_cycles);

        auto frame_time = duration_cast<microseconds>(steady_clock::now() - start_time);
        max_frame_time = std::max(max_frame_time, frame_time);
        avg_frame_time += frame_time;
        if (++frame_count == 60) {
            sdl_context.UpdateFrameTimes(avg_frame_time.count() / 60, max_frame_time.count());
            max_frame_time = 0us;
            avg_frame_time = 0us;
            frame_count = 0;
        }

        sdl_context.PushBackAudio(audio->output_buffer);
        sdl_context.RenderFrame(front_buffer.data());
    }

    sdl_context.PauseAudio();
}

void GameBoy::RegisterCallbacks() {
    using Emu::InputEvent;

    sdl_context.RegisterCallback(InputEvent::Quit,         [this](bool) { quit = true; });
    sdl_context.RegisterCallback(InputEvent::Pause,        [this](bool) { pause = !pause; });
    sdl_context.RegisterCallback(InputEvent::LogLevel,     [this](bool) { logging->SwitchLogLevel(); });
    sdl_context.RegisterCallback(InputEvent::Fullscreen,   [this](bool) { sdl_context.ToggleFullscreen(); });
    sdl_context.RegisterCallback(InputEvent::Screenshot,   [this](bool) { Screenshot(); });
    sdl_context.RegisterCallback(InputEvent::LcdDebug,     [this](bool) { lcd->DumpEverything(); });
    sdl_context.RegisterCallback(InputEvent::HideWindow,   [this](bool) { old_pause = pause; pause = true; });
    sdl_context.RegisterCallback(InputEvent::ShowWindow,   [this](bool) { pause = old_pause; });
    sdl_context.RegisterCallback(InputEvent::FrameAdvance, [this](bool) { frame_advance = true; });

    sdl_context.RegisterCallback(InputEvent::Up,     [this](bool press) { joypad->Press(Joypad::Up, press); });
    sdl_context.RegisterCallback(InputEvent::Left,   [this](bool press) { joypad->Press(Joypad::Left, press); });
    sdl_context.RegisterCallback(InputEvent::Down,   [this](bool press) { joypad->Press(Joypad::Down, press); });
    sdl_context.RegisterCallback(InputEvent::Right,  [this](bool press) { joypad->Press(Joypad::Right, press); });
    sdl_context.RegisterCallback(InputEvent::A,      [this](bool press) { joypad->Press(Joypad::A, press); });
    sdl_context.RegisterCallback(InputEvent::B,      [this](bool press) { joypad->Press(Joypad::B, press); });
    sdl_context.RegisterCallback(InputEvent::L,      [](bool) { });
    sdl_context.RegisterCallback(InputEvent::R,      [](bool) { });
    sdl_context.RegisterCallback(InputEvent::Start,  [this](bool press) { joypad->Press(Joypad::Start, press); });
    sdl_context.RegisterCallback(InputEvent::Select, [this](bool press) { joypad->Press(Joypad::Select, press); });
}

void GameBoy::SwapBuffers(std::vector<u16>& back_buffer) {
    front_buffer.swap(back_buffer);
}

void GameBoy::Screenshot() const {
    Common::WriteImageToFile(Common::BGR5ToRGB8(front_buffer), "screenshot", 160, 144);
}

void GameBoy::HardwareTick(unsigned int cycles) {
    for (; cycles != 0; cycles -= 4) {
        // Enable interrupts if EI was previously called.
        cpu->EnableInterruptsDelayed();

        // Update the rest of the system hardware.
        mem->UpdateOamDma();
        mem->UpdateHdma();
        timer->UpdateTimer();
        serial->UpdateSerial();
        lcd->UpdateLcd();

        // The APU always updates at 2MHz, regardless of double speed mode. So we need to update it twice an M-cycle
        // in single-speed mode.
        for (int i = 0; i < (2 >> mem->double_speed); ++i) {
            audio->UpdateAudio();
        }

        mem->IF_written_this_cycle = false;
    }
}

void GameBoy::HaltedTick(unsigned int cycles) {
    for (; cycles != 0; cycles -= 4) {
        // Update the rest of the system hardware.
        timer->UpdateTimer();
        serial->UpdateSerial();
        lcd->UpdateLcd();

        // The APU always updates at 2MHz, regardless of double speed mode. So we need to update it twice an M-cycle
        // in single-speed mode.
        for (int i = 0; i < (2 >> mem->double_speed); ++i) {
            audio->UpdateAudio();
        }
    }
}

bool GameBoy::JoypadPress() const {
    return joypad->JoypadPress();
}

void GameBoy::StopLcd() {
    // Record the current LCD power state for when we exit stop mode.
    lcd_on_when_stopped = lcd->lcdc & 0x80;

    // Turn off the LCD.
    lcd->lcdc &= 0x7F;
}

void GameBoy::SpeedSwitch() {
    mem->ToggleCpuSpeed();

    // If the LCD was on when we entered STOP mode, turn it back on.
    lcd->lcdc |= lcd_on_when_stopped;
}

} // End namespace Gb
