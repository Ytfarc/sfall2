/*
 *    sfall
 *    Copyright (C) 2008, 2009, 2010, 2011, 2012  The sfall team
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

#include <math.h>
#include <stdio.h>

#include "AI.h"
#include "AmmoMod.h"
#include "AnimationsAtOnceLimit.h"
#include "BarBoxes.h"
#include "Books.h"
#include "Bugs.h"
#include "BurstMods.h"
#include "console.h"
#include "Credits.h"
#include "Criticals.h"
#include "CRC.h"
#include "Define.h"
#include "Elevators.h"
#include "Explosions.h"
#include "FalloutEngine.h"
#include "FileSystem.h"
#include "Graphics.h"
#include "HeroAppearance.h"
#include "Inventory.h"
#include "KillCounter.h"
#include "knockback.h"
#include "LoadGameHook.h"
#include "Logging.h"
#include "MainMenu.h"
#include "movies.h"
#include "PartyControl.h"
#include "perks.h"
#include "Premade.h"
#include "QuestList.h"
#include "Reputations.h"
#include "ScriptExtender.h"
#include "skills.h"
#include "sound.h"
#include "stats.h"
#include "SuperSave.h"
#include "Tiles.h"
#include "timer.h"
#include "version.h"

char ini[65];
char translationIni[65];

static char mapName[65];
static char configName[65];
static char patchName[65];
static char versionString[65];
static char windowName[65];

static char smModelName[65];
char dmModelName[65];
static char sfModelName[65];
char dfModelName[65];

static const char* debugLog="LOG";
static const char* debugGnw="GNW";

static const char* musicOverridePath="data\\sound\\music\\";

bool npcautolevel;

DWORD MotionSensorFlags;

static int* scriptDialog;

static const DWORD AgeMin[] = {
 0x51D860,
 0x4373C9,
 0x4373F3,
 0x43754C,
};

static const DWORD AgeMax[] = {
 0x43731B,
 0x437352,
 0x437536,
};

//GetTickCount calls
static const DWORD offsetsA[] = {
 0x004c8d34,
 0x004c9375,
 0x004c9384,
 0x004c93c0,
 0x004c93e8,
 0x004c9d2e,
 0x004fe01e,
};

//Delayed GetTickCount calls
static const DWORD offsetsB[] = {
 0x004fdf64,
};

//timeGetTime calls
static const DWORD offsetsC[] = {
 0x004a3179,
 0x004a325d,
 0x004f482b,
 0x004f4e53,
 0x004f5542,
 0x004f56cc,
 0x004f59c6,
 0x004fe036,
};

static const DWORD origFuncPosA=0x006c0200;
static const DWORD origFuncPosB=0x006c03b0;
static const DWORD origFuncPosC=0x006c0164;

static const DWORD getLocalTimePos=0x004fdf58;

static const DWORD dinputPos=0x0050FB70;
static const DWORD DDrawPos=0x0050FB5C;

static DWORD AddrGetTickCount;
static DWORD AddrGetLocalTime;

static DWORD ViewportX;
static DWORD ViewportY;

struct sMessage {
 DWORD number;
 DWORD flags;
 char* audio;
 char* message;
};

static const DWORD WalkDistance[] = {
 0x411FF0,                                  // action_use_an_item_on_object_
 0x4121C4,                                  // action_get_an_object_
 0x412475,                                  // action_loot_container_
 0x412906,                                  // action_use_skill_on_
};

static DWORD tmp;

static __declspec(naked) void GetDateWrapper() {
 __asm {
  push ecx;
  push esi;
  push ebx;
  call game_time_date_
  mov ecx, ds:[_pc_proto + 0x4C]
  pop esi;
  test esi, esi;
  jz end;
  add ecx, [esi];
  mov [esi], ecx;
end:
  pop esi;
  pop ecx;
  retn;
 }
}
static void TimerReset() {
 *((DWORD*)_fallout_game_time)=0;
 *((DWORD*)_pc_proto + 0x4C)+=13;
}

static double TickFrac=0;
static double MapMulti=1;
static double MapMulti2=1;
void _stdcall SetMapMulti(float d) { MapMulti2=d; }
static DWORD _stdcall PathfinderFix2(DWORD perkLevel, DWORD ticks) {
 double d = MapMulti*MapMulti2;
 if(perkLevel==1) d*=0.75;
 else if(perkLevel==2) d*=0.5;
 else if(perkLevel==3) d*=0.25;
 d=((double)ticks)*d + TickFrac;
 TickFrac = modf(d, &d);
 return (DWORD)d;
}
static __declspec(naked) void PathfinderFix() {
 __asm {
  push eax;
  mov eax, ds:[_obj_dude]
  mov edx, PERK_pathfinder
  call perk_level_
  push eax;
  call PathfinderFix2;
  jmp  inc_game_time_
 }
}

static double FadeMulti;
static __declspec(naked) void FadeHook() {
 __asm {
  pushf;
  push ebx;
  fild [esp];
  fmul FadeMulti;
  fistp [esp];
  pop ebx;
  popf;
  jmp  fadeSystemPalette_
 }
}

static int mapSlotsScrollMax=27 * (17 - 7);

static __declspec(naked) void ScrollCityListHook() {
 __asm {
  push ebx;
  mov ebx, ds:[0x672F10];
  test eax, eax;
  jl up;
  cmp ebx, mapSlotsScrollMax;
  je end;
  jmp run;
up:
  test ebx, ebx;
  jz end;
run:
  call wmInterfaceScrollTabsStart_
end:
  pop ebx;
  retn;
 }
}

static DWORD wp_delay;
static void __declspec(naked) worldmap_patch() {
 __asm {
  pushad;
  call RunGlobalScripts3;
  mov ecx, wp_delay;
tck:
  mov eax,ds:[0x50fb08];
  call elapsed_time_
  cmp eax,ecx;
  jl tck;
  call get_time_
  mov ds:[0x50fb08],eax;
  popad;
  jmp get_input_
 }
}

static void __declspec(naked) WorldMapEncPatch1() {
 __asm {
  inc  dword ptr ds:[_wmLastRndTime]
  jmp  wmPartyWalkingStep_
 }
}

static void __declspec(naked) WorldMapEncPatch2() {
 __asm {
  mov  dword ptr ds:[_wmLastRndTime], 0
  retn
 }
}

static void __declspec(naked) WorldMapEncPatch3() {
 __asm {
  mov  eax, ds:[_wmLastRndTime]
  retn
 }
}

static void __declspec(naked) Combat_p_procFix() {
 __asm {
  push eax;

  mov eax,dword ptr ds:[_combat_state]
  cmp eax,3;
  jnz end_cppf;

  push esi;
  push ebx;
  push edx;

  mov esi, _main_ctd
  mov eax,[esi];
  mov ebx,[esi+0x20];
  xor edx,edx;
  mov eax,[eax+0x78];
  call scr_set_objs_
  mov eax,[esi];

  cmp dword ptr ds:[esi+0x2c],+0x0;
  jng jmp1;

  test byte ptr ds:[esi+0x15],0x1;
  jz jmp1;
  mov edx,0x2;
  jmp jmp2;
jmp1:
  mov edx,0x1;
jmp2:
  mov eax,[eax+0x78];
  call scr_set_ext_param_
  mov eax,[esi];
  mov edx,0xd;
  mov eax,[eax+0x78];
  call exec_script_proc_
  pop edx;
  pop ebx;
  pop esi;

end_cppf:
  pop eax;
  jmp  stat_level_
 }
}
static double wm_nexttick;
static double wm_wait;
static bool wm_usingperf;
static __int64 wm_perfadd;
static __int64 wm_perfnext;
static DWORD WorldMapLoopCount;
static void WorldMapSpeedPatch3() {
 RunGlobalScripts3();
 if(wm_usingperf) {
  __int64 timer;
  while(true) {
   QueryPerformanceCounter((LARGE_INTEGER*)&timer);
   if(timer>wm_perfnext) break;
   Sleep(0);
  }
  if(wm_perfnext+wm_perfadd < timer) wm_perfnext = timer+wm_perfadd;
  else wm_perfnext+=wm_perfadd;
 } else {
  DWORD tick;
  DWORD nexttick=(DWORD)wm_nexttick;
  while((tick=GetTickCount())<nexttick) Sleep(0);
  if(nexttick+wm_wait < tick) wm_nexttick = tick + wm_wait;
  else wm_nexttick+=wm_wait;
 }
}
static void __declspec(naked) WorldMapSpeedPatch2() {
 __asm {
  pushad;
  call WorldMapSpeedPatch3;
  popad;
  jmp  get_input_
 }
}
static void __declspec(naked) WorldMapSpeedPatch() {
 __asm {
  pushad;
  call RunGlobalScripts3;
  popad;
  push ecx;
  mov ecx, WorldMapLoopCount;
ls:
  mov eax, eax;
  loop ls;
  pop ecx;
  jmp  get_input_
 }
}
//Only used if the world map speed patch is disabled, so that world map scripts are still run
static void WorldMapHook() {
 __asm {
  pushad;
  call RunGlobalScripts3;
  popad;
  jmp  get_input_
 }
}

static void __declspec(naked) ViewportHook() {
 __asm {
  call wmWorldMapLoadTempData_
  mov eax, ViewportX;
  mov  ds:[_wmWorldOffsetX], eax
  mov eax, ViewportY;
  mov  ds:[_wmWorldOffsetY], eax
  retn;
 }
}

HANDLE _stdcall FakeFindFirstFile(const char* str, WIN32_FIND_DATAA* data) {
 HANDLE h=FindFirstFileA(str,data);
 if(h==INVALID_HANDLE_VALUE) return h;
 while(strlen(data->cFileName)>12) {
  int i=FindNextFileA(h, data);
  if(i==0) {
   FindClose(h);
   return INVALID_HANDLE_VALUE;
  }
 }
 return h;
}
int _stdcall FakeFindNextFile(HANDLE h, WIN32_FIND_DATAA* data) {
 int i=FindNextFileA(h, data);
 while(strlen(data->cFileName)>12&&i) {
  i=FindNextFileA(h, data);
 }
 return i;
}

static void __declspec(naked) WeaponAnimHook() {
 __asm {
  cmp edx, 11;
  je c11;
  cmp edx, 15;
  je c15;
  jmp end
c11:
  mov edx, 16;
  jmp end
c15:
  mov edx, 17;
end:
  jmp art_get_code_
 }
}

static char KarmaGainMsg[128];
static char KarmaLossMsg[128];
static void _stdcall SetKarma(int value) {
 int old;
 __asm {
  xor eax, eax;
  call game_get_global_var_
  mov old, eax;
 }
 old=value-old;
 char buf[64];
 if(old==0) return;
 if(old>0) {
  sprintf_s(buf, KarmaGainMsg, old);
 } else {
  sprintf_s(buf, KarmaLossMsg, -old);
 }
 DisplayConsoleMessage(buf);
}
static void __declspec(naked) SetGlobalVarWrapper() {
 __asm {
  test eax, eax;
  jnz end;
  pushad;
  push edx;
  call SetKarma;
  popad;
end:
  jmp game_set_global_var_
 }
}

static void __declspec(naked) ReloadHook() {
 __asm {
  push eax
  push ebx
  push edx
  mov  eax, dword ptr ds:[_obj_dude]
  call register_clear_
  xor  eax, eax
  inc  eax
  call register_begin_
  xor  edx, edx
  xor  ebx, ebx
  mov  eax, dword ptr ds:[_obj_dude]
  dec  ebx
  call register_object_animate_
  call register_end_
  pop  edx
  pop  ebx
  pop  eax
  jmp gsound_play_sfx_file_
 }
}

static void __declspec(naked) obj_shoot_blocking_at_hook() {
 __asm {
  je   itsCritter
  cmp  edx, ObjType_Wall
  retn
itsCritter:
  xchg edi, eax
  call critter_is_dead_                     // check if it's a dead critter
  xchg edi, eax
  test edi, edi
  retn
 }
}

// same logic as above, for different loop
static void __declspec(naked) obj_shoot_blocking_at_hook1() {
 __asm {
  je   itsCritter
  cmp  eax, ObjType_Wall
  retn
itsCritter:
  mov  eax, [edx]
  call critter_is_dead_
  test eax, eax
  retn
 }
}

static DWORD RetryCombatMinAP;
static void __declspec(naked) combat_turn_hook() {
 __asm {
  xor  eax, eax
retry:
  xchg ecx, eax
  mov  eax, esi
  push edx
  call combat_ai_
  pop  edx
process:
  cmp  dword ptr ds:[_combat_turn_running], 0
  jle  next
  call process_bk_
  jmp  process
next:
  mov  eax, [esi+0x40]                      // curr_mp
  cmp  eax, RetryCombatMinAP
  jl   end
  cmp  eax, ecx
  jne  retry
end:
  retn
 }
}

static void __declspec(naked) intface_rotate_numbers_hook() {
 __asm {
// ebx=old value, ecx=new value
  push edi
  push ebp
  sub  esp, 0x54
  cmp  ebx, ecx
  je   end
  mov  ebx, ecx
  jg   greater
  inc  ebx
  jmp  end
greater:
  cmp  ebx, 0
  jg   skip
  xor  ebx, ebx
  inc  ebx
skip:
  dec  ebx
end:
  mov  esi, 0x460BA6
  jmp  esi
 }
}

static DWORD KarmaFrmCount;
static DWORD* KarmaFrms;
static int* KarmaPoints;
static DWORD _stdcall DrawCardHook2() {
 int rep=**(int**)_game_global_vars;
 for(DWORD i=0;i<KarmaFrmCount-1;i++) {
  if(rep < KarmaPoints[i]) return KarmaFrms[i];
 }
 return KarmaFrms[KarmaFrmCount-1];
}
static void __declspec(naked) DrawCardHook() {
 __asm {
  cmp ds:[_info_line], 10;
  jne skip;
  cmp eax, 0x30;
  jne skip;
  push ecx;
  push edx;
  call DrawCardHook2;
  pop edx;
  pop ecx;
skip:
  jmp DrawCard_
 }
}

static void __declspec(naked) ScienceCritterCheckHook() {
 __asm {
  cmp  esi, ds:[_obj_dude]
  jne  end
  mov  eax, 10
  retn
end:
  jmp  critter_kill_count_type_
 }
}

static const DWORD NPCStage6Fix1End = 0x493D16;
static void __declspec(naked) NPCStage6Fix1() {
 __asm {
  mov eax,0xcc;    // set record size to 204 bytes
  imul eax,edx;    // multiply by number of NPC records in party.txt
  call mem_malloc_   // malloc the necessary memory
  mov edx,dword ptr ds:[_partyMemberMaxCount]; // retrieve number of NPC records in party.txt
  mov ebx,0xcc;    // set record size to 204 bytes
  imul ebx,edx;    // multiply by number of NPC records in party.txt
  jmp NPCStage6Fix1End;   // call memset to set all malloc'ed memory to 0
 }
}

static const DWORD NPCStage6Fix2End = 0x49423A;
static void __declspec(naked) NPCStage6Fix2() {
 __asm {
  mov eax,0xcc;    // record size is 204 bytes
  imul edx,eax;    // multiply by NPC number as listed in party.txt
  mov eax,dword ptr ds:[_partyMemberAIOptions]; // get starting offset of internal NPC table
  jmp NPCStage6Fix2End;   // eax+edx = offset of specific NPC record
 }
}

static void __declspec(naked) MultiHexFix() {
 __asm {
  xor  ecx, ecx
  test byte ptr [ebx+0x25], 0x8             // is target multihex?
  mov  ebx, dword ptr [ebx+0x4]             // ebx = pobj.tile_num (target's tilenum)
  jz   end                                  // skip if not multihex
  inc  ebx                                  // otherwise, increase tilenum by 1
end:
  retn
 }
}

static void __declspec(naked) item_w_called_shot_hook() {
 __asm {
  mov  edx, 0x478E7F
  test eax, eax                             // does player have Fast Shot trait?
  jz   end                                  // skip ahead if no
  push edx
  mov  edx, ecx                             // hit_mode
  mov  eax, ebx                             // who
  call item_w_range_                        // get weapon's range
  pop  edx
  cmp  eax, 2                               // is weapon range less than or equal 2 (i.e. melee/unarmed attack)?
  jle  end                                  // skip ahead if yes
  mov  edx, 0x478E9B                        // otherwise, disallow called shot attempt
end:
  jmp  edx
 }
}

static void __declspec(naked) automap_hook() {
 __asm {
  mov  edx, PID_MOTION_SENSOR
  jmp  inven_pid_is_carried_ptr_
 }
}

static void __declspec(naked) objCanSeeObj_ShootThru_Fix() {//(EAX *objStruct, EDX hexNum1, EBX hexNum2, ECX ?, stack1 **ret_objStruct, stack2 flags)
 __asm {
   push esi
   push edi

   push obj_shoot_blocking_at_ //arg3 check hex objects func pointer
   mov esi, 0x20//arg2 flags, 0x20 = check shootthru
   push esi
   mov edi, dword ptr ss : [esp + 0x14] //arg1 **ret_objStruct
   push edi
   call make_straight_path_func_ //(EAX *objStruct, EDX hexNum1, EBX hexNum2, ECX ?, stack1 **ret_objStruct, stack2 flags, stack3 *check_hex_objs_func)

   pop edi
   pop esi
   ret 0x8
 }
}

static byte XltTable[94];
static byte XltKey = 4;                     // 4 = Scroll Lock, 2 = Caps Lock, 1 = Num Lock
static void __declspec(naked) get_input_str_hook() {
 __asm {
  push ecx
  mov  cl, XltKey
  test byte ptr ds:[_kb_lock_flags], cl
  jz   end
  mov  ecx, offset XltTable
  and  eax, 0xFF
  mov  al, [ecx+eax-0x20]
end:
  mov  byte ptr [esp+esi+4], al
  mov  eax, 0x433F43
  jmp  eax
 }
}

static void __declspec(naked) get_input_str2_hook() {
 __asm {
  push edx
  mov  dl, XltKey
  test byte ptr ds:[_kb_lock_flags], dl
  jz   end
  mov  edx, offset XltTable
  and  eax, 0xFF
  mov  al, [edx+eax-0x20]
end:
  mov  byte ptr [esp+edi+4], al
  mov  eax, 0x47F369
  jmp  eax
 }
}

static void __declspec(naked) kb_next_ascii_English_US_hook() {
 __asm {
  mov  dh, [eax]
  cmp  dh, 0x1A                             // DIK_LBRACKET
  je   end
  cmp  dh, 0x1B                             // DIK_RBRACKET
  je   end
  cmp  dh, 0x27                             // DIK_SEMICOLON
  je   end
  cmp  dh, 0x28                             // DIK_APOSTROPHE
  je   end
  cmp  dh, 0x33                             // DIK_COMMA
  je   end
  cmp  dh, 0x34                             // DIK_PERIOD
  je   end
  cmp  dh, 0x30                             // DIK_B
end:
  mov  eax, 0x4CC35D
  jmp  eax
 }
}

static void __declspec(naked) pipboy_hook() {
 __asm {
  call get_input_
  cmp  eax, '1'
  jne  notOne
  mov  eax, 0x1F4
  jmp  click
notOne:
  cmp  eax, '2'
  jne  notTwo
  mov  eax, 0x1F8
  jmp  click
notTwo:
  cmp  eax, '3'
  jne  notThree
  mov  eax, 0x1F5
  jmp  click
notThree:
  cmp  eax, '4'
  jne  notFour
  mov  eax, 0x1F6
click:
  push eax
  mov  eax, 0x50CC50                        // 'ib1p1xx1'
  call gsound_play_sfx_file_
  pop  eax
notFour:
  retn
 }
}

static void __declspec(naked) FirstTurnAndNoEnemy() {
 __asm {
  xor  eax, eax
  test byte ptr ds:[_combat_state], 1
  jz   end                                  // �� � ���
  cmp  dword ptr ds:[_combatNumTurns], eax
  jne  end                                  // ��� �� ������ ���
  call combat_should_end_
  test eax, eax                             // ����� ����?
  jz   end                                  // ��
  pushad
  mov  ecx, ds:[_list_total]
  mov  edx, ds:[_obj_dude]
  mov  edx, [edx+0x50]                      // team_num ������ ��������� ������
  mov  edi, ds:[_combat_list]
loopCritter:
  mov  eax, [edi]                           // eax = ��������
  mov  ebx, [eax+0x50]                      // team_num ������ ��������� ���������
  cmp  edx, ebx                             // �������?
  je   nextCritter                          // ��
  mov  eax, [eax+0x54]                      // who_hit_me
  test eax, eax                             // � ��������� ��������?
  jz   nextCritter                          // ���
  cmp  edx, [eax+0x50]                      // �������� �� ������ ��������� ������?
  jne  nextCritter                          // ���
  popad
  dec  eax                                  // ��������!!!
  retn
nextCritter:
  add  edi, 4                               // � ���������� ��������� � ������
  loop loopCritter                          // ���������� ���� ������
  popad
end:
  retn
 }
}

sMessage cantdothat = {675, 0, 0, 0};       // '� �� ���� ����� �������.'
static void __declspec(naked) FirstTurnCheckDist() {
 __asm {
  push eax
  push edx
  call obj_dist_
  cmp  eax, 1                               // ���������� �� ������� ������ 1?
  pop  edx
  pop  eax
  jle  end                                  // ���
  lea  edx, cantdothat
  mov  eax, _proto_main_msg_file
  call message_search_
  cmp  eax, 1
  jne  skip
  mov  eax, cantdothat.message
  call display_print_
skip:
  pop  eax                                  // ���������� ����� ��������
  xor  eax, eax
  dec  eax
end:
  retn
 }
}

static void __declspec(naked) check_move_hook() {
 __asm {
  call FirstTurnAndNoEnemy
  test eax, eax                             // ��� ������ ��� � ��� � ������ ���?
  jnz  skip                                 // ��
  cmp  dword ptr [ecx], -1
  je   end
  retn
skip:
  xor  esi, esi
  dec  esi
end:
  pop  eax                                  // ���������� ����� ��������
  mov  eax, 0x4180A7
  jmp  eax
 }
}

static void __declspec(naked) gmouse_bk_process_hook() {
 __asm {
  xchg edi, eax
  call FirstTurnAndNoEnemy
  test eax, eax                             // ��� ������ ��� � ��� � ������ ���?
  jnz  end                                  // ��
  xchg edi, eax
  cmp  eax, dword ptr [edx+0x40]
  jg   end
  retn
end:
  pop  eax                                  // ���������� ����� ��������
  mov  eax, 0x44B8C5
  jmp  eax
 }
}

static void __declspec(naked) FakeCombatFix1() {
 __asm {
  push eax                                  // _obj_dude
  call FirstTurnAndNoEnemy
  test eax, eax                             // ��� ������ ��� � ��� � ������ ���?
  pop  eax
  jz   end                                  // ���
  call FirstTurnCheckDist
end:
  jmp  action_get_an_object_
 }
}

static void __declspec(naked) FakeCombatFix2() {
 __asm {
  push eax                                  // _obj_dude
  call FirstTurnAndNoEnemy
  test eax, eax                             // ��� ������ ��� � ��� � ������ ���?
  pop  eax
  jz   end                                  // ���
  call FirstTurnCheckDist
end:
  jmp  action_loot_container_
 }
}

static void __declspec(naked) FakeCombatFix3() {
 __asm {
  cmp  dword ptr ds:[_obj_dude], eax
  jne  end
  push eax
  call FirstTurnAndNoEnemy
  test eax, eax                             // ��� ������ ��� � ��� � ������ ���?
  pop  eax
  jz   end                                  // ���
  call FirstTurnCheckDist
end:
  jmp  action_use_an_item_on_object_
 }
}

static void __declspec(naked) wmTownMapFunc_hook() {
 __asm {
  cmp  edx, 0x31
  jl   end
  cmp  edx, ecx
  jge  end
  push edx
  sub  edx, 0x31
  lea  eax, ds:0[edx*8]
  sub  eax, edx
  pop  edx
  cmp  dword ptr [edi+eax*4+0x0], 0         // Visited
  je   end
  cmp  dword ptr [edi+eax*4+0x4], -1        // Xpos
  je   end
  cmp  dword ptr [edi+eax*4+0x8], -1        // Ypos
  je   end
  retn
end:
  pop  eax                                  // ���������� ����� ��������
  mov  eax, 0x4C4976
  jmp  eax
 }
}

FILETIME ftCurr, ftPrev;
static void _stdcall _GetFileTime(char* filename) {
 char fname[65];
 sprintf_s(fname, "%s%s", "data\\", filename);
 HANDLE hFile = CreateFile(fname, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
 if (hFile != INVALID_HANDLE_VALUE) {
  GetFileTime(hFile, NULL, NULL, &ftCurr);
  CloseHandle(hFile);
 } else {
  ftCurr.dwHighDateTime = 0;
  ftCurr.dwLowDateTime = 0;
 };
}

static const char* commentFmt="%02d/%02d/%d  %02d:%02d:%02d";
static void _stdcall createComment(char* bufstr) {
 SYSTEMTIME stUTC, stLocal;
 char buf[30];
 GetSystemTime(&stUTC);
 SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);
 sprintf_s(buf, commentFmt, stLocal.wDay, stLocal.wMonth, stLocal.wYear, stLocal.wHour, stLocal.wMinute, stLocal.wSecond);
 strcpy(bufstr, buf);
}

static DWORD AutoQuickSave = 0;
static void __declspec(naked) SaveGame_hook() {
 __asm {
  pushad
  mov  ecx, dword ptr ds:[_slot_cursor]
  mov  dword ptr ds:[_flptr], eax
  test eax, eax
  jz   end                                  // ��� ������ ����, ����� ����������
  call db_fclose_
  push ecx
  push edi
  call _GetFileTime
  pop  ecx
  mov  edx, ftCurr.dwHighDateTime
  mov  ebx, ftCurr.dwLowDateTime
  jecxz nextSlot                            // ��� ������ ����
  cmp  edx, ftPrev.dwHighDateTime
  ja   nextSlot                             // ������� ���� ������� ����� �����������
  jb   end                                  // ������� ���� ������� ������ �����������
  cmp  ebx, ftPrev.dwLowDateTime
  jbe  end
nextSlot:
  mov  ftPrev.dwHighDateTime, edx
  mov  ftPrev.dwLowDateTime, ebx
  inc  ecx
  cmp  ecx, AutoQuickSave                   // ��������� ����+1?
  ja   firstSlot                            // ��
  mov  dword ptr ds:[_slot_cursor], ecx
  popad
  mov  eax, 0x47B929
  jmp  eax
firstSlot:
  xor  ecx, ecx
end:
  mov  dword ptr ds:[_slot_cursor], ecx
  mov  eax, ecx
  shl  eax, 4
  add  eax, ecx
  shl  eax, 3
  add  eax, _LSData+0x3D                    // eax->_LSData[_slot_cursor].Comment
  push eax
  call createComment
  popad
  xor  ebx, ebx
  inc  ebx
  mov  dword ptr ds:[_quick_done], ebx
  mov  ebx, 0x47B9A4
  jmp  ebx
 }
}

static char LvlMsg[6];
static char extraFmt[16]="%d/%d  (%s: %d)";
static void __declspec(naked) partyMemberCurLevel() {
 __asm {
  mov  ebx, eax                             // _dialog_target
  mov  edx, offset tmp
  call partyMemberGetAIOptions_
  inc  eax
  test eax, eax
  jz   skip
  mov  eax, ebx                             // _dialog_target
  call partyMemberGetCurLevel_
skip:
  push eax
  mov  eax, offset LvlMsg
  push eax
  mov  edx, STAT_max_hit_points
  mov  eax, ebx                             // _dialog_target
  call stat_level_
  push eax
  mov  edx, STAT_current_hp
  mov  eax, ebx                             // eax=_dialog_target
  call stat_level_
  push eax
  mov  eax, offset extraFmt
  push eax
  lea  eax, [esp+5*4]
  push eax
  call sprintf_
  add  esp, 6*4
  xchg ebx, eax                             // eax=_dialog_target
  mov  edx, 2                               // type = �����������
  call queue_find_first_
  test eax, eax
  mov  al, byte ptr ds:[_GreenColor]
  jz   end
  mov  al, byte ptr ds:[_RedColor]
end:
  and  eax, 0xFF
  mov  ebx, 0x44909D
  jmp  ebx
 }
}

static char AcMsg[6];
static void __declspec(naked) partyMemberAC() {
 __asm {
  mov  ecx, eax                             // _dialog_target
  mov  edx, STAT_ac
  call stat_level_
  push eax
  mov  eax, offset AcMsg
  push eax
  xchg ecx, eax                             // eax=_dialog_target
  mov  edx, STAT_max_move_points
  call stat_level_
  push eax
  push ebx
  mov  eax, offset extraFmt
  push eax
  lea  eax, [esp+5*4]
  push eax
  call sprintf_
  add  esp, 6*4
  xor  eax, eax
  mov  edx, 0x44923D
  jmp  edx
 }
}

static void __declspec(naked) print_with_linebreak() {
 __asm {
  push esi
  push ecx
  test eax, eax                             // � ���� ������?
  jz   end                                  // ���
  mov  esi, eax
  xor  ecx, ecx
loopString:
  cmp  byte ptr [esi], 0                    // ����� ������
  je   printLine                            // ��
  cmp  byte ptr [esi], 0x5C                 // �������� ������� ������? '\'
  jne  nextChar                             // ���
  cmp  byte ptr [esi+1], 0x6E               // ����� ������� ������? 'n'
  jne  nextChar                             // ���
  inc  ecx
  mov  byte ptr [esi], 0
printLine:
  call edi
  jecxz end
  dec  ecx
  mov  byte ptr [esi], 0x5C
  inc  esi
  mov  eax, esi
  inc  eax
nextChar:
  inc  esi
  jmp  loopString
end:
  pop  ecx
  pop  esi
  retn
 }
}

static void __declspec(naked) display_print_with_linebreak() {
 __asm {
  push edi
  mov  edi, display_print_
  call print_with_linebreak
  pop  edi
  retn
 }
}

static void __declspec(naked) inven_display_msg_with_linebreak() {
 __asm {
  push edi
  mov  edi, inven_display_msg_
  call print_with_linebreak
  pop  edi
  retn
 }
}

static char EncounterMsg[128];
static void __declspec(naked) wmSetupRandomEncounter_hook() {
 __asm {
  push eax
  push edi
  push 0x500B64                             // '%s %s'
  lea  eax, EncounterMsg
  push eax
  xchg edi, eax
  call sprintf_
  add  esp, 4*4
  xchg edi, eax
  jmp  display_print_
 }
}

static int drugExploit = 0;
static void __declspec(naked) protinst_use_item_hook() {
 __asm {
  dec  drugExploit
  call obj_use_book_
  inc  drugExploit
  retn
 }
}

static void __declspec(naked) UpdateLevel_hook() {
 __asm {
  inc  drugExploit
  call perks_dialog_
  dec  drugExploit
  retn
 }
}

static void __declspec(naked) skill_level_hook() {
 __asm {
  dec  drugExploit
  call skill_level_
  inc  drugExploit
  retn
 }
}

static void __declspec(naked) SliderBtn_hook() {
 __asm {
  dec  drugExploit
  call skill_inc_point_
  inc  drugExploit
  retn
 }
}

static void __declspec(naked) SliderBtn_hook1() {
 __asm {
  dec  drugExploit
  call skill_dec_point_
  inc  drugExploit
  retn
 }
}

static void __declspec(naked) stat_level_hook() {
 __asm {
  call stat_get_bonus_
  cmp  esi, STAT_lu                         // ��������� ������ ����-�����
  ja   end
//  test eax, eax                             // � ���� ���� ����� [+/-]�����?
//  jz   end                                  // ���
  cmp  drugExploit, 0                       // ����� �� ������ ����?
  jl   checkPenalty                         // �������� ������ ����/������
  jg   noBonus                              // ��������� ������
  retn
checkPenalty:
  cmp  eax, 1                               // ������������� ������?
  jge  end                                  // �� - ��������� ���
noBonus:
  xor  eax, eax                             // �� ��������� ������ �� ����������/��������/etc
end:
  retn
 }
}

static void __declspec(naked) barter_attempt_transaction_hook() {
 __asm {
  cmp  dword ptr [eax+0x64], PID_ACTIVE_GEIGER_COUNTER
  je   found
  cmp  dword ptr [eax+0x64], PID_ACTIVE_STEALTH_BOY
  je   found
  mov  eax, 0x474D34
  jmp  eax
found:
  call item_m_turn_off_
  mov  eax, 0x474D17
  jmp  eax                                  // � ���� �� ��� ���������� �������� ����� �����������?
 }
}

static void __declspec(naked) item_m_turn_off_hook() {
 __asm {
  and  byte ptr [eax+0x25], 0xDF            // ������� ���� ��������������� ��������
  jmp  queue_remove_this_
 }
}

static void DllMain2() {
 //SafeWrite8(0x4B15E8, 0xc3);
 //SafeWrite8(0x4B30C4, 0xc3); //this is the one I need to override for bigger tiles
 dlogr("In DllMain2", DL_MAIN);

 //BlockCall(0x4123BC);

 if(GetPrivateProfileIntA("Speed", "Enable", 0, ini)) {
  dlog("Applying speed patch.", DL_INIT);
  AddrGetTickCount = (DWORD)&FakeGetTickCount;
  AddrGetLocalTime = (DWORD)&FakeGetLocalTime;

  for(int i=0;i<sizeof(offsetsA)/4;i++) {
   SafeWrite32(offsetsA[i], (DWORD)&AddrGetTickCount);
  }
  dlog(".", DL_INIT);
  for(int i=0;i<sizeof(offsetsB)/4;i++) {
   SafeWrite32(offsetsB[i], (DWORD)&AddrGetTickCount);
  }
  dlog(".", DL_INIT);
  for(int i=0;i<sizeof(offsetsC)/4;i++) {
   SafeWrite32(offsetsC[i], (DWORD)&AddrGetTickCount);
  }
  dlog(".", DL_INIT);

  SafeWrite32(getLocalTimePos, (DWORD)&AddrGetLocalTime);
  TimerInit();
  dlogr(" Done", DL_INIT);
 }

 //if(GetPrivateProfileIntA("Input", "Enable", 0, ini)) {
  dlog("Applying input patch.", DL_INIT);
  SafeWriteStr(dinputPos, "ddraw.dll");
  AvailableGlobalScriptTypes|=1;
  dlogr(" Done", DL_INIT);
 //}

 GraphicsMode=GetPrivateProfileIntA("Graphics", "Mode", 0, ini);
 if(GraphicsMode!=4&&GraphicsMode!=5) GraphicsMode=0;
 if(GraphicsMode==4||GraphicsMode==5) {
  dlog("Applying dx9 graphics patch.", DL_INIT);
  HMODULE h=LoadLibraryEx("d3dx9_43.dll", 0, LOAD_LIBRARY_AS_DATAFILE);
  if(!h) {
   MessageBoxA(0, "You have selected graphics mode 4 or 5, but d3dx9_43.dll is missing\nSwitch back to mode 0, or install an up to date version of DirectX", "Error", 0);
   ExitProcess(-1);
  } else {
   FreeLibrary(h);
  }
  SafeWrite8(0x0050FB6B, '2');
  dlogr(" Done", DL_INIT);
 }
 tmp=GetPrivateProfileIntA("Graphics", "FadeMultiplier", 100, ini);
 if(tmp!=100) {
  dlog("Applying fade patch.", DL_INIT);
  SafeWrite32(0x00493B17, ((DWORD)&FadeHook) - 0x00493B1b);
  FadeMulti=((double)tmp)/100.0;
  dlogr(" Done", DL_INIT);
 }

 AmmoModInit();
 MoviesInit();

 mapName[64]=0;
 if(GetPrivateProfileString("Misc", "StartingMap", "", mapName, 64, ini)) {
  dlog("Applying starting map patch.", DL_INIT);
  SafeWrite32(0x00480AAA, (DWORD)&mapName);
  dlogr(" Done", DL_INIT);
 }

 versionString[64]=0;
 if(GetPrivateProfileString("Misc", "VersionString", "", versionString, 64, ini)) {
  dlog("Applying version string patch.", DL_INIT);
  SafeWrite32(0x004B4588, (DWORD)&versionString);
  dlogr(" Done", DL_INIT);
 }

 configName[64]=0;
 if(GetPrivateProfileString("Misc", "ConfigFile", "", configName, 64, ini)) {
  dlog("Applying config file patch.", DL_INIT);
  SafeWrite32(0x00444BA5, (DWORD)&configName);
  SafeWrite32(0x00444BCA, (DWORD)&configName);
  dlogr(" Done", DL_INIT);
 }

 patchName[64]=0;
 if(GetPrivateProfileString("Misc", "PatchFile", "", patchName, 64, ini)) {
  dlog("Applying patch file patch.", DL_INIT);
  SafeWrite32(0x00444323, (DWORD)&patchName);
  dlogr(" Done", DL_INIT);
 }

 smModelName[64]=0;
 if(GetPrivateProfileString("Misc", "MaleStartModel", "", smModelName, 64, ini)) {
  dlog("Applying male start model patch.", DL_INIT);
  SafeWrite32(0x00418B88, (DWORD)&smModelName);
  dlogr(" Done", DL_INIT);
 }

 sfModelName[64]=0;
 if(GetPrivateProfileString("Misc", "FemaleStartModel", "", sfModelName, 64, ini)) {
  dlog("Applying female start model patch.", DL_INIT);
  SafeWrite32(0x00418BAB, (DWORD)&sfModelName);
  dlogr(" Done", DL_INIT);
 }

 dmModelName[64]=0;
 GetPrivateProfileString("Misc", "MaleDefaultModel", "hmjmps", dmModelName, 64, ini);
 dlog("Applying male model patch.", DL_INIT);
 SafeWrite32(0x00418B50, (DWORD)&dmModelName);
 dlogr(" Done", DL_INIT);

 dfModelName[64]=0;
 GetPrivateProfileString("Misc", "FemaleDefaultModel", "hfjmps", dfModelName, 64, ini);
 dlog("Applying female model patch.", DL_INIT);
 SafeWrite32(0x00418B6D, (DWORD)&dfModelName);
 dlogr(" Done", DL_INIT);

 int date=GetPrivateProfileInt("Misc", "StartYear", -1, ini);
 if(date!=-1) {
  dlog("Applying starting year patch.", DL_INIT);
  SafeWrite32(0x4A336C, date);
  dlogr(" Done", DL_INIT);
 }
 date=GetPrivateProfileInt("Misc", "StartMonth", -1, ini);
 if(date!=-1) {
  dlog("Applying starting month patch.", DL_INIT);
  SafeWrite32(0x4A3382, date);
  dlogr(" Done", DL_INIT);
 }
 date=GetPrivateProfileInt("Misc", "StartDay", -1, ini);
 if(date!=-1) {
  dlog("Applying starting day patch.", DL_INIT);
  SafeWrite8(0x4A3356, date);
  dlogr(" Done", DL_INIT);
 }

 date=GetPrivateProfileInt("Misc", "LocalMapXLimit", 0, ini);
 if(date) {
  dlog("Applying local map x limit patch.", DL_INIT);
  SafeWrite32(0x004B13B9, date);
  dlogr(" Done", DL_INIT);
 }
 date=GetPrivateProfileInt("Misc", "LocalMapYLimit", 0, ini);
 if(date) {
  dlog("Applying local map y limit patch.", DL_INIT);
  SafeWrite32(0x004B13C7, date);
  dlogr(" Done", DL_INIT);
 }

 date=GetPrivateProfileInt("Misc", "StartXPos", -1, ini);
 if(date!=-1) {
  dlog("Applying starting x position patch.", DL_INIT);
  SafeWrite32(0x4BC990, date);
  SafeWrite32(0x4BCC08, date);
  dlogr(" Done", DL_INIT);
 }
 date=GetPrivateProfileInt("Misc", "StartYPos", -1, ini);
 if(date!=-1) {
  dlog("Applying starting y position patch.", DL_INIT);
  SafeWrite32(0x4BC995, date);
  SafeWrite32(0x4BCC0D, date);
  dlogr(" Done", DL_INIT);
 }
 ViewportX=GetPrivateProfileInt("Misc", "ViewXPos", -1, ini);
 if(ViewportX!=-1) {
  dlog("Applying starting x view patch.", DL_INIT);
  SafeWrite32(_wmWorldOffsetX, ViewportX);
  SafeWrite32(0x004BCF08, (DWORD)&ViewportHook - 0x4BCF0C);
  dlogr(" Done", DL_INIT);
 }
 ViewportY=GetPrivateProfileInt("Misc", "ViewYPos", -1, ini);
 if(ViewportY!=-1) {
  dlog("Applying starting y view patch.", DL_INIT);
  SafeWrite32(_wmWorldOffsetY, ViewportY);
  SafeWrite32(0x004BCF08, (DWORD)&ViewportHook - 0x4BCF0C);
  dlogr(" Done", DL_INIT);
 }

 //if(GetPrivateProfileIntA("Misc", "PathfinderFix", 0, ini)) {
  dlog("Applying pathfinder patch.", DL_INIT);
  SafeWrite8(0x4C1FF7, 0xC0);               // sub eax, eax
  SafeWrite32(0x004C1C79, ((DWORD)&PathfinderFix) - 0x004c1c7d);
  MapMulti=(double)GetPrivateProfileIntA("Misc", "WorldMapTimeMod", 100, ini)/100.0;
  dlogr(" Done", DL_INIT);
 //}

 if(GetPrivateProfileInt("Misc", "WorldMapFPSPatch", 0, ini)) {
  dlog("Applying world map fps patch.", DL_INIT);
  if(*(DWORD*)0x004BFE5E != 0x8d16) {
   dlogr(" Failed", DL_INIT);
  } else {
   wp_delay=GetPrivateProfileInt("Misc", "WorldMapDelay2", 66, ini);
   HookCall(0x004BFE5D, worldmap_patch);
   dlogr(" Done", DL_INIT);
  }
 } else {
  tmp=GetPrivateProfileIntA("Misc", "WorldMapFPS", 0, ini);
  if(tmp) {
   dlog("Applying world map fps patch.", DL_INIT);
   if(*((WORD*)0x004CAFB9)==0x0000) {
    AvailableGlobalScriptTypes|=2;
    SafeWrite32(0x004BFE5E, ((DWORD)&WorldMapSpeedPatch2) - 0x004BFE62);
    if(GetPrivateProfileIntA("Misc", "ForceLowResolutionTimer", 0, ini)||!QueryPerformanceFrequency((LARGE_INTEGER*)&wm_perfadd)||wm_perfadd<=1000) {
     wm_wait=1000.0/(double)tmp;
     wm_nexttick=GetTickCount();
     wm_usingperf=false;
    } else {
     wm_usingperf=true;
     wm_perfadd/=tmp;
     wm_perfnext=0;
    }
   }
   dlogr(" Done", DL_INIT);
  } else {
   tmp=GetPrivateProfileIntA("Misc", "WorldMapDelay", 0, ini);
   if(tmp) {
    if(*((WORD*)0x004CAFB9)==0x3d40)
     SafeWrite32(0x004CAFBB, tmp);
    else if(*((WORD*)0x004CAFB9)==0x0000) {
     SafeWrite32(0x004BFE5E, ((DWORD)&WorldMapSpeedPatch) - 0x004BFE62);
     WorldMapLoopCount=tmp;
    }
   } else {
    if(*(DWORD*)0x004BFE5E==0x0000d816) {
     SafeWrite32(0x004BFE5E, ((DWORD)&WorldMapHook) - 0x004BFE62);
    }
   }
  }
  if(GetPrivateProfileIntA("Misc", "WorldMapEncounterFix", 0, ini)) {
   dlog("Applying world map encounter patch.", DL_INIT);
   SafeWrite32(0x004bfee1, ((DWORD)&WorldMapEncPatch1) - 0x004bfee5);
   SafeWrite32(0x004c0663, ((DWORD)&WorldMapEncPatch3) - 0x004c0667);//replaces 'call Difference(GetTickCount(), TicksSinceLastEncounter)'
   SafeWrite16(0x004c0668, GetPrivateProfileIntA("Misc", "WorldMapEncounterRate", 5, ini)); //replaces cmp eax, 0x5dc with cmp eax, <rate>
   SafeWrite16(0x004c0677, 0xE890); //nop, call relative. Replaces 'mov TicksSinceLastEncounter, ecx'
   SafeWrite32(0x004c0679, ((DWORD)&WorldMapEncPatch2) - 0x004c067D);
   SafeWrite8(0x004c232d, 0xb8); //'mov eax, 0', replaces 'mov eax, GetTickCount()'
   SafeWrite32(0x004c232e, 0);
   dlogr(" Done", DL_INIT);
  }
 }

 if(GetPrivateProfileIntA("Misc", "DialogueFix", 1, ini)) {
  dlog("Applying dialogue patch.", DL_INIT);
  SafeWrite8(0x00446848, 0x31);
  dlogr(" Done", DL_INIT);
 }

 if(GetPrivateProfileIntA("Misc", "ExtraKillTypes", 0, ini)) {
  dlog("Applying extra kill types patch.", DL_INIT);
  KillCounterInit(true);
  dlogr(" Done", DL_INIT);
 } else KillCounterInit(false);

 //if(GetPrivateProfileIntA("Misc", "ScriptExtender", 0, ini)) {
  dlog("Applying script extender patch.", DL_INIT);
  StatsInit();
  dlog(".", DL_INIT);
  ScriptExtenderSetup();
  dlog(".", DL_INIT);
  LoadGameHookInit();
  dlog(".", DL_INIT);
  PerksInit();
  dlog(".", DL_INIT);
  KnockbackInit();
  dlog(".", DL_INIT);
  SkillsInit();
  dlog(".", DL_INIT);

  //Ray's combat_p_proc fix
  SafeWrite32(0x0425253, ((DWORD)&Combat_p_procFix) - 0x0425257);
  SafeWrite8(0x0424dbc, 0xE9);
  SafeWrite32(0x0424dbd, 0x00000034);
  dlogr(" Done", DL_INIT);
 //}

 //if(GetPrivateProfileIntA("Misc", "WorldMapCitiesListFix", 0, ini)) {
  dlog("Applying world map cities list patch.", DL_INIT);

  SafeWrite32(0x004C04BA, ((DWORD)&ScrollCityListHook) - 0x004C04BE);
  SafeWrite32(0x004C04C9, ((DWORD)&ScrollCityListHook) - 0x004C04CD);
  SafeWrite32(0x004C4A35, ((DWORD)&ScrollCityListHook) - 0x004C4A39);
  SafeWrite32(0x004C4A3E, ((DWORD)&ScrollCityListHook) - 0x004C4A42);
  dlogr(" Done", DL_INIT);
 //}

 //if(GetPrivateProfileIntA("Misc", "CitiesLimitFix", 0, ini)) {
  dlog("Applying cities limit patch.", DL_INIT);
  if(*((BYTE*)0x004BF3BB)!=0xeb) {
   SafeWrite8(0x004BF3BB, 0xeb);
  }
  dlogr(" Done", DL_INIT);
 //}

 tmp=GetPrivateProfileIntA("Misc", "WorldMapSlots", 0, ini);
 if(tmp&&tmp<128) {
  dlog("Applying world map slots patch.", DL_INIT);
  if(tmp<7) tmp=7;
  mapSlotsScrollMax = (tmp-7)*27;
  if(tmp<25) SafeWrite32(0x004C21FD, 230 - (tmp-17)*27);
  else {
   SafeWrite8(0x004C21FC, 0xC2);
   SafeWrite32(0x004C21FD, 2 + 27*(tmp-26));
  }
  dlogr(" Done", DL_INIT);
 }

 int limit=GetPrivateProfileIntA("Misc", "TimeLimit", 13, ini);
 if(limit==-2) limit=14;
 if(limit==-3) {
  dlog("Applying time limit patch (-3).", DL_INIT);
  limit=-1;
  AddUnarmedStatToGetYear=1;

  SafeWrite32(0x004392F9, ((DWORD)&GetDateWrapper) - 0x004392Fd);
  SafeWrite32(0x00443809, ((DWORD)&GetDateWrapper) - 0x0044380d);
  SafeWrite32(0x0047E128, ((DWORD)&GetDateWrapper) - 0x0047E12c);
  SafeWrite32(0x004975A3, ((DWORD)&GetDateWrapper) - 0x004975A7);
  SafeWrite32(0x00497713, ((DWORD)&GetDateWrapper) - 0x00497717);
  SafeWrite32(0x004979Ca, ((DWORD)&GetDateWrapper) - 0x004979Ce);
  SafeWrite32(0x004C3CB6, ((DWORD)&GetDateWrapper) - 0x004C3CBa);
  dlogr(" Done", DL_INIT);
 }
 if(limit<=14&&limit>=-1&&limit!=13) {
  dlog("Applying time limit patch.", DL_INIT);
  if(limit==-1) {
   SafeWrite32(0x004A34Fa, ((DWORD)&TimerReset) - 0x004A34Fe);
   SafeWrite32(0x004A3552, ((DWORD)&TimerReset) - 0x004A3556);

   SafeWrite32(0x004A34EF, 0x90909090);
   SafeWrite32(0x004A34f3, 0x90909090);
   SafeWrite16(0x004A34f7, 0x9090);
   SafeWrite32(0x004A34FE, 0x90909090);
   SafeWrite16(0x004A3502, 0x9090);

   SafeWrite32(0x004A3547, 0x90909090);
   SafeWrite32(0x004A354b, 0x90909090);
   SafeWrite16(0x004A354f, 0x9090);
   SafeWrite32(0x004A3556, 0x90909090);
   SafeWrite16(0x004A355a, 0x9090);
  } else {
   SafeWrite8(0x004A34EC, limit);
   SafeWrite8(0x004A3544, limit);
  }
  dlogr(" Done", DL_INIT);
 }

 tmp=GetPrivateProfileIntA("Debugging", "DebugMode", 0, ini);
 if(tmp) {
  dlog("Applying debugmode patch.", DL_INIT);
  //If the player is using an exe with the debug patch already applied, just skip this block without erroring
  if(*((DWORD*)0x00444A64)!=0x082327e8) {
   SafeWrite32(0x00444A64, 0x082327e8);
   SafeWrite32(0x00444A68, 0x0120e900);
   SafeWrite8(0x00444A6D, 0);
   SafeWrite32(0x00444A6E, 0x90909090);
  }
  SafeWrite8(0x004C6D9B, 0xb8);
  if(tmp==1) SafeWrite32(0x004C6D9C, (DWORD)debugGnw);
  else SafeWrite32(0x004C6D9C, (DWORD)debugLog);
  dlogr(" Done", DL_INIT);
 }

 npcautolevel=GetPrivateProfileIntA("Misc", "NPCAutoLevel", 0, ini)!=0;
 if(npcautolevel) {
  dlog("Applying npc autolevel patch.", DL_INIT);
  SafeWrite16(0x00495D22, 0x9090);
  SafeWrite32(0x00495D24, 0x90909090);
  dlogr(" Done", DL_INIT);
 }
 //if(GetPrivateProfileIntA("Misc", "NPCLevelFix", 0, ini)) {
  dlog("Applying NPCLevelFix.", DL_INIT);
  HookCall(0x495BC9,  (void*)0x495E51);
  dlogr(" Done", DL_INIT);
 //}

 if(GetPrivateProfileIntA("Misc", "SingleCore", 1, ini)) {
  dlog("Applying single core patch.", DL_INIT);
  HANDLE process=GetCurrentProcess();
  SetProcessAffinityMask(process, 1);
  CloseHandle(process);
  dlogr(" Done", DL_INIT);
 }

 if(GetPrivateProfileIntA("Misc", "OverrideArtCacheSize", 0, ini)) {
  dlog("Applying override art cache size patch.", DL_INIT);
  SafeWrite32(0x00418867, 0x90909090);
  SafeWrite32(0x00418872, 256);
  dlogr(" Done", DL_INIT);
 }

 char elevPath[MAX_PATH];
 GetPrivateProfileString("Misc", "ElevatorsFile", "", elevPath, MAX_PATH, ini);
 if(strlen(elevPath)>0) {
  dlog("Applying elevator patch.", DL_INIT);
  ElevatorsInit(elevPath);
  dlogr(" Done", DL_INIT);
 }

 if(GetPrivateProfileIntA("Misc", "UseFileSystemOverride", 0, ini)) {
  FileSystemInit();
 }

 /*DWORD horrigan=GetPrivateProfileIntA("Misc", "DisableHorrigan", 0, ini);
 if(horrigan) {
  if(*((BYTE*)0x004C06D8)!=0x75) return false;
  SafeWrite8(0x004C06D8, 0xeb);
 }*/

 //if(GetPrivateProfileIntA("Misc", "PrintToFileFix", 0, ini)) {
  dlog("Applying print to file patch.", DL_INIT);
  SafeWrite32(0x006C0364, (DWORD)&FakeFindFirstFile);
  SafeWrite32(0x006C0368, (DWORD)&FakeFindNextFile);
  dlogr(" Done", DL_INIT);
 //}

 if(GetPrivateProfileIntA("Misc", "AdditionalWeaponAnims", 0, ini)) {
  dlog("Applying additional weapon animations patch.", DL_INIT);
  SafeWrite8(0x419320, 0x12);
  HookCall(0x4194CC, WeaponAnimHook);
  HookCall(0x451648, WeaponAnimHook);
  HookCall(0x451671, WeaponAnimHook);
  dlogr(" Done", DL_INIT);
 }

