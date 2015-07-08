/*
 *    sfall
 *    Copyright (C) 2008, 2009, 2010  The sfall team
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
#include "Elevators.h"
#include "FalloutEngine.h"

static const int ElevatorCount = 50;
static char File[MAX_PATH];

struct sElevator {
 DWORD ID1;
 DWORD Elevation1;
 DWORD Tile1;
 DWORD ID2;
 DWORD Elevation2;
 DWORD Tile2;
 DWORD ID3;
 DWORD Elevation3;
 DWORD Tile3;
 DWORD ID4;
 DWORD Elevation4;
 DWORD Tile4;
};

struct intotal {
 DWORD Frm1;
 DWORD Frm2;
};

static sElevator Elevators[ElevatorCount];
static DWORD Menus[ElevatorCount];

static intotal intotals[24];

static void __declspec(naked) elevator_select_hook() {
 __asm {
  lea  esi, Menus
  mov  eax, [esi+eax*4]
  jmp  elevator_start_
 }
}

static void __declspec(naked) elevator_select_hook1() {
 __asm {
  lea  ebx, Menus
  mov  eax, [ebx+eax*4]
  jmp  Check4Keys_
 }
}

static void __declspec(naked) elevator_select_hook2() {
 __asm {
  lea  edx, Menus
  mov  eax, [edx+eax*4]
  jmp  elevator_end_
 }
}

static const DWORD elevator_select_hook3_End = 0x43F064;
static void __declspec(naked) elevator_select_hook3() {
 __asm {
  lea  esi, Menus
  mov  eax, [esi+edi*4]
  mov  eax, [0x43EA1C+eax*4]                // _btncnt
  jmp  elevator_select_hook3_End
 }
}

static const DWORD elevator_select_hook4_End = 0x43F18B;
static void __declspec(naked) elevator_select_hook4() {
 __asm {
  lea  edx, Menus
  mov  eax, [edx+edi*4]
  mov  eax, [0x43EA1C+eax*4]                // _btncnt
  jmp  elevator_select_hook4_End
 }
}

static const DWORD elevator_select_hook5_End = 0x43F1EB;
static void __declspec(naked) elevator_select_hook5() {
 __asm {
  lea  eax, Menus
  mov  eax, [eax+edi*4]
  mov  eax, [0x43EA1C+eax*4]                // _btncnt
  jmp  elevator_select_hook5_End
 }
}

void ResetElevators() {
 memcpy(Elevators, (void*)0x43EA7C, sizeof(sElevator)*24);
 memset(&Elevators[24], 0, sizeof(sElevator)*(ElevatorCount-24));
 for (int i = 0; i < 24; i++) Menus[i] = i;
 for (int i = 24; i < ElevatorCount; i++) Menus[i] = 0;
 memcpy(intotals, (void*)0x43E95C, sizeof(intotal)*24);
 char section[4], buf[64], imgnum[8];
 if (File) {
  for (int i = 0; i < 24; i++) {
   sprintf(imgnum, "Image%d", i);
   if (GetPrivateProfileStringA("Frms", imgnum, "", buf, 64, File)) {
    char *Frm;
    Frm = strtok(buf, "|");
    if (Frm) {
     int _frm = atoi(Frm);
     if (_frm != 0) intotals[i].Frm1 = _frm;
     Frm = strtok(0, "|");
     if (Frm) {
      _frm = atoi(Frm);
      if (_frm != 0) intotals[i].Frm2 = _frm;
     }
    }
   }
  }
  for (int i = 0; i < ElevatorCount; i++) {
   _itoa_s(i, section, 10);
   Menus[i] = GetPrivateProfileIntA(section, "Image", Menus[i], File);
   Elevators[i].ID1 = GetPrivateProfileIntA(section, "ID1", Elevators[i].ID1, File);
   Elevators[i].ID2 = GetPrivateProfileIntA(section, "ID2", Elevators[i].ID2, File);
   Elevators[i].ID3 = GetPrivateProfileIntA(section, "ID3", Elevators[i].ID3, File);
   Elevators[i].ID4 = GetPrivateProfileIntA(section, "ID4", Elevators[i].ID4, File);
   Elevators[i].Elevation1 = GetPrivateProfileIntA(section, "Elevation1", Elevators[i].Elevation1, File);
   Elevators[i].Elevation2 = GetPrivateProfileIntA(section, "Elevation2", Elevators[i].Elevation2, File);
   Elevators[i].Elevation3 = GetPrivateProfileIntA(section, "Elevation3", Elevators[i].Elevation3, File);
   Elevators[i].Elevation4 = GetPrivateProfileIntA(section, "Elevation4", Elevators[i].Elevation4, File);
   Elevators[i].Tile1 = GetPrivateProfileIntA(section, "Tile1", Elevators[i].Tile1, File);
   Elevators[i].Tile2 = GetPrivateProfileIntA(section, "Tile2", Elevators[i].Tile2, File);
   Elevators[i].Tile3 = GetPrivateProfileIntA(section, "Tile3", Elevators[i].Tile3, File);
   Elevators[i].Tile4 = GetPrivateProfileIntA(section, "Tile4", Elevators[i].Tile4, File);
  }
 }
}

void ElevatorsInit(char* file) {
 strcpy_s(File, ".\\");
 strcat_s(File, file);

 HookCall(0x43EF83, elevator_select_hook);
 HookCall(0x43F141, elevator_select_hook1);
 HookCall(0x43F2D2, elevator_select_hook2);
 SafeWrite8(0x43EF76, (BYTE)ElevatorCount);

 SafeWrite32(0x43EFA4, (DWORD)&Elevators[0].ID1);
 SafeWrite32(0x43EFB9, (DWORD)&Elevators[0].ID1);
 SafeWrite32(0x43EFEA, (DWORD)&Elevators[0].Tile1);
 SafeWrite32(0x43F2FC, (DWORD)&Elevators[0].ID1);
 SafeWrite32(0x43F309, (DWORD)&Elevators[0].Elevation1);
 SafeWrite32(0x43F315, (DWORD)&Elevators[0].Tile1);

 SafeWrite32(0x43F438, (DWORD)&intotals[0].Frm1);
 SafeWrite32(0x43F475, (DWORD)&intotals[0].Frm2);

 MakeCall(0x43F05D, elevator_select_hook3, true);
 MakeCall(0x43F184, elevator_select_hook4, true);
 MakeCall(0x43F1E4, elevator_select_hook5, true);

 ResetElevators();
}
