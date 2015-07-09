/*
 *    sfall
 *    Copyright (C) 2011  Timeslip
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

#include <stdio.h>
#include "Bugs.h"
#include "Define.h"
#include "FalloutEngine.h"
#include "HookScripts.h"
#include "input.h"
#include "Inventory.h"
#include "LoadGameHook.h"

static DWORD mode;
static DWORD MaxItemSize;
static DWORD ReloadWeaponKey = 0;

static const int PartyMax=15;
static int npcCount=0;
static char iniArmor[MAX_PATH];

struct npcArmor {
 int Pid;
 int Default;
 int Leather;
 int Power;
 int Advanced;
 int Metal;
 int Cured;
 int Combat;
 int Robe;
};

static npcArmor armors[PartyMax];

struct sMessage {
 DWORD number;
 DWORD flags;
 char* audio;
 char* message;
};
static const char* MsgSearch(int msgno, DWORD file) {
 if(!file) return 0;
 sMessage msg = { msgno, 0, 0, 0 };
 __asm {
  lea edx, msg;
  mov eax, file;
  call message_search_
 }
 return msg.message;
}

/*static DWORD _stdcall item_total_size(void* critter) {
 //TODO: Don't really want to be overwriting stuff like this after init. Rewrite properly.
 HookCall(0x477EBD, (void*)0x477B68);
 HookCall(0x477EF6, (void*)0x477B68);
 HookCall(0x477F12, (void*)0x477B68);
 HookCall(0x477F2A, (void*)0x477B68);

 DWORD result;
 __asm {
  mov eax, critter;
  call item_total_weight_
  mov result, eax;
 }

 HookCall(0x477EBD, (void*)0x477B88);
 HookCall(0x477EF6, (void*)0x477B88);
 HookCall(0x477F12, (void*)0x477B88);
 HookCall(0x477F2A, (void*)0x477B88);

 return result;
}*/

//TODO: Do we actually want to include this in the limit anyway?
static __declspec(naked) DWORD item_total_size(void* critter) {
 __asm {
  push    ebx;
  push    ecx;
  push    edx;
  push    esi;
  push    edi;
  push    ebp;
  mov     ebp, eax;
  test    eax, eax;
  jz      loc_477F33;
  lea     edi, [eax+2Ch];
  xor     edx, edx;
  mov     ebx, [edi];
  xor     esi, esi;
  test    ebx, ebx;
  jle     loc_477ED3;
  xor     ebx, ebx;
loc_477EB7:
  mov     ecx, [edi+8];
  mov     eax, [ecx+ebx];
  call    item_size_
  imul    eax, [ecx+ebx+4];
  add     ebx, 8;
  inc     edx;
  mov     ecx, [edi];
  add     esi, eax;
  cmp     edx, ecx;
  jl      loc_477EB7;
loc_477ED3:
  mov     eax, [ebp+20h];
  and     eax, 0F000000h;
  sar     eax, 18h;
  cmp     eax, 1;
  jnz     loc_477F31;
  mov     eax, ebp;
  call    inven_right_hand_
  mov     edx, eax;
  test    eax, eax;
  jz      loc_477EFD;
  test    byte ptr [eax+27h], 2;
  jnz     loc_477EFD;
  call    item_size_
  add     esi, eax;
loc_477EFD:
  mov     eax, ebp;
  call    inven_left_hand_
  test    eax, eax;
  jz      loc_477F19;
  cmp     edx, eax;
  jz      loc_477F19;
  test    byte ptr [eax+27h], 1;
  jnz     loc_477F19;
  call    item_size_
  add     esi, eax;
loc_477F19:
  mov     eax, ebp;
  call    inven_worn_
  test    eax, eax;
  jz      loc_477F31;
  test    byte ptr [eax+27h], 4;
  jnz     loc_477F31;
  call    item_size_
  add     esi, eax;
loc_477F31:
  mov     eax, esi;
loc_477F33:
  pop     ebp;
  pop     edi;
  pop     esi;
  pop     edx;
  pop     ecx;
  pop     ebx;
  retn;
 }
}

/*static const DWORD ObjPickupFail=0x49B70D;
static const DWORD ObjPickupEnd=0x49B6F8;
static const DWORD size_limit;
static __declspec(naked) void  ObjPickupHook() {
 __asm {
  cmp edi, ds:[_obj_dude];
  jnz end;
end:
  lea edx, [esp+0x10];
  mov eax, ecx;
  jmp ObjPickupEnd;
 }
}*/

static __declspec(naked) int CritterCheck() {
 __asm {
  push ebx;
  push edx;
  sub esp, 4;
  mov ebx, eax;

  cmp eax, dword ptr ds:[_obj_dude];
  je single;
  test mode, 3;
  jnz run;
  test mode, 2;
  jz fail;
  call isPartyMember_
  test eax, eax;
  jz end;
run:
  test mode, 8;
  jz single;
  mov edx, esp;
  mov eax, ebx;
  call proto_ptr_
  mov eax, [esp];
  mov eax, [eax + 0xB0 + 40]; //The unused stat in the extra block
  jmp end;
single:
  mov eax, MaxItemSize;
  jmp end;
fail:
  xor eax, eax;
end:
  add esp, 4;
  pop edx;
  pop ebx;
  retn;
 }
}

static const DWORD IsOverloadedEnd=0x42E68D;
static __declspec(naked) void CritterIsOverloadedHook() {
 __asm {
  and eax, 0xff;
  jnz end;
  mov eax, ebx;
  call CritterCheck;
  test eax, eax;
  jz end;
  xchg eax, ebx;
  call item_total_size;
  cmp eax, ebx;
  setg al;
  and eax, 0xff;
end:
  jmp IsOverloadedEnd;
 }
}

