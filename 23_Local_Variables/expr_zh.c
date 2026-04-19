#include "defs.h"
#include "data.h"
#include "decl.h"

// 表达式解析
// Copyright (c) 2019 Warren Toomey, GPL3

// 解析带有单个表达式参数
// 的函数调用并返回其抽象语法树
static struct ASTnode *funccall(void) {
  struct ASTnode *tree;
  int id;

  // 检查该标识符已被定义为函数，
  // 然后为其创建一个叶节点
  if ((id = findsymbol(Text)) == -1 || Symtable[id].stype != S_FUNCTION) {
    fatals("Undeclared function", Text);
  }
  // 获取 '('
  lparen();

  // 解析随后的表达式
  tree = binexpr(0);

  // 构建函数调用的抽象语法树节点。存储
  // 函数的返回类型作为此节点的类型。
  // 同时记录函数的符号ID
  tree = mkastunary(A_FUNCCALL, Symtable[id].type, tree, id);

  // 获取 ')'
  rparen();
  return (tree);
}

// 解析数组索引并
// 返回其抽象语法树
static struct ASTnode *array_access(void) {
  struct ASTnode *left, *right;
  int id;

  // 检查该标识符已被定义为数组，
  // 然后创建一个指向基址的叶节点
  if ((id = findsymbol(Text)) == -1 || Symtable[id].stype != S_ARRAY) {
    fatals("Undeclared array", Text);
  }
  left = mkastleaf(A_ADDR, Symtable[id].type, id);

  // 获取 '['
  scan(&Token);

  // 解析随后的表达式
  right = binexpr(0);

  // 获取 ']'
  match(T_RBRACKET, "]");

  // 确保这是int类型
  if (!inttype(right->type))
    fatal("Array index is not of integer type");

  // 按元素类型的大小缩放索引
  right = modify_type(right, left->type, A_ADD);

  // 返回一个抽象语法树，其中数组的基址加上了偏移量，
  // 并解引用元素。此时仍是左值。
  left = mkastnode(A_ADD, Symtable[id].type, left, NULL, right, 0);
  left = mkastunary(A_DEREF, value_at(left->type), left, 0);
  return (left);
}

// 解析后缀表达式并返回
// 表示它的抽象语法树节点。标识符已在Text中。
static struct ASTnode *postfix(void) {
  struct ASTnode *n;
  int id;

  // 扫描下一个词法单元查看是否有后缀表达式
  scan(&Token);

  // 函数调用
  if (Token.token == T_LPAREN)
    return (funccall());

  // 数组引用
  if (Token.token == T_LBRACKET)
    return (array_access());

  // 变量。检查该变量是否存在
  id = findsymbol(Text);
  if (id == -1 || Symtable[id].stype != S_VARIABLE)
    fatals("Unknown variable", Text);

  switch (Token.token) {
      // 后置递增：跳过词法单元
    case T_INC:
      scan(&Token);
      n = mkastleaf(A_POSTINC, Symtable[id].type, id);
      break;

      // 后置递减：跳过词法单元
    case T_DEC:
      scan(&Token);
      n = mkastleaf(A_POSTDEC, Symtable[id].type, id);
      break;

      // 只是变量引用
    default:
      n = mkastleaf(A_IDENT, Symtable[id].type, id);
  }
  return (n);
}

// 解析基本因子并返回
// 表示它的抽象语法树节点。
static struct ASTnode *primary(void) {
  struct ASTnode *n;
  int id;

