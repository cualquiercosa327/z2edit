#include "imwidget/misc_hacks.h"

#include "imwidget/imapp.h"
#include "nes/mapper.h"
#include "proto/rominfo.pb.h"
#include "util/config.h"
#include "imgui.h"


namespace z2util {

bool MiscellaneousHacks::Draw() {
    if (!visible_)
        return false;

    ImGui::Begin("Miscellaneous Hacks", &visible_);

    ImGui::RadioButton("Miscellaneous", &tab_, 0);
    ImGui::SameLine(); ImGui::RadioButton("Dynamic Banks", &tab_, 1);
    ImGui::SameLine(ImGui::GetWindowWidth() - 50);
    ImApp::Get()->HelpButton("misc");
    ImGui::Separator();

    bool changed = false;
    switch(tab_) {
    case 0:
        changed = DrawMiscHacks();
        break;
    case 1:
        changed = DrawDynamicBanks();
        break;
    default:
        ImGui::Text("Unknown tab value");
    }
    ImGui::End();
    return changed;
}

bool MiscellaneousHacks::DrawMiscHacks() {
    const auto& ri = ConfigLoader<RomInfo>::GetConfig();
    const auto& misc = ri.misc();

    // At bank=0, offset $71d is near the end of the subroutine which determines
    // where Link can walk on the overworld:
    //
    // 712: c90d          CMP #$0d
    // 714: d005          BNE $05
    // 716: ac8807        LDY $0788
    // 719: d004          BNE $04
    // 71b: c90b          CMP #$0b
    // 71d: b002          BCS $02
    // 71f: 18            CLC
    // 720: 60            RTS
    //
    // By re-writing the target of the BCS instruction, we can force the carry
    // flag to be clear all the time, thus allowing link to walk anywhere on
    // the overworld.
    bool walk_anywhere = mapper_->ReadPrgBank(0, 0x71e) == 0x00;
    if (ImGui::Checkbox("Walk Anywhere on Overworld", &walk_anywhere)) {
        if (walk_anywhere) {
            mapper_->WritePrgBank(0, 0x71e, 0);
        } else {
            mapper_->WritePrgBank(0, 0x71e, 2);
        }
    }


    ImGui::PushItemWidth(100);
    int item_delay = mapper_->Read(misc.item_pickup_delay(), 0);
    if (ImGui::InputInt("Item Pickup Delay", &item_delay)) {
        mapper_->Write(misc.item_pickup_delay(), 0, item_delay & 0xFF);
    }

    int text_delay = mapper_->Read(misc.text_delay(0), 0);
    if (ImGui::InputInt("Text Delay", &text_delay)) {
        text_delay &= 0xFF;
        int delay1 = text_delay - 0x1f; if (delay1 < 0) delay1 = 0;
        int delay2 = text_delay - 0x25; if (delay2 < 0) delay2 = 0;
        mapper_->Write(misc.text_delay(0), 0, text_delay);
        mapper_->Write(misc.text_delay(1), 0, delay1);
        mapper_->Write(misc.text_delay(2), 0, delay2);
    }

    int beam_time = 256 - mapper_->Read(misc.beam_sword_time(), 0);
    if (ImGui::InputInt("Beam Sword Time", &beam_time)) {
        mapper_->Write(misc.beam_sword_time(), 0, 256-beam_time);
    }

    int beam_speed = mapper_->Read(misc.beam_sword_speed(), 0);
    if (ImGui::InputInt("Beam Sword Speed", &beam_speed)) {
        mapper_->Write(misc.beam_sword_speed(), 0, beam_speed);
        mapper_->Write(misc.beam_sword_speed(), 1, -beam_speed);
    }

    int elevator_speed = mapper_->Read(misc.elevator_speed(), 1);
    if (ImGui::InputInt("Elevator Speed", &elevator_speed)) {
        mapper_->Write(misc.elevator_speed(), 1, elevator_speed);
        mapper_->Write(misc.elevator_speed(), 2, -elevator_speed);
    }

    // 0x92aa: 10 18 e8 ff, 00 18 e8 ff, 00 18 e8 00
    int fairy_speed = mapper_->Read(misc.fairy_speed(), 1);
    if (ImGui::InputInt("Fairy Speed", &fairy_speed)) {
        mapper_->Write(misc.fairy_speed(), 1, fairy_speed);
        mapper_->Write(misc.fairy_speed(), 2, -fairy_speed);
        mapper_->Write(misc.fairy_speed(), 4+1, fairy_speed);
        mapper_->Write(misc.fairy_speed(), 4+2, -fairy_speed);
        mapper_->Write(misc.fairy_speed(), 8+1, fairy_speed);
        mapper_->Write(misc.fairy_speed(), 8+2, -fairy_speed);
    }

    ImGui::PopItemWidth();

    Hack("Palace 5 detect", ri.palace5_detect_size(),
        [&]() { return ri.palace5_detect(); },
        [&](int n) { return ri.palace5_detect(n); });

    Hack("Palace Continue", ri.palace_continue_size(),
        [&]() { return ri.palace_continue(); },
        [&](int n) { return ri.palace_continue(n); });

    Hack("Completed Places", ri.palace_to_stone_size(),
        [&]() { return ri.palace_to_stone(); },
        [&](int n) { return ri.palace_to_stone(n); });

    Hack("Overworld BreakBlocks", ri.overworld_breakblocks_size(),
        [&]() { return ri.overworld_breakblocks(); },
        [&](int n) { return ri.overworld_breakblocks(n); });

    Hack("Spell Bits", ri.spell_bits_size(),
        [&]() { return ri.spell_bits(); },
        [&](int n) { return ri.spell_bits(n); });

    Hack("Spell Cast", ri.spell_cast_size(),
        [&]() { return ri.spell_cast(); },
        [&](int n) { return ri.spell_cast(n); });

    Hack("Spell Restrictions", ri.spell_restrictions_size(),
        [&]() { return ri.spell_restrictions(); },
        [&](int n) { return ri.spell_restrictions(n); });

    Hack("Overworld Tiles", ri.overworld_tiles_size(),
        [&]() { return ri.overworld_tiles(); },
        [&](int n) { return ri.overworld_tiles(n); });

    return false;
}

bool MiscellaneousHacks::DrawDynamicBanks() {
    const auto& ri = ConfigLoader<RomInfo>::GetConfig();
    char roombuf[8], bankbuf[8];
    const char *overworlds[] = {"West", "DM/Maze", "East"};
    const char *worlds[] = {"Caves", "Towns", "Towns", "P125", "P346", "GP" };

    int banks = Hack("CHR Banks", ri.dynamic_banks_size(),
        [&]() { return ri.dynamic_banks(); },
        [&](int n) { return ri.dynamic_banks(n); });
    int ro = (banks == 0) ? ImGuiInputTextFlags_ReadOnly : 0;

    ImGui::Columns(9, NULL, true);
    ImGui::Text("Overworld World");
    ImGui::NextColumn();
    for(int i=0; i<8; i++) {
        ImGui::Text("Room CHR");
        ImGui::NextColumn();
    }
    ImGui::Separator();
    for(int ov=0; ov<3; ov++) {
        for(int world=0; world<6; world++) {
            if ((ov == 0 && world == 2) ||
                (ov == 1 && (world == 1 || world == 2)) ||
                (ov == 2 && world == 1)) {
                ImGui::Text("%15s", "Not Used");
            } else {
                ImGui::Text("%9s %5s", overworlds[ov], worlds[world]);
            }
            ImGui::NextColumn();
            for(int i=0; i<16; i+=2) {
                int addr = 0xbe60 + (ov * 5 + world) * 16 + i;
                ImGui::PushID(addr);
                int room = mapper_->ReadPrgBank(0, addr + 0);
                int bank = mapper_->ReadPrgBank(0, addr + 1);
                snprintf(roombuf, sizeof(roombuf), "%d", room);
                snprintf(bankbuf, sizeof(bankbuf), "%02x", bank);
                ImGui::PushItemWidth(32);
                if (ImGui::InputText("##room", roombuf, sizeof(roombuf), ImGuiInputTextFlags_CharsDecimal | ro)) {
                    room = strtoul(roombuf, 0, 10);
                    mapper_->WritePrgBank(0, addr+0, room);
                }
                ImGui::SameLine();
                if (ImGui::InputText("##bank", bankbuf, sizeof(bankbuf), ImGuiInputTextFlags_CharsHexadecimal | ro)) {
                    bank = strtoul(bankbuf, 0, 16);
                    mapper_->WritePrgBank(0, addr+1, bank);
                }
                ImGui::PopItemWidth();
                ImGui::PopID();
                ImGui::NextColumn();
            }
            ImGui::Separator();
        }
    }
    ImGui::Columns(1);
    return false;
}

void MiscellaneousHacks::CheckConfig() {
    auto* ri = ConfigLoader<RomInfo>::MutableConfig();
    if (EnabledIndex([&]() { return ri->overworld_tiles(); }) > 0) {
        auto *p = ri->mutable_palettes(0);
        p->mutable_palette(0)->set_hidden(true);
        p->mutable_palette(1)->set_hidden(true);
        p->mutable_palette(2)->set_hidden(false);
        p->mutable_palette(3)->set_hidden(false);
        p->mutable_palette(4)->set_hidden(false);
        p->mutable_palette(5)->set_hidden(false);
        p->mutable_palette(6)->set_hidden(false);
        p->mutable_palette(7)->set_hidden(false);
        ri->mutable_map(0)->mutable_objtable(0)->set_address(0xbb80);
        ri->mutable_map(1)->mutable_objtable(0)->set_address(0xbc00);
        ri->mutable_map(2)->mutable_objtable(0)->set_address(0xbc80);
        ri->mutable_map(3)->mutable_objtable(0)->set_address(0xbc00);
        *ri->mutable_map(0)->mutable_palette() = p->palette(2).address();
        *ri->mutable_map(1)->mutable_palette() = p->palette(4).address();
        *ri->mutable_map(2)->mutable_palette() = p->palette(6).address();
        *ri->mutable_map(3)->mutable_palette() = p->palette(4).address();

        int size = 128*3;
        int i;
        for(i=0; i<size; i++) {
            if (mapper_->ReadPrgBank(0, 0xbb80+i) != 0xff)
                break;
        }
        if (i == size) {
            for(i=0; i<0x50; i++) {
                mapper_->WritePrgBank(0, 0xbb80+i, mapper_->ReadPrgBank(0, 0x87a3+i));
                mapper_->WritePrgBank(0, 0xbc00+i, mapper_->ReadPrgBank(0, 0x87a3+i));
                mapper_->WritePrgBank(0, 0xbc80+i, mapper_->ReadPrgBank(0, 0x87a3+i));
            }
            for(i=0; i<0x24; i++) {
                mapper_->WritePrgBank(0, 0xbbd0+i, mapper_->ReadPrgBank(-1, 0xc458+i));
                mapper_->WritePrgBank(0, 0xbc50+i, mapper_->ReadPrgBank(-1, 0xc458+i));
                mapper_->WritePrgBank(0, 0xbcd0+i, mapper_->ReadPrgBank(-1, 0xc458+i));
            }
        }
    } else {
        auto *p = ri->mutable_palettes(0);
        p->mutable_palette(0)->set_hidden(false);
        p->mutable_palette(1)->set_hidden(false);
        p->mutable_palette(2)->set_hidden(true);
        p->mutable_palette(3)->set_hidden(true);
        p->mutable_palette(4)->set_hidden(true);
        p->mutable_palette(5)->set_hidden(true);
        p->mutable_palette(6)->set_hidden(true);
        p->mutable_palette(7)->set_hidden(true);
        ri->mutable_map(0)->mutable_objtable(0)->set_address(0x87a3);
        ri->mutable_map(1)->mutable_objtable(0)->set_address(0x87a3);
        ri->mutable_map(2)->mutable_objtable(0)->set_address(0x87a3);
        ri->mutable_map(3)->mutable_objtable(0)->set_address(0x87a3);
        *ri->mutable_map(0)->mutable_palette() = p->palette(0).address();
        *ri->mutable_map(1)->mutable_palette() = p->palette(0).address();
        *ri->mutable_map(2)->mutable_palette() = p->palette(0).address();
        *ri->mutable_map(3)->mutable_palette() = p->palette(0).address();
    }
}

template<class GETALL>
int MiscellaneousHacks::EnabledIndex(GETALL getall) {
    int index = 0;
    for(const auto& hack: getall()) {
        if (MemcmpHack(hack.hack(0))) {
            return index;
        }
        index++;
    }
    // If we couldn't identify the hack, return -1
    return -1;
}


template<class GETALL, class GET>
int MiscellaneousHacks::Hack(const char* hackname, int n,
                              GETALL getall, GET get) {
    const char *names[n];
    int len = 0;
    int method = 0;
    auto all = getall();
    for(const auto& hack: all) {
        names[len] = hack.name().c_str();
        if (MemcmpHack(hack.hack(0))) {
            method = len;
        }
        len++;
    }
    ImGui::PushItemWidth(400);
    if (ImGui::Combo(hackname, &method, names, len)) {
        PutGameHack(get(method));
        CheckConfig();
        ImApp::Get()->ProcessMessage("overworld_tile_hack",
                                     reinterpret_cast<void*>(-1));
    }
    ImGui::PopItemWidth();
    return method;
}


bool MiscellaneousHacks::MemcmpHack(const PokeData& data) {
    for(int i=0; i<data.data_size(); i++) {
        if (mapper_->Read(data.address(), i) != data.data(i)) {
            return false;
        }
    }
    return true;
}

void MiscellaneousHacks::PutPokeData(const PokeData& data) {
    for(int i=0; i<data.data_size(); i++) {
        mapper_->Write(data.address(), i, data.data(i));
    }
}

void MiscellaneousHacks::PutGameHack(const GameHack& hack) {
    for(const auto& h : hack.hack()) {
        PutPokeData(h);
    }
}

}  // namespace
