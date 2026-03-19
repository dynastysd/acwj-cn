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

// 从输入文件扫描并返回一个整数字面量值。
static int scanint(int c) {
  int k, val = 0;

  // 将每个字符转换为一个整数值
  while ((k = chrpos("0123456789", c)) >= 0) {
    val = val * 10 + k;
    c = next();
  }

  // 遇到一个非整数字符，将其放回。
  putback(c);
  return val;
}

// 扫描并返回在输入中找到的下一个 token。
// 如果找到有效 token 返回 1，如果没有 token 剩余返回 0。
int scan(struct token *t) {
  int c;

  // 跳过空白符
  c = skip();

  // 根据输入字符确定 token 类型
  switch (c) {
  case EOF:
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
    // 字面整数值
    if (isdigit(c)) {
      t->intvalue = scanint(c);
      t->token = T_INTLIT;
      break;
    }

    printf("Unrecognised character %c on line %d\n", c, Line);
    exit(1);
  }

  // 我们找到了一个 token
  return (1);
}
