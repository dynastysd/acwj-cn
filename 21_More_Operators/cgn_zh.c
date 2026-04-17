#include "defs.h"
#include "data.h"
#include "decl.h"

// x86-64 代码生成器
// Copyright (c) 2019 Warren Toomey, GPL3


// 可用寄存器列表及其名称。
// 我们还需要字节寄存器和双字寄存器的列表
static int freereg[4];
static char *reglist[4]  = { "r8",  "r9",  "r10",  "r11" };
static char *breglist[4] = { "r8b", "r9b", "r10b", "r11b" };
static char *dreglist[4] = { "r8d", "r9d", "r10d", "r11d" };

// 将所有寄存器设置为可用
void freeall_registers(void) {
  freereg[0] = freereg[1] = freereg[2] = freereg[3] = 1;
}

// 分配一个空闲寄存器。返回
// 寄存器编号。如果没有可用寄存器则终止程序。
static int alloc_register(void) {
  for (int i = 0; i < 4; i++) {
    if (freereg[i]) {
      freereg[i] = 0;
      return (i);
    }
  }
  fatal("Out of registers");
  return (NOREG);		// Keep -Wall happy
}

// 将寄存器返还到可用寄存器列表。
// 检查它是否已经在列表中。
static void free_register(int reg) {
  if (freereg[reg] != 0)
    fatald("Error trying to free register", reg);
  freereg[reg] = 1;
}

// 输出汇编语言前导代码
void cgpreamble() {
  freeall_registers();
  fputs("\textern\tprintint\n", Outfile);
  fputs("\textern\tprintchar\n", Outfile);
}

// 无操作
void cgpostamble() {
}

// 输出函数前导代码
void cgfuncpreamble(int id) {
  char *name = Gsym[id].name;
  fprintf(Outfile,
	  "\tsection\t.text\n"
	  "\tglobal\t%s\n"
	  "%s:\n" "\tpush\trbp\n"
	  "\tmov\trbp, rsp\n", name, name);
}

// 输出函数后导代码
void cgfuncpostamble(int id) {
  cglabel(Gsym[id].endlabel);
  fputs("\tpop	rbp\n" "\tret\n", Outfile);
}

// 将整数字面量加载到寄存器中。
// 返回寄存器编号。
// 对于 x86-64，我们不需要担心类型。
int cgloadint(int value, int type) {
  // 获取一个新寄存器
  int r = alloc_register();

  fprintf(Outfile, "\tmov\t%s, %d\n", reglist[r], value);
  return (r);
}

// 从变量加载值到寄存器。
// 返回寄存器编号。如果
// 操作是前置或后置递增/递减，
// 还要执行此操作。
int cgloadglob(int id, int op) {
  // 获取一个新寄存器
  int r = alloc_register();

  // 输出初始化代码
  switch (Gsym[id].type) {
    case P_CHAR:
      if (op == A_PREINC)
	fprintf(Outfile, "\tinc\tbyte [%s]\n", Gsym[id].name);
      if (op == A_PREDEC)
	fprintf(Outfile, "\tdec\tbyte [%s]\n", Gsym[id].name);
      fprintf(Outfile, "\tmovzx\t%s, byte [%s]\n", reglist[r],
              Gsym[id].name);
      if (op == A_POSTINC)
	fprintf(Outfile, "\tinc\tbyte [%s]\n", Gsym[id].name);
      if (op == A_POSTDEC)
	fprintf(Outfile, "\tdec\tbyte [%s]\n", Gsym[id].name);
      break;
    case P_INT:
      if (op == A_PREINC)
	fprintf(Outfile, "\tinc\tdword [%s]\n", Gsym[id].name);
      if (op == A_PREDEC)
	fprintf(Outfile, "\tdec\tdword [%s]\n", Gsym[id].name);
      fprintf(Outfile, "\tmovsx\t%s, word [%s]\n", dreglist[r],
              Gsym[id].name);
      fprintf(Outfile, "\tmovsxd\t%s, %s\n", reglist[r], dreglist[r]);
      if (op == A_POSTINC)
	fprintf(Outfile, "\tinc\tdword [%s]\n", Gsym[id].name);
      if (op == A_POSTDEC)
	fprintf(Outfile, "\tdec\tdword [%s]\n", Gsym[id].name);
      break;
    case P_LONG:
    case P_CHARPTR:
    case P_INTPTR:
    case P_LONGPTR:
      if (op == A_PREINC)
	fprintf(Outfile, "\tinc\tqword [%s]\n", Gsym[id].name);
      if (op == A_PREDEC)
	fprintf(Outfile, "\tdec\tqword [%s]\n", Gsym[id].name);
      fprintf(Outfile, "\tmov\t%s, [%s]\n", reglist[r], Gsym[id].name);
      if (op == A_POSTINC)
	fprintf(Outfile, "\tinc\tqword [%s]\n", Gsym[id].name);
      if (op == A_POSTDEC)
	fprintf(Outfile, "\tdec\tqword [%s]\n", Gsym[id].name);
      break;
    default:
      fatald("Bad type in cgloadglob:", Gsym[id].type);
  }
  return (r);
}

