#include "defs.h"
#include "data.h"
#include "decl.h"

// 通用代码生成器
// Copyright (c) 2019 Warren Toomey, GPL3

// 生成并返回一个新的标签号
int genlabel(void) {
  static int id = 1;
  return (id++);
}

// 生成 IF 语句的代码
// 以及可选的 ELSE 子句
static int genIF(struct ASTnode *n) {
  int Lfalse, Lend;

  // 生成两个标签：一个用于
  // 假的复合语句，一个用于
  // 整体 IF 语句的结束。
  // 当没有 ELSE 子句时，Lfalse 就是结束标签！
  Lfalse = genlabel();
  if (n->right)
    Lend = genlabel();

  // 生成条件代码后跟
  // 跳转到假标签的指令
  genAST(n->left, Lfalse, n->op);
  genfreeregs();

  // 生成真的复合语句
  genAST(n->mid, NOLABEL, n->op);
  genfreeregs();

  // 如果有可选的 ELSE 子句，
  // 生成跳转到末尾的指令
  if (n->right)
    cgjump(Lend);

  // 现在是假标签
  cglabel(Lfalse);

  // 可选的 ELSE 子句：生成
  // 假的复合语句和结束标签
  if (n->right) {
    genAST(n->right, NOLABEL, n->op);
    genfreeregs();
    cglabel(Lend);
  }

  return (NOREG);
}

// 生成 WHILE 语句的代码
static int genWHILE(struct ASTnode *n) {
  int Lstart, Lend;

  // 生成开始和结束标签
  // 并输出开始标签
  Lstart = genlabel();
  Lend = genlabel();
  cglabel(Lstart);

  // 生成条件代码后跟
  // 跳转到结束标签的指令
  genAST(n->left, Lend, n->op);
  genfreeregs();

  // 生成循环体的复合语句
  genAST(n->right, NOLABEL, n->op);
  genfreeregs();

  // 最后输出跳回条件的指令，
  // 和结束标签
  cgjump(Lstart);
  cglabel(Lend);
  return (NOREG);
}