static const DWORD ItemAddMultiRet=0x4772A6;
static const DWORD ItemAddMultiFail=0x4771C7;
static __declspec(naked) void ItemAddMultiHook1() {
 __asm {
  push ebp;
  mov eax, ecx;
  call CritterCheck;
  test eax, eax;
  jz end;
  mov ebp, eax;
  mov eax, esi;
  call item_size_
  mov edx, eax;
  imul edx, ebx;
  mov eax, ecx;
  call item_total_size;
  add edx, eax;
  cmp edx, ebp;
  jle end;
  mov eax, -6; //TODO: Switch this to a lower number, and add custom error messages.
  pop ebp;
  jmp ItemAddMultiFail;
end:
  pop ebp;
  jmp ItemAddMultiRet;
 }
}

static __declspec(naked) void ItemAddMultiHook2() {
 __asm {
  cmp eax, edi;
  jl fail;
  jmp ItemAddMultiHook1;
fail:
  mov eax, -6;
  jmp ItemAddMultiFail;
 }
}

static const DWORD BarterAttemptTransactionHook1Fail=0x474C81;
static const DWORD BarterAttemptTransactionHook1End=0x474CA8;
static __declspec(naked) void BarterAttemptTransactionHook1() {
 __asm {
  cmp eax, edx;
  jg fail;
  mov eax, edi;
  call CritterCheck;
  test eax, eax;
  jz end;
  mov edx, eax;
  mov eax, edi;
  call item_total_size;
  sub edx, eax;
  mov eax, ebp;
  call item_total_size;
  cmp eax, edx;
  jle end;
fail:
  mov esi, 0x1f;
  jmp BarterAttemptTransactionHook1Fail;
end:
  jmp BarterAttemptTransactionHook1End;
 }
}

static const DWORD BarterAttemptTransactionHook2Fail=0x474CD8;
static const DWORD BarterAttemptTransactionHook2End=0x474D01;
static __declspec(naked) void BarterAttemptTransactionHook2() {
 __asm {
  cmp eax, edx;
  jg fail;
  mov eax, ebx;
  call CritterCheck;
  test eax, eax;
  jz end;
  mov edx, eax;
  mov eax, ebx;
  call item_total_size;
  sub edx, eax;
  mov eax, esi;
  call item_total_size;
  cmp eax, edx;
  jle end;
fail:
  mov ecx, 0x20;
  jmp BarterAttemptTransactionHook2Fail;
end:
  jmp BarterAttemptTransactionHook2End;
 }
}

static char SizeStr[16];
static char InvenFmt[32];
static const char* InvenFmt1="%s %d/%d  %s %d/%d";
static const char* InvenFmt2="%s %d/%d";

static const char* _stdcall GetInvenMsg() {
 const char* tmp=MsgSearch(35, 0x59E814);
 if(!tmp) return "S:";
 else return tmp;
}

static void _stdcall strcpy_wrapper(char* buf, const char* str) {
 strcpy(buf, str);
}

static const DWORD DisplayStatsEnd=0x4725E5;
static __declspec(naked) void DisplayStatsHook() {
 __asm {
  call CritterCheck;
  jz nolimit;
  push eax;
  mov eax, ds:[_stack];
  push ecx;
  push InvenFmt1;
  push offset InvenFmt;
  call strcpy_wrapper;
  pop ecx;
  mov eax, ds:[_stack];
  call item_total_size;
  push eax;
  push ecx;
  call GetInvenMsg;
  pop ecx;
  push eax;
  jmp end;
nolimit:
  push ecx;
  push InvenFmt2;
  push offset InvenFmt;
  call strcpy_wrapper;
  pop ecx;
  push eax;
  push eax;
  push eax;
end:
  mov eax, ds:[_stack];
  mov edx, 0xc;
  jmp DisplayStatsEnd;
 }
}

static char SizeMsgBuf[32];
static const char* _stdcall FmtSizeMsg(int size) {
 if(size==1) {
  const char* tmp=MsgSearch(543, _proto_main_msg_file);
  if(!tmp) strcpy(SizeMsgBuf, "It occupies 1 unit.");
  else sprintf(SizeMsgBuf, tmp, size);
 } else {
  const char* tmp=MsgSearch(542, _proto_main_msg_file);
  if(!tmp) sprintf(SizeMsgBuf, "It occupies %d units.", size);
  else sprintf(SizeMsgBuf, tmp, size);
 }
 return SizeMsgBuf;
}

static __declspec(naked) void InvenObjExamineFuncHook() {
 __asm {
  call inven_display_msg_
  push edx;
  push ecx;
  mov eax, esi;
  call item_size_
  push eax;
  call FmtSizeMsg;
  pop ecx;
  pop edx;
  call inven_display_msg_
  retn;
 }
}

static char SuperStimMsg[128];
static int _stdcall SuperStimFix2(DWORD* item, DWORD* target) {
 if(!item || !target) return 0;
 DWORD itm_pid=item[0x64/4], target_pid=target[0x64/4];
 if((target_pid&0xff000000) != 0x01000000) return 0;
 if((itm_pid&0xff000000) != 0) return 0;
 if((itm_pid&0xffffff) != 144) return 0;
 DWORD curr_hp, max_hp;
 __asm {
  mov eax, target;
  mov edx, STAT_current_hp
  call stat_level_
  mov curr_hp, eax;
  mov eax, target;
  mov edx, STAT_max_hit_points
  call stat_level_
  mov max_hp, eax;
 }
 if(curr_hp<max_hp) return 0;
 DisplayConsoleMessage(SuperStimMsg);
 return 1;
}

static const DWORD UseItemHookRet=0x49C3D3;
static void __declspec(naked) SuperStimFix() {
 __asm {
  push eax;
  push ecx;
  push edx;

  push edx;
  push ebx;
  call SuperStimFix2;
  pop edx;
  pop ecx;
  test eax, eax;
  jz end;
  pop eax;
  xor eax, eax;
  retn;
end:
  pop eax;
  push ecx;
  push esi;
  push edi;
  push ebp;
  sub esp, 0x14;
  jmp UseItemHookRet;
 }
}

