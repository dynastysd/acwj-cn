#include "defs.h"
#include "data.h"
#include "decl.h"

// 表达式解析
// Copyright (c) 2019 Warren Toomey, GPL3

// 解析一个基本因子并返回
// 表示它的抽象语法树节点。
static struct ASTnode *primary(void) {
  struct ASTnode *n;

  // 对于整数字面量词法单元，为它创建一个叶子抽象语法树节点
  // 并扫描下一个词法单元。对于其他词法单元类型则报语法错误。
  switch (Token.token) {
  case T_INTLIT:
    n = mkastleaf(A_INTLIT, Token.intvalue);
    scan(&Token);
    return (n);
  default:
    fprintf(stderr, "syntax error on line %d, token %d\n", Line, Token.token);
    exit(1);
  }
}


// 将二元运算符词法单元转换为抽象语法树操作。
int arithop(int tokentype) {
  switch (tokentype) {
  case T_PLUS:
    return (A_ADD);
  case T_MINUS:
    return (A_SUBTRACT);
  case T_STAR:
    return (A_MULTIPLY);
  case T_SLASH:
    return (A_DIVIDE);
  default:
    fprintf(stderr, "syntax error on line %d, token %d\n", Line, tokentype);
    exit(1);
  }
}

// 每个词法单元的运算符优先级
static int OpPrec[] = { 0, 10, 10, 20, 20, 0 };

// 检查我们是否有二元运算符并
// 返回其优先级。
static int op_precedence(int tokentype) {
  int prec = OpPrec[tokentype];
  if (prec == 0) {
    fprintf(stderr, "syntax error on line %d, token %d\n", Line, tokentype);
    exit(1);
  }
  return (prec);
}

// 返回一个以二元运算符为根节点的抽象语法树。
// 参数 ptp 是前一个词法单元的优先级。
struct ASTnode *binexpr(int ptp) {
  struct ASTnode *left, *right;
  int tokentype;

  // 获取左边的整数字面量。
  // 同时获取下一个词法单元。
  left = primary();

  // 如果没有剩余词法单元，只返回左节点
  tokentype = Token.token;
  if (tokentype == T_EOF)
    return (left);

  // 当这个词法单元的优先级
  // 高于前一个词法单元的优先级时循环
  while (op_precedence(tokentype) > ptp) {
    // 获取下一个整数字面量
    scan(&Token);

    // 递归调用 binexpr()，使用我们词法单元的
    // 优先级来构建一个子树
    right = binexpr(OpPrec[tokentype]);

    // 将那个子树与我们当前的连接起来。同时将词法单元
    // 转换为抽象语法树操作。
    left = mkastnode(arithop(tokentype), left, right, 0);

    // 更新当前词法单元的详情。
    // 如果没有剩余词法单元，只返回左节点
    tokentype = Token.token;
    if (tokentype == T_EOF)
      return (left);
  }

  // 当优先级相同或更低时返回我们已有的树
  return (left);
}