#ifdef TRACE
 if(GetPrivateProfileIntA("Debugging", "DontDeleteProtos", 0, ini)) {
  dlog("Applying permanent protos patch.", DL_INIT);
  SafeWrite8(0x48007E, 0xeb);
  dlogr(" Done", DL_INIT);
 }
#endif

 CritInit();

 int number_patch_loop=GetPrivateProfileInt("Misc", "NumberPatchLoop", -1, ini);
 if(number_patch_loop>-1) {
  dlog("Applying load multiple patches patch. ", DL_INIT);
  // Disable check
  SafeWrite8(0x0444363, 0xE9);
  SafeWrite32(0x0444364, 0xFFFFFFB9);
  // New loop count
  SafeWrite32(0x0444357, number_patch_loop);
  // Change step from 2 to 1
  SafeWrite8(0x0444354, 0x90);
  dlogr(" Done", DL_INIT);
 }

 if(GetPrivateProfileInt("Misc", "DisplayKarmaChanges", 0, ini)) {
  dlog("Applying display karma changes patch. ", DL_INIT);
  GetPrivateProfileString("sfall", "KarmaGain", "You gained %d karma.", KarmaGainMsg, 128, translationIni);
  GetPrivateProfileString("sfall", "KarmaLoss", "You lost %d karma.", KarmaLossMsg, 128, translationIni);
  HookCall(0x455A6D, SetGlobalVarWrapper);
  dlogr(" Done", DL_INIT);
 }

 //if(GetPrivateProfileInt("Misc", "ImportedProcedureFix", 0, ini)) {
  dlog("Applying imported procedure patch. ", DL_INIT);
  SafeWrite16(0x46B35B, 0x1c60);
  SafeWrite32(0x46B35D, 0x90909090);
  SafeWrite8(0x46DBF1, 0xeb);
  SafeWrite8(0x46DDC4, 0xeb);
  SafeWrite8(0x4415CC, 0x00);
  dlogr(" Done", DL_INIT);
 //}

 if(GetPrivateProfileInt("Misc", "AlwaysReloadMsgs", 0, ini)) {
  dlog("Applying always reload messages patch. ", DL_INIT);
  SafeWrite8(0x4A6B8A, 0xff);
  SafeWrite32(0x4A6B8B, 0x02eb0074);
  dlogr(" Done", DL_INIT);
 }

 if(GetPrivateProfileInt("Misc", "PlayIdleAnimOnReload", 0, ini)) {
  dlog("Applying idle anim on reload patch. ", DL_INIT);
  HookCall(0x460B8C, ReloadHook);
  dlogr(" Done", DL_INIT);
 }

 if (GetPrivateProfileInt("Misc", "CorpseLineOfFireFix", 0, ini)) {
  dlog("Applying corpse line of fire patch. ", DL_INIT);
  MakeCall(0x48B98D, obj_shoot_blocking_at_hook, false);
  MakeCall(0x48B9FD, obj_shoot_blocking_at_hook1, false);
  dlogr(" Done", DL_INIT);
 }

 if(GetPrivateProfileIntA("Misc", "EnableHeroAppearanceMod", 0, ini)) {
  dlog("Setting up Appearance Char Screen buttons. ", DL_INIT);
  EnableHeroAppearanceMod();
  dlogr(" Done", DL_INIT);
 }

 if (GetPrivateProfileIntA("Misc", "SkipOpeningMovies", 0, ini)) {
  dlog("Blocking opening movies. ", DL_INIT);
  SafeWrite16(0x4809C7, 0x1CEB);            // jmps 0x4809E5
  dlogr(" Done", DL_INIT);
 }

 RetryCombatMinAP = GetPrivateProfileIntA("Misc", "NPCsTryToSpendExtraAP", 0, ini);
 if (RetryCombatMinAP) {
  dlog("Applying retry combat patch. ", DL_INIT);
  HookCall(0x422B94, &combat_turn_hook);
  dlogr(" Done", DL_INIT);
 }

 dlog("Checking for changed skilldex images. ", DL_INIT);
 tmp=GetPrivateProfileIntA("Misc", "Lockpick", 293, ini);
 if(tmp!=293) SafeWrite32(0x00518D54, tmp);
 tmp=GetPrivateProfileIntA("Misc", "Steal", 293, ini);
 if(tmp!=293) SafeWrite32(0x00518D58, tmp);
 tmp=GetPrivateProfileIntA("Misc", "Traps", 293, ini);
 if(tmp!=293) SafeWrite32(0x00518D5C, tmp);
 tmp=GetPrivateProfileIntA("Misc", "FirstAid", 293, ini);
 if(tmp!=293) SafeWrite32(0x00518D4C, tmp);
 tmp=GetPrivateProfileIntA("Misc", "Doctor", 293, ini);
 if(tmp!=293) SafeWrite32(0x00518D50, tmp);
 tmp=GetPrivateProfileIntA("Misc", "Science", 293, ini);
 if(tmp!=293) SafeWrite32(0x00518D60, tmp);
 tmp=GetPrivateProfileIntA("Misc", "Repair", 293, ini);
 if(tmp!=293) SafeWrite32(0x00518D64, tmp);
 dlogr(" Done", DL_INIT);

 if(GetPrivateProfileIntA("Misc", "RemoveWindowRounding", 0, ini)) {
  SafeWrite32(0x4B8090, 0x90909090);
  SafeWrite16(0x4B8094, 0x9090);
 }

 dlogr("Running TilesInit().", DL_INIT);
 TilesInit();

 CreditsInit();

 char xltcodes[512];
 if (GetPrivateProfileStringA("Misc", "XltTable", "", xltcodes, 512, ini) > 0) {
  char *xltcode;
  int count = 0;
  xltcode = strtok(xltcodes, ",");
  while (xltcode) {
   int _xltcode = atoi(xltcode);
   if (_xltcode<32 || _xltcode>255) break;
   XltTable[count] = byte(_xltcode);
   if (count == 93) break;
   count++;
   xltcode = strtok(0, ",");
  }
  if (count == 93) {
   XltKey = GetPrivateProfileIntA("Misc", "XltKey", 4, ini);
   if (XltKey!=4 && XltKey!=2 && XltKey!=1) XltKey=4;
   MakeCall(0x433F3E, &get_input_str_hook, true);
   SafeWrite8(0x433ED6, 0x7D);
   MakeCall(0x47F364, &get_input_str2_hook, true);
   SafeWrite8(0x47F2F7, 0x7D);
   MakeCall(0x4CC358, &kb_next_ascii_English_US_hook, true);
  }
 }

 HookCall(0x497075, &pipboy_hook);
 if(GetPrivateProfileIntA("Misc", "UseScrollingQuestsList", 0, ini)) {
  dlog("Applying quests list patch ", DL_INIT);
  QuestListInit();
  dlogr(" Done", DL_INIT);
 }

 dlog("Applying premade characters patch", DL_INIT);
 PremadeInit();

 dlogr("Running SoundInit().", DL_INIT);
 SoundInit();

 dlogr("Running ReputationsInit().", DL_INIT);
 ReputationsInit();

 dlogr("Running ConsoleInit().", DL_INIT);
 ConsoleInit();

 if(GetPrivateProfileIntA("Misc", "ExtraSaveSlots", 0, ini)) {
  dlog("Running EnableSuperSaving()", DL_INIT);
  EnableSuperSaving();
  dlogr(" Done", DL_INIT);
 }

 if(GetPrivateProfileIntA("Misc", "SpeedInterfaceCounterAnims", 0, ini)) {
  dlog("Applying SpeedInterfaceCounterAnims patch.", DL_INIT);
  MakeCall(0x460BA1, &intface_rotate_numbers_hook, true);
  dlogr(" Done", DL_INIT);
 }

 KarmaFrmCount=GetPrivateProfileIntA("Misc", "KarmaFRMsCount", 0, ini);
 if(KarmaFrmCount) {
  KarmaFrms=new DWORD[KarmaFrmCount];
  KarmaPoints=new int[KarmaFrmCount-1];
  dlog("Applying karma frm patch.", DL_INIT);
  char buf[512];
  GetPrivateProfileStringA("Misc", "KarmaFRMs", "", buf, 512, ini);
  char *ptr=buf, *ptr2;
  for(DWORD i=0;i<KarmaFrmCount-1;i++) {
   ptr2=strchr(ptr, ',');
   *ptr2='\0';
   KarmaFrms[i]=atoi(ptr);
   ptr=ptr2+1;
  }
  KarmaFrms[KarmaFrmCount-1]=atoi(ptr);
  GetPrivateProfileStringA("Misc", "KarmaPoints", "", buf, 512, ini);
  ptr=buf;
  for(DWORD i=0;i<KarmaFrmCount-2;i++) {
   ptr2=strchr(ptr, ',');
   *ptr2='\0';
   KarmaPoints[i]=atoi(ptr);
   ptr=ptr2+1;
  }
  KarmaPoints[KarmaFrmCount-2]=atoi(ptr);
  HookCall(0x4367A9, DrawCardHook);
  dlogr(" Done", DL_INIT);
 }

 switch(GetPrivateProfileIntA("Misc", "ScienceOnCritters", 0, ini)) {
 case 1:
  HookCall(0x41276E, ScienceCritterCheckHook);
  break;
 case 2:
  SafeWrite8(0x41276A, 0xeb);
  break;
 }

 tmp=GetPrivateProfileIntA("Misc", "SpeedInventoryPCRotation", 166, ini);
 if(tmp!=166 && tmp<=1000) {
  dlog("Applying SpeedInventoryPCRotation patch.", DL_INIT);
  SafeWrite32(0x47066B, tmp);
  dlogr(" Done", DL_INIT);
 }

 dlogr("Running BarBoxesInit().", DL_INIT);
 BarBoxesInit();

 dlogr("Patching out ereg call.", DL_INIT);
 BlockCall(0x4425E6);

 tmp=GetPrivateProfileIntA("Misc", "AnimationsAtOnceLimit", 32, ini);
 if((signed char)tmp>32) {
  dlog("Applying AnimationsAtOnceLimit patch.", DL_INIT);
  AnimationsAtOnceInit((signed char)tmp);
  dlogr(" Done", DL_INIT);
 }

 if (GetPrivateProfileIntA("Misc", "RemoveCriticalTimelimits", 0, ini)) {
  dlog("Removing critical time limits.", DL_INIT);
  SafeWrite8(0x424118, 0xEB);
  SafeWrite8(0x4A3053, 0x0);
  SafeWrite8(0x4A3094, 0x0);
  dlogr(" Done", DL_INIT);
 }

 if(tmp=GetPrivateProfileIntA("Sound", "OverrideMusicDir", 0, ini)) {
  SafeWrite32(0x4449C2, (DWORD)&musicOverridePath);
  SafeWrite32(0x4449DB, (DWORD)&musicOverridePath);
  if(tmp==2) {
   SafeWrite32(0x518E78, (DWORD)&musicOverridePath);
   SafeWrite32(0x518E7C, (DWORD)&musicOverridePath);
  }
 }

 if(GetPrivateProfileIntA("Misc", "NPCStage6Fix", 0, ini)) {
  dlog("Applying NPC Stage 6 Fix.", DL_INIT);
  MakeCall(0x493CE9, &NPCStage6Fix1, true);
  SafeWrite8(0x494063, 0x06);  // loop should look for a potential 6th stage
  SafeWrite8(0x4940BB, 0xCC);  // move pointer by 204 bytes instead of 200
  MakeCall(0x494224, &NPCStage6Fix2, true);
  dlogr(" Done", DL_INIT);
 }

 if (GetPrivateProfileIntA("Misc", "MultiHexPathingFix", 1, ini)) {
  dlog("Applying MultiHex Pathing Fix.", DL_INIT);
  MakeCall(0x42901F, &MultiHexFix, false);
  MakeCall(0x429170, &MultiHexFix, false);
  dlogr(" Done", DL_INIT);
 }

 switch (GetPrivateProfileIntA("Misc", "FastShotFix", 1, ini)) {
 case 1:
  dlog("Applying Fast Shot Trait Fix.", DL_INIT);
  MakeCall(0x478E75, &item_w_called_shot_hook, true);
  dlogr(" Done", DL_INIT);
  break;
 case 2:
  dlog("Applying Fast Shot Trait Fix. (Fallout 1 version)", DL_INIT);
  SafeWrite8(0x478CA0, 0x0);
#define hook(a) SafeWrite32(a+1, 0x478C7D - (a+5))
  hook(0x478C2F);
  hook(0x478C08);
  hook(0x478BF9);
  hook(0x478BEA);
  hook(0x478BD6);
  hook(0x478BC7);
  hook(0x478BB8);
#undef hook
  dlogr(" Done", DL_INIT);
  break;
 }

 if(GetPrivateProfileIntA("Misc", "BoostScriptDialogLimit", 0, ini)) {
  const int scriptDialogCount=10000;
  dlog("Applying script dialog limit patch.", DL_INIT);
  scriptDialog=new int[scriptDialogCount*2]; //Because the msg structure is 8 bytes, not 4.
  //scr_init
  SafeWrite32(0x4A50C2, (DWORD)scriptDialog);
  SafeWrite32(0x4A50E3, scriptDialogCount);
  //scr_game_init
  SafeWrite32(0x4A5169, (DWORD)scriptDialog);
  SafeWrite32(0x4A519F, scriptDialogCount);
  //scr_message_free
  SafeWrite32(0x4A52FA, (DWORD)scriptDialog);
  SafeWrite32(0x4A5302, (DWORD)scriptDialog);
  SafeWrite32(0x4A534F, scriptDialogCount*8);
  //scr_get_dialog_msg_file
  SafeWrite32(0x4A6B86, (DWORD)scriptDialog);
  SafeWrite32(0x4A6BE0, (DWORD)scriptDialog);
  SafeWrite32(0x4A6C37, (DWORD)scriptDialog);
  dlogr(" Done", DL_INIT);
 }

 dlog("Running InventoryInit.", DL_INIT);
 InventoryInit();
 dlogr(" Done", DL_INIT);

 MotionSensorFlags = GetPrivateProfileIntA("Misc", "MotionScannerFlags", 1, ini);
 if (MotionSensorFlags != 0) {
  dlog("Applying MotionScannerFlags patch.", DL_INIT);
  if (MotionSensorFlags&1) MakeCall(0x41BBEE, &automap_hook, true);
  if (MotionSensorFlags&2) BlockCall(0x41BC3C);
  dlogr(" Done", DL_INIT);
 }

 if(tmp=GetPrivateProfileIntA("Misc", "EncounterTableSize", 0, ini) && tmp<=127) {
  dlog("Applying EncounterTableSize patch.", DL_INIT);
  DWORD nsize=(tmp+1)*180+0x50;
  SafeWrite32(0x4BD1A3, nsize);
  SafeWrite32(0x4BD1D9, nsize);
  SafeWrite32(0x4BD270, nsize);
  SafeWrite32(0x4BD604, nsize);
  SafeWrite32(0x4BDA14, nsize);
  SafeWrite32(0x4BDA44, nsize);
  SafeWrite32(0x4BE707, nsize);
  SafeWrite32(0x4C0815, nsize);
  SafeWrite32(0x4C0D4a, nsize);
  SafeWrite32(0x4C0FD4, nsize);
  SafeWrite8(0x4BDB17, (BYTE)tmp);
  dlogr(" Done", DL_INIT);
 }

 dlog("Initing main menu patches.", DL_INIT);
 MainMenuInit();
 dlogr(" Done", DL_INIT);

 if(GetPrivateProfileIntA("Misc", "DisablePipboyAlarm", 0, ini)) {
  SafeWrite8(0x499518, 0xc3);
 }

 dlog("Initing AI patches.", DL_INIT);
 AIInit();
 dlogr(" Done", DL_INIT);

 dlog("Initing AI control.", DL_INIT);
 PartyControlInit();
 dlogr(" Done", DL_INIT);

 if (GetPrivateProfileIntA("Misc", "ObjCanSeeObj_ShootThru_Fix", 0, ini)) {
  dlog("Applying ObjCanSeeObj ShootThru Fix.", DL_INIT);
  SafeWrite32(0x456BC7, (DWORD)&objCanSeeObj_ShootThru_Fix - 0x456BCB);
  dlogr(" Done", DL_INIT);
 }

 // phobos2077:
 ComputeSprayModInit();
 ExplosionLightingInit();
 tmp = SimplePatch<DWORD>(0x4A2873, "Misc", "Dynamite_DmgMax", 50, 0, 9999);
 SimplePatch<DWORD>(0x4A2878, "Misc", "Dynamite_DmgMin", 30, 0, tmp);
 tmp = SimplePatch<DWORD>(0x4A287F, "Misc", "PlasticExplosive_DmgMax", 80, 0, 9999);
 SimplePatch<DWORD>(0x4A2884, "Misc", "PlasticExplosive_DmgMin", 40, 0, tmp);
 BooksInit();
 DWORD addrs[2] = {0x45F9DE, 0x45FB33};
 SimplePatch<WORD>(addrs, 2, "Misc", "CombatPanelAnimDelay", 1000, 0, 65535);
 addrs[0] = 0x447DF4; addrs[1] = 0x447EB6;
 SimplePatch<BYTE>(addrs, 2, "Misc", "DialogPanelAnimDelay", 33, 0, 255);