static int invenapcost;
static char invenapqpreduction;
void _stdcall SetInvenApCost(int a) { invenapcost=a; }
static const DWORD inven_ap_cost_hook_ret=0x46E816;
static void __declspec(naked) inven_ap_cost_hook() {
 _asm {
  movzx ebx, byte ptr invenapqpreduction;
  mul bl;
  mov edx, invenapcost;
  sub edx, eax;
  mov eax, edx;
  jmp inven_ap_cost_hook_ret;
 }
}

static const DWORD item_w_compute_ammo_cost_ = 0x4790AC; // signed int aWeapon<eax>, int *aRoundsSpent<edx>
static const DWORD add_check_for_item_ammo_cost_back = 0x4266EE;
// adds check for weapons which require more than 1 ammo for single shot (super cattle prod & mega power fist)
static void __declspec(naked) add_check_for_item_ammo_cost() {
 __asm {
  push    edx
  push    ebx
  sub     esp, 4
  call    item_w_cur_ammo_
  mov     ebx, eax
  mov     eax, ecx // weapon
  mov     edx, esp
  mov     dword ptr [esp], 1
  pushad
  push    1 // hook type
  call    AmmoCostHookWrapper
  add  esp, 4
  popad
  mov     eax, [esp]
  cmp     eax, ebx
  jle     enoughammo
  xor     eax, eax // this will force "Out of ammo"
  jmp     end
enoughammo:
  mov     eax, 1 // this will force success
end:
  add     esp, 4
  pop     ebx
  pop     edx
  jmp     add_check_for_item_ammo_cost_back; // jump back
 }
}

static const DWORD divide_burst_rounds_by_ammo_cost_back = 0x4234B9;
static void __declspec(naked) divide_burst_rounds_by_ammo_cost() {
 __asm {
  // ecx - current ammo, eax - burst rounds; need to set ebp
  push edx
  sub     esp, 4
  mov     ebp, eax
  mov     eax, edx // weapon
  mov     dword ptr [esp], 1
  mov     edx, esp // *rounds
  pushad
  push    2
  call    AmmoCostHookWrapper
  add  esp, 4
  popad
  mov     edx, 0
  mov     eax, ebp // rounds in burst
  imul    dword ptr [esp] // so much ammo is required for this burst
  cmp     eax, ecx
  jle     skip
  mov     eax, ecx // if more than current ammo, set it to current
skip:
  idiv    dword ptr [esp] // divide back to get proper number of rounds for damage calculations
  mov     ebp, eax
  add     esp, 4
  pop edx
  // end overwriten code
  jmp     divide_burst_rounds_by_ammo_cost_back; // jump back
 }
}

static const DWORD ReloadActiveHand_play_sfx_ = 0x460B85;
static void __declspec(naked) ReloadActiveHand() {
 __asm {
// esi=-1 если не перезарядили неактивную руку или смещение неактивной руки
  push ebx
  push ecx
  push edx
  mov  eax, ds:[_itemCurrentItem]
  imul ebx, eax, 24
  mov  eax, ds:[_obj_dude]
  xor  ecx, ecx
reloadItem:
  push eax
  mov  edx, ds:[_itemButtonItems][ebx]
  call item_w_try_reload_
  test eax, eax
  pop  eax
  jnz  endReloadItem
  inc  ecx
  jmp  reloadItem
endReloadItem:
  cmp  dword ptr ds:[0x597108][ebx], 5      // mode
  jne  skip_toggle_item_state
  call intface_toggle_item_state_
skip_toggle_item_state:
  test ecx, ecx
  jnz  useActiveHand
  xchg esi, ebx
useActiveHand:
  push ebx
  xor  esi, esi
  dec  esi
  mov  ebx, esi
  xor  eax, eax
  mov  edx, ebx
  call intface_update_items_
  pop  eax
  cmp  eax, esi
  je   end
  mov  ebx, 2
  xor  ecx, ecx
  mov  edx, ds:[_itemButtonItems][eax]
  jmp  ReloadActiveHand_play_sfx_
end:
  pop  edx
  pop  ecx
  pop  ebx
  retn
 }
}

static void __declspec(naked) ReloadWeaponHotKey() {
 __asm {
  call gmouse_is_scrolling_
  test eax, eax
  jnz  end
  pushad
  xchg ebx, eax
  push ReloadWeaponKey
  call KeyDown
  test eax, eax
  jnz  ourKey
  popad
  retn
ourKey:
  cmp  dword ptr ds:[_intfaceEnabled], ebx
  je   endReload
  xor  esi, esi
  dec  esi
  cmp  dword ptr ds:[_interfaceWindow], esi
  je   endReload
  mov  edx, ds:[_itemCurrentItem]
  imul eax, edx, 24
  cmp  byte ptr ds:[0x5970FD][eax], bl      // itsWeapon
  jne  itsWeapon                            // Да
  call intface_use_item_
  jmp  endReload
itsWeapon:
  test byte ptr ds:[_combat_state], 1
  jnz  inCombat
  call ReloadActiveHand
  jmp  endReload
inCombat:
//  xor  ebx, ebx                             // is_secondary
  add  edx, 6                               // edx = 6/7 - перезарядка оружия в левой/правой руке
  mov  eax, ds:[_obj_dude]
  push eax
  call item_mp_cost_
  xchg ecx, eax
  pop  eax                                  // _obj_dude
  mov  edx, [eax+0x40]                      // movePoints
  cmp  ecx, edx
  jg   endReload
  push eax
  call ReloadActiveHand
  test eax, eax
  pop  eax                                  // _obj_dude
  jnz  endReload
  sub  edx, ecx
  mov  [eax+0x40], edx                      // movePoints
  xchg edx, eax
  mov  edx, ds:[_combat_free_move]
  call intface_update_move_points_
endReload:
  popad
  inc  eax
end:
  retn
 }
}