  switch (Token.token) {
    case T_INTLIT:
      // 对于INTLIT词法单元，创建一个叶抽象语法树节点。
      // 如果它在P_CHAR范围内，则将其设为P_CHAR
      if ((Token.intvalue) >= 0 && (Token.intvalue < 256))
	n = mkastleaf(A_INTLIT, P_CHAR, Token.intvalue);
      else
	n = mkastleaf(A_INTLIT, P_INT, Token.intvalue);
      break;

    case T_STRLIT:
      // 对于STRLIT词法单元，生成其汇编代码。
      // 然后为其创建一个叶抽象语法树节点。id是字符串的标签
      id = genglobstr(Text);
      n = mkastleaf(A_STRLIT, P_CHARPTR, id);
      break;

    case T_IDENT:
      return (postfix());

    case T_LPAREN:
      // 带括号表达式的开始，跳过 '('。
      // 扫描表达式和右括号
      scan(&Token);
      n = binexpr(0);
      rparen();
      return (n);

    default:
      fatald("Expecting a primary expression, got token", Token.token);
  }

  // 扫描下一个词法单元并返回叶节点
  scan(&Token);
  return (n);
}

// 将二元操作符词法单元转换为二元抽象语法树操作。
// 我们依赖于从词法单元到抽象语法树操作的1:1映射
static int binastop(int tokentype) {
  if (tokentype > T_EOF && tokentype <= T_SLASH)
    return (tokentype);
  fatald("Syntax error, token", tokentype);
  return (0);			// 保持-Wall愉快
}

// 如果词法单元是右结合的则返回true，
// 否则返回false。
static int rightassoc(int tokentype) {
  if (tokentype == T_ASSIGN)
    return (1);
  return (0);
}

// 每个词法单元的操作符优先级。必须
// 与defs.h中的词法单元顺序匹配
static int OpPrec[] = {
  0, 10, 20, 30,		// T_EOF, T_ASSIGN, T_LOGOR, T_LOGAND
  40, 50, 60,			// T_OR, T_XOR, T_AMPER 
  70, 70,			// T_EQ, T_NE
  80, 80, 80, 80,		// T_LT, T_GT, T_LE, T_GE
  90, 90,			// T_LSHIFT, T_RSHIFT
  100, 100,			// T_PLUS, T_MINUS
  110, 110			// T_STAR, T_SLASH
};

// 检查我们是否有二元操作符并
// 返回其优先级。
static int op_precedence(int tokentype) {
  int prec;
  if (tokentype > T_SLASH)
    fatald("Token with no precedence in op_precedence:", tokentype);
  prec = OpPrec[tokentype];
  if (prec == 0)
    fatald("Syntax error, token", tokentype);
  return (prec);
}

// prefix_expression: primary
//     | '*'  prefix_expression
//     | '&'  prefix_expression
//     | '-'  prefix_expression
//     | '++' prefix_expression
//     | '--' prefix_expression
//     ;

// 解析前缀表达式并返回
// 表示它的子树。
struct ASTnode *prefix(void) {
  struct ASTnode *tree;
  switch (Token.token) {
    case T_AMPER:
      // 获取下一个词法单元并
      // 递归解析为前缀表达式
      scan(&Token);
      tree = prefix();

      // 确保它是一个标识符
      if (tree->op != A_IDENT)
	fatal("& operator must be followed by an identifier");

      // 现在将操作符改为A_ADDR，类型改为
      // 原始类型的指针
      tree->op = A_ADDR;
      tree->type = pointer_to(tree->type);
      break;
    case T_STAR:
      // 获取下一个词法单元并
      // 递归解析为前缀表达式
      scan(&Token);
      tree = prefix();

      // 目前，确保它是另一个解引用或
      // 标识符
      if (tree->op != A_IDENT && tree->op != A_DEREF)
	fatal("* operator must be followed by an identifier or *");

      // 在树前添加一个A_DEREF操作
      tree = mkastunary(A_DEREF, value_at(tree->type), tree, 0);
      break;
    case T_MINUS:
      // 获取下一个词法单元并
      // 递归解析为前缀表达式
      scan(&Token);
      tree = prefix();

      // 在树前添加一个A_NEGATE操作并
      // 使子节点成为右值。因为char是无符号的，
      // 还要将其扩展为int以使其有符号
      tree->rvalue = 1;
      tree = modify_type(tree, P_INT, 0);
      tree = mkastunary(A_NEGATE, tree->type, tree, 0);
      break;
    case T_INVERT:
      // 获取下一个词法单元并
      // 递归解析为前缀表达式
      scan(&Token);
      tree = prefix();

      // 在树前添加一个A_INVERT操作并
      // 使子节点成为右值
      tree->rvalue = 1;
      tree = mkastunary(A_INVERT, tree->type, tree, 0);
      break;
    case T_LOGNOT:
      // 获取下一个词法单元并
      // 递归解析为前缀表达式
      scan(&Token);
      tree = prefix();

      // 在树前添加一个A_LOGNOT操作并
      // 使子节点成为右值
      tree->rvalue = 1;
      tree = mkastunary(A_LOGNOT, tree->type, tree, 0);
      break;
    case T_INC:
      // 获取下一个词法单元并
      // 递归解析为前缀表达式
      scan(&Token);
      tree = prefix();

      // 目前，确保它是一个标识符
      if (tree->op != A_IDENT)
	fatal("++ operator must be followed by an identifier");

      // 在树前添加一个A_PREINC操作
      tree = mkastunary(A_PREINC, tree->type, tree, 0);
      break;
    case T_DEC:
      // 获取下一个词法单元并
      // 递归解析为前缀表达式
      scan(&Token);
      tree = prefix();

      // 目前，确保它是一个标识符
      if (tree->op != A_IDENT)
	fatal("-- operator must be followed by an identifier");

      // 在树前添加一个A_PREDEC操作
      tree = mkastunary(A_PREDEC, tree->type, tree, 0);
      break;
    default:
      tree = primary();
  }
  return (tree);
}

