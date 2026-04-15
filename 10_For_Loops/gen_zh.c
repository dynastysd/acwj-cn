#include "defs.h"
#include "data.h"
#include "decl.h"

// 通用代码生成器
// Copyright (c) 2019 Warren Toomey, GPL3

// 生成并返回一个新的标签号
static int label(void) {
  static int id = 1;
  return (id++);
}

// 为 IF 语句和可选的 ELSE 子句
// 生成代码
static int genIF(struct ASTnode *n) {
  int Lfalse, Lend;

  // 生成两个标签：一个用于
  // 假的复合语句，一个用于
  // 整体 IF 语句的结尾。
  // 当没有 ELSE 子句时，Lfalse _就是_
  // 结尾标签！
  Lfalse = label();
  if (n->right)
    Lend = label();

  // 生成条件代码，后跟
  // 一个到假标签的跳转。
  // 我们通过将 Lfalse 标签作为一个寄存器来骗过它。
  genAST(n->left, Lfalse, n->op);
  genfreeregs();

  // 生成真的复合语句
  genAST(n->mid, NOREG, n->op);
  genfreeregs();

  // 如果有可选的 ELSE 子句，
  // 生成跳转到结尾的跳转
  if (n->right)
    cgjump(Lend);

  // 现在是假标签
  cglabel(Lfalse);

  // 可选的 ELSE 子句：生成
  // 假的复合语句和
  // 结尾标签
  if (n->right) {
    genAST(n->right, NOREG, n->op);
    genfreeregs();
    cglabel(Lend);
  }

  return (NOREG);
}

// 为 WHILE 语句和可选的 ELSE 子句
// 生成代码
static int genWHILE(struct ASTnode *n) {
  int Lstart, Lend;

  // 生成起始和结尾标签
  // 并输出起始标签
  Lstart = label();
  Lend = label();
  cglabel(Lstart);

  // 生成条件代码，后跟
  // 一个到结尾标签的跳转。
  // 我们通过将 Lfalse 标签作为一个寄存器来骗过它。
  genAST(n->left, Lend, n->op);
  genfreeregs();

  // 生成作为循环体的复合语句
  genAST(n->right, NOREG, n->op);
  genfreeregs();

  // 最后输出跳回条件的跳转，
  // 和结尾标签
  cgjump(Lstart);
  cglabel(Lend);
  return (NOREG);
}

// 给定一个 AST，一个持有
// 之前右值的寄存器（如果有的话），以及父节点的 AST op，
// 递归生成汇编代码。
// 返回包含树的最终值的寄存器 id
int genAST(struct ASTnode *n, int reg, int parentASTop) {
  int leftreg, rightreg;

  // 我们现在在顶部有特定的 AST 节点处理
  switch (n->op) {
    case A_IF:
      return (genIF(n));
    case A_WHILE:
      return (genWHILE(n));
    case A_GLUE:
      // 处理每个子语句，并在每个子语句之后释放
      // 寄存器
      genAST(n->left, NOREG, n->op);
      genfreeregs();
      genAST(n->right, NOREG, n->op);
      genfreeregs();
      return (NOREG);
  }

  // 通用的 AST 节点处理在下面

  // 获取左右子树的值
  if (n->left)
    leftreg = genAST(n->left, NOREG, n->op);
  if (n->right)
    rightreg = genAST(n->right, leftreg, n->op);

  switch (n->op) {
    case A_ADD:
      return (cgadd(leftreg, rightreg));
    case A_SUBTRACT:
      return (cgsub(leftreg, rightreg));
    case A_MULTIPLY:
      return (cgmul(leftreg, rightreg));
    case A_DIVIDE:
      return (cgdiv(leftreg, rightreg));
    case A_EQ:
    case A_NE:
    case A_LT:
    case A_GT:
    case A_LE:
    case A_GE:
      // 如果父 AST 节点是 A_IF 或 A_WHILE，生成
      // 一个比较后跟一个跳转。否则，比较寄存器并根据比较结果
      // 将其中一个设置为 1 或 0。
      if (parentASTop == A_IF || parentASTop == A_WHILE)
	return (cgcompare_and_jump(n->op, leftreg, rightreg, reg));
      else
	return (cgcompare_and_set(n->op, leftreg, rightreg));
    case A_INTLIT:
      return (cgloadint(n->v.intvalue));
    case A_IDENT:
      return (cgloadglob(Gsym[n->v.id].name));
    case A_LVIDENT:
      return (cgstorglob(reg, Gsym[n->v.id].name));
    case A_ASSIGN:
      // 工作已经完成，返回结果
      return (rightreg);
    case A_PRINT:
      // 打印左子节点的值
      // 并返回无寄存器
      genprintint(leftreg);
      genfreeregs();
      return (NOREG);
    default:
      fatald("未知的 AST 运算符", n->op);
  }
}

void genpreamble() {
  cgpreamble();
}
void genpostamble() {
  cgpostamble();
}
void genfreeregs() {
  freeall_registers();
}
void genprintint(int reg) {
  cgprintint(reg);
}

void genglobsym(char *s) {
  cgglobsym(s);
}