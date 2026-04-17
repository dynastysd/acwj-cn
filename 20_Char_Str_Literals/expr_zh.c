#include "defs.h"
#include "data.h"
#include "decl.h"

// 表达式解析
// Copyright (c) 2019 Warren Toomey, GPL3

// 解析带有单个表达式实参的函数调用并返回其 AST
static struct ASTnode *funccall(void) {
  struct ASTnode *tree;
  int id;

  // 检查标识符是否已定义为函数，
  // 然后为其创建一个叶子节点。
  if ((id = findglob(Text)) == -1 || Gsym[id].stype != S_FUNCTION) {
    fatals("Undeclared function", Text);
  }
  // 获取 '('
  lparen();

  // 解析随后的表达式
  tree = binexpr(0);

  // 构建函数调用 AST 节点。将
  // 函数的返回类型存储为该节点的类型。
  // 同时记录函数的符号ID
  tree = mkastunary(A_FUNCCALL, Gsym[id].type, tree, id);

  // 获取 ')'
  rparen();
  return (tree);
}

// 解析数组索引并
// 返回其 AST 树
static struct ASTnode *array_access(void) {
  struct ASTnode *left, *right;
  int id;

  // 检查标识符是否已定义为数组，
  // 然后创建一个指向基址的叶子节点。
  if ((id = findglob(Text)) == -1 || Gsym[id].stype != S_ARRAY) {
    fatals("Undeclared array", Text);
  }
  left = mkastleaf(A_ADDR, Gsym[id].type, id);

  // 获取 '['
  scan(&Token);

  // 解析随后的表达式
  right = binexpr(0);

  // 获取 ']'
  match(T_RBRACKET, "]");

  // 确保是 int 类型
  if (!inttype(right->type))
    fatal("Array index is not of integer type");

  // 按元素类型的大小对索引进行缩放
  right = modify_type(right, left->type, A_ADD);

  // 返回一个 AST 树，其中数组基址加上了偏移量，
  // 并解引用元素。此时仍为左值。
  left = mkastnode(A_ADD, Gsym[id].type, left, NULL, right, 0);
  left = mkastunary(A_DEREF, value_at(left->type), left, 0);
  return (left);
}

// 解析一个基本因子并返回其
// AST 节点。
static struct ASTnode *primary(void) {
  struct ASTnode *n;
  int id;


  switch (Token.token) {
  case T_INTLIT:
    // 对于 INTLIT 标记，为其创建一个叶子 AST 节点。
    // 如果在 P_CHAR 范围内则将其设为 P_CHAR 类型
    if ((Token.intvalue) >= 0 && (Token.intvalue < 256))
      n = mkastleaf(A_INTLIT, P_CHAR, Token.intvalue);
    else
      n = mkastleaf(A_INTLIT, P_INT, Token.intvalue);
    break;

  case T_STRLIT:
    // 对于 STRLIT 标记，为其生成汇编代码。
    // 然后创建一个叶子 AST 节点。id 是字符串的标签。
    id= genglobstr(Text);
    n= mkastleaf(A_STRLIT, P_CHARPTR, id);
    break;

  case T_IDENT:
    // 这可能是变量、数组索引或
    // 函数调用。扫描下一个标记来确定
    scan(&Token);


    // 如果是 '('，则是函数调用
    if (Token.token == T_LPAREN)
      return (funccall());

    // 如果是 '['，则是数组引用
    if (Token.token == T_LBRACKET) {
      return (array_access());
    }
    // 不是函数调用，拒绝新标记
    reject_token(&Token);

    // 检查变量是否存在
    id = findglob(Text);
    if (id == -1 || Gsym[id].stype != S_VARIABLE)
      fatals("Unknown variable", Text);

    // 为其创建一个叶子 AST 节点
    n = mkastleaf(A_IDENT, Gsym[id].type, id);
    break;

  case T_LPAREN:
    // 括号表达式的开始，跳过 '('。
    // 扫描表达式和右括号
    scan(&Token);
    n = binexpr(0);
    rparen();
    return (n);

  default:
    fatald("Expecting a primary expression, got token", Token.token);
  }

  // 扫描下一个标记并返回叶子节点
  scan(&Token);
  return (n);
}


// 将二元运算符标记转换为二元 AST 操作。
// 我们依赖于从标记到 AST 操作的 1:1 映射
static int binastop(int tokentype) {
  if (tokentype > T_EOF && tokentype < T_INTLIT)
    return (tokentype);
  fatald("Syntax error, token", tokentype);
  return (0);			// 保持 -Wall 编译器高兴
}

// 如果标记是右结合的则返回 true，
// 否则返回 false。
static int rightassoc(int tokentype) {
  if (tokentype == T_ASSIGN)
    return (1);
  return (0);
}

