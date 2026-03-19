#include "defs.h"
#include "data.h"
#include "decl.h"

// 表达式解析
// Copyright (c) 2019 Warren Toomey, GPL3

// 解析一个主因子并返回代表它的 AST 节点。
static struct ASTnode *primary(void) {
  struct ASTnode *n;

  // 对于 INTLIT token，为它创建一个叶子 AST 节点，
  // 并扫描下一个 token。否则，对于任何其他 token 类型，
  // 这是一个语法错误。
  switch (Token.token) {
    case T_INTLIT:
      n = mkastleaf(A_INTLIT, Token.intvalue);
      scan(&Token);
      return (n);
    default:
      fprintf(stderr, "syntax error on line %d\n", Line);
      exit(1);
  }
}


// 将一个 token 转换为一个 AST 操作。
int arithop(int tok) {
  switch (tok) {
    case T_PLUS:
      return (A_ADD);
    case T_MINUS:
      return (A_SUBTRACT);
    case T_STAR:
      return (A_MULTIPLY);
    case T_SLASH:
      return (A_DIVIDE);
    default:
      fprintf(stderr, "unknown token in arithop() on line %d\n", Line);
      exit(1);
  }
}


// 返回一个以二元运算符为根的 AST 树
struct ASTnode *binexpr(void) {
  struct ASTnode *n, *left, *right;
  int nodetype;

  // 获取左边的整数字面量。
  // 同时获取下一个 token。
  left = primary();

  // 如果没有剩余 token，仅返回左节点
  if (Token.token == T_EOF)
    return (left);

  // 将 token 转换为一个节点类型
  nodetype = arithop(Token.token);

  // 获取下一个 token
  scan(&Token);

  // 递归获取右边的树
  right = binexpr();

  // 现在用两个子树构建一棵树
  n = mkastnode(nodetype, left, right, 0);
  return (n);
}
