/*
 *    sfall
 *    Copyright (C) 2008-2015  The sfall team
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "main.h"

#include "Logging.h"
#include "FalloutEngine.h"

// global variables
TGameObj** obj_dude_ptr = (TGameObj**)(0x6610B8);
TGameObj** inven_dude_ptr = (TGameObj**)(0x519058);
DWORD* activeUIHand_ptr = (DWORD*)(0x518F78); // 0 - left, 1 - right
DWORD* dude_traits = (DWORD*)(0x66BE40); // 2 of them
DWORD* itemCurrentItem = (DWORD*)(0x518F78); 
DWORD* itemButtonItems = (DWORD*)(0x5970F8);

/**
 ENGINE FUNCTIONS OFFSETS
 const names should end with underscore
*/

// PROTO FUNCTIONS
const DWORD proto_ptr_ = 0x4A2108; // eax - PID, edx - int** - pointer to a pointer to a proto struct
const DWORD item_w_max_ammo_ = 0x478674; // eax - object
const DWORD item_w_cur_ammo_ = 0x4786A0; // eax - object

// AI FUNCTIONS
const DWORD ai_can_use_weapon_ = 0x4298EC;  // (TGameObj *aCritter<eax>, int aWeapon<edx>, int a2Or3<ebx>) returns 1 or 0

// UI FUNCTIONS
const DWORD isPartyMember_ = 0x494FC4; // eax - object
const DWORD intface_redraw_ = 0x45EB98;
const DWORD interface_disable_ = 0x45EAFC;
const DWORD interface_enable_ = 0x45EA64;
const DWORD intface_toggle_items_ = 0x45F404;
const DWORD intface_item_reload_ = 0x460B20;
const DWORD intface_toggle_item_state_ = 0x45F4E0;
const DWORD intface_use_item_ = 0x45F5EC;

// OBJECTS manipulation
const DWORD obj_set_light_ = 0x48AC90; // <eax>(int aObj<eax>, signed int aDist<edx>, int a3<ecx>, int aIntensity<ebx>)
const DWORD obj_new_ = 0x489A84;  // int aObj*<eax>, int aPid<ebx>
const DWORD obj_turn_off_ = 0x48AE68;  // int aObj<eax>, int ???<edx>
const DWORD obj_move_to_tile_ = 0x48A568;  // int aObj<eax>, int aTile<edx>, int aElev<ebx>

const DWORD obj_find_first_at_tile_ = 0x48B5A8; //  <eax>(int elevation<eax>, int tile<edx>)
const DWORD obj_find_next_at_tile_ = 0x48B608; // no args
const DWORD critter_is_dead_ = 0x42DD18; // eax - critter

// ANIMATION
const DWORD tile_refresh_rect_ = 0x4B12C0; // (int elevation<edx>, unkown<ecx>)
const DWORD register_object_animate_ = 0x4149D0;  // int aObj<eax>, int aAnim<edx>, int delay<ebx>
const DWORD register_object_animate_and_hide_ = 0x414B7C;  // int aObj<eax>, int aAnim<edx>, int delay<ebx>
const DWORD register_object_must_erase_ = 0x414E20;  // int aObj<eax>
const DWORD register_object_change_fid_ = 0x41518C;  // int aObj<eax>, int aFid<edx>, int aDelay<ebx>
const DWORD register_object_light_ = 0x415334; // <eax>(int aObj<eax>, int aRadius<edx>, int aDelay<ebx>)
const DWORD register_object_funset_ = 0x4150A8; // int aObj<eax>, int ???<edx>, int aDelay<ebx> - not really sure what this does
const DWORD register_object_take_out_ = 0x415238; // int aObj<eax>, int aHoldFrame<edx> - hold frame ID (1 - spear, 2 - club, etc.)
const DWORD register_object_turn_towards_ = 0x414C50; // int aObj<eax>, int aTile<edx>

// WRAPPERS
// please, use CamelCase for those