static void __declspec(naked) AutoReloadWeapon() {
 __asm {
  call scr_exec_map_update_scripts_
  pushad
  cmp  dword ptr ds:[_game_user_wants_to_quit], 0
  jnz  end
  mov  eax, ds:[_obj_dude]
  call critter_is_dead_                     // Дополнительная проверка не помешает
  test eax, eax
  jnz  end
  cmp  dword ptr ds:[_intfaceEnabled], eax
  je   end
  xor  esi, esi
  dec  esi
  cmp  dword ptr ds:[_interfaceWindow], esi
  je   end
  inc  eax
  mov  ebx, ds:[_itemCurrentItem]
  push ebx
  sub  eax, ebx
  mov  ds:[_itemCurrentItem], eax           // Устанавливаем неактивную руку
  imul ebx, eax, 24
  mov  eax, ds:[_obj_dude]
  xor  ecx, ecx
reloadOffhand:
  push eax
  mov  edx, ds:[_itemButtonItems][ebx]
  call item_w_try_reload_
  test eax, eax
  pop  eax
  jnz  endReloadOffhand
  inc  ecx
  jmp  reloadOffhand
endReloadOffhand:
  cmp  dword ptr ds:[0x597108][ebx], 5      // mode
  jne  skip_toggle_item_state
  call intface_toggle_item_state_
skip_toggle_item_state:
  test ecx, ecx
  jnz  useOffhand
  xchg ebx, esi
useOffhand:
  xchg esi, ebx                             // esi=-1 если не перезарядили или смещению неактивной руки
  pop  eax
  mov  ds:[_itemCurrentItem], eax           // Восстанавливаем активную руку
  call ReloadActiveHand
end:
  popad
  retn
 }
}

static int pobj;
static int _stdcall ChangeArmorFid(DWORD* item, DWORD* npc) {
 int itm_pid = 0, npc_pid = PID_Player;
 if (item) itm_pid=item[0x64/4]&0xffffff;
 if (npc) npc_pid=npc[0x64/4];
 pobj=0;
 for (int k=0; k<npcCount; k++) {
  if (armors[k].Pid==npc_pid) {
   pobj=armors[k].Default;
   switch (itm_pid) {
    case PID_LEATHER_ARMOR: case PID_LEATHER_ARMOR_MK_II:
     if (armors[k].Leather!=0) pobj=armors[k].Leather;
     break;
    case PID_POWERED_ARMOR: case PID_HARDENED_POWER_ARMOR:
     if (armors[k].Power!=0) pobj=armors[k].Power;
     break;
    case PID_ADVANCED_POWER_ARMOR: case PID_ADVANCED_POWER_ARMOR_MK2:
     if (armors[k].Advanced!=0) pobj=armors[k].Advanced;
     break;
    case PID_METAL_ARMOR: case PID_TESLA_ARMOR: case PID_METAL_ARMOR_MK_II:
     if (armors[k].Metal!=0) pobj=armors[k].Metal;
     break;
    case PID_LEATHER_JACKET: case PID_CURED_LEATHER_ARMOR:
     if (armors[k].Cured!=0) pobj=armors[k].Cured;
     break;
    case PID_COMBAT_ARMOR: case PID_BROTHERHOOD_COMBAT_ARMOR: case PID_COMBAT_ARMOR_MK_II:
     if (armors[k].Combat!=0) pobj=armors[k].Combat;
     break;
    case PID_PURPLE_ROBE: case PID_BRIDGEKEEPERS_ROBE:
     if (armors[k].Robe!=0) pobj=armors[k].Robe;
     break;
   }
   __asm {
    mov  ebx, npc
    mov  eax, ebx
    call inven_right_hand_
    push eax                                // Сохраним указатель на оружие, если оно конечно есть
    test eax, eax
    jz   noWeapon                           // Оружия в руках нет
    mov  eax, ebx
    mov  edx, 1                             // правая рука
    call inven_unwield_                     // Разоружимся
noWeapon:
    push pobj                               // pobj = fid брони
    mov  edx, offset pobj
    mov  eax, npc_pid
    call proto_ptr_
    mov  edx, pobj
    mov  eax, [edx+0x8]
    mov  pobj, eax                          // pobj = fid из pro-файла
    pop  edx
    test edx, edx                           // А был ли мальчик?
    jnz  haveFid                            // Да
    mov  edx, eax                           // Нет, используем fid из pro-файла
haveFid:
    and  edx, 0x0FFF                        // edx = индекс в LST-файле
    mov  ebx, [ebx+0x20]                    // fid
    mov  eax, ebx
    and  eax, 0x70000000
    sar  eax, 0x1C
    push eax                                // ID3 (Direction code)
    and  ebx, 0x0FF0000
    sar  ebx, 0x10                          // ebx = ID2 (Animation code)
    xor  ecx, ecx                           // ecx = ID1 (Weapon code = None)
    mov  eax, 1                             // eax = тип объекта (ObjType_Critter)
    call art_id_
    mov  edx, eax
    call art_exists_
    test eax, eax
    jnz  canUse                             // Такое изображение можно использовать
    mov  edx, pobj                          // Используем изображение из pro-файла
canUse:
    mov  pobj, edx                          // Сохраним fid для выхода
    xor  ebx, ebx
    mov  ecx, npc
    mov  eax, ecx
    call obj_change_fid_                    // Меняем вид
    pop  edx                                // Восстановим указатель на оружие, если оно конечно есть
    test edx, edx                           // Оружие было в руках?
    jz   end                                // Нет
    mov  ebx, 1                             // правая рука
    mov  eax, ecx                           // *npc
    call inven_wield_                       // Одеваем оружие
    mov  edx, [ecx+0x20]                    // fid
    mov  pobj, edx                          // Сохраним fid уже с оружием для correctFidForRemovedItem
end:
   }
   break;
  }
 }
 return pobj;                               // Возвращаем fid
}

static void __declspec(naked) correctFidForRemovedItem_hook() {
 __asm {
  call adjust_ac_
nextArmor:
  mov  eax, esi
  call inven_worn_
  test eax, eax
  jz   noArmor
  and  byte ptr [eax+0x27], 0xFB            // Сбрасываем флаг одетой брони
  jmp  nextArmor
noArmor:
  cmp  edi, -1                              // Это игрок?
  jnz  end                                  // Да
  cmp  npcCount, 0
  je   end
  push esi                                  // указатель на нпс
  push eax                                  // новой брони нет
  call ChangeArmorFid
  test eax, eax
  jz   end
  mov  edi, eax
end:
  retn
 }
}

