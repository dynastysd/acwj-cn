#include "defs.h"
#include "data.h"
#include "decl.h"

// x86-64代码生成器
// Copyright (c) 2019 Warren Toomey, GPL3

// 标志，表示我们正在输出哪个段
enum { no_seg, text_seg, data_seg } currSeg = no_seg;

void cgtextseg() {
  if (currSeg != text_seg) {
    fputs("\t.text\n", Outfile);
    currSeg = text_seg;
  }
}

void cgdataseg() {
  if (currSeg != data_seg) {
    fputs("\t.data\n", Outfile);
    currSeg = data_seg;
  }
}

// 下一个局部变量相对于栈基址寄存器的位置。
// 我们将偏移量存储为正数，以使栈指针对齐更容易
static int localOffset;
static int stackOffset;

// 解析新函数时重置新局部变量的位置
void cgresetlocals(void) {
  localOffset = 0;
}

// 获取下一个局部变量的位置。
// 使用isparam标志来分配形参（尚未实现XXX）。
int cggetlocaloffset(int type, int isparam) {
  // 目前只是将偏移量递减至少4个字节
  // 并在栈上分配
  localOffset += (cgprimsize(type) > 4) ? cgprimsize(type) : 4;
// printf("Returning offset %d for type %d\n", localOffset, type);
  return (-localOffset);
}

// 可用寄存器及其名称的列表。
// 我们也需要字节和双字寄存器的列表
#define NUMFREEREGS 4
static int freereg[NUMFREEREGS];
static char *reglist[] = { "%r8", "%r9", "%r10", "%r11" };
static char *breglist[] = { "%r8b", "%r9b", "%r10b", "%r11b" };
static char *dreglist[] = { "%r8d", "%r9d", "%r10d", "%r11d" };

// 将所有寄存器设置为可用
void freeall_registers(void) {
  freereg[0] = freereg[1] = freereg[2] = freereg[3] = 1;
}

// 分配一个空闲寄存器。返回
// 寄存器的编号。如果没有可用寄存器则报错。
static int alloc_register(void) {
  for (int i = 0; i < NUMFREEREGS; i++) {
    if (freereg[i]) {
      freereg[i] = 0;
      return (i);
    }
  }
  fatal("Out of registers");
  return (NOREG);		// 保持-Wall愉快
}

// 将寄存器返回到可用寄存器列表。
// 检查它是否已经在那里。
static void free_register(int reg) {
  if (freereg[reg] != 0)
    fatald("Error trying to free register", reg);
  freereg[reg] = 1;
}

// 打印汇编前导码
void cgpreamble() {
  freeall_registers();
}

// 无操作
void cgpostamble() {
}

// 打印函数前导码
void cgfuncpreamble(int id) {
  char *name = Symtable[id].name;
  cgtextseg();

  // 将栈指针对齐为16的倍数
  // 小于其之前的值
  stackOffset= (localOffset+15) & ~15;
  // printf("preamble local %d stack %d\n", localOffset, stackOffset);
  
  fprintf(Outfile,
	  "\t.globl\t%s\n"
	  "\t.type\t%s, @function\n"
	  "%s:\n" "\tpushq\t%%rbp\n"
	  "\tmovq\t%%rsp, %%rbp\n"
	  "\taddq\t$%d,%%rsp\n", name, name, name, -stackOffset);
}

// 打印函数后导码
void cgfuncpostamble(int id) {
  cglabel(Symtable[id].endlabel);
  fprintf(Outfile, "\taddq\t$%d,%%rsp\n", stackOffset);
  fputs("\tpopq	%rbp\n" "\tret\n", Outfile);
}

// 将整数字面量值加载到寄存器中。
// 返回寄存器的编号。
// 对于x86-64，我们不需要担心类型。
int cgloadint(int value, int type) {
  // 获取一个新寄存器
  int r = alloc_register();

  fprintf(Outfile, "\tmovq\t$%d, %s\n", value, reglist[r]);
  return (r);
}