// 给定全局字符串的标签编号，
// 将其地址加载到新寄存器中
int cgloadglobstr(int id) {
  // 获取一个新寄存器
  int r = alloc_register();
  fprintf(Outfile, "\tmov\t%s, L%d\n", reglist[r], id);
  return (r);
}

// 将两个寄存器相加并返回
// 包含结果的寄存器编号
int cgadd(int r1, int r2) {
  fprintf(Outfile, "\tadd\t%s, %s\n", reglist[r2], reglist[r1]);
  free_register(r1);
  return (r2);
}

// 从第一个寄存器减去第二个寄存器并
// 返回包含结果的寄存器编号
int cgsub(int r1, int r2) {
  fprintf(Outfile, "\tsub\t%s, %s\n", reglist[r1], reglist[r2]);
  free_register(r2);
  return (r1);
}

// 将两个寄存器相乘并返回
// 包含结果的寄存器编号
int cgmul(int r1, int r2) {
  fprintf(Outfile, "\timul\t%s, %s\n", reglist[r2], reglist[r1]);
  free_register(r1);
  return (r2);
}

// 将第一个寄存器除以第二个寄存器并
// 返回包含结果的寄存器编号
int cgdiv(int r1, int r2) {
  fprintf(Outfile, "\tmov\trax, %s\n", reglist[r1]);
  fprintf(Outfile, "\tcqo\n");
  fprintf(Outfile, "\tidiv\t%s\n", reglist[r2]);
  fprintf(Outfile, "\tmov\t%s, rax\n", reglist[r1]);
  free_register(r2);
  return (r1);
}

int cgand(int r1, int r2) {
  fprintf(Outfile, "\tand\t%s, %s\n", reglist[r2], reglist[r1]);
  free_register(r1);
  return (r2);
}

int cgor(int r1, int r2) {
  fprintf(Outfile, "\tor\t%s, %s\n", reglist[r2], reglist[r1]);
  free_register(r1);
  return (r2);
}

int cgxor(int r1, int r2) {
  fprintf(Outfile, "\txor\t%s, %s\n", reglist[r2], reglist[r1]);
  free_register(r1);
  return (r2);
}

int cgshl(int r1, int r2) {
  fprintf(Outfile, "\tmov\tcl, %s\n", breglist[r2]);
  fprintf(Outfile, "\tshl\t%s, cl\n", reglist[r1]);
  free_register(r2);
  return (r1);
}

int cgshr(int r1, int r2) {
  fprintf(Outfile, "\tmov\tcl, %s\n", breglist[r2]);
  fprintf(Outfile, "\tshr\t%s, cl\n", reglist[r1]);
  free_register(r2);
  return (r1);
}

// 取反寄存器的值
int cgnegate(int r) {
  fprintf(Outfile, "\tneg\t%s\n", reglist[r]);
  return (r);
}

// 按位取反寄存器的值
int cginvert(int r) {
  fprintf(Outfile, "\tnot\t%s\n", reglist[r]);
  return (r);
}

// 逻辑取反寄存器的值
int cglognot(int r) {
  fprintf(Outfile, "\ttest\t%s, %s\n", reglist[r], reglist[r]);
  fprintf(Outfile, "\tsete\t%s\n", breglist[r]);
  fprintf(Outfile, "\tmovzx\t%s, %s\n", reglist[r], breglist[r]);
  return (r);
}