// ����������� ������� ������
 for (int i = 0; i < sizeof(AgeMin)/4; i++) {
  SafeWrite8(AgeMin[i], 8);
 }

// ������������ ������� ������
 for (int i = 0; i < sizeof(AgeMax)/4; i++) {
  SafeWrite8(AgeMax[i], 60);
 }

 windowName[64] = 0;
 if (GetPrivateProfileString("Misc", "WindowName", "", windowName, 64, ini)) {
  dlog("Applying window name patch.", DL_INIT);
  SafeWrite32(0x480CCB, (DWORD)&windowName);
  dlogr(" Done", DL_INIT);
 }

// ���������� ��������� ������������ �� ������ ��� ����� �� �������
 for (int i = 0; i < sizeof(WalkDistance)/4; i++) {
  SafeWrite8(WalkDistance[i], 1);
 }

// fix "Pressing A to enter combat before anything else happens, thus getting infinite free running"
 if (GetPrivateProfileIntA("Misc", "FakeCombatFix", 0, ini)) {
  MakeCall(0x41803A, &check_move_hook, false);
  MakeCall(0x44B8A9, &gmouse_bk_process_hook, false);
  HookCall(0x44C130, &FakeCombatFix1);      // action_get_an_object_
  HookCall(0x44C7B0, &FakeCombatFix1);      // action_get_an_object_
  HookCall(0x44C1D9, &FakeCombatFix2);      // action_loot_container_
  HookCall(0x44C79C, &FakeCombatFix2);      // action_loot_container_
  HookCall(0x412117, &FakeCombatFix3);      // action_use_an_object_
  HookCall(0x44C33B, &FakeCombatFix3);      // gmouse_handle_event_
  HookCall(0x471AC8, &FakeCombatFix3);      // use_inventory_on_
 }

 if (GetPrivateProfileIntA("Misc", "DisableHotkeysForUnknownAreasInCity", 0, ini)) {
  MakeCall(0x4C4945, &wmTownMapFunc_hook, false);
 }

 if (GetPrivateProfileIntA("Misc", "EnableMusicInDialogue", 0, ini)) {
  SafeWrite8(0x44525B, 0x00);
//  BlockCall(0x450627);
 }

 AutoQuickSave = GetPrivateProfileIntA("Misc", "AutoQuickSave", 0, ini);
 if (AutoQuickSave >= 1 && AutoQuickSave <= 10) {
  AutoQuickSave--;
  SafeWrite16(0x47B91C, 0xC031);            // xor  eax, eax
  SafeWrite32(0x47B91E, 0x5193B8A3);        // mov  ds:_slot_cursor, eax
  SafeWrite16(0x47B923, 0x04EB);            // jmp  0x47B929
  MakeCall(0x47B984, &SaveGame_hook, true);
 }

 if (GetPrivateProfileIntA("Misc", "DontTurnOffSneakIfYouRun", 0, ini)) {
  SafeWrite8(0x418135, 0xEB);
 }