// 将变量值加载到寄存器中。
// 返回寄存器的编号。如果
// 操作是前置或后置递增/递减，
// 也要执行此操作。
int cgloadglob(int id, int op) {
  // 获取一个新寄存器
  int r = alloc_register();

  // 打印初始化它的代码
  switch (Symtable[id].type) {
    case P_CHAR:
      if (op == A_PREINC)
	fprintf(Outfile, "\tincb\t%s(%%rip)\n", Symtable[id].name);
      if (op == A_PREDEC)
	fprintf(Outfile, "\tdecb\t%s(%%rip)\n", Symtable[id].name);
      fprintf(Outfile, "\tmovzbq\t%s(%%rip), %s\n", Symtable[id].name,
	      reglist[r]);
      if (op == A_POSTINC)
	fprintf(Outfile, "\tincb\t%s(%%rip)\n", Symtable[id].name);
      if (op == A_POSTDEC)
	fprintf(Outfile, "\tdecb\t%s(%%rip)\n", Symtable[id].name);
      break;
    case P_INT:
      if (op == A_PREINC)
	fprintf(Outfile, "\tincl\t%s(%%rip)\n", Symtable[id].name);
      if (op == A_PREDEC)
	fprintf(Outfile, "\tdecl\t%s(%%rip)\n", Symtable[id].name);
      fprintf(Outfile, "\tmovslq\t%s(%%rip), %s\n", Symtable[id].name,
	      reglist[r]);
      if (op == A_POSTINC)
	fprintf(Outfile, "\tincl\t%s(%%rip)\n", Symtable[id].name);
      if (op == A_POSTDEC)
	fprintf(Outfile, "\tdecl\t%s(%%rip)\n", Symtable[id].name);
      break;
    case P_LONG:
    case P_CHARPTR:
    case P_INTPTR:
    case P_LONGPTR:
      if (op == A_PREINC)
	fprintf(Outfile, "\tincq\t%s(%%rip)\n", Symtable[id].name);
      if (op == A_PREDEC)
	fprintf(Outfile, "\tdecq\t%s(%%rip)\n", Symtable[id].name);
      fprintf(Outfile, "\tmovq\t%s(%%rip), %s\n", Symtable[id].name,
	      reglist[r]);
      if (op == A_POSTINC)
	fprintf(Outfile, "\tincq\t%s(%%rip)\n", Symtable[id].name);
      if (op == A_POSTDEC)
	fprintf(Outfile, "\tdecq\t%s(%%rip)\n", Symtable[id].name);
      break;
    default:
      fatald("Bad type in cgloadglob:", Symtable[id].type);
  }
  return (r);
}

// 将局部变量值加载到寄存器中。
// 返回寄存器的编号。如果
// 操作是前置或后置递增/递减，
// 也要执行此操作。
int cgloadlocal(int id, int op) {
  // 获取一个新寄存器
  int r = alloc_register();

  // 打印初始化它的代码
  switch (Symtable[id].type) {
    case P_CHAR:
      if (op == A_PREINC)
	fprintf(Outfile, "\tincb\t%d(%%rbp)\n", Symtable[id].posn);
      if (op == A_PREDEC)
	fprintf(Outfile, "\tdecb\t%d(%%rbp)\n", Symtable[id].posn);
      fprintf(Outfile, "\tmovzbq\t%d(%%rbp), %s\n", Symtable[id].posn,
	      reglist[r]);
      if (op == A_POSTINC)
	fprintf(Outfile, "\tincb\t%d(%%rbp)\n", Symtable[id].posn);
      if (op == A_POSTDEC)
	fprintf(Outfile, "\tdecb\t%d(%%rbp)\n", Symtable[id].posn);
      break;
    case P_INT:
      if (op == A_PREINC)
	fprintf(Outfile, "\tincl\t%d(%%rbp)\n", Symtable[id].posn);
      if (op == A_PREDEC)
	fprintf(Outfile, "\tdecl\t%d(%%rbp)\n", Symtable[id].posn);
      fprintf(Outfile, "\tmovslq\t%d(%%rbp), %s\n", Symtable[id].posn,
	      reglist[r]);
      if (op == A_POSTINC)
	fprintf(Outfile, "\tincl\t%d(%%rbp)\n", Symtable[id].posn);
      if (op == A_POSTDEC)
	fprintf(Outfile, "\tdecl\t%d(%%rbp)\n", Symtable[id].posn);
      break;
    case P_LONG:
    case P_CHARPTR:
    case P_INTPTR:
    case P_LONGPTR:
      if (op == A_PREINC)
	fprintf(Outfile, "\tincq\t%d(%%rbp)\n", Symtable[id].posn);
      if (op == A_PREDEC)
	fprintf(Outfile, "\tdecq\t%d(%%rbp)\n", Symtable[id].posn);
      fprintf(Outfile, "\tmovq\t%d(%%rbp), %s\n", Symtable[id].posn,
	      reglist[r]);
      if (op == A_POSTINC)
	fprintf(Outfile, "\tincq\t%d(%%rbp)\n", Symtable[id].posn);
      if (op == A_POSTDEC)
	fprintf(Outfile, "\tdecq\t%d(%%rbp)\n", Symtable[id].posn);
      break;
    default:
      fatald("Bad type in cgloadlocal:", Symtable[id].type);
  }
  return (r);
}

