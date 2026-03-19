#include "defs.h"
#include "data.h"
#include "decl.h"

// 使用完全递归下降的表达式解析
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
static int arithop(int tok) {
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
    fprintf(stderr, "syntax error on line %d, token %d\n", Line, tok);
    exit(1);
  }
}

struct ASTnode *additive_expr(void);

// 返回一个以 '*' 或 '/' 二元运算符为根节点的抽象语法树
struct ASTnode *multiplicative_expr(void) {
  struct ASTnode *left, *right;
  int tokentype;

  // 获取左边的整数字面量。
  // 同时获取下一个词法单元。
  left = primary();

  // 如果没有剩余词法单元，只返回左节点
  tokentype = Token.token;
  if (tokentype == T_EOF)
    return (left);

  // 当词法单元是 '*' 或 '/' 时循环
  while ((tokentype == T_STAR) || (tokentype == T_SLASH)) {
    // 获取下一个整数字面量
    scan(&Token);
    right = primary();

    // 将其与左边的整数字面量连接起来
    left = mkastnode(arithop(tokentype), left, right, 0);

    // 更新当前词法单元的详情。
    // 如果没有剩余词法单元，只返回左节点
    tokentype = Token.token;
    if (tokentype == T_EOF)
      break;
  }

  // 返回我们已构建的树
  return (left);
}

// 返回一个以 '+' 或 '-' 二元运算符为根节点的抽象语法树
struct ASTnode *additive_expr(void) {
  struct ASTnode *left, *right;
  int tokentype;

  // 获取比我们优先级高的左子树
  left = multiplicative_expr();

  // 如果没有剩余词法单元，只返回左节点
  tokentype = Token.token;
  if (tokentype == T_EOF)
    return (left);

  // 缓存 '+' 或 '-' 词法单元类型

  // 在我们的优先级层面上循环处理词法单元
  while (1) {
    // 获取下一个整数字面量
    scan(&Token);

    // 获取比我们优先级高的右子树
    right = multiplicative_expr();

    // 用我们低优先级的运算符连接两个子树
    left = mkastnode(arithop(tokentype), left, right, 0);

    // 获取我们优先级层面的下一个词法单元
    tokentype = Token.token;
    if (tokentype == T_EOF)
      break;
  }

  // 返回我们已构建的树
  return (left);
}

struct ASTnode *binexpr(int n) {
  return (additive_expr());
}
