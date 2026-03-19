#include "defs.h"
#include "data.h"
#include "decl.h"

// 词法扫描
// Copyright (c) 2019 Warren Toomey, GPL3

// 返回字符 c 在字符串 s 中的位置，
// 如果未找到则返回 -1
static int chrpos(char *s, int c) {
  char *p;

  p = strchr(s, c);
  return (p ? p - s : -1);
}

// 从输入文件获取下一个字符。
static int next(void) {
  int c;

  if (Putback) {		// 如果有放回的字符，
    c = Putback;		// 就使用它
    Putback = 0;
    return c;
  }

  c = fgetc(Infile);		// 从输入文件读取
  if ('\n' == c)
    Line++;			// 增加行计数
  return c;
}

// 放回一个不需要的字符
static void putback(int c) {
  Putback = c;
}

// 跳过我们不需要处理的输入，
// 即空白符、换行符。返回我们需要的
// 第一个字符。
static int skip(void) {
  int c;

  c = next();
  while (' ' == c || '\t' == c || '\n' == c || '\r' == c || '\f' == c) {
    c = next();
  }
  return (c);
}

// 从输入文件中扫描并返回
// 一个整数字面量值。
static int scanint(int c) {
  int k, val = 0;

  // 将每个字符转换为一个整数值
  while ((k = chrpos("0123456789", c)) >= 0) {
    val = val * 10 + k;
    c = next();
  }

  // 遇到非数字字符，放回去
  putback(c);
  return val;
}

// 扫描并返回在输入中找到的下一个词法单元。
// 如果词法单元有效返回 1，没有词法单元则返回 0。
int scan(struct token *t) {
  int c;

  // 跳过空白符
  c = skip();

  // 根据输入字符确定词法单元类型
  switch (c) {
  case EOF:
    t->token = T_EOF;
    return (0);
  case '+':
    t->token = T_PLUS;
    break;
  case '-':
    t->token = T_MINUS;
    break;
  case '*':
    t->token = T_STAR;
    break;
  case '/':
    t->token = T_SLASH;
    break;
  default:

    // 如果是数字，扫描
    // 整数字面量值
    if (isdigit(c)) {
      t->intvalue = scanint(c);
      t->token = T_INTLIT;
      break;
    }

    printf("Unrecognised character %c on line %d\n", c, Line);
    exit(1);
  }

  // 我们找到了一个词法单元
  return (1);
}