// 给定全局字符串的标签号，
// 将其地址加载到新寄存器中
int cgloadglobstr(int id) {
  // 获取一个新寄存器
  int r = alloc_register();
  fprintf(Outfile, "\tleaq\tL%d(%%rip), %s\n", id, reglist[r]);
  return (r);
}

// 将两个寄存器相加并返回
// 包含结果的寄存器编号
int cgadd(int r1, int r2) {
  fprintf(Outfile, "\taddq\t%s, %s\n", reglist[r1], reglist[r2]);
  free_register(r1);
  return (r2);
}

// 从第一个寄存器减去第二个寄存器并
// 返回包含结果的寄存器编号
int cgsub(int r1, int r2) {
  fprintf(Outfile, "\tsubq\t%s, %s\n", reglist[r2], reglist[r1]);
  free_register(r2);
  return (r1);
}

// 将两个寄存器相乘并返回
// 包含结果的寄存器编号
int cgmul(int r1, int r2) {
  fprintf(Outfile, "\timulq\t%s, %s\n", reglist[r1], reglist[r2]);
  free_register(r1);
  return (r2);
}

// 用第一个寄存器除以第二个寄存器并
// 返回包含结果的寄存器编号
int cgdiv(int r1, int r2) {
  fprintf(Outfile, "\tmovq\t%s,%%rax\n", reglist[r1]);
  fprintf(Outfile, "\tcqo\n");
  fprintf(Outfile, "\tidivq\t%s\n", reglist[r2]);
  fprintf(Outfile, "\tmovq\t%%rax,%s\n", reglist[r1]);
  free_register(r2);
  return (r1);
}

int cgand(int r1, int r2) {
  fprintf(Outfile, "\tandq\t%s, %s\n", reglist[r1], reglist[r2]);
  free_register(r1);
  return (r2);
}

int cgor(int r1, int r2) {
  fprintf(Outfile, "\torq\t%s, %s\n", reglist[r1], reglist[r2]);
  free_register(r1);
  return (r2);
}

int cgxor(int r1, int r2) {
  fprintf(Outfile, "\txorq\t%s, %s\n", reglist[r1], reglist[r2]);
  free_register(r1);
  return (r2);
}

int cgshl(int r1, int r2) {
  fprintf(Outfile, "\tmovb\t%s, %%cl\n", breglist[r2]);
  fprintf(Outfile, "\tshlq\t%%cl, %s\n", reglist[r1]);
  free_register(r2);
  return (r1);
}

int cgshr(int r1, int r2) {
  fprintf(Outfile, "\tmovb\t%s, %%cl\n", breglist[r2]);
  fprintf(Outfile, "\tshrq\t%%cl, %s\n", reglist[r1]);
  free_register(r2);
  return (r1);
}

// 求反寄存器的值
int cgnegate(int r) {
  fprintf(Outfile, "\tnegq\t%s\n", reglist[r]);
  return (r);
}

// 反转寄存器的值
int cginvert(int r) {
  fprintf(Outfile, "\tnotq\t%s\n", reglist[r]);
  return (r);
}

// 逻辑求反寄存器的值
int cglognot(int r) {
  fprintf(Outfile, "\ttest\t%s, %s\n", reglist[r], reglist[r]);
  fprintf(Outfile, "\tsete\t%s\n", breglist[r]);
  fprintf(Outfile, "\tmovzbq\t%s, %s\n", breglist[r], reglist[r]);
  return (r);
}

// 将整数值转换为布尔值。如果是
// IF或WHILE操作则跳转
int cgboolean(int r, int op, int label) {
  fprintf(Outfile, "\ttest\t%s, %s\n", reglist[r], reglist[r]);
  if (op == A_IF || op == A_WHILE)
    fprintf(Outfile, "\tje\tL%d\n", label);
  else {
    fprintf(Outfile, "\tsetnz\t%s\n", breglist[r]);
    fprintf(Outfile, "\tmovzbq\t%s, %s\n", breglist[r], reglist[r]);
  }
  return (r);
}

