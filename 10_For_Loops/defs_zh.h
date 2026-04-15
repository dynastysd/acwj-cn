#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

// 结构体和枚举定义
// Copyright (c) 2019 Warren Toomey, GPL3

#define TEXTLEN		512	// 输入中符号的长度
#define NSYMBOLS        1024	// 符号表条目数量

// 词法单元类型
enum {
  T_EOF,
  T_PLUS, T_MINUS,
  T_STAR, T_SLASH,
  T_EQ, T_NE,
  T_LT, T_GT, T_LE, T_GE,
  T_INTLIT, T_SEMI, T_ASSIGN, T_IDENT,
  T_LBRACE, T_RBRACE, T_LPAREN, T_RPAREN,
  // 关键字
  T_PRINT, T_INT, T_IF, T_ELSE, T_WHILE,
  T_FOR
};

// 词法单元结构体
struct token {
  int token;			// 词法单元类型，来自上面的枚举列表
  int intvalue;			// 对于 T_INTLIT，整数值
};

// AST 节点类型。前几个与
// 相关词法单元对应
enum {
  A_ADD = 1, A_SUBTRACT, A_MULTIPLY, A_DIVIDE,
  A_EQ, A_NE, A_LT, A_GT, A_LE, A_GE,
  A_INTLIT,
  A_IDENT, A_LVIDENT, A_ASSIGN, A_PRINT, A_GLUE,
  A_IF, A_WHILE
};

// 抽象语法树结构
struct ASTnode {
  int op;			// 在此树上执行的"操作"
  struct ASTnode *left;		// 左、中、右子树
  struct ASTnode *mid;
  struct ASTnode *right;
  union {
    int intvalue;		// 对于 A_INTLIT，整数值
    int id;			// 对于 A_IDENT，符号槽号
  } v;
};

#define NOREG	-1		// 当 AST 生成函数没有寄存器返回时使用 NOREG

// 符号表结构
struct symtable {
  char *name;			// 符号的名称
};