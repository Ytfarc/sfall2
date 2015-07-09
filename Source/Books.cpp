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
#include "Books.h"
#include "FalloutEngine.h"

static int BooksCount=0;
static const int BooksMax=30;
static char iniBooks[MAX_PATH];
static int Patched=0;
static int offsetPID;
static int offsetBook;
static int Reported=0;

struct sBook {
 int bookPid;
 int msgID;
 int count;
 int independent;
 int skill[18];
};

static sBook books[BooksMax];

static sBook* _stdcall FindBook(int pid) {
 for (int i=0; i<BooksCount; i++) {
  if (books[i].bookPid == pid) {
   if (books[i].count == 0) break;
   return &books[i];
  }
 }
 return 0;
}

static const DWORD obj_use_book_hook_back = 0x49BA5A;
static void __declspec(naked) obj_use_book_hook() {
 __asm {
  cmp Patched, 0
  jne patched_
  mov Reported, 0
  mov offsetPID, eax;
patched_:
  mov eax, [eax+0x64]
  push eax
  call FindBook;
  test eax, eax
  jnz found
  mov edi, -1
  jmp end;
found:
  mov offsetBook, eax
  mov edi, [eax+4];                         // msgID
  mov ecx, Patched;
  mov ecx, [eax+16+ecx*4]                   // skill[Patched]
end:
  jmp obj_use_book_hook_back;
 }
}


/*static void _stdcall NeedReport(sBook& book) {
 if (book.count != (Patched+1) && book.independent == 1) {
  Reported = 1;
  SafeWrite16(0x49BB0E, 0x7DEB);            // отключаем сообщения и учёт времени
 }
}*/

static const DWORD obj_use_book_hook2_back = 0x49BB09;
static void __declspec(naked) obj_use_book_hook2() {
 __asm {
  mov eax, offsetBook
  cmp  [eax+8], 1;                          // Количество скиллов для книги <= 1?
  jbe  endrun
/*  push eax;
  call NeedReport;*/

  mov  ebx, Patched                         // Аналог NeedReport
  inc  ebx
  cmp  [eax+8], ebx
  je   endrun
  xor  ebx, ebx
  cmp  [eax+12], ebx
  je   endrun
  inc  ebx
  mov  Reported, ebx
  push 0x7DEB;
  push 0x49BB0E;
  call SafeWrite16;

endrun:
  mov edi, 801                              // Вы не узнаете ничего нового.
  jmp obj_use_book_hook2_back;
 }
}

// book = ссылка на книгу, baseskill = значение первого скилла (нужно если independent == 0)
static void _stdcall NextSkill(sBook& book, int baseskill) {
 int i = 0x7FEB;
 if (Patched == 0) {                        // Не патчили?
  if (book.independent == 1) i = 0x29EB;    // Каждый скилл высчитывается независимо?
  SafeWrite16(0x49BA6A, i);                 // Отключаем проверку боя [и подсчёт скилла]
 }
 Patched++;
 if (book.count == Patched)    {            // Все скиллы обработали?
  SafeWrite16(0x49BA6A, 0x2974);            // Включаем проверку боя и подсчёт скилла
  SafeWrite16(0x49BB0E, 0x04BA);            // Включаем сообщения и учёт времени
  SafeWrite8(0x49BAB9, 0x4A);               // Включаем условный переход если скилл не повышен
  Patched=0;
 } else {
  if (Reported != 2) {
   i = 0x7DEB;                              // Отключаем сообщения и учёт времени
   int k = 0x4F;                            // Отключаем условный переход если скилл не повышен
   if (Reported == 1) {
    Reported = 0;
    i = 0x04BA;                             // Включаем сообщения и учёт времени
    k = 0x4A;                               // Включаем условный переход если скилл не повышен
   } else {
    Reported = 2;
   }
   SafeWrite16(0x49BB0E, i);
   SafeWrite8(0x49BAB9, k);
  }
  __asm {
   mov esi, baseskill;
   mov eax, offsetPID;
   call obj_use_book_                       // Рекурсивный вызов чтения книги для каждого скилла
  }
 }
}