static const DWORD ControlWeapon_hook_End = 0x4494FA;
static const DWORD ControlWeapon_hook_End1 = 0x44952E;
static void __declspec(naked) ControlWeapon_hook() {
 __asm {
  mov  edx, ds:[_dialog_target]
  mov  eax, edx
  call inven_right_hand_
  test eax, eax
  mov  eax, edx                             // _dialog_target
  jnz  haveWeapon
  jmp  ControlWeapon_hook_End
haveWeapon:
  mov  edx, 1                               // правая рука
  call inven_unwield_
  jmp  ControlWeapon_hook_End1
 }
}

static void __declspec(naked) ControlArmor_hook() {
 __asm {
  mov  ecx, eax                             // _dialog_target
  call inven_worn_
  test eax, eax
  jnz  haveArmor
  mov  eax, ecx                             // _dialog_target
  call ai_search_inven_armor_
  jmp  end
haveArmor:
  xor  ebx, ebx                             // новой брони нет
  mov  edx, eax                             // указатель на снимаемую броню
  mov  eax, ecx                             // _dialog_target
  call adjust_ac_
nextArmor:
  mov  eax, ecx                             // _dialog_target
  call inven_worn_
  test eax, eax
  jz   noArmor
  and  byte ptr [eax+0x27], 0xFB            // Сбрасываем флаг одетой брони
  jmp  nextArmor
noArmor:
  cmp  npcCount, 0
  je   end
  push ecx                                  // указатель на нпс
  push eax                                  // новой брони нет
  call ChangeArmorFid
  xor  eax, eax
end:
  retn
 }
}

static void __declspec(naked) fontHeight() {
 __asm {
  push ebx
  mov  ebx, ds:[_curr_font_num]
  mov  eax, 0x65
  call text_font_
  call dword ptr ds:[_text_height]
  xchg ebx, eax
  call text_font_
  xchg ebx, eax
  pop  ebx
  retn
 }
}

static void __declspec(naked) printFreeMaxWeight() {
 __asm {
// ebx = who, ecx = ToWidth, edi = posOffset, esi = extraWeight
  mov  eax, ds:[_curr_font_num]
  push eax
  mov  eax, 101
  call text_font_
  mov  eax, ds:[_i_wid]
  call win_get_buf_                         // eax=ToSurface
  add  edi, eax                             // ToSurface+posOffset (Ypos*ToWidth+Xpos)
  mov  eax, [ebx+0x20]
  and  eax, 0x0F000000
  sar  eax, 0x18
  test eax, eax                             // Это ObjType_Item?
  jz   itsItem                              // Да
  cmp  eax, ObjType_Critter                 // Это ObjType_Critter?
  jne  noWeight                             // Нет
  mov  eax, ebx
  call item_total_weight_                   // eax = общий вес груза
  xchg ebx, eax                             // ebx = общий вес груза, eax = кто
  mov  edx, STAT_carry_amt
  call stat_level_                          // eax = макс. вес груза
  jmp  print
itsItem:
  mov  eax, ebx
  call item_get_type_
  cmp  eax, item_type_container             // Это item_type_container?
  jne  noWeight                             // Нет
  mov  eax, ebx
  call item_c_curr_size_
  xchg ebx, eax
  call item_c_max_size_
print:
  push eax                                  // eax = макс. вес/объём груза
  add  ebx, esi
  sub  eax, ebx                             // eax = свободный вес/размер
  push eax
  xchg ebx, eax
  push 0x503180                             // '%d/%d'
  lea  eax, SizeMsgBuf
  push eax
  call sprintf_
  add  esp, 0x10
  movzx eax, byte ptr ds:[_GreenColor]
  cmp  ebx, 0
  jge  noRed
  mov  al, ds:[_RedColor]
noRed:
  push eax
  lea  esi, SizeMsgBuf
  push esi
  xor  edx, edx
nextChar:
  xor  eax, eax
  mov  al, [esi]
  call dword ptr ds:[_text_char_width]
  inc  eax
  add  edx, eax
  inc  esi
  cmp  byte ptr [esi-1], '/'
  jne  nextChar
  sub  edi, edx
  xchg edi, eax                             // ToSurface+posOffset (Ypos*ToWidth+Xpos)
  mov  ebx, 64                              // TxtWidth
  pop  edx                                  // DisplayText
  call dword ptr ds:[_text_to_buf]
noWeight:
  pop  eax
  call text_font_
  retn
 }
}

static const DWORD display_inventory_hook_End = 0x4700C5;
static void __declspec(naked) display_inventory_hook() {
 __asm {
  call fontHeight
  inc  eax
  add  [esp+0x8], eax                       // height = height + text_height + 1
  call buf_to_buf_
  add  esp, 0x18
  mov  eax, [esp+0x4]
  call art_ptr_unlock_
  pushad
  mov  ebx, ds:[_stack]
  mov  ecx, 537
  mov  edi, 325*537+180+32                  // Xpos=180, Ypos=325, max text width/2=32
  xor  esi, esi
  call printFreeMaxWeight
  popad
  jmp  display_inventory_hook_End
 }
}

static const DWORD display_target_inventory_hook_End = 0x470468;
static void __declspec(naked) display_target_inventory_hook() {
 __asm {
  call fontHeight
  inc  eax
  add  [esp+0x8], eax                       // height = height + text_height + 1
  call buf_to_buf_
  add  esp, 0x18
  mov  eax, [esp]
  call art_ptr_unlock_
  pushad
  mov  ebx, ds:[_target_stack]
  mov  ecx, 537
  mov  edi, 325*537+301+32                  // Xpos=301, Ypos=325, max text width/2=32
  mov  esi, WeightOnBody                    // Учитываем вес одетой на цели брони и оружия
  call printFreeMaxWeight
  popad
  jmp  display_target_inventory_hook_End
 }
}