// ���������� � ���� �������� ���������� � ���� "��������" � ������� �������
 GetPrivateProfileString("sfall", "LvlMsg", "Lvl", LvlMsg, 6, translationIni);
 MakeCall(0x44906E, &partyMemberCurLevel, true);

// ���������� � ���� �������� ���������� � ���� "��" � ������� ����� �����
 GetPrivateProfileString("sfall", "ACMsg", "AC", AcMsg, 6, translationIni);
 MakeCall(0x449222, &partyMemberAC, true);

// ����� ������������ ����������� ������ ����� ������ (\n) � �������� �������� �� pro_*.msg
 SafeWrite32(0x46ED87, (DWORD)&display_print_with_linebreak);
 SafeWrite32(0x49AD7A, (DWORD)&display_print_with_linebreak);
 SafeWrite32(0x472F9A, (DWORD)&inven_display_msg_with_linebreak);

// ��� �������� ���������� �������� ���� ������ ������ ���� � �������������� ����
 SafeWrite8(0x4C1011, 0x97);                // xchg edi, eax
 SafeWrite32(0x4C1012, 0x90909090);         // nop
 HookCall(0x4C1042, &wmSetupRandomEncounter_hook);

// Force the party members to play the idle animation when reloading their weapon
// SafeWrite16(0x421F2E, 0xEA89);             // mov  edx, ebp

 if (GetPrivateProfileIntA("Misc", "DrugExploitFix", 0, ini)) {
  HookCall(0x49BF5B, &protinst_use_item_hook);
  HookCall(0x43C342, &UpdateLevel_hook);
  HookCall(0x43A8A1, &skill_level_hook);    // SavePlayer_
  HookCall(0x43B3FC, &SliderBtn_hook);
  HookCall(0x43B463, &skill_level_hook);    // SliderBtn_
  HookCall(0x43B47C, &SliderBtn_hook1);
  HookCall(0x4AEFC3, &stat_level_hook);
 }

 dlog("Running BugsInit.", DL_INIT);
 BugsInit();
 dlogr(" Done", DL_INIT);