// 给定一个抽象语法树、一个可选标签和父节点的
// 抽象语法树操作符，递归生成汇编代码。
// 返回包含树最终值的寄存器 id。
int genAST(struct ASTnode *n, int label, int parentASTop) {
  int leftreg, rightreg;

  // 在顶部处理一些特定的 AST 节点，
  // 以便立即不求值子节点
  switch (n->op) {
    case A_IF:
      return (genIF(n));
    case A_WHILE:
      return (genWHILE(n));
    case A_GLUE:
      // 执行每个子语句，并在每个子语句
      // 之后释放寄存器
      genAST(n->left, NOLABEL, n->op);
      genfreeregs();
      genAST(n->right, NOLABEL, n->op);
      genfreeregs();
      return (NOREG);
    case A_FUNCTION:
      // 在子树的代码之前生成
      // 函数的序言
      cgfuncpreamble(n->v.id);
      genAST(n->left, NOLABEL, n->op);
      cgfuncpostamble(n->v.id);
      return (NOREG);
  }

  // 通用的 AST 节点处理在下方

  // 获取左右子树的值
  if (n->left)
    leftreg = genAST(n->left, NOLABEL, n->op);
  if (n->right)
    rightreg = genAST(n->right, NOLABEL, n->op);

  switch (n->op) {
    case A_ADD:
      return (cgadd(leftreg, rightreg));
    case A_SUBTRACT:
      return (cgsub(leftreg, rightreg));
    case A_MULTIPLY:
      return (cgmul(leftreg, rightreg));
    case A_DIVIDE:
      return (cgdiv(leftreg, rightreg));
    case A_AND:
      return (cgand(leftreg, rightreg));
    case A_OR:
      return (cgor(leftreg, rightreg));
    case A_XOR:
      return (cgxor(leftreg, rightreg));
    case A_LSHIFT:
      return (cgshl(leftreg, rightreg));
    case A_RSHIFT:
      return (cgshr(leftreg, rightreg));
    case A_EQ:
    case A_NE:
    case A_LT:
    case A_GT:
    case A_LE:
    case A_GE:
      // 如果父 AST 节点是 A_IF 或 A_WHILE，生成
      // 一个比较后跟跳转。否则，比较寄存器
      // 并根据比较结果将其中一个设为 1 或 0。
      if (parentASTop == A_IF || parentASTop == A_WHILE)
	return (cgcompare_and_jump(n->op, leftreg, rightreg, label));
      else
	return (cgcompare_and_set(n->op, leftreg, rightreg));
    case A_INTLIT:
      return (cgloadint(n->v.intvalue, n->type));
    case A_STRLIT:
      return (cgloadglobstr(n->v.id));
    case A_IDENT:
      // 如果我们是右值或者正在被解引用，
      // 则加载我们的值
      if (n->rvalue || parentASTop == A_DEREF) {
	if (Symtable[n->v.id].class == C_GLOBAL) {
	  return (cgloadglob(n->v.id, n->op));
	} else {
	  return (cgloadlocal(n->v.id, n->op));
	}
      } else
	return (NOREG);
    case A_ASSIGN:
      // 我们是赋值给标识符还是通过指针赋值？
      switch (n->right->op) {
	case A_IDENT:
	  if (Symtable[n->right->v.id].class == C_GLOBAL)
	    return (cgstorglob(leftreg, n->right->v.id));
	  else
	    return (cgstorlocal(leftreg, n->right->v.id));
	case A_DEREF:
	  return (cgstorderef(leftreg, rightreg, n->right->type));
	default:
	  fatald("Can't A_ASSIGN in genAST(), op", n->op);
      }
    case A_WIDEN:
      // 将子节点的类型扩展到父节点的类型
      return (cgwiden(leftreg, n->left->type, n->type));
    case A_RETURN:
      cgreturn(leftreg, Functionid);
      return (NOREG);
    case A_FUNCCALL:
      return (cgcall(leftreg, n->v.id));
    case A_ADDR:
      return (cgaddress(n->v.id));
    case A_DEREF:
      // 如果我们是右值，解引用获取我们指向的值，
      // 否则将其保留给 A_ASSIGN 通过指针存储
      if (n->rvalue)
	return (cgderef(leftreg, n->left->type));
      else
	return (leftreg);
    case A_SCALE:
      // 小优化：如果比例值是已知的 2 的幂，
      // 则使用移位
      switch (n->v.size) {
	case 2:
	  return (cgshlconst(leftreg, 1));
	case 4:
	  return (cgshlconst(leftreg, 2));
	case 8:
	  return (cgshlconst(leftreg, 3));
	default:
	  // 加载一个寄存器中的大小，并
	  // 用这个大小乘以 leftreg
	  rightreg = cgloadint(n->v.size, P_INT);
	  return (cgmul(leftreg, rightreg));
      }
    case A_POSTINC:
      // 将变量的值加载到寄存器中，
      // 然后递增它
      return (cgloadglob(n->v.id, n->op));
    case A_POSTDEC:
      // 将变量的值加载到寄存器中，
      // 然后递减它
      return (cgloadglob(n->v.id, n->op));
    case A_PREINC:
      // 将变量的值加载并递增到寄存器中
      return (cgloadglob(n->left->v.id, n->op));
    case A_PREDEC:
      // 将变量的值加载并递减到寄存器中
      return (cgloadglob(n->left->v.id, n->op));
    case A_NEGATE:
      return (cgnegate(leftreg));
    case A_INVERT:
      return (cginvert(leftreg));
    case A_LOGNOT:
      return (cglognot(leftreg));
    case A_TOBOOL:
      // 如果父 AST 节点是 A_IF 或 A_WHILE，生成
      // 一个比较后跟跳转。否则，根据其为零或非零
      // 将寄存器设为 0 或 1
      return (cgboolean(leftreg, parentASTop, label));
    default:
      fatald("Unknown AST operator", n->op);
  }
  return (NOREG);		// 保持 -Wall 开心
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
void genglobsym(int id) {
  cgglobsym(id);
}
int genglobstr(char *strvalue) {
  int l = genlabel();
  cgglobstr(l, strvalue);
  return (l);
}
int genprimsize(int type) {
  return (cgprimsize(type));
}