int __stdcall ItemGetType(TGameObj* item) {
 __asm {
  mov eax, item;
  call item_get_type_;
 }
}

int _stdcall IsPartyMember(TGameObj* obj) {
 __asm {
  mov eax, obj;
  call isPartyMember_;
 }
}

TGameObj* GetInvenWeaponLeft(TGameObj* obj) {
 __asm {
  mov eax, obj;
  call inven_left_hand_;
 }
}

TGameObj* GetInvenWeaponRight(TGameObj* obj) {
 __asm {
  mov eax, obj;
  call inven_right_hand_;
 }
}


char* GetProtoPtr(DWORD pid) {
 char* proto;
 __asm {
  mov eax, pid;
  lea edx, proto;
  call proto_ptr_;
 }
 return proto;
}

char AnimCodeByWeapon(TGameObj* weapon) {
 if (weapon != NULL) {
  char* proto = GetProtoPtr(weapon->pid);
  if (proto && *(int*)(proto + 32) == 3) {
   return (char)(*(int*)(proto + 36)); 
  }
 }
 return 0;
}


void DisplayConsoleMessage(const char* msg) {
 __asm {
  mov eax, msg;
  call display_print_
 }
}

static DWORD mesg_buf[4] = {0, 0, 0, 0};
const char* _stdcall GetMessageStr(DWORD fileAddr, DWORD messageId)
{
 DWORD buf = (DWORD)mesg_buf;
 const char* result;
 __asm {
  mov eax, fileAddr
  mov ebx, messageId
  mov edx, buf
  call getmsg_
  mov result, eax
 }
 return result;
}