// ����������� ������������� ������� ����� �������������� "������� �������"/"���������"
 if (GetPrivateProfileIntA("Misc", "CanSellUsedGeiger", 0, ini)) {
  SafeWrite8(0x478115, 0xBA);
  SafeWrite8(0x478138, 0xBA);
  MakeCall(0x474D22, &barter_attempt_transaction_hook, true);
  HookCall(0x4798B1, &item_m_turn_off_hook);
 }

 dlogr("Leave DllMain2", DL_MAIN);
}

static void _stdcall OnExit() {
 ConsoleExit();
 AnimationsAtOnceExit();
 HeroAppearanceModExit();
 //SoundExit();
}

static void __declspec(naked) OnExitFunc() {
 __asm {
  pushad
  call OnExit
  popad
  jmp  DOSCmdLineDestroy_
 }
}

static void CompatModeCheck(HKEY root, const char* filepath, int extra) {
 HKEY key;
 char buf[MAX_PATH];
 DWORD size=MAX_PATH;
 DWORD type;
 if(!(type=RegOpenKeyEx(root, "Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers", 0, extra|STANDARD_RIGHTS_READ|KEY_QUERY_VALUE, &key))) {
  if(!RegQueryValueEx(key, filepath, 0, &type, (BYTE*)buf, &size)) {
   if(size&&(type==REG_EXPAND_SZ||type==REG_MULTI_SZ||type==REG_SZ)) {
    if(strstr(buf, "256COLOR")||strstr(buf, "640X480")||strstr(buf, "WIN")) {
     RegCloseKey(key);
     /*if(!RegOpenKeyEx(root, "Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers", 0, extra|KEY_READ|KEY_WRITE, &key)) {
      if((type=RegDeleteValueA(key, filepath))==ERROR_SUCCESS) {
       MessageBoxA(0, "Fallout was set to run in compatibility mode.\n"
        "Please restart fallout to ensure it runs correctly.", "Error", 0);
       RegCloseKey(key);
       ExitProcess(-1);
      } else {
       //FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, type, 0, buf, 260, 0);
       //MessageBoxA(0, buf, "", 0);
      }
     }*/

     MessageBoxA(0, "Fallout appears to be running in compatibility mode.\n" //, and sfall was not able to disable it.\n"
      "Please check the compatibility tab of fallout2.exe, and ensure that the following settings are unchecked.\n"
      "Run this program in compatibility mode for..., run in 256 colours, and run in 640x480 resolution.\n"
      "If these options are disabled, click the 'change settings for all users' button and see if that enables them.", "Error", 0);
     //RegCloseKey(key);
     ExitProcess(-1);
    }
   }
  }
  RegCloseKey(key);
 } else {
  //FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, type, 0, buf, 260, 0);
  //MessageBoxA(0, buf, "", 0);
 }
}