// 返回以二元操作符为根的抽象语法树。
// 参数ptp是前一个词法单元的优先级。
struct ASTnode *binexpr(int ptp) {
  struct ASTnode *left, *right;
  struct ASTnode *ltemp, *rtemp;
  int ASTop;
  int tokentype;

  // 获取左边的树。
  // 同时获取下一个词法单元
  left = prefix();

  // 如果遇到分号或')'，则只返回左节点
  tokentype = Token.token;
  if (tokentype == T_SEMI || tokentype == T_RPAREN || tokentype == T_RBRACKET) {
    left->rvalue = 1;
    return (left);
  }
  // 当此词法单元的优先级高于
  // 前一个词法单元的优先级时，或者它是右结合的且
  // 等于前一个词法单元的优先级时
  while ((op_precedence(tokentype) > ptp) ||
	 (rightassoc(tokentype) && op_precedence(tokentype) == ptp)) {
    // 获取下一个整数字面量
    scan(&Token);

    // 用我们词法单元的优先级递归调用binexpr()
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

      // 构建一个赋值抽象语法树。但是，交换
      // 左右，这样右表达式的代码将在左表达式之前生成
      ltemp = left;
      left = right;
      right = ltemp;
    } else {

      // 我们不是在做赋值，所以两个树都应该是右值
      // 如果它们是左值树，则将两个树都转换为右值
      left->rvalue = 1;
      right->rvalue = 1;

      // 尝试修改每个树以匹配对方的类型，
      // 确保两种类型是兼容的
      ltemp = modify_type(left, right->type, ASTop);
      rtemp = modify_type(right, left->type, ASTop);
      if (ltemp == NULL && rtemp == NULL)
	fatal("Incompatible types in binary expression");
      if (ltemp != NULL)
	left = ltemp;
      if (rtemp != NULL)
	right = rtemp;
    }

    // 将该子树与我们现有的树连接。同时
    // 将词法单元转换为抽象语法树操作
    left = mkastnode(binastop(tokentype), left->type, left, NULL, right, 0);

    // 更新当前词法单元的详细信息。
    // 如果遇到分号、')'或']'，则只返回左节点
    tokentype = Token.token;
    if (tokentype == T_SEMI || tokentype == T_RPAREN
	|| tokentype == T_RBRACKET) {
      left->rvalue = 1;
      return (left);
    }
  }

  // 当优先级相同或更低时，
  // 返回我们拥有的树
  left->rvalue = 1;
  return (left);
}