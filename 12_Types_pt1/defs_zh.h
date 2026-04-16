#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

// 结构体和枚举定义
// Copyright (c) 2019 Warren Toomey, GPL3

#define TEXTLEN		512	// 输入中符号的长度
#define NSYMBOLS        1024	// 符号表条目数量

// Token 类型
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
  T_FOR, T_VOID, T_CHAR
};

// Token 结构体
struct token {
  int token;			// Token 类型，来自上面的枚举列表
  int intvalue;			// 对于 T_INTLIT，整数值
};

// AST 节点类型。前几个与相关的 token
// 相对应
enum {
  A_ADD = 1, A_SUBTRACT, A_MULTIPLY, A_DIVIDE,
  A_EQ, A_NE, A_LT, A_GT, A_LE, A_GE,
  A_INTLIT,
  A_IDENT, A_LVIDENT, A_ASSIGN, A_PRINT, A_GLUE,
  A_IF, A_WHILE, A_FUNCTION, A_WIDEN
};

// 原始类型
enum {
  P_NONE, P_VOID, P_CHAR, P_INT
};

// 抽象语法树结构
struct ASTnode {
  int op;			// 在此树上执行的"操作"
  int type;			// 此树生成的任何表达式的类型
  struct ASTnode *left;		// 左、中、右子树
  struct ASTnode *mid;
  struct ASTnode *right;
  union {
    int intvalue;		// 对于 A_INTLIT，整数值
    int id;			// 对于 A_IDENT，符号槽号
  } v;				// 对于 A_FUNCTION，符号槽号
};

#define NOREG	-1		// 当 AST 生成函数没有寄存器
				// 可以返回时使用 NOREG

// 结构类型
enum {
  S_VARIABLE, S_FUNCTION
};

// 符号表结构
struct symtable {
  char *name;			// 符号的名称
  int type;			// 符号的原始类型
  int stype;			// 符号的结构类型
};