#include "defs.h"
#include "data.h"
#include "decl.h"

// 符号表函数
// Copyright (c) 2019 Warren Toomey, GPL3

// 确定符号s是否在全局符号表中。
// 返回其槽位置，如果未找到则返回-1。
int findglob(char *s) {
  int i;

  for (i = 0; i < Globs; i++) {
    if (*s == *Symtable[i].name && !strcmp(s, Symtable[i].name))
      return (i);
  }
  return (-1);
}

// 获取新的全局符号槽的位置，如果用完则报错。
static int newglob(void) {
  int p;

  if ((p = Globs++) >= Locls)
    fatal("Too many global symbols");
  return (p);
}

// 确定符号s是否在局部符号表中。
// 返回其槽位置，如果未找到则返回-1。
int findlocl(char *s) {
  int i;

  for (i = Locls + 1; i < NSYMBOLS; i++) {
    if (*s == *Symtable[i].name && !strcmp(s, Symtable[i].name))
      return (i);
  }
  return (-1);
}

// 获取新的局部符号槽的位置，如果用完则报错。
static int newlocl(void) {
  int p;

  if ((p = Locls--) <= Globs)
    fatal("Too many local symbols");
  return (p);
}

// 更新符号表中给定槽号的符号。设置其：
// + type: char, int等
// + structural type: var, function, array等
// + size: 元素数量
// + endlabel: 如果是函数
// + posn: 局部符号的位置信息
static void updatesym(int slot, char *name, int type, int stype,
		      int class, int endlabel, int size, int posn) {
  if (slot < 0 || slot >= NSYMBOLS)
    fatal("Invalid symbol slot number in updatesym()");
  Symtable[slot].name = strdup(name);
  Symtable[slot].type = type;
  Symtable[slot].stype = stype;
  Symtable[slot].class = class;
  Symtable[slot].endlabel = endlabel;
  Symtable[slot].size = size;
  Symtable[slot].posn = posn;
}

// 将全局符号添加到符号表。设置其：
// + type: char, int等
// + structural type: var, function, array等
// + size: 元素数量
// + endlabel: 如果是函数
// 返回符号表中的槽号
int addglob(char *name, int type, int stype, int endlabel, int size) {
  int slot;

  // 如果已在符号表中，返回现有槽
  if ((slot = findglob(name)) != -1)
    return (slot);

  // 否则获取新槽，填充它
  // 并返回槽号
  slot = newglob();
  updatesym(slot, name, type, stype, C_GLOBAL, endlabel, size, 0);
  genglobsym(slot);
  return (slot);
}

// 将局部符号添加到符号表。设置其：
// + type: char, int等
// + structural type: var, function, array等
// + size: 元素数量
// + endlabel: 如果是函数
// 返回符号表中的槽号
int addlocl(char *name, int type, int stype, int endlabel, int size) {
  int slot, posn;

  // 如果已在符号表中，返回现有槽
  if ((slot = findlocl(name)) != -1)
    return (slot);

  // 否则获取新符号槽和此局部的位置。
  // 更新符号表条目并返回槽号
  slot = newlocl();
  posn = gengetlocaloffset(type, 0);	// 目前为0
  updatesym(slot, name, type, stype, C_LOCAL, endlabel, size, posn);
  return (slot);
}

// 确定符号s是否在符号表中。
// 返回其槽位置，如果未找到则返回-1。
int findsymbol(char *s) {
  int slot;

  slot = findlocl(s);
  if (slot == -1)
    slot = findglob(s);
  return (slot);
}