static void __declspec(naked) display_table_inventories_hook() {
 __asm {
  call win_get_buf_
  mov  edi, ds:[_btable]
  mov  [esp+0x8C+4], edi
  mov  edi, ds:[_ptable]
  mov  [esp+0x94+4], edi
  retn
 }
}

static const DWORD display_table_inventories_hook1_End = 0x47548C;
static void __declspec(naked) display_table_inventories_hook1() {
 __asm {
  add  dword ptr [esp+8], 20
  sub  dword ptr [esp+16], 20*480
  call buf_to_buf_
  add  esp, 0x18
  pushad
  mov  eax, ds:[_btable]
  call item_total_weight_                   // eax = вес вещей цели в окне бартера
  xchg esi, eax
  mov  ebx, ds:[_stack]
  mov  ecx, 480
  mov  edi, 10*480+169+32                   // Xpos=169, Ypos=10, max text width/2=32
  call printFreeMaxWeight
  popad
  jmp  display_table_inventories_hook1_End
 }
}

// Рисуем участок окна
static void __declspec(naked) display_table_inventories_hook2() {
 __asm {
  mov  dword ptr [edx+4], 4                 // WinRect.y_start = 4
  call win_draw_rect_
  retn
 }
}

static const DWORD display_table_inventories_hook3_End = 0x475612;
static void __declspec(naked) display_table_inventories_hook3() {
 __asm {
  add  dword ptr [esp+8], 20
  sub  dword ptr [esp+16], 20*480
  call buf_to_buf_
  add  esp, 0x18
#ifndef TRACE
  cmp  dword ptr ds:[_dialog_target_is_party], 0
  je   end                                  // это торговля
#endif
  pushad
  mov  eax, ds:[_ptable]
  call item_total_weight_                   // eax = вес вещей игрока в окне бартера
  xchg esi, eax
  add  esi, WeightOnBody                    // Учитываем вес одетой на цели брони и оружия
  mov  ebx, ds:[_target_stack]
  mov  ecx, 480
  mov  edi, 10*480+254+32                   // Xpos=254, Ypos=10, max text width/2=32
  call printFreeMaxWeight
  popad
#ifndef TRACE
end:
#endif
  jmp  display_table_inventories_hook3_End
 }
}

static void __declspec(naked) gdProcess_hook() {
 __asm {
  mov  eax, ds:[_dialog_target]
  cmp  dword ptr [eax+0x64], PID_Marcus
  jz   noArmor
  mov  edx, eax
  call ai_search_inven_armor_
  test eax, eax
  jz   noArmor
  xor  ebx, ebx
  xchg edx, eax                             // edx=указатель на объект (броню), eax=_dialog_target
  call inven_wield_
noArmor:
  call gdialog_barter_cleanup_tables_
  retn
 }
}

static void __declspec(naked) invenWieldFunc_hook() {
 __asm {
  call adjust_ac_
  push esi                                  // указатель на нпс
  push edi                                  // указатель на объект (броню)
  call ChangeArmorFid
  retn
 }
}

static void __declspec(naked) SetDefaultAmmo() {
 __asm {
  push eax
  push ebx
  push edx
  xchg edx, eax
  mov  ebx, eax
  call item_get_type_
  cmp  eax, 3                               // Это item_type_weapon?
  jne  end                                  // Нет
  cmp  dword ptr [ebx+0x3C], 0              // Есть патроны в оружии?
  jne  end                                  // Да
  sub  esp, 4
  mov  edx, esp
  mov  eax, [ebx+0x64]                      // eax = pid оружия
  call proto_ptr_
  mov  edx, [esp]
  mov  eax, [edx+0x5C]                      // eax = идентификатор прототипа патронов по умолчанию
  mov  [ebx+0x40], eax                      // прототип используемых патронов
  add  esp, 4
end:
  pop  edx
  pop  ebx
  pop  eax
  retn
 }
}

static const DWORD inven_action_cursor_hook_End = 0x4736CB;
static void __declspec(naked) inven_action_cursor_hook() {
 __asm {
  mov  edx, [esp+0x1C]
  call SetDefaultAmmo
  cmp  dword ptr [esp+0x18], 0
  jmp  inven_action_cursor_hook_End
 }
}

static void __declspec(naked) item_add_mult_hook1() {
 __asm {
  call SetDefaultAmmo
  call item_add_force_
  retn
 }
}

static const DWORD inven_pickup_hook2_End = 0x47146A;
static const DWORD inven_pickup_hook2_End1 = 0x471481;
static void __declspec(naked) inven_pickup_hook2() {
 __asm {
  mov  eax, 246                             // x_start
  mov  ebx, 306                             // x_end
//  mov  edx, 37                              // y_start
//  mov  ecx, 137                             // y_end
  push edi
  mov  edi, cs:[0x471146]
  add  edi, 0x47114A
  call edi                                  // mouse_click_in или процедура из F2_RES
  pop  edi
  test eax, eax
  jz   end
  mov  edx, ds:[_curr_stack]
  test edx, edx
  jz   useOnPlayer
  jmp  inven_pickup_hook2_End
useOnPlayer:
  cmp  edi, 1006                            // Руки?
  jge  end                                  // Да
  mov  edx, [esp+0x18]                      // item
  mov  eax, edx
  call item_get_type_
  cmp  eax, 2                               // item_type_drug
  jne  end
  mov  eax, ds:[_stack]
  push eax
  push edx
  call item_d_take_drug_
  pop  edx
  pop  ebx
  cmp  eax, 1
  jne  notUsed
  xchg ebx, eax
  push edx
  push eax
  call item_remove_mult_
  pop  eax
  xor  ecx, ecx
  mov  ebx, [eax+0x28]
  mov  edx, [eax+0x4]
  pop  eax
  push eax
  call obj_connect_
  pop  eax
  call obj_destroy_
notUsed:
  mov  eax, 1
  call intface_update_hit_points_
end:
  jmp  inven_pickup_hook2_End1
 }
}