// 将整数值转换为布尔值。如果
// 是 IF 或 WHILE 操作则跳转
int cgboolean(int r, int op, int label) {
  fprintf(Outfile, "\ttest\t%s, %s\n", reglist[r], reglist[r]);
  if (op == A_IF || op == A_WHILE)
    fprintf(Outfile, "\tje\tL%d\n", label);
  else {
    fprintf(Outfile, "\tsetnz\t%s\n", breglist[r]);
    fprintf(Outfile, "\tmovzx\t%s, byte %s\n", reglist[r], breglist[r]);
  }
  return (r);
}

// 使用给定寄存器中的一个参数调用函数
// 返回包含结果的寄存器
int cgcall(int r, int id) {
  // 获取一个新寄存器
  int outr = alloc_register();
  fprintf(Outfile, "\tmov\trdi, %s\n", reglist[r]);
  fprintf(Outfile, "\tcall\t%s\n", Gsym[id].name);
  fprintf(Outfile, "\tmov\t%s, rax\n", reglist[outr]);
  free_register(r);
  return (outr);
}

// 将寄存器左移常量值
int cgshlconst(int r, int val) {
  fprintf(Outfile, "\tsal\t%s, %d\n", reglist[r], val);
  return (r);
}

// 将寄存器的值存储到变量中
int cgstorglob(int r, int id) {
  switch (Gsym[id].type) {
    case P_CHAR:
      fprintf(Outfile, "\tmov\t[%s], %s\n", Gsym[id].name, breglist[r]);
      break;
    case P_INT:
      fprintf(Outfile, "\tmov\t[%s], %s\n", Gsym[id].name, dreglist[r]);
      break;
    case P_LONG:
    case P_CHARPTR:
    case P_INTPTR:
    case P_LONGPTR:
      fprintf(Outfile, "\tmov\t[%s], %s\n", Gsym[id].name, reglist[r]);
      break;
    default:
      fatald("Bad type in cgloadglob:", Gsym[id].type);
  }
  return (r);
}

// P_XXX 顺序的类型大小数组。
// 0 表示无大小。
static int psize[] = { 0, 0, 1, 4, 8, 8, 8, 8, 8 };

// 给定 P_XXX 类型值，返回
// 基本类型的大小（以字节为单位）。
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
  fprintf(Outfile, "\tsection\t.data\n" "\tglobal\t%s\n", Gsym[id].name);
  fprintf(Outfile, "%s:", Gsym[id].name);

  // 生成空间
  // 原始版本
  for (int i = 0; i < Gsym[id].size; i++) {
    switch(typesize) {
      case 1:
        fprintf(Outfile, "\tdb\t0\n");
        break;
      case 4:
        fprintf(Outfile, "\tdd\t0\n");
        break;
      case 8:
        fprintf(Outfile, "\tdq\t0\n");
        break;
      default:
        fatald("Unknown typesize in cgglobsym: ", typesize);
    }
  }

  /* 使用 times 代替循环的紧凑版本
  switch(typesize) {
    case 1:
      fprintf(Outfile, "\ttimes\t%d\tdb\t0\n", Gsym[id].size);
      break;
    case 4:
      fprintf(Outfile, "\ttimes\t%d\tdd\t0\n", Gsym[id].size);
      break;
    case 8:
      fprintf(Outfile, "\ttimes\t%d\tdq\t0\n", Gsym[id].size);
      break;
    default:
      fatald("Unknown typesize in cgglobsym: ", typesize);
  }
  */
}

// 生成全局字符串及其起始标签
void cgglobstr(int l, char *strvalue) {
  char *cptr;
  cglabel(l);
  for (cptr = strvalue; *cptr; cptr++) {
    fprintf(Outfile, "\tdb\t%d\n", *cptr);
  }
  fprintf(Outfile, "\tdb\t0\n");

  /* 将字符串以单行可读格式输出
  // 可能在错误检查上过度设计了
  int comma = 0, quote = 0, start = 1;
  fprintf(Outfile, "\tdb\t");
  for (cptr=strvalue; *cptr; cptr++) {
    if ( ! isprint(*cptr) )
      if (comma || start) {
        fprintf(Outfile, "%d, ", *cptr);
        start = 0;
        comma = 1;
      }
      else if (quote) {
        fprintf(Outfile, "\', %d, ", *cptr);
        comma = 1;
        quote = 0;
      }
      else {
        fprintf(Outfile, "%d, ", *cptr);
        comma = 1;
        quote = 0;
      }
    else
      if (start || comma) {
        fprintf(Outfile, "\'%c", *cptr);
        start = comma = 0;
        quote = 1;
      }
      else {
        fprintf(Outfile, "%c", *cptr);
        comma = 0;
        quote = 1;
      }
  }
  if (comma || start)
    fprintf(Outfile, "0\n");
  else
    fprintf(Outfile, "\', 0\n");
  */
}

