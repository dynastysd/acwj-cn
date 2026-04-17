#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

// 结构体和枚举定义
// Copyright (c) 2019 Warren Toomey, GPL3

#define TEXTLEN		512	// 输入中符号的长度
#define NSYMBOLS        1024	// 符号表条目数量

// 词法标记类型
enum {
  T_EOF,

  // 二元运算符
  T_ASSIGN, T_LOGOR, T_LOGAND,
  T_OR, T_XOR, T_AMPER,
  T_EQ, T_NE,
  T_LT, T_GT, T_LE, T_GE,
  T_LSHIFT, T_RSHIFT,
  T_PLUS, T_MINUS, T_STAR, T_SLASH,

  // 其他运算符
  T_INC, T_DEC, T_INVERT, T_LOGNOT,

  // 类型关键字
  T_VOID, T_CHAR, T_INT, T_LONG,

  // 其他关键字
  T_IF, T_ELSE, T_WHILE, T_FOR, T_RETURN,

  // 结构标记
  T_INTLIT, T_STRLIT, T_SEMI, T_IDENT,
  T_LBRACE, T_RBRACE, T_LPAREN, T_RPAREN,
  T_LBRACKET, T_RBRACKET
};

// 词法标记结构体
struct token {
  int token;			// 标记类型，取自上面的枚举列表
  int intvalue;			// 对于 T_INTLIT，整数值
};

// AST 节点类型。前几个与相关标记对应
enum {
  A_ASSIGN= 1, A_LOGOR, A_LOGAND, A_OR, A_XOR, A_AND,
  A_EQ, A_NE, A_LT, A_GT, A_LE, A_GE, A_LSHIFT, A_RSHIFT,
  A_ADD, A_SUBTRACT, A_MULTIPLY, A_DIVIDE,
  A_INTLIT, A_STRLIT, A_IDENT, A_GLUE,
  A_IF, A_WHILE, A_FUNCTION, A_WIDEN, A_RETURN,
  A_FUNCCALL, A_DEREF, A_ADDR, A_SCALE,
  A_PREINC, A_PREDEC, A_POSTINC, A_POSTDEC,
  A_NEGATE, A_INVERT, A_LOGNOT, A_TOBOOL
};

// 基础类型
enum {
  P_NONE, P_VOID, P_CHAR, P_INT, P_LONG,
  P_VOIDPTR, P_CHARPTR, P_INTPTR, P_LONGPTR
};

// 抽象语法树结构
struct ASTnode {
  int op;			// 要在该树上执行的"操作"
  int type;			// 此树生成的表达式的类型
  int rvalue;			// 如果节点是 rvalue 则为真
  struct ASTnode *left;		// 左、中、右子树
  struct ASTnode *mid;
  struct ASTnode *right;
  union {			// 对于 A_INTLIT，整数值
    int intvalue;		// 对于 A_IDENT，符号槽号
    int id;			// 对于 A_FUNCTION，符号槽号
    int size;			// 对于 A_SCALE，要缩放的大小
  } v;				// 对于 A_FUNCCALL，符号槽号
};

#define NOREG	-1		// 当 AST 生成函数没有寄存器可返回时使用 NOREG
				//
#define NOLABEL	 0		// 当我们没有标签可传递给 genAST() 时使用 NOLABEL
				//

// 结构类型
enum {
  S_VARIABLE, S_FUNCTION, S_ARRAY
};

// 符号表结构
struct symtable {
  char *name;			// 符号名称
  int type;			// 符号的基础类型
  int stype;			// 符号的结构类型
  int endlabel;			// 对于 S_FUNCTION，结束标签
  int size;			// 符号中元素的数量
};