bool _stdcall DllMain(HANDLE hDllHandle, DWORD dwReason, LPVOID  lpreserved) {
 if(dwReason==DLL_PROCESS_ATTACH) {
#ifdef TRACE
  LoggingInit();
#endif

  HookCall(0x4DE7D2, &OnExitFunc);

  char filepath[MAX_PATH];
  GetModuleFileName(0, filepath, MAX_PATH);

  CRC(filepath);

#ifdef TRACE
  if(!GetPrivateProfileIntA("Debugging", "SkipCompatModeCheck", 0, ".\\ddraw.ini")) {
#else
  if(1) {
#endif
   int is64bit;
   typedef int (_stdcall *chk64bitproc)(HANDLE, int*);
   HMODULE h=LoadLibrary("Kernel32.dll");
   chk64bitproc proc = (chk64bitproc)GetProcAddress(h, "IsWow64Process");
   if(proc) proc(GetCurrentProcess(), &is64bit);
   else is64bit=0;
   FreeLibrary(h);

   CompatModeCheck(HKEY_CURRENT_USER, filepath, is64bit?KEY_WOW64_64KEY:0);
   CompatModeCheck(HKEY_LOCAL_MACHINE, filepath, is64bit?KEY_WOW64_64KEY:0);
  }


  bool cmdlineexists=false;
  char* cmdline=GetCommandLineA();
  if(GetPrivateProfileIntA("Main", "UseCommandLine", 0, ".\\ddraw.ini")) {
   while(cmdline[0]==' ') cmdline++;
   bool InQuote=false;
   int count=-1;

   while(true) {
    count++;
    if(cmdline[count]==0) break;;
    if(cmdline[count]==' '&&!InQuote) break;
    if(cmdline[count]=='"') {
     InQuote=!InQuote;
     if(!InQuote) break;
    }
   }
   if(cmdline[count]!=0) {
    count++;
    while(cmdline[count]==' ') count++;
    cmdline=&cmdline[count];
    cmdlineexists=true;
   }
  }

  if(cmdlineexists&&strlen(cmdline)) {
   strcpy_s(ini, ".\\");
   strcat_s(ini, cmdline);
   HANDLE h = CreateFileA(cmdline, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
   if(h!=INVALID_HANDLE_VALUE) CloseHandle(h);
   else {
    MessageBox(0, "You gave a command line argument to fallout, but it couldn't be matched to a file\n" \
     "Using default ddraw.ini instead", "Warning", MB_TASKMODAL);
    strcpy_s(ini, ".\\ddraw.ini");
   }
  } else strcpy_s(ini, ".\\ddraw.ini");

  GetPrivateProfileStringA("Main", "TranslationsINI", "./Translations.ini", translationIni, 65, ini);

  DllMain2();
 }
 return true;
}