static void __declspec(naked) make_drop_button() {
 __asm {
  mov  ebx, [esp+0x28+0x4]
  mov  eax, [ebx+0x20]
  and  eax, 0xF000000
  sar  eax, 0x18
  test eax, eax                             // Это ObjType_Item?
  jnz  skip                                 // Нет
  xchg ebx, eax
  call item_get_type_
  cmp  eax, item_type_container             // Это item_type_container?
  je   goodTarget                           // Да
  jmp  noButton
skip:
  cmp  eax, ObjType_Critter                 // Это ObjType_Critter?
  jne  noButton                             // Нет
  xchg ebx, eax
  call critter_body_type_
  test eax, eax                             // Это Body_Type_Biped?
  jnz  noButton                             // Нет
goodTarget:
  push ebp
  mov  edx, 255                             // DROPN.FRM (Action menu drop normal)
  mov  eax, ObjType_Intrface
  xor  ecx, ecx
  xor  ebx, ebx
  call art_id_
  mov  ecx, 0x59E7E4
  xor  ebx, ebx
  xor  edx, edx
  call art_ptr_lock_data_
  test eax, eax
  jz   noButton
  xchg esi, eax
  push ebp
  mov  edx, 254                             // DROPH.FRM (Action menu drop highlighted )
  mov  eax, ObjType_Intrface
  xor  ecx, ecx
  xor  ebx, ebx
  call art_id_
  mov  ecx, 0x59E7E8
  xor  ebx, ebx
  xor  edx, edx
  call art_ptr_lock_data_
  test eax, eax
  jz   noButton
  push ebp                                  // ButType
  push ebp
  push eax                                  // PicDown
  push esi                                  // PicUp
  xor  ecx, ecx
  dec  ecx
  push ecx                                  // ButtUp
  mov  edx, 68                              // Xpos
  push edx                                  // ButtDown
  push ecx                                  // HovOff
  push ecx                                  // HovOn
  mov  ecx, 40                              // Width
  push ecx                                  // Height
  mov  ebx, 204                             // Ypos
  mov  eax, ds:[_i_wid]                     // WinRef
  call win_register_button_
noButton:
  mov  edx, 436
  retn
 }
}

static char OverloadedDrop[48];
static const DWORD drop_all_End = 0x4740DB;
static const DWORD drop_all_End1 = 0x4741B2;
static const DWORD drop_all_End2 = 0x474435;
static void __declspec(naked) drop_all_() {
 __asm {
  cmp  esi, 'a'
  jne  skip
  jmp  drop_all_End
skip:
  cmp  esi, 'D'
  je   dropKey
  cmp  esi, 'd'
  je   dropKey
  jmp  drop_all_End1
dropKey:
  pushad
  cmp  dword ptr ds:[_gIsSteal], 0
  jne  end
  mov  ecx, [esp+0x134+0x20]
  mov  eax, [ebp+0x20]
  and  eax, 0xF000000
  sar  eax, 0x18
  test eax, eax                             // Это ObjType_Item?
  jz   itsItem                              // Да
  cmp  eax, ObjType_Critter                 // Это ObjType_Critter?
  jne  end                                  // Нет
  mov  eax, ebp
  call critter_body_type_
  test eax, eax                             // Это Body_Type_Biped?
  jnz  end                                  // Нет
  mov  edx, STAT_carry_amt
  mov  eax, ebp
  call stat_level_
  xchg edx, eax                             // edx = макс. вес груза цели
  sub  edx, WeightOnBody                    // Учитываем вес одетой на цели брони и оружия
  mov  eax, ebp
  call item_total_weight_                   // eax = общий вес груза цели
  sub  edx, eax
  mov  eax, ecx
  call item_total_weight_
  jmp  compareSizeWeight
itsItem:
  mov  eax, ebp
  call item_get_type_
  cmp  eax, item_type_container             // Это item_type_container?
  jne  end                                  // Нет
  mov  eax, ebp
  call item_c_max_size_
  xchg edx, eax
  mov  eax, ebp
  call item_c_curr_size_
  sub  edx, eax
  mov  eax, ecx
  call item_c_curr_size_
compareSizeWeight:
  cmp  eax, edx
  jg   cantDrop
  mov  eax, 0x503E14                        // 'ib1p1xx1'
  call gsound_play_sfx_file_
  mov  edx, ebp
  mov  eax, ecx
  call item_move_all_
  mov  ecx, 2
  push ecx
  mov  eax, ds:[_target_curr_stack]
  mov  edx, -1
  push edx
  mov  ebx, ds:[_target_pud]
  mov  eax, ds:[_target_stack_offset][eax*4]
  call display_target_inventory_
  mov  eax, ds:[_curr_stack]
  pop  edx                                  // -1
  pop  ebx                                  // 2
  mov  eax, ds:[_stack_offset][eax*4]
  call display_inventory_
  jmp  end
cantDrop:
  mov  eax, 0x503E14                        // 'ib1p1xx1'
  call gsound_play_sfx_file_
  xor  eax, eax
  push eax
  mov  al, ds:[0x6AB718]                    // color
  push eax
  xor  ebx, ebx
  push ebx
  push eax
  mov  ecx, 169
  push 117
  xor  edx, edx
  lea  eax, OverloadedDrop
  call dialog_out_
end:
  popad
  jmp  drop_all_End2
 }
}

