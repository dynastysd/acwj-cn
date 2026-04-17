#include "defs.h"
#include "data.h"
#include "decl.h"

// x86-64 代码生成器
// Copyright (c) 2019 Warren Toomey, GPL3


// 可用寄存器及其名称列表
// 我们还需要字节寄存器和双字寄存器的列表
static int freereg[4];
static char *reglist[4] = { "%r8", "%r9", "%r10", "%r11" };
static char *breglist[4] = { "%r8b", "%r9b", "%r10b", "%r11b" };
static char *dreglist[4] = { "%r8d", "%r9d", "%r10d", "%r11d" };

// 将所有寄存器设置为可用状态
void freeall_registers(void) {
  freereg[0] = freereg[1] = freereg[2] = freereg[3] = 1;
}

// 分配一个空闲寄存器，返回寄存器的编号
// 如果没有可用寄存器则终止程序
static int alloc_register(void) {
  for (int i = 0; i < 4; i++) {
    if (freereg[i]) {
      freereg[i] = 0;
      return (i);
    }
  }
  fatal("Out of registers");
  return (NOREG);		// 保持 -Wall 编译器高兴
}

// 将寄存器返回到可用寄存器列表中
// 检查它是否已经在列表中
static void free_register(int reg) {
  if (freereg[reg] != 0)
    fatald("Error trying to free register", reg);
  freereg[reg] = 1;
}

// 输出汇编序言
void cgpreamble() {
  freeall_registers();
  fputs("\t.text\n", Outfile);
}

// 无需操作
void cgpostamble() {
}

// 输出函数序言
void cgfuncpreamble(int id) {
  char *name = Gsym[id].name;
  fprintf(Outfile,
	  "\t.text\n"
	  "\t.globl\t%s\n"
	  "\t.type\t%s, @function\n"
	  "%s:\n" "\tpushq\t%%rbp\n"
	  "\tmovq\t%%rsp, %%rbp\n", name, name, name);
}

// 输出函数尾声
void cgfuncpostamble(int id) {
  cglabel(Gsym[id].endlabel);
  fputs("\tpopq	%rbp\n" "\tret\n", Outfile);
}

// 将整数字面值加载到寄存器中
// 返回寄存器编号
// 对于 x86-64，我们不需要担心类型
int cgloadint(int value, int type) {
  // 获取一个新寄存器
  int r = alloc_register();

  fprintf(Outfile, "\tmovq\t$%d, %s\n", value, reglist[r]);
  return (r);
}

// 将变量中的值加载到寄存器中
// 返回寄存器编号
int cgloadglob(int id) {
  // 获取一个新寄存器
  int r = alloc_register();

  // 输出初始化的代码
  switch (Gsym[id].type) {
    case P_CHAR:
      fprintf(Outfile, "\tmovzbq\t%s(%%rip), %s\n", Gsym[id].name,
	      reglist[r]);
      break;
    case P_INT:
      fprintf(Outfile, "\tmovzbl\t%s(\%%rip), %s\n", Gsym[id].name,
	      reglist[r]);
      break;
    case P_LONG:
    case P_CHARPTR:
    case P_INTPTR:
    case P_LONGPTR:
      fprintf(Outfile, "\tmovq\t%s(\%%rip), %s\n", Gsym[id].name, reglist[r]);
      break;
    default:
      fatald("Bad type in cgloadglob:", Gsym[id].type);
  }
  return (r);
}

// 给定全局字符串的标签号，
// 将其地址加载到新寄存器中
int cgloadglobstr(int id) {
  // 获取一个新寄存器
  int r = alloc_register();
  fprintf(Outfile, "\tleaq\tL%d(\%%rip), %s\n", id, reglist[r]);
  return (r);
}

// 将两个寄存器相加并返回
// 结果所在寄存器的编号
int cgadd(int r1, int r2) {
  fprintf(Outfile, "\taddq\t%s, %s\n", reglist[r1], reglist[r2]);
  free_register(r1);
  return (r2);
}

// 用第一个寄存器减去第二个寄存器并
// 返回结果所在寄存器的编号
int cgsub(int r1, int r2) {
  fprintf(Outfile, "\tsubq\t%s, %s\n", reglist[r2], reglist[r1]);
  free_register(r2);
  return (r1);
}

// 将两个寄存器相乘并返回
// 结果所在寄存器的编号
int cgmul(int r1, int r2) {
  fprintf(Outfile, "\timulq\t%s, %s\n", reglist[r1], reglist[r2]);
  free_register(r1);
  return (r2);
}

// 用第一个寄存器除以第二个寄存器并
// 返回结果所在寄存器的编号
int cgdiv(int r1, int r2) {
  fprintf(Outfile, "\tmovq\t%s,%%rax\n", reglist[r1]);
  fprintf(Outfile, "\tcqo\n");
  fprintf(Outfile, "\tidivq\t%s\n", reglist[r2]);
  fprintf(Outfile, "\tmovq\t%%rax,%s\n", reglist[r1]);
  free_register(r2);
  return (r1);
}

// 使用给定寄存器中的一个参数调用函数
// 返回保存结果的寄存器
int cgcall(int r, int id) {
  // 获取一个新寄存器
  int outr = alloc_register();
  fprintf(Outfile, "\tmovq\t%s, %%rdi\n", reglist[r]);
  fprintf(Outfile, "\tcall\t%s\n", Gsym[id].name);
  fprintf(Outfile, "\tmovq\t%%rax, %s\n", reglist[outr]);
  free_register(r);
  return (outr);
}

