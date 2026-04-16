#include "defs.h"
#include "data.h"
#include "decl.h"

// 表达式解析
// Copyright (c) 2019 Warren Toomey, GPL3

// 解析带有单个表达式实参的函数调用并返回其 AST
struct ASTnode *funccall(void) {
  struct ASTnode *tree;
  int id;

  // 检查标识符已被定义，然后为其创建叶节点
  // XXX 添加结构类型测试
  if ((id = findglob(Text)) == -1) {
    fatals("Undeclared function", Text);
  }
  // 获取 '('
  lparen();

  // 解析随后的表达式
  tree = binexpr(0);

  // 构造函数调用 AST 节点。将
  // 函数的返回类型存储为该节点的类型。
  // 同时记录函数的符号 ID
  tree = mkastunary(A_FUNCCALL, Gsym[id].type, tree, id);

  // 获取 ')'
  rparen();
  return (tree);
}

// 解析一个基本因子并返回表示它的 AST 节点
static struct ASTnode *primary(void) {
  struct ASTnode *n;
  int id;

  switch (Token.token) {
    case T_INTLIT:
      // 对于 INTLIT 标记，为其创建一个叶 AST 节点
      // 如果在 P_CHAR 范围内则将其设为 P_CHAR
      if ((Token.intvalue) >= 0 && (Token.intvalue < 256))
	n = mkastleaf(A_INTLIT, P_CHAR, Token.intvalue);
      else
	n = mkastleaf(A_INTLIT, P_INT, Token.intvalue);
      break;

    case T_IDENT:
      // 这可能是变量或函数调用
      // 扫描下一个标记以查明
      scan(&Token);

      // 是 '('，所以是函数调用
      if (Token.token == T_LPAREN)
	return (funccall());

      // 不是函数调用，所以拒绝新标记
      reject_token(&Token);

      // 检查变量是否存在
      // XXX 添加结构类型测试
      id = findglob(Text);
      if (id == -1)
	fatals("Unknown variable", Text);

      // 为其创建一个叶 AST 节点
      n = mkastleaf(A_IDENT, Gsym[id].type, id);
      break;

    default:
      fatald("Syntax error, token", Token.token);
  }

  // 扫描下一个标记并返回叶节点
  scan(&Token);
  return (n);
}


// 将二元运算符标记转换为 AST 操作
// 我们依赖从标记到 AST 操作的 1:1 映射
static int arithop(int tokentype) {
  if (tokentype > T_EOF && tokentype < T_INTLIT)
    return (tokentype);
  fatald("Syntax error, token", tokentype);
}

// 每个标记的运算符优先级
// 必须与 defs.h 中的标记顺序相匹配
static int OpPrec[] = {
  0, 10, 10,			// T_EOF, T_PLUS, T_MINUS
  20, 20,			// T_STAR, T_SLASH
  30, 30,			// T_EQ, T_NE
  40, 40, 40, 40		// T_LT, T_GT, T_LE, T_GE
};

// 检查我们是否有二元运算符并返回其优先级
static int op_precedence(int tokentype) {
  int prec = OpPrec[tokentype];
  if (prec == 0)
    fatald("Syntax error, token", tokentype);
  return (prec);
}

// 返回以二元运算符为根的 AST 树
// 参数 ptp 是前一个标记的优先级
struct ASTnode *binexpr(int ptp) {
  struct ASTnode *left, *right;
  int lefttype, righttype;
  int tokentype;

  // 获取左侧的基本树
  // 同时获取下一个标记
  left = primary();

  // 如果遇到分号或 ')'，则仅返回左节点
  tokentype = Token.token;
  if (tokentype == T_SEMI || tokentype == T_RPAREN)
    return (left);

  // 当此标记的优先级大于前一个标记优先级时
  while (op_precedence(tokentype) > ptp) {
    // 获取下一个整数字面量
    scan(&Token);

    // 使用我们标记的优先级递归调用 binexpr()
    // 来构建子树
    right = binexpr(OpPrec[tokentype]);

    // 确保两种类型兼容
    lefttype = left->type;
    righttype = right->type;
    if (!type_compatible(&lefttype, &righttype, 0))
      fatal("Incompatible types");

    // 如需要则加宽任一侧。类型变量现在是 A_WIDEN
    if (lefttype)
      left = mkastunary(lefttype, right->type, left, 0);
    if (righttype)
      right = mkastunary(righttype, left->type, right, 0);

    // 将该子树与我们的树连接起来。同时将标记转换为 AST 操作
    left = mkastnode(arithop(tokentype), left->type, left, NULL, right, 0);

    // 更新当前标记的详细信息
    // 如果遇到分号或 ')'，则仅返回左节点
    tokentype = Token.token;
    if (tokentype == T_SEMI || tokentype == T_RPAREN)
      return (left);
  }

  // 当优先级相同或更低时返回我们拥有的树
  return (left);
}