void InventoryInit() {
 mode=GetPrivateProfileInt("Misc", "CritterInvSizeLimitMode", 0, ini);
 invenapcost=GetPrivateProfileInt("Misc", "InventoryApCost", 4, ini);
 invenapqpreduction=GetPrivateProfileInt("Misc", "QuickPocketsApCostReduction", 2, ini);
 MakeCall(0x46E80B, inven_ap_cost_hook, true);
 if(mode>7) mode=0;
 if(mode>=4) {
  mode-=4;
  SafeWrite8(0x477EB3, 0xeb);
 }
 if(mode) {
  MaxItemSize=GetPrivateProfileInt("Misc", "CritterInvSizeLimit", 100, ini);

  //Check item_add_multi (picking stuff from the floor, etc.)
  HookCall(0x4771BD, &ItemAddMultiHook1);
  MakeCall(0x47726D, &ItemAddMultiHook2, true);
  MakeCall(0x42E688, &CritterIsOverloadedHook, true);

  //Check capacity of player and barteree when bartering
  MakeCall(0x474C78, &BarterAttemptTransactionHook1, true);
  MakeCall(0x474CCF, &BarterAttemptTransactionHook2, true);

  //Display total weight on the inventory screen
  SafeWrite32(0x4725FF, (DWORD)&InvenFmt);
  MakeCall(0x4725E0, &DisplayStatsHook, true);
  SafeWrite8(0x47260F, 0x20);
  SafeWrite32(0x4725F9, 0x9c+0xc);
  SafeWrite8(0x472606, 0x10+0xc);
  SafeWrite32(0x472632, 150);
  SafeWrite8(0x472638, 0);

  //Display item weight when examening
  HookCall(0x472FFE, &InvenObjExamineFuncHook);
 }

 if(GetPrivateProfileInt("Misc", "SuperStimExploitFix", 0, ini)) {
  GetPrivateProfileString("sfall", "SuperStimExploitMsg", "You cannot use a super stim on someone who is not injured!", SuperStimMsg, 128, translationIni);
  MakeCall(0x49C3CC, SuperStimFix, true);
 }

 if(GetPrivateProfileInt("Misc", "CheckWeaponAmmoCost", 0, ini)) {
  MakeCall(0x4266E9, &add_check_for_item_ammo_cost, true);
  MakeCall(0x4234B3, &divide_burst_rounds_by_ammo_cost, true);
 }

 ReloadWeaponKey = GetPrivateProfileIntA("Input", "ReloadWeaponKey", 0, ini);
 if (ReloadWeaponKey) HookCall(0x442DFF, &ReloadWeaponHotKey);

 if (GetPrivateProfileInt("Misc", "AutoReloadWeapon", 0, ini)) {
  HookCall(0x422E8A, &AutoReloadWeapon);
 }

 HookCall(0x45419B, &correctFidForRemovedItem_hook);
 MakeCall(0x4494F5, &ControlWeapon_hook, true);
 HookCall(0x449570, &ControlArmor_hook);

 if (!mode && GetPrivateProfileInt("Misc", "FreeWeight", 0, ini)) {
  MakeCall(0x47002D, &display_inventory_hook, true);
  MakeCall(0x4703E9, &display_target_inventory_hook, true);

  HookCall(0x475363, &display_table_inventories_hook);

  SafeWrite16(0x4753D5, 0xD231);
  MakeCall(0x47541F, &display_table_inventories_hook1, true);
  HookCall(0x47558A, &display_table_inventories_hook2);

  SafeWrite16(0x4755BF, 0xFF31);
  MakeCall(0x47560A, &display_table_inventories_hook3, true);
  HookCall(0x4757D2, &display_table_inventories_hook2);
 }

 if (GetPrivateProfileInt("Misc", "EquipArmor", 0, ini)) {
  HookCall(0x4466CC, &gdProcess_hook);
 }

 char buf[MAX_PATH-3];
 int i, count;
 memset(armors,0,sizeof(npcArmor)*PartyMax);
 GetPrivateProfileString("Misc", "ArmorFile", "", buf, MAX_PATH, ini);
 if (strlen(buf)>0) {
  sprintf(iniArmor, ".\\%s", buf);
  count = GetPrivateProfileIntA("Main", "Count", 0, iniArmor);
  char section[4];
  for (i=1; i<=count; i++) {
   _itoa_s(i, section, 10);
   armors[npcCount].Pid = GetPrivateProfileIntA(section, "PID", 0, iniArmor);
   armors[npcCount].Default = GetPrivateProfileIntA(section, "Default", 0, iniArmor);
   armors[npcCount].Leather = GetPrivateProfileIntA(section, "Leather", 0, iniArmor);
   armors[npcCount].Power = GetPrivateProfileIntA(section, "Power", 0, iniArmor);
   armors[npcCount].Advanced = GetPrivateProfileIntA(section, "Advanced", 0, iniArmor);
   armors[npcCount].Metal = GetPrivateProfileIntA(section, "Metal", 0, iniArmor);
   armors[npcCount].Cured = GetPrivateProfileIntA(section, "Cured", 0, iniArmor);
   armors[npcCount].Combat = GetPrivateProfileIntA(section, "Combat", 0, iniArmor);
   armors[npcCount].Robe = GetPrivateProfileIntA(section, "Robe", 0, iniArmor);
   npcCount++;
  }
  if (npcCount>0) HookCall(0x472833, &invenWieldFunc_hook);
 }

 if (GetPrivateProfileIntA("Misc", "StackEmptyWeapons", 0, ini)) {
  MakeCall(0x4736C6, &inven_action_cursor_hook, true);
  HookCall(0x4772AA, &item_add_mult_hook1);
 }

// Не вызывать окошко выбора количества при перетаскивании патронов в оружие
 int ReloadReserve = GetPrivateProfileIntA("Misc", "ReloadReserve", 1, ini);
 if (ReloadReserve >= 0) {
  SafeWrite32(0x47655F, ReloadReserve);      // mov  eax, ReloadReserve
  SafeWrite32(0x476563, 0x097EC139);         // cmp  ecx, eax; jle  0x476570
  SafeWrite16(0x476567, 0xC129);             // sub  ecx, eax
  SafeWrite8(0x476569, 0x91);                // xchg ecx, eax
 };

// Использование химии из инвентаря на картинке игрока
 MakeCall(0x471457, &inven_pickup_hook2, true);

// Кнопка "Положить всё"
 MakeCall(0x46FA7A, &make_drop_button, false);
 MakeCall(0x473EFC, &drop_all_, true);
 GetPrivateProfileString("sfall", "OverloadedDrop", "Sorry, there is no space left.", OverloadedDrop, 48, translationIni);

}

void InventoryReset() {
 invenapcost=GetPrivateProfileInt("Misc", "InventoryApCost", 4, ini);
}