const DWORD action_get_an_object_ = 0x412134;
const DWORD action_loot_container_ = 0x4123E8;
const DWORD action_use_an_item_on_object_ = 0x411F2C;
const DWORD adjust_ac_ = 0x4715F8;
const DWORD AddHotLines_ = 0x4998C0;
const DWORD ai_search_inven_armor_ = 0x429A6C;
const DWORD art_exists_ = 0x4198C8;
const DWORD art_id_ = 0x419C88;
const DWORD art_ptr_lock_data_ = 0x419188;
const DWORD art_ptr_unlock_ = 0x419260;
const DWORD buf_to_buf_ = 0x4D36D4;
const DWORD Check4Keys_ = 0x43F73C;
const DWORD combat_should_end_ = 0x422C60;
const DWORD combat_turn_ = 0x42299C;
const DWORD critter_body_type_ = 0x42DDC4;
const DWORD critter_can_obj_dude_rest_ = 0x42E564;
const DWORD critter_name_ = 0x42D0A8;
const DWORD critter_pc_set_name_ = 0x42D138;
const DWORD db_fclose_ = 0x4C5EB4;
const DWORD dialog_out_ = 0x41CF20;
const DWORD display_inventory_ = 0x46FDF4;
const DWORD display_print_ = 0x43186C;
const DWORD display_target_inventory_ = 0x47036C;
const DWORD elevator_end_ = 0x43F6D0;
const DWORD elevator_start_ = 0x43F324;
const DWORD endgame_slideshow_ = 0x43F788;
const DWORD gdialog_barter_cleanup_tables_ = 0x448660;
const DWORD getmsg_ = 0x48504C;
const DWORD get_input_ = 0x4C8B78;
const DWORD gmouse_is_scrolling_ = 0x44B54C;
const DWORD gsnd_build_weapon_sfx_name_ = 0x451760;
const DWORD gsound_play_sfx_file_ = 0x4519A8;
const DWORD insert_withdrawal_ = 0x47A290;
const DWORD intface_update_hit_points_ = 0x45EBD8;
const DWORD intface_update_items_ = 0x45EFEC;
const DWORD intface_update_move_points_ = 0x45EE0C;
const DWORD inven_left_hand_ = 0x471BBC;
const DWORD inven_right_hand_ = 0x471B70;
const DWORD inven_unwield_ = 0x472A54;
const DWORD inven_wield_ = 0x472758;
const DWORD inven_worn_ = 0x471C08;
const DWORD item_add_force_ = 0x4772B8;
const DWORD item_c_curr_size_ = 0x479A20;
const DWORD item_c_max_size_ = 0x479A00;
const DWORD item_d_check_addict_ = 0x47A640;
const DWORD item_d_take_drug_ = 0x479F60;
const DWORD item_get_type_ = 0x477AFC;
const DWORD item_move_all_ = 0x4776AC;
const DWORD item_mp_cost_ = 0x478040;
const DWORD item_remove_mult_ = 0x477490;
const DWORD item_total_weight_ = 0x477E98;
const DWORD item_w_anim_code_ = 0x478DA8;
const DWORD item_w_anim_weap_ = 0x47860C;
const DWORD item_w_try_reload_ = 0x478768;
const DWORD item_weight_ = 0x477B88;
const DWORD ListSkills_ = 0x436154;
const DWORD mem_free_ = 0x4C5C24;
const DWORD message_search_ = 0x484C30;
const DWORD move_inventory_ = 0x474708;
const DWORD NixHotLines_ = 0x4999C0;
const DWORD obj_change_fid_ = 0x48AA3C;
const DWORD obj_connect_ = 0x489EC4;
const DWORD obj_destroy_ = 0x49B9A0;
const DWORD obj_dist_ = 0x48BBD4;
const DWORD obj_use_book_ = 0x49B9F0;
const DWORD obj_use_power_on_car_ = 0x49BDE8;
const DWORD partyMemberCopyLevelInfo_ = 0x495EA8;
const DWORD partyMemberGetAIOptions_ = 0x4941F0;
const DWORD partyMemberGetCurLevel_ = 0x495FF0;
const DWORD pc_flag_off_ = 0x42E220;
const DWORD pc_flag_on_ = 0x42E26C;
const DWORD pc_flag_toggle_ = 0x42E2B0;
const DWORD perform_withdrawal_end_ = 0x47A558;
const DWORD perkGetLevelData_ = 0x49678C;
const DWORD perk_add_effect_ = 0x496BFC;
const DWORD perk_level_ = 0x496B78;
const DWORD perks_dialog_ = 0x43C4F0;
const DWORD pip_back_ = 0x497B64;
const DWORD pip_print_ = 0x497A40;
const DWORD PipStatus_ = 0x497BD8;
const DWORD queue_clear_type_ = 0x4A2790;
const DWORD queue_find_first_ = 0x4A295C;
const DWORD queue_find_next_ = 0x4A2994;
const DWORD roll_random_ = 0x4A30C0;
const DWORD RestorePlayer_ = 0x43A8BC;
const DWORD scr_write_ScriptNode_ = 0x4A5704;
const DWORD skill_dec_point_ = 0x4AA8C4;
const DWORD skill_get_tags_ = 0x4AA508;
const DWORD skill_inc_point_ = 0x4AA6BC;
const DWORD skill_level_ = 0x4AA558;
const DWORD skill_set_tags_ = 0x4AA4E4;
const DWORD sprintf_ = 0x4F0041;
const DWORD stat_get_bonus_ = 0x4AF474;
const DWORD stat_level_ = 0x4AEF48;
const DWORD SavePlayer_ = 0x43A7DC;
const DWORD text_font_ = 0x4D58DC;
const DWORD tile_scroll_to_ = 0x4B3924;
const DWORD trait_get_ = 0x4B3B54;
const DWORD trait_set_ = 0x4B3B48;
const DWORD win_disable_button_ = 0x4D94D0;
const DWORD win_draw_ = 0x4D6F5C;
const DWORD win_draw_rect_ = 0x4D6F80;
const DWORD win_enable_button_ = 0x4D9474;
const DWORD win_get_buf_ = 0x4D78B0;
const DWORD win_register_button_ = 0x4D8260;
const DWORD win_register_button_disable_ = 0x4D8674;
const DWORD _word_wrap_ = 0x4BC6F0;
