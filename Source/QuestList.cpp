/*
 *    sfall
 *    Copyright (C) 2009, 2010  The sfall team
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

#include "Define.h"
#include "FalloutEngine.h"

static DWORD zone_number = 0;
static DWORD QuestsScrollButtonsX = 220;
static DWORD QuestsScrollButtonsY = 229;

static void __declspec(naked) StartPipboy_hook() {
 __asm {
// load new texture for first (up) button. I used memory address for texture from buttons at
// chracter screen. Everything fine, because this buttons can't use in one time, and they
// everytime recreating
  pushad
  push 0
  mov  edx, 49                              // INVUPOUT.FRM
  mov  eax, ObjType_Intrface
  xor  ecx, ecx
  xor  ebx, ebx
  call art_id_
  mov  ecx, 0x59E7C4
  xor  ebx, ebx
  xor  edx, edx
  call art_ptr_lock_data_
  test eax, eax
  jz   noUpButton
  xchg ebp, eax
  push 0
  mov  edx, 50                              // INVUPIN.FRM
  mov  eax, ObjType_Intrface
  xor  ecx, ecx
  xor  ebx, ebx
  call art_id_
  mov  ecx, 0x59E7C8
  xor  ebx, ebx
  xor  edx, edx
  call art_ptr_lock_data_
  test eax, eax
  jz   noUpButton
  xchg edi, eax
  push 0
  mov  edx, 53                              // INVUPDS.FRM
  mov  eax, ObjType_Intrface
  xor  ecx, ecx
  xor  ebx, ebx
  call art_id_
  mov  ecx, 0x59E7CC
  xor  ebx, ebx
  xor  edx, edx
  call art_ptr_lock_data_
  test eax, eax
  jz   noUpButton
  mov  esi, eax
// creating first button
  push 0                                    // ButType
  push 0
  push edi                                  // PicDown
  push ebp                                  // PicUp
  mov  ebp, 0x149                           // this number will return when button pressed (Page Up)
  push ebp                                  // ButtUp
  push -1                                   // ButtDown
  push -1                                   // HovOff
  push -1                                   // HovOn
  mov  ecx, 22                              // Width
  mov  ebx, 23                              // Height
  push ebx                                  // Height
  mov  edx, QuestsScrollButtonsX            // Xpos
  mov  ebx, QuestsScrollButtonsY            // Ypos
  mov  eax, ds:[_pip_win]                   // WinRef
  call win_register_button_
  mov  ds:[_inven_scroll_up_bid], eax
  cmp  eax, -1
  je   noUpButton
  mov  edi, eax
  mov  ecx, esi
  mov  ebx, esi
  mov  edx, esi
  call win_register_button_disable_
  mov  eax, edi
  call win_disable_button_
noUpButton:
// load new texture for second (down) button
  push 0
  mov  edx, 51                              // INVDNOUT.FRM
  mov  eax, ObjType_Intrface
  xor  ecx, ecx
  xor  ebx, ebx
  call art_id_
  mov  ecx, 0x59E7D0
  xor  ebx, ebx
  xor  edx, edx
  call art_ptr_lock_data_
  test eax, eax
  jz   noDownButton
  xchg ebp, eax
  push 0
  mov  edx, 52                              // INVDNIN.FRM
  mov  eax, ObjType_Intrface
  xor  ecx, ecx
  xor  ebx, ebx
  call art_id_
  mov  ecx, 0x59E7D4
  xor  ebx, ebx
  xor  edx, edx
  call art_ptr_lock_data_
  test eax, eax
  jz   noDownButton
  xchg edi, eax
  push 0
  mov  edx, 54                              // INVDNDS.FRM
  mov  eax, ObjType_Intrface
  xor  ecx, ecx
  xor  ebx, ebx
  call art_id_
  mov  ecx, 0x59E7D8
  xor  ebx, ebx
  xor  edx, edx
  call art_ptr_lock_data_
  test eax, eax
  jz   noDownButton
  mov  esi, eax
// creating second button
  push 0                                    // ButType
  push 0
  push edi                                  // PicDown
  push ebp                                  // PicUp
  mov  ebp, 0x151                           // this number will return when button pressed (Page Down)
  push ebp                                  // ButtUp
  push -1                                   // ButtDown
  push -1                                   // HovOff
  push -1                                   // HovOn
  mov  ecx, 22                              // Width
  mov  ebx, 23                              // Height
  push ebx                                  // Height
  mov  edx, QuestsScrollButtonsX            // Xpos
  add  ebx, QuestsScrollButtonsY            // Ypos
  mov  eax, ds:[_pip_win]                   // WinRef
  call win_register_button_
  mov  ds:[_inven_scroll_dn_bid], eax
  cmp  eax, -1
  je   noDownButton
  mov  edi, eax
  mov  ecx, esi
  mov  ebx, esi
  mov  edx, esi
  call win_register_button_disable_
  mov  eax, edi
  call win_disable_button_
noDownButton:
  popad
  cmp  dword ptr [esp+0x118], 1
  mov  eax, 0x49754C
  jmp  eax
 }
}

static void __declspec(naked) pipboy_hook() {
 __asm {
  cmp  byte ptr ds:[_stat_flag], 0
  je   end
  cmp  zone_number, 0
  je   end
  mov  eax, ds:[_view_page]
  cmp  ebx, 0x149                           // Page Up?
  jne  notPgUp
  test eax, eax
  jz   endZone
  dec  eax
  jmp  click
endZone:
  mov  ebx, 0x210                           // Кнопка НАЗАД
notPgUp:
  cmp  ebx, 0x151                           // Page Down?
  jne  checkClickBug
  cmp  eax, ds:[_holopages]
  je   noKey
  inc  eax
click:
  mov  ds:[_view_page], eax
  mov  eax, 0x50CC50                        // 'ib1p1xx1'
  call gsound_play_sfx_file_
  mov  ebx, zone_number
  jmp  end
checkClickBug:
  cmp  ebx, 0x1F9
  jl   end
  cmp  ebx, 0x20F
  jg   end
noKey:
  xor  ebx, ebx
end:
  mov  edx, _mouse_y
  mov  eax, 0x49708D
  jmp  eax
 }
}

static void __declspec(naked) pipboy_hook1() {
 __asm {
  mov  edx, ds:[_crnt_func]
  test edx, edx
  jnz  end
  cmp  zone_number, 0
  mov  zone_number, ebx
  mov  ds:[_holopages], edx
  jne  end
  mov  eax, ds:[_inven_scroll_up_bid]
  cmp  eax, -1
  je   end
  call win_enable_button_
end:
  mov  eax, 0x4971B8
  jmp  eax
 }
}

static void __declspec(naked) PipStatus_hook() {
 __asm {
  call _word_wrap_
  push eax
  test eax, eax
  jnz  end
  mov  ax, [esp+0x4A4]                      // Количество строк + 1
  dec  eax                                  // Количество строк
  shl  eax, 1                               // На каждую строку по две линии
  mov  ecx, ds:[_cursor_line]
  add  ecx, eax                             // Номер линии после вывода текста
  cmp  ecx, ds:[_bottom_line]
  jl   currentPage                          // Все строки вписываются
  mov  dword ptr ds:[_cursor_line], 3
  inc  dword ptr ds:[_holopages]
  mov  eax, 20
  sub  dword ptr [esp+0x490], eax
  dec  dword ptr [esp+0x494]                // Номер текущего квеста
  sub  dword ptr [esp+0x498], eax
  dec  dword ptr [esp+0x4A0]                // Номер квеста в списке квестов
  jmp  noWrite
currentPage:
  mov  eax, ds:[_view_page]
  cmp  eax, ds:[_holopages]
  je   end
  mov  ds:[_cursor_line], ecx
noWrite:
  mov  word ptr [esp+0x4A4], 1              // Количество строк
end:
  pop  eax
  retn
 }       
}

static void __declspec(naked) DownButton() {
 __asm {
  push eax
  push edx
  mov  eax, ds:[_inven_scroll_dn_bid]
  cmp  eax, -1
  je   end
  mov  edx, ds:[_view_page]
  cmp  edx, ds:[_holopages]
  je   disableButton
  call win_enable_button_
  jmp  end
disableButton:
  call win_disable_button_
end:
  pop  edx
  pop  eax
  retn
 }       
}

static void __declspec(naked) PipStatus_hook1() {
 __asm {
  call DownButton
  call pip_back_
  mov  eax, ds:[_holopages]
  test eax, eax
  jz   end
  push edx
  inc  eax
  push eax
  mov  ebx, 212                             // 'из'
  mov  edx, _pipmesg
  mov  eax, _pipboy_message_file
  call getmsg_
  push eax
  mov  eax, ds:[_view_page]
  inc  eax
  push eax
  push 0x50CD30                             // '%d %s %d'
  lea  eax, [esp+0x18]
  push eax
  call sprintf_
  add  esp, 0x14
  xor  ebx, ebx
  inc  ebx
  mov  ds:[_cursor_line], ebx
  mov  bl, ds:[_GreenColor]
  mov  edx, 0x20
  lea  eax, [esp+0x8]
  call pip_print_
  pop  edx
end:
  retn
 }       
}

static void __declspec(naked) ShowHoloDisk_hook() {
 __asm {
  call DownButton
  jmp  win_draw_
 }       
}

static void __declspec(naked) DisableButtons() {
 __asm {
  push eax
  xor  eax, eax
  mov  zone_number, eax
  mov  ds:[_holopages], eax
  mov  eax, ds:[_inven_scroll_up_bid]
  cmp  eax, -1
  je   noUpButton
  call win_disable_button_
noUpButton:
  mov  eax, ds:[_inven_scroll_dn_bid]
  cmp  eax, -1
  je   noDownButton
  call win_disable_button_
noDownButton:
  pop  eax
  retn
 }
}

static void __declspec(naked) PipFunc_hook() {
 __asm {
  call DisableButtons
  jmp  NixHotLines_
 }
}

static void __declspec(naked) PipAlarm_hook() {
 __asm {
  call DisableButtons
  jmp  critter_can_obj_dude_rest_
 }
}

void QuestListInit() {
//<comments removed because they couldn't display correctly in this encoding>
 MakeCall(0x497544, &StartPipboy_hook, true);
 MakeCall(0x497088, &pipboy_hook, true);
 MakeCall(0x4971B2, &pipboy_hook1, true);
 HookCall(0x498186, &PipStatus_hook);
 HookCall(0x4982B0, &PipStatus_hook1);
 HookCall(0x498C31, &ShowHoloDisk_hook);
 HookCall(0x497BF6, &PipFunc_hook);         // PipStatus_
 HookCall(0x498D53, &PipFunc_hook);         // PipAutomaps_
 HookCall(0x499337, &PipFunc_hook);         // PipArchives_
 HookCall(0x499527, &PipAlarm_hook);        // PipAlarm_
 QuestsScrollButtonsX = GetPrivateProfileIntA("Misc", "QuestsScrollButtonsX", 220, ini);
 if (QuestsScrollButtonsX < 0 || QuestsScrollButtonsX > 618) QuestsScrollButtonsX = 220;
 QuestsScrollButtonsY = GetPrivateProfileIntA("Misc", "QuestsScrollButtonsY", 229, ini);
 if (QuestsScrollButtonsY < 0 || QuestsScrollButtonsY > 434) QuestsScrollButtonsY = 229;
}