static const DWORD obj_use_book_hook3_back = 0x49BB9D;
static void __declspec(naked) obj_use_book_hook3() {
 __asm {
  mov eax, offsetBook
  cmp  [eax+8], 1;                          // Количество скиллов для книги <= 1?
  jbe  endrun
  push esi;
  push eax;
  call NextSkill;
endrun:
  mov eax, 1;
  jmp obj_use_book_hook3_back;
 }
}

void LoadVanillaBooks() {
 int i = BooksCount;
 // Big Book of Science
 books[i+0].bookPid = 73;
 books[i+0].msgID = 802;
 books[i+0].count = 1;
 books[i+0].independent = 1;
 books[i+0].skill[0] = 12;
 // Deans Electronics
 books[i+1].bookPid = 76;
 books[i+1].msgID = 803;
 books[i+1].count = 1;
 books[i+1].independent = 1;
 books[i+1].skill[0] = 13;
 // First Aid Book
 books[i+2].bookPid = 80;
 books[i+2].msgID = 804;
 books[i+2].count = 1;
 books[i+2].independent = 1;
 books[i+2].skill[0] = 6;
 // Guns and Bullets
 books[i+3].bookPid = 102;
 books[i+3].msgID = 805;
 books[i+3].count = 1;
 books[i+3].independent = 1;
 books[i+3].skill[0] = 0;
 // Scout Handbook
 books[i+4].bookPid = 86;
 books[i+4].msgID = 806;
 books[i+4].count = 1;
 books[i+4].independent = 1;
 books[i+4].skill[0] = 17;
 BooksCount += 5;
}

void BooksInit() {
 char buf[MAX_PATH-3];
 GetPrivateProfileString("Misc", "BooksFile", "", buf, MAX_PATH, ini);
 if (strlen(buf)>0) {
  sprintf(iniBooks, ".\\%s", buf);
  dlog("Applying books patch... ", DL_INIT);
  memset(books,0,sizeof(sBook)*BooksMax);

  int i, ii, n = 0, count;
  if (GetPrivateProfileIntA("main", "overrideVanilla", 0, iniBooks) == 0) {
   LoadVanillaBooks();
  }
  count = GetPrivateProfileIntA("main", "count", 0, iniBooks);

  char section[4];
  for (i=1; i<=count; i++) {
   _itoa_s(i, section, 10);

   if (offsetPID = GetPrivateProfileIntA(section, "PID", 0, iniBooks)) {
    ii = BooksCount;
    for (int k=0; k<BooksCount; k++) {
     if (books[k].bookPid == offsetPID) {   // Такая книга уже есть?
      ii = k;
      break;
     }
    }
    if (ii >= BooksMax) break;
    books[ii].bookPid = offsetPID;
    books[ii].msgID = GetPrivateProfileIntA(section, "TextID", 0, iniBooks);
    books[ii].count = 0;
    books[ii].independent = GetPrivateProfileIntA(section, "Independent", 1, iniBooks);
    char skills[512];
    if (GetPrivateProfileStringA(section, "Skill", "", skills, 512, iniBooks)>0) {
     char *skill;
     skill=strtok(skills, "|");
     while(skill) {
      int _skill=atoi(skill);
      if (_skill>=0&&_skill<18) {
       books[ii].skill[books[ii].count] = _skill;
       books[ii].count++;
      }
      if (books[ii].count >= 18) break;
      skill=strtok(0, "|");
     }
    }
    n++;
    if (ii == BooksCount) BooksCount++;     // Увеличиваем счётчик только если добавили новую книгу
   }
  }
  MakeCall(0x49B9F8, &obj_use_book_hook, true);
  MakeCall(0x49BB04, &obj_use_book_hook2, true);
  MakeCall(0x49BB8D, &obj_use_book_hook3, true);
  dlog_f(" (%d/%d books) Done\r\n", DL_INIT, n, count);
 }
}
