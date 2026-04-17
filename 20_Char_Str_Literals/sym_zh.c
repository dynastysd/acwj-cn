#include "defs.h"
#include "data.h"
#include "decl.h"

// 符号表函数
// Copyright (c) 2019 Warren Toomey, GPL3

// 判断符号 s 是否在全局符号表中
// 返回其槽位位置，若未找到则返回 -1
int findglob(char *s) {
  int i;

  for (i = 0; i < Globs; i++) {
    if (*s == *Gsym[i].name && !strcmp(s, Gsym[i].name))
      return (i);
  }
  return (-1);
}

// 获取一个新全局符号的槽位位置
// 如果已经用尽所有槽位则报错退出
static int newglob(void) {
  int p;

  if ((p = Globs++) >= NSYMBOLS)
    fatal("Too many global symbols");
  return (p);
}

// 将全局符号添加到符号表中。设置其：
// + 类型：char、int 等
// + 结构类型：变量、函数、数组等
// + 大小：元素数量
// + 结束标签：如果是函数
// 返回符号表中的槽位号
int addglob(char *name, int type, int stype, int endlabel, int size) {
  int y;

  // 如果该符号已在符号表中，返回现有槽位
  if ((y = findglob(name)) != -1)
    return (y);

  // 否则获取新槽位，填充数据后
  // 返回槽位号
  y = newglob();
  Gsym[y].name = strdup(name);
  Gsym[y].type = type;
  Gsym[y].stype = stype;
  Gsym[y].endlabel = endlabel;
  Gsym[y].size = size;
  return (y);
}