// 使用给定寄存器中的一个参数调用函数
// 返回包含结果的寄存器
int cgcall(int r, int id) {
  // 获取一个新寄存器
  int outr = alloc_register();
  fprintf(Outfile, "\tmovq\t%s, %%rdi\n", reglist[r]);
  fprintf(Outfile, "\tcall\t%s\n", Symtable[id].name);
  fprintf(Outfile, "\tmovq\t%%rax, %s\n", reglist[outr]);
  free_register(r);
  return (outr);
}

// 将寄存器左移一个常量
int cgshlconst(int r, int val) {
  fprintf(Outfile, "\tsalq\t$%d, %s\n", val, reglist[r]);
  return (r);
}

// 将寄存器的值存储到变量中
int cgstorglob(int r, int id) {
  switch (Symtable[id].type) {
    case P_CHAR:
      fprintf(Outfile, "\tmovb\t%s, %s(%%rip)\n", breglist[r],
	      Symtable[id].name);
      break;
    case P_INT:
      fprintf(Outfile, "\tmovl\t%s, %s(%%rip)\n", dreglist[r],
	      Symtable[id].name);
      break;
    case P_LONG:
    case P_CHARPTR:
    case P_INTPTR:
    case P_LONGPTR:
      fprintf(Outfile, "\tmovq\t%s, %s(%%rip)\n", reglist[r],
	      Symtable[id].name);
      break;
    default:
      fatald("Bad type in cgstorglob:", Symtable[id].type);
  }
  return (r);
}

// 将寄存器的值存储到局部变量中
int cgstorlocal(int r, int id) {
  switch (Symtable[id].type) {
    case P_CHAR:
      fprintf(Outfile, "\tmovb\t%s, %d(%%rbp)\n", breglist[r],
	      Symtable[id].posn);
      break;
    case P_INT:
      fprintf(Outfile, "\tmovl\t%s, %d(%%rbp)\n", dreglist[r],
	      Symtable[id].posn);
      break;
    case P_LONG:
    case P_CHARPTR:
    case P_INTPTR:
    case P_LONGPTR:
      fprintf(Outfile, "\tmovq\t%s, %d(%%rbp)\n", reglist[r],
	      Symtable[id].posn);
      break;
    default:
      fatald("Bad type in cgstorlocal:", Symtable[id].type);
  }
  return (r);
}

// 类型大小数组，按P_XXX顺序。
// 0表示无大小。
static int psize[] = { 0, 0, 1, 4, 8, 8, 8, 8, 8 };

// 给定P_XXX类型值，返回
// 原始类型的大小（字节）。
int cgprimsize(int type) {
  // 检查类型是否有效
  if (type < P_NONE || type > P_LONGPTR)
    fatal("Bad type in cgprimsize()");
  return (psize[type]);
}

// 生成全局符号但不包括函数
void cgglobsym(int id) {
  int typesize;

  if (Symtable[id].stype == S_FUNCTION)
    return;

  // 获取类型的大小
  typesize = cgprimsize(Symtable[id].type);

  // 生成全局标识和标签
  cgdataseg();
  fprintf(Outfile, "\t.globl\t%s\n", Symtable[id].name);
  fprintf(Outfile, "%s:", Symtable[id].name);

  // 生成空间
  for (int i = 0; i < Symtable[id].size; i++) {
    switch (typesize) {
      case 1:
	fprintf(Outfile, "\t.byte\t0\n");
	break;
      case 4:
	fprintf(Outfile, "\t.long\t0\n");
	break;
      case 8:
	fprintf(Outfile, "\t.quad\t0\n");
	break;
      default:
	fatald("Unknown typesize in cgglobsym: ", typesize);
    }
  }
}

// 生成全局字符串及其起始标签
void cgglobstr(int l, char *strvalue) {
  char *cptr;
  cglabel(l);
  for (cptr = strvalue; *cptr; cptr++) {
    fprintf(Outfile, "\t.byte\t%d\n", *cptr);
  }
  fprintf(Outfile, "\t.byte\t0\n");
}

// 比较指令列表，
// 按抽象语法树顺序：A_EQ, A_NE, A_LT, A_GT, A_LE, A_GE
static char *cmplist[] =
  { "sete", "setne", "setl", "setg", "setle", "setge" };

