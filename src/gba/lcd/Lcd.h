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

#pragma once

#include <vector>
#include <array>

#include "common/CommonTypes.h"
#include "common/CommonFuncs.h"
#include "gba/memory/IOReg.h"

namespace Gba {

class Core;
class Bg;

class Sprite {
public:
    Sprite(u32 attr1, u32 attr2)
            : y_pos(attr1 & 0xFF)
            , affine(attr1 & 0x100)
            , disable(attr1 & 0x200)
            , double_size(attr1 & 0x200)
            , mode(static_cast<Mode>((attr1 >> 10) & 0x3))
            , mosaic(attr1 & 0x1000)
            , single_palette(attr1 & 0x2000)
            , x_pos(SignExtend((attr1 >> 16) & 0x1FF, 9))
            , affine_select((attr1 >> 25) & 0x1F)
            , h_flip(attr1 & 0x1000'0000)
            , v_flip(attr1 & 0x2000'0000)
            , tile_num(attr2 & (single_palette ? 0x3FE : 0x3FF))
            , priority((attr2 >> 10) & 0x3)
            , palette(single_palette ? 0 : ((attr2 >> 12) & 0xF))
            , pixel_width(Width(attr1))
            , pixel_height(Height(attr1))
            , tile_width(pixel_width / ((affine && double_size) ? 16 : 8))
            , tile_height(pixel_height / ((affine && double_size) ? 16 : 8))
            , tile_bytes(single_palette ? 64 : 32)
            , tile_base_addr(sprite_vram_base + tile_num * 32) {

        if (y_pos + pixel_height > 0xFF) {
            y_pos -= 0x100;
        }
    }

    enum class Mode {Normal          = 0,
                     SemiTransparent = 1,
                     ObjWindow       = 2,
                     Prohibited      = 3};

    enum class Shape {Square     = 0,
                      Horizontal = 1,
                      Vertical   = 2,
                      Prohibited = 3};

    static constexpr int sprite_vram_base = 0x1'0000;

    int y_pos;
    bool affine;
    bool disable;
    bool double_size;
    Mode mode;
    bool mosaic;
    bool single_palette;

    int x_pos;
    int affine_select;
    bool h_flip;
    bool v_flip;

    int tile_num;
    int priority;
    int palette;

    int pixel_width;
    int pixel_height;
    int tile_width;
    int tile_height;

    int tile_bytes;
    int tile_base_addr;

    bool drawn = false;

    static bool Disabled(u32 attr1) { return (attr1 & 0x200) && !(attr1 & 0x100); }
    static Shape GetShape(u32 attr1) { return static_cast<Shape>((attr1 >> 14) & 0x3); }
    static int GetSize(u32 attr1) { return (attr1 >> 30) & 0x3; }
    static bool DoubleSize(u32 attr1) { return (attr1 & 0x200) && (attr1 & 0x100); }

    static int Height(u32 attr1);
    static int Width(u32 attr1);
};

class Window {
public:
    IOReg width  = {0x0000, 0x0000, 0xFFFF};
    IOReg height = {0x0000, 0x0000, 0xFFFF};

    bool on_this_scanline = false;

    int Left() const { return width >> 8; }
    int Right() const { return width & 0xFF; }
    int Top() const { return height >> 8; }
    int Bottom() const { return height & 0xFF; }

    void IsOnThisScanline(bool enabled, int y) {
        on_this_scanline = enabled && y >= Top() && y < Bottom();
    }

    bool Contains(int x) const {
        if (!on_this_scanline) {
            return false;
        }

        if (Right() >= Left()) {
            return x >= Left() && x < Right();
        } else {
            // The window wraps around the side of the screen.
            return x >= Left() || x < Right();
        }
    }
};

struct PixelInfo {
    u8 last_layer = 5;
    u8 highest_first_target = 5;
};

class Lcd {
public:
    Lcd(const std::vector<u16>& _pram, const std::vector<u16>& _vram, const std::vector<u32>& _oam, Core& _core);
    ~Lcd();

    IOReg control       = {0x0000, 0xFFF7, 0xFFF7};
    IOReg green_swap    = {0x0000, 0x0001, 0x0001};
    IOReg status        = {0x0000, 0xFF3F, 0xFF38};
    IOReg vcount        = {0x0000, 0x00FF, 0x0000};

    IOReg winin         = {0x0000, 0x3F3F, 0x3F3F};
    IOReg winout        = {0x0000, 0x3F3F, 0x3F3F};

    IOReg mosaic        = {0x0000, 0x0000, 0xFFFF};
    IOReg blend_control = {0x0000, 0x3FFF, 0x3FFF};
    IOReg blend_alpha   = {0x0000, 0x1F1F, 0x1F1F};
    IOReg blend_fade    = {0x0000, 0x0000, 0x001F};

    std::vector<Bg> bgs;
    std::array<Window, 2> windows;

    const std::vector<u16>& pram;
    const std::vector<u16>& vram;
    const std::vector<u32>& oam;

    bool bg_dirty = true;
    bool oam_dirty = true;

    static constexpr int h_pixels = 240;
    static constexpr int v_pixels = 160;
    static constexpr u16 alpha_bit = 0x8000;

    void Update(int cycles);
    void WriteControl(const u16 data, const u16 mask);
    int NextEvent() const;

    void DumpDebugInfo() const;
    void DumpSprites() const;
    void DumpTileset(int base, bool single_palette) const;

    std::array<u16, 8> GetTilePixels(int tile_addr, bool single_palette, bool h_flip,
                                     int pixel_row, int palette, int base) const;

    // Mosaic flags
    int MosaicBgH() const { return (mosaic & 0xF) + 1; }
    int MosaicBgV() const { return ((mosaic >> 4) & 0xF) + 1; }

private:
    Core& core;

    std::vector<u16> back_buffer;

    int scanline_cycles = 0;

    std::vector<Sprite> sprites;
    std::array<std::array<u16, 240>, 4> sprite_scanlines;
    std::array<bool, 4> sprite_scanline_used{{true, true, true, true}};
    std::array<u8, 240> sprite_flags;
    static constexpr u8 semi_transparent_flag = 0x1;
    static constexpr u8 obj_window_flag = 0x2;
    bool semi_transparent_used = true;
    bool obj_window_used = true;

    void DrawScanline();
    std::array<std::vector<const Bg*>, 4> DrawBackgrounds();

    void ReadOam();
    void DrawSprites();
    void DrawRegularSprite(const Sprite& sprite);
    void DrawAffineSprite(const Sprite& sprite);
    void UpdateSpritePixel(const Sprite& sprite, int scanline_index);

    bool IsWithinWindow(int layer_id, int x) const;

    bool IsFirstTarget(int target) const { return (FirstTargets() >> target) & 0x1; }
    bool IsSecondTarget(int target) const { return (SecondTargets() >> target) & 0x1; }

    int Brighten(int t) const { return t + (31.0 - t) * Intensity(); }
    int Darken(int t) const { return t * (1.0 - Intensity()); }
    int Blend(int t1, int t2) const { return std::min(t1 * FirstAlpha() + t2 * SecondAlpha(), 31.0); }

    // Control flags
    int BgMode() const { return control & 0x7; }
    bool DisplayFrame1() const { return control & 0x10; }
    bool HBlankFree() const { return control & 0x20; }
    bool ObjMapping2D() const { return !(control & 0x40); }
    bool ForcedBlank() const { return control & 0x80; }
    bool ObjEnabled() const { return control & 0x1000; }
    bool WinEnabled(int win_id) const { return control & (0x2000 << win_id); }
    bool ObjWinEnabled() const { return control & 0x8000; }
    bool NoWinEnabled() const { return !(control & 0xE000); }

    // Status flags
    static constexpr u16 vblank_flag = 0x01;
    static constexpr u16 hblank_flag = 0x02;
    static constexpr u16 vcount_flag = 0x04;
    bool VBlankIrqEnabled() const { return status & 0x08; }
    bool HBlankIrqEnabled() const { return status & 0x10; }
    bool VCountIrqEnabled() const { return status & 0x20; }
    int VTrigger() const { return status >> 8; }

    // Mosaic flags
    int MosaicObjH() const { return ((mosaic >> 8) & 0xF) + 1; }
    int MosaicObjV() const { return ((mosaic >> 12) & 0xF) + 1; }

    // Blending flags
    enum class Effect {None = 0,
                       AlphaBlend = 1,
                       Brighten = 2,
                       Darken = 3};
    int FirstTargets() const { return blend_control & 0x3F; }
    Effect BlendMode() const { return static_cast<Effect>((blend_control >> 6) & 0x3); }
    int SecondTargets() const { return (blend_control >> 8) & 0x3F; }

    double FirstAlpha() const { return std::min(static_cast<double>(blend_alpha & 0x1F) / 16.0, 1.0); }
    double SecondAlpha() const { return std::min(static_cast<double>((blend_alpha >> 8) & 0x1F) / 16.0, 1.0); }

    double Intensity() const { return std::min(static_cast<double>(blend_fade & 0x1F) / 16.0, 1.0); }
};

} // End namespace Gba
