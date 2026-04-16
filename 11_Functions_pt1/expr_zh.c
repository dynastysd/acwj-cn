#include "defs.h"
#include "data.h"
#include "decl.h"

// 表达式解析
// Copyright (c) 2019 Warren Toomey, GPL3

// 解析一个基本因子并返回
// 表示它的 AST 节点。
static struct ASTnode *primary(void) {
  struct ASTnode *n;
  int id;

  switch (Token.token) {
    case T_INTLIT:
      // 对于 INTLIT 词法单元，为其创建一个叶子 AST 节点
      n = mkastleaf(A_INTLIT, Token.intvalue);
      break;

    case T_IDENT:
      // 检查这个标识符是否存在
      id = findglob(Text);
      if (id == -1)
	fatals("Unknown variable", Text);

      // 为其创建一个叶子 AST 节点
      n = mkastleaf(A_IDENT, id);
      break;

    default:
      fatald("Syntax error, token", Token.token);
  }

  // 扫描下一个词法单元并返回叶子节点
  scan(&Token);
  return (n);
}


// 将二元运算符词法单元转换为 AST 运算。
// 我们依赖于从词法单元到 AST 运算的 1:1 映射
static int arithop(int tokentype) {
  if (tokentype > T_EOF && tokentype < T_INTLIT)
    return (tokentype);
  fatald("Syntax error, token", tokentype);
}

// 每个词法单元的运算符优先级。
// 必须与 defs.h 中的词法单元顺序一致
static int OpPrec[] = {
  0, 10, 10,			// T_EOF, T_PLUS, T_MINUS
  20, 20,			// T_STAR, T_SLASH
  30, 30,			// T_EQ, T_NE
  40, 40, 40, 40		// T_LT, T_GT, T_LE, T_GE
};

// 检查我们有一个二元运算符
// 并返回其优先级
static int op_precedence(int tokentype) {
  int prec = OpPrec[tokentype];
  if (prec == 0)
    fatald("Syntax error, token", tokentype);
  return (prec);
}

// 返回一个以二元运算符为根的 AST 树。
// 参数 ptp 是前一个词法单元的优先级
struct ASTnode *binexpr(int ptp) {
  struct ASTnode *left, *right;
  int tokentype;

  // 获取左侧的基本树。
  // 同时获取下一个词法单元
  left = primary();

  // 如果遇到分号或 ')'，则只返回左节点
  tokentype = Token.token;
  if (tokentype == T_SEMI || tokentype == T_RPAREN)
    return (left);

  // 当这个词法单元的优先级
  // 大于前一个词法单元的优先级时
  while (op_precedence(tokentype) > ptp) {
    // 扫描下一个整数字面量
    scan(&Token);

    // 递归调用 binexpr() 并使用
    // 我们词法单元的优先级来构建子树
    right = binexpr(OpPrec[tokentype]);

    // 将该子树与我们的树连接起来。同时将词法单元
    // 转换为 AST 运算
    left = mkastnode(arithop(tokentype), left, NULL, right, 0);

    // 更新当前词法单元的详情。
    // 如果遇到分号或 ')'，则只返回左节点
    tokentype = Token.token;
    if (tokentype == T_SEMI || tokentype == T_RPAREN)
      return (left);
  }

  // 当优先级相同或更低时，
  // 返回我们已有的树
  return (left);
}