// 比较指令列表，
// 按 AST 顺序：A_EQ, A_NE, A_LT, A_GT, A_LE, A_GE
static char *cmplist[] =
  { "sete", "setne", "setl", "setg", "setle", "setge" };

// 比较两个寄存器，如果为真则设置。
int cgcompare_and_set(int ASTop, int r1, int r2) {

  // 检查 AST 操作的范围
  if (ASTop < A_EQ || ASTop > A_GE)
    fatal("Bad ASTop in cgcompare_and_set()");

  fprintf(Outfile, "\tcmp\t%s, %s\n", reglist[r1], reglist[r2]);
  fprintf(Outfile, "\t%s\t%s\n", cmplist[ASTop - A_EQ], breglist[r2]);
  fprintf(Outfile, "\tmovzx\t%s, %s\n", reglist[r2], breglist[r2]);
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

// 比较两个寄存器，如果为假则跳转。
int cgcompare_and_jump(int ASTop, int r1, int r2, int label) {

  // 检查 AST 操作的范围
  if (ASTop < A_EQ || ASTop > A_GE)
    fatal("Bad ASTop in cgcompare_and_set()");

  fprintf(Outfile, "\tcmp\t%s, %s\n", reglist[r1], reglist[r2]);
  fprintf(Outfile, "\t%s\tL%d\n", invcmplist[ASTop - A_EQ], label);
  freeall_registers();
  return (NOREG);
}

// 将寄存器中的值从旧类型
// 扩展到新类型，并返回包含
// 此新值的寄存器
int cgwiden(int r, int oldtype, int newtype) {
  // 无操作
  return (r);
}

// 生成从函数返回值的代码
void cgreturn(int reg, int id) {
  // 根据函数的类型生成代码
  switch (Gsym[id].type) {
    case P_CHAR:
      fprintf(Outfile, "\tmovzx\teax, %s\n", breglist[reg]);
      break;
    case P_INT:
      fprintf(Outfile, "\tmov\teax, %s\n", dreglist[reg]);
      break;
    case P_LONG:
      fprintf(Outfile, "\tmov\trax, %s\n", reglist[reg]);
      break;
    default:
      fatald("Bad function type in cgreturn:", Gsym[id].type);
  }
  cgjump(Gsym[id].endlabel);
}

// 生成加载全局变量地址的代码
// 到变量中。返回一个新寄存器
int cgaddress(int id) {
  int r = alloc_register();

  fprintf(Outfile, "\tmov\t%s, %s\n", reglist[r], Gsym[id].name);
  return (r);
}

// 解引用指针以获取其
// 指向的值到同一寄存器
int cgderef(int r, int type) {
  switch (type) {
    case P_CHARPTR:
      fprintf(Outfile, "\tmovzx\t%s, byte [%s]\n", reglist[r], reglist[r]);
      break;
    case P_INTPTR:
      fprintf(Outfile, "\tmovsx\t%s, word [%s]\n", reglist[r], dreglist[r]);
      fprintf(Outfile, "\tmovsxd\t%s, %s\n", reglist[r], dreglist[r]);
      break;
    case P_LONGPTR:
      fprintf(Outfile, "\tmov\t%s, [%s]\n", reglist[r], reglist[r]);
      break;
  }
  return (r);
}

// 通过解引用指针存储
int cgstorderef(int r1, int r2, int type) {
  switch (type) {
    case P_CHAR:
      fprintf(Outfile, "\tmov\t[%s], byte %s\n", reglist[r2], breglist[r1]);
      break;
    case P_INT:
      fprintf(Outfile, "\tmov\t[%s], %s\n", reglist[r2], reglist[r1]);
      break;
    case P_LONG:
      fprintf(Outfile, "\tmov\t[%s], %s\n", reglist[r2], reglist[r1]);
      break;
    default:
      fatald("Can't cgstoderef on type:", type);
  }
  return (r1);
}