#include "defs.h"
#include "data.h"
#include "decl.h"

// 表达式解析
// Copyright (c) 2019 Warren Toomey, GPL3

// 解析一个基本因子并返回一个表示它的 AST 节点。
static struct ASTnode *primary(void) {
  struct ASTnode *n;
  int id;

  switch (Token.token) {
    case T_INTLIT:
      // 对于 INTLIT token，为其创建一个叶子 AST 节点。
      // 如果它在 P_CHAR 范围内，则将其设为 P_CHAR
      if ((Token.intvalue) >= 0 && (Token.intvalue < 256))
	n = mkastleaf(A_INTLIT, P_CHAR, Token.intvalue);
      else
	n = mkastleaf(A_INTLIT, P_INT, Token.intvalue);
      break;

    case T_IDENT:
      // 检查此标识符是否存在
      id = findglob(Text);
      if (id == -1)
	fatals("Unknown variable", Text);

      // 为其创建一个叶子 AST 节点
      n = mkastleaf(A_IDENT, Gsym[id].type, id);
      break;

    default:
      fatald("Syntax error, token", Token.token);
  }

  // 扫描下一个 token 并返回叶子节点
  scan(&Token);
  return (n);
}


// 将二进制操作符 token 转换为 AST 操作。
// 我们依赖于从 token 到 AST 操作的 1:1 映射
static int arithop(int tokentype) {
  if (tokentype > T_EOF && tokentype < T_INTLIT)
    return (tokentype);
  fatald("Syntax error, token", tokentype);
}

// 每个 token 的运算符优先级。
// 必须与 defs.h 中的 token 顺序相匹配
static int OpPrec[] = {
  0, 10, 10,			// T_EOF, T_PLUS, T_MINUS
  20, 20,			// T_STAR, T_SLASH
  30, 30,			// T_EQ, T_NE
  40, 40, 40, 40		// T_LT, T_GT, T_LE, T_GE
};

// 检查我们是否有一个二元运算符并返回其优先级。
static int op_precedence(int tokentype) {
  int prec = OpPrec[tokentype];
  if (prec == 0)
    fatald("Syntax error, token", tokentype);
  return (prec);
}

// 返回一个以二元运算符为根的 AST 树。
// 参数 ptp 是前一个 token 的优先级。
struct ASTnode *binexpr(int ptp) {
  struct ASTnode *left, *right;
  int lefttype, righttype;
  int tokentype;

  // 获取左侧的主树。
  // 同时获取下一个 token。
  left = primary();

  // 如果遇到分号或 ')'，则只返回左节点
  tokentype = Token.token;
  if (tokentype == T_SEMI || tokentype == T_RPAREN)
    return (left);

  // 当这个 token 的优先级大于前一个 token 优先级时
  while (op_precedence(tokentype) > ptp) {
    // 获取下一个整数字面量
    scan(&Token);

    // 递归调用 binexpr() 并用我们 token 的
    // 优先级来构建一个子树
    right = binexpr(OpPrec[tokentype]);

    // 确保两种类型是兼容的。
    lefttype = left->type;
    righttype = right->type;
    if (!type_compatible(&lefttype, &righttype, 0))
      fatal("Incompatible types");

    // 根据需要扩展任一边。类型变量现在是 A_WIDEN
    if (lefttype)
      left = mkastunary(lefttype, right->type, left, 0);
    if (righttype)
      right = mkastunary(righttype, left->type, right, 0);

    // 将该子树与我们的树连接起来。同时将 token 转换为 AST 操作。
    left = mkastnode(arithop(tokentype), left->type, left, NULL, right, 0);

    // 更新当前 token 的详情。
    // 如果遇到分号或 ')'，则只返回左节点
    tokentype = Token.token;
    if (tokentype == T_SEMI || tokentype == T_RPAREN)
      return (left);
  }

  // 当优先级相同或更低时，返回我们拥有的树
  return (left);
}