// 比较两个寄存器并在为真时设置。
int cgcompare_and_set(int ASTop, int r1, int r2) {

  // 检查抽象语法树操作的范围
  if (ASTop < A_EQ || ASTop > A_GE)
    fatal("Bad ASTop in cgcompare_and_set()");

  fprintf(Outfile, "\tcmpq\t%s, %s\n", reglist[r2], reglist[r1]);
  fprintf(Outfile, "\t%s\t%s\n", cmplist[ASTop - A_EQ], breglist[r2]);
  fprintf(Outfile, "\tmovzbq\t%s, %s\n", breglist[r2], reglist[r2]);
  free_register(r1);
  return (r2);
}

// 生成标签
void cglabel(int l) {
  fprintf(Outfile, "L%d:\n", l);
}

// 生成跳转到标签
void cgjump(int l) {
  fprintf(Outfile, "\tjmp\tL%d\n", l);
}

// 反转跳转指令列表，
// 按抽象语法树顺序：A_EQ, A_NE, A_LT, A_GT, A_LE, A_GE
static char *invcmplist[] = { "jne", "je", "jge", "jle", "jg", "jl" };

// 比较两个寄存器并在为假时跳转。
int cgcompare_and_jump(int ASTop, int r1, int r2, int label) {

  // 检查抽象语法树操作的范围
  if (ASTop < A_EQ || ASTop > A_GE)
    fatal("Bad ASTop in cgcompare_and_set()");

  fprintf(Outfile, "\tcmpq\t%s, %s\n", reglist[r2], reglist[r1]);
  fprintf(Outfile, "\t%s\tL%d\n", invcmplist[ASTop - A_EQ], label);
  freeall_registers();
  return (NOREG);
}

// 将寄存器中的值从旧类型扩展到新类型，
// 并返回包含此新值的寄存器
int cgwiden(int r, int oldtype, int newtype) {
  // 无操作
  return (r);
}

// 生成从函数返回值的代码
void cgreturn(int reg, int id) {
  // 根据函数的类型生成代码
  switch (Symtable[id].type) {
    case P_CHAR:
      fprintf(Outfile, "\tmovzbl\t%s, %%eax\n", breglist[reg]);
      break;
    case P_INT:
      fprintf(Outfile, "\tmovl\t%s, %%eax\n", dreglist[reg]);
      break;
    case P_LONG:
      fprintf(Outfile, "\tmovq\t%s, %%rax\n", reglist[reg]);
      break;
    default:
      fatald("Bad function type in cgreturn:", Symtable[id].type);
  }
  cgjump(Symtable[id].endlabel);
}


// 生成将标识符的地址加载到
// 变量中的代码。返回一个新寄存器
int cgaddress(int id) {
  int r = alloc_register();

  if (Symtable[id].class == C_LOCAL)
    fprintf(Outfile, "\tleaq\t%d(%%rbp), %s\n", Symtable[id].posn,
	    reglist[r]);
  else
    fprintf(Outfile, "\tleaq\t%s(%%rip), %s\n", Symtable[id].name,
	    reglist[r]);
  return (r);
}

// 解引用指针并将其
// 指向的值加载到同一寄存器中
int cgderef(int r, int type) {
  switch (type) {
    case P_CHARPTR:
      fprintf(Outfile, "\tmovzbq\t(%s), %s\n", reglist[r], reglist[r]);
      break;
    case P_INTPTR:
      fprintf(Outfile, "\tmovslq\t(%s), %s\n", reglist[r], reglist[r]);
      break;
    case P_LONGPTR:
      fprintf(Outfile, "\tmovq\t(%s), %s\n", reglist[r], reglist[r]);
      break;
    default:
      fatald("Can't cgderef on type:", type);
  }
  return (r);
}

// 通过解引用的指针存储
int cgstorderef(int r1, int r2, int type) {
  switch (type) {
    case P_CHAR:
      fprintf(Outfile, "\tmovb\t%s, (%s)\n", breglist[r1], reglist[r2]);
      break;
    case P_INT:
      fprintf(Outfile, "\tmovq\t%s, (%s)\n", reglist[r1], reglist[r2]);
      break;
    case P_LONG:
      fprintf(Outfile, "\tmovq\t%s, (%s)\n", reglist[r1], reglist[r2]);
      break;
    default:
      fatald("Can't cgstoderef on type:", type);
  }
  return (r1);
}