// 每个标记的运算符优先级。必须
// 与 defs.h 中的标记顺序匹配
static int OpPrec[] = {
  0, 10,			// T_EOF,  T_ASSIGN
  20, 20,			// T_PLUS, T_MINUS
  30, 30,			// T_STAR, T_SLASH
  40, 40,			// T_EQ, T_NE
  50, 50, 50, 50		// T_LT, T_GT, T_LE, T_GE
};

// 检查我们是否有二元运算符并
// 返回其优先级。
static int op_precedence(int tokentype) {
  int prec;
  if (tokentype >= T_VOID)
    fatald("Token with no precedence in op_precedence:", tokentype);
  prec = OpPrec[tokentype];
  if (prec == 0)
    fatald("Syntax error, token", tokentype);
  return (prec);
}

// prefix_expression: primary
//     | '*' prefix_expression
//     | '&' prefix_expression
//     ;

// 解析前缀表达式并返回
// 其子树。
struct ASTnode *prefix(void) {
  struct ASTnode *tree;
  switch (Token.token) {
  case T_AMPER:
    // 获取下一个标记并递归
    // 将其解析为前缀表达式
    scan(&Token);
    tree = prefix();

    // 确保它是标识符
    if (tree->op != A_IDENT)
      fatal("& operator must be followed by an identifier");

    // 现在将运算符改为 A_ADDR，类型改为
    // 指向原类型的指针
    tree->op = A_ADDR;
    tree->type = pointer_to(tree->type);
    break;
  case T_STAR:
    // 获取下一个标记并递归
    // 将其解析为前缀表达式
    scan(&Token);
    tree = prefix();

    // 目前，确保它是另一个解引用或
    // 标识符
    if (tree->op != A_IDENT && tree->op != A_DEREF)
      fatal("* operator must be followed by an identifier or *");

    // 在树前添加一个 A_DEREF 操作
    tree = mkastunary(A_DEREF, value_at(tree->type), tree, 0);
    break;
  default:
    tree = primary();
  }
  return (tree);
}

// 返回以二元运算符为根的 AST 树。
// 参数 ptp 是前一个标记的优先级。
struct ASTnode *binexpr(int ptp) {
  struct ASTnode *left, *right;
  struct ASTnode *ltemp, *rtemp;
  int ASTop;
  int tokentype;

  // 获取左边的树。
  // 同时获取下一个标记。
  left = prefix();

  // 如果遇到分号或 ')'，则只返回左节点
  tokentype = Token.token;
  if (tokentype == T_SEMI || tokentype == T_RPAREN || tokentype == T_RBRACKET) {
    left->rvalue = 1;
    return (left);
  }
  // 当此标记的优先级高于
  // 前一个标记的优先级，或它是右结合的且
  // 优先级与前一个标记相等时
  while ((op_precedence(tokentype) > ptp) ||
	 (rightassoc(tokentype) && op_precedence(tokentype) == ptp)) {
    // 读取下一个整数字面量
    scan(&Token);

    // 使用我们标记的优先级递归调用 binexpr()
    // 来构建子树
    right = binexpr(OpPrec[tokentype]);

    // 确定要对子树执行的操作
    ASTop = binastop(tokentype);

    if (ASTop == A_ASSIGN) {
      // 赋值
      // 将右树转换为右值
      right->rvalue = 1;

      // 确保右边的类型与左边匹配
      right = modify_type(right, left->type, 0);
      if (left == NULL)
	fatal("Incompatible expression in assignment");

      // 创建一个赋值 AST 树。但是，切换
      // 左右顺序，这样右表达式的
      // 代码会在左表达式之前生成
      ltemp = left;
      left = right;
      right = ltemp;
    } else {

      // 我们不是在做赋值，所以两个树都应该是右值
      // 如果两个树都是左值树则将它们转换为右值
      left->rvalue = 1;
      right->rvalue = 1;

      // 尝试修改每个树以匹配对方的类型，
      // 以确保两个类型兼容。
      ltemp = modify_type(left, right->type, ASTop);
      rtemp = modify_type(right, left->type, ASTop);
      if (ltemp == NULL && rtemp == NULL)
	fatal("Incompatible types in binary expression");
      if (ltemp != NULL)
	left = ltemp;
      if (rtemp != NULL)
	right = rtemp;
    }

    // 将子树与我们的树连接。同时将标记
    // 转换为 AST 操作。
    left = mkastnode(binastop(tokentype), left->type, left, NULL, right, 0);

    // 更新当前标记的详细信息。
    // 如果遇到分号、')'或']'，则只返回左节点
    tokentype = Token.token;
    if (tokentype == T_SEMI || tokentype == T_RPAREN
	|| tokentype == T_RBRACKET) {
      left->rvalue = 1;
      return (left);
    }
  }

  // 当优先级相同或更低时返回我们拥有的树
  left->rvalue = 1;
  return (left);
}