// 将寄存器左移常量位
int cgshlconst(int r, int val) {
  fprintf(Outfile, "\tsalq\t$%d, %s\n", val, reglist[r]);
  return(r);
}

// 将寄存器的值存储到变量中
int cgstorglob(int r, int id) {
  switch (Gsym[id].type) {
    case P_CHAR:
      fprintf(Outfile, "\tmovb\t%s, %s(\%%rip)\n", breglist[r],
	      Gsym[id].name);
      break;
    case P_INT:
      fprintf(Outfile, "\tmovl\t%s, %s(\%%rip)\n", dreglist[r],
	      Gsym[id].name);
      break;
    case P_LONG:
    case P_CHARPTR:
    case P_INTPTR:
    case P_LONGPTR:
      fprintf(Outfile, "\tmovq\t%s, %s(\%%rip)\n", reglist[r], Gsym[id].name);
      break;
    default:
      fatald("Bad type in cgloadglob:", Gsym[id].type);
  }
  return (r);
}

// P_XXX 顺序的类型大小数组
// 0 表示无大小
static int psize[] = { 0, 0, 1, 4, 8, 8, 8, 8, 8 };

// 给定 P_XXX 类型值，返回
// 原始类型的字节大小
int cgprimsize(int type) {
  // 检查类型是否有效
  if (type < P_NONE || type > P_LONGPTR)
    fatal("Bad type in cgprimsize()");
  return (psize[type]);
}

// 生成全局符号
void cgglobsym(int id) {
  int typesize;
  // 获取类型的大小
  typesize = cgprimsize(Gsym[id].type);

  // 生成全局标识和标签
  fprintf(Outfile, "\t.data\n" "\t.globl\t%s\n", Gsym[id].name);
  fprintf(Outfile, "%s:", Gsym[id].name);

  // 生成存储空间
  for (int i=0; i < Gsym[id].size; i++) {
    switch(typesize) {
      case 1: fprintf(Outfile, "\t.byte\t0\n"); break;
      case 4: fprintf(Outfile, "\t.long\t0\n"); break;
      case 8: fprintf(Outfile, "\t.quad\t0\n"); break;
      default: fatald("Unknown typesize in cgglobsym: ", typesize);
    }
  }
}

// 生成全局字符串及其起始标签
void cgglobstr(int l, char *strvalue) {
  char *cptr;
  cglabel(l);
  for (cptr= strvalue; *cptr; cptr++) {
    fprintf(Outfile, "\t.byte\t%d\n", *cptr);
  }
  fprintf(Outfile, "\t.byte\t0\n");
}

// 比较指令列表，
// 按 AST 顺序：A_EQ, A_NE, A_LT, A_GT, A_LE, A_GE
static char *cmplist[] =
  { "sete", "setne", "setl", "setg", "setle", "setge" };

// 比较两个寄存器，如果为真则设置
int cgcompare_and_set(int ASTop, int r1, int r2) {

  // 检查 AST 操作的范围
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

// 生成跳转到标签的指令
void cgjump(int l) {
  fprintf(Outfile, "\tjmp\tL%d\n", l);
}

// 反转跳转指令列表，
// 按 AST 顺序：A_EQ, A_NE, A_LT, A_GT, A_LE, A_GE
static char *invcmplist[] = { "jne", "je", "jge", "jle", "jg", "jl" };

// 比较两个寄存器，如果为假则跳转
int cgcompare_and_jump(int ASTop, int r1, int r2, int label) {

  // 检查 AST 操作的范围
  if (ASTop < A_EQ || ASTop > A_GE)
    fatal("Bad ASTop in cgcompare_and_set()");

  fprintf(Outfile, "\tcmpq\t%s, %s\n", reglist[r2], reglist[r1]);
  fprintf(Outfile, "\t%s\tL%d\n", invcmplist[ASTop - A_EQ], label);
  freeall_registers();
  return (NOREG);
}

// 将寄存器中的值从旧类型扩展到新类型，
// 并返回一个包含新值的寄存器
int cgwiden(int r, int oldtype, int newtype) {
  // 无需操作
  return (r);
}

// 生成从函数返回值的代码
void cgreturn(int reg, int id) {
  // 根据函数类型生成代码
  switch (Gsym[id].type) {
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
      fatald("Bad function type in cgreturn:", Gsym[id].type);
  }
  cgjump(Gsym[id].endlabel);
}


// 生成将全局标识符地址加载到
// 变量中的代码，返回一个新寄存器
int cgaddress(int id) {
  int r = alloc_register();

  fprintf(Outfile, "\tleaq\t%s(\%%rip), %s\n", Gsym[id].name, reglist[r]);
  return (r);
}

// 解引用指针以获取其指向的值
// 到同一寄存器中
int cgderef(int r, int type) {
  switch (type) {
    case P_CHARPTR:
      fprintf(Outfile, "\tmovzbq\t(%s), %s\n", reglist[r], reglist[r]);
      break;
    case P_INTPTR:
      fprintf(Outfile, "\tmovq\t(%s), %s\n", reglist[r], reglist[r]);
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