#include "defs.h"
#include "data.h"
#include "decl.h"

// 通用代码生成器
// Copyright (c) 2019 Warren Toomey, GPL3

// 给定一个抽象语法树，递归地
// 生成汇编代码
static int genAST(struct ASTnode *n) {
  int leftreg, rightreg;

  // 获取左右子树的求值结果
  if (n->left)
    leftreg = genAST(n->left);
  if (n->right)
    rightreg = genAST(n->right);

  switch (n->op) {
    case A_ADD:
      return (cgadd(leftreg,rightreg));
    case A_SUBTRACT:
      return (cgsub(leftreg,rightreg));
    case A_MULTIPLY:
      return (cgmul(leftreg,rightreg));
    case A_DIVIDE:
      return (cgdiv(leftreg,rightreg));
    case A_INTLIT:
      return (cgload(n->intvalue));
    default:
      fprintf(stderr, "Unknown AST operator %d\n", n->op);
      exit(1);
  }
}

void generatecode(struct ASTnode *n) {
  int reg;

  cgpreamble();
  reg= genAST(n);
  cgprintint(reg);
  cgpostamble();
}
