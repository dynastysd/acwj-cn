# 第 23 章：局部变量

我刚刚按照本编译器编写旅程上一部分描述的设计思路，在栈上实现了局部变量，一切都很顺利。下面，我将概述实际的代码变更。

## 符号表变更

我们从符号表的变更开始，因为这些变更是支持两种变量作用域（全局和局部）的核心。符号表条目的结构现在如下（位于 `defs.h`）：

```c
// 存储类
enum {
        C_GLOBAL = 1,           // 全局可见的符号
        C_LOCAL                 // 局部可见的符号
};

// 符号表结构
struct symtable {
  char *name;                   // 符号的名称
  int type;                     // 符号的原始类型
  int stype;                    // 符号的结构类型
  int class;                    // 符号的存储类
  int endlabel;                 // 对于函数，结束标签
  int size;                     // 符号中元素的数量
  int posn;                     // 对于局部变量，相对于栈基指针的负偏移量
};
```

新增了 `class` 和 `posn` 字段。正如上一部分所述，`posn` 为负值，表示相对于栈基指针的偏移量，即局部变量存储在栈上。在这一部分中，我只实现了局部变量，没有实现形参。还要注意，我们现在有标记为 C_GLOBAL 或 C_LOCAL 的符号。

符号表的名称也发生了变化，其中的索引也是如此（位于 `data.h`）：

```c
extern_ struct symtable Symtable[NSYMBOLS];     // 全局符号表
extern_ int Globs;                              // 下一个空闲全局符号槽的位置
extern_ int Locls;                              // 下一个空闲局部符号槽的位置
```

直观地看，全局符号存储在符号表的左侧，`Globs` 指向下一个空闲全局符号槽，`Locls` 指向下一个空闲局部符号槽。

```
0xxxx......................................xxxxxxxxxxxxNSYMBOLS-1
     ^                                    ^
     |                                    |
   Globs                                Locls
```

在 `sym.c` 中，除了现有的用于查找或分配全局符号的 `findglob()` 和 `newglob()` 函数外，我们现在还有 `findlocl()` 和 `newlocl()`。它们有代码来检测 `Globs` 和 `Locls` 之间的冲突：

```c
// 获取新的全局符号槽的位置，如果用完了就报错。
static int newglob(void) {
  int p;

  if ((p = Globs++) >= Locls)
    fatal("Too many global symbols");
  return (p);
}

// 获取新的局部符号槽的位置，如果用完了就报错。
static int newlocl(void) {
  int p;

  if ((p = Locls--) <= Globs)
    fatal("Too many local symbols");
  return (p);
}
```

现在有一个通用函数 `updatesym()` 用于设置符号表条目中的所有字段。我不会在这里给出代码，因为它只是逐个设置每个字段。

`updatesym()` 函数由 `addglobl()` 和 `addlocl()` 调用。这些函数首先尝试查找现有符号，如果找不到就分配一个新的，然后调用 `updatesym()` 来设置该符号的值。最后，有一个新函数 `findsymbol()`，它可以在符号表的局部和全局部分中搜索符号：

```c
// 判断符号 s 是否在符号表中。
// 返回其槽位位置，如果未找到则返回 -1。
int findsymbol(char *s) {
  int slot;

  slot = findlocl(s);
  if (slot == -1)
    slot = findglob(s);
  return (slot);
}
```

在整个代码的其余部分，旧的 `findglob()` 调用已被 `findsymbol()` 调用替换。

## 声明解析的变更

我们需要能够解析全局和局部变量声明。解析它们的代码（目前）是相同的，所以我给函数添加了一个标志：

```c
void var_declaration(int type, int islocal) {
    ...
      // 将其添加为已知数组
      if (islocal) {
        addlocl(Text, pointer_to(type), S_ARRAY, 0, Token.intvalue);
      } else {
        addglob(Text, pointer_to(type), S_ARRAY, 0, Token.intvalue);
      }
    ...
    // 将其添加为已知标量
    if (islocal) {
      addlocl(Text, type, S_VARIABLE, 0, 1);
    } else {
      addglob(Text, type, S_VARIABLE, 0, 1);
    }
    ...
}
```

目前我们的编译器中有两处对 `var_declaration()` 的调用。`decl.c` 中 `global_declarations()` 里的这一处解析全局变量声明：

```c
void global_declarations(void) {
      ...
      // 解析全局变量声明
      var_declaration(type, 0);
      ...
}
```

`stmt.c` 中 `single_statement()` 里的这一处解析局部变量声明：

```c
static struct ASTnode *single_statement(void) {
  int type;

  switch (Token.token) {
    case T_CHAR:
    case T_INT:
    case T_LONG:

      // 变量声明的开始。
      // 解析类型并获取标识符。
      // 然后解析声明的其余部分。
      type = parse_type();
      ident();
      var_declaration(type, 1);
   ...
  }
  ...
}
```

## x86-64 代码生成器的变更

一如既往，`cg.c` 中平台特定代码的许多 `cgXX()` 函数通过 `gen.c` 中的 `genXX()` 函数暴露给编译器的其余部分。情况就是这样。因此，虽然我只提到 `cgXX()` 函数，但不要忘记通常有匹配的 `genXX()` 函数。

对于每个局部变量，我们需要为其分配一个位置，并将其记录在符号表的 `posn` 字段中。下面是我们如何做到这一点。在 `cg.c` 中我们有一个新的静态变量和两个操作它的函数：

```c
// 下一个局部变量相对于栈基指针的位置。
// 我们将偏移量存储为正数，以简化栈指针的对齐
static int localOffset;
static int stackOffset;

// 解析新函数时重置新局部变量的位置
void cgresetlocals(void) {
  localOffset = 0;
}

// 获取下一个局部变量的位置。
// 使用 isparam 标志来分配形参（尚未实现 XXX）。
int cggetlocaloffset(int type, int isparam) {
  // 偏移量至少减少 4 个字节
  // 并在栈上分配
  localOffset += (cgprimsize(type) > 4) ? cgprimsize(type) : 4;
  return (-localOffset);
}
```

目前，我们将所有局部变量都分配在栈上。它们之间的对齐最小间隔为 4 个字节。不过，对于 64 位整数和指针，每个变量是 8 个字节。

> 我知道在过去，多字节数据项必须在内存中正确对齐，否则 CPU 会出错。似乎至少对于 x86-64，[不需要对齐数据项](https://lemire.me/blog/2012/05/31/data-alignment-for-speed-myth-or-reality/)。

> 但是，x86-64 上的栈指针在函数调用之前确实需要正确对齐。在 Agner Fog 的"[优化汇编语言子程序](https://www.agner.org/optimize/optimizing_assembly.pdf)"第 30 页，他指出"栈指针必须在任何 CALL 指令之前按 16 对齐，这样函数入口时 RSP 的值是 8 modulo 16。"

> 这意味着，作为函数序言的一部分，我们需要将 `%rsp` 设置为正确对齐的值。

一旦我们将函数名添加到符号表之后、开始解析局部变量声明之前，`function_declaration()` 中会调用 `cgresetlocals()`。这会将 `localOffset` 重置为零。

我们看到，当解析新的局部标量或局部数组时会调用 `addlocl()`。`addlocl()` 使用新变量的类型调用 `cggetlocaloffset()`。这会从栈基指针开始递减适当的偏移量，这个偏移量存储在符号的 `posn` 字段中。

现在我们有了符号相对于栈基指针的偏移量，我们需要修改代码生成器，以便在访问局部变量而不是全局变量时，输出相对于 `%rbp` 的偏移量，而不是使用全局位置名称。

因此，我们现在有一个 `cgloadlocal()` 函数，它与 `cgloadglob()` 几乎相同，只是所有用于打印 `Symtable[id].name` 的 `%s(%%rip)` 格式字符串都被替换为用于打印 `Symtable[id].posn` 的 `%d(%%rbp)` 格式字符串。实际上，如果在 `cg.c` 中搜索 `Symtable[id].posn`，你会发现所有这些新的局部变量引用。

### 更新栈指针

既然我们正在使用栈上的位置，我们最好将栈指针移到局部变量区域下方。因此，我们需要修改函数序言和尾声中的栈指针：

```c
// 输出函数序言
void cgfuncpreamble(int id) {
  char *name = Symtable[id].name;
  cgtextseg();

  // 将栈指针对齐到比其原值小 16 的倍数
  stackOffset= (localOffset+15) & ~15;

  fprintf(Outfile,
          "\t.globl\t%s\n"
          "\t.type\t%s, @function\n"
          "%s:\n" "\tpushq\t%%rbp\n"
          "\tmovq\t%%rsp, %%rbp\n"
          "\taddq\t$%d,%%rsp\n", name, name, name, -stackOffset);
}

// 输出函数尾声
void cgfuncpostamble(int id) {
  cglabel(Symtable[id].endlabel);
  fprintf(Outfile, "\taddq\t$%d,%%rsp\n", stackOffset);
  fputs("\tpopq %rbp\n" "\tret\n", Outfile);
}
```

记住 `localOffset` 是负的。所以我们在函数序言中添加一个负值，在函数尾声中添加一个负负值（实际上是加回去）。

## 测试变更

我认为这就是向编译器添加局部变量的主要变更。测试程序 `tests/input25.c` 演示了局部变量在栈上的存储：

```c
int a; int b; int c;

int main()
{
  char z; int y; int x;
  x= 10;  y= 20; z= 30;
  a= 5;   b= 15; c= 25;
}
```

以下是带注释的汇编输出：

```
        .data
        .globl  a
a:      .long   0                       # 三个全局变量
        .globl  b
b:      .long   0
        .globl  c
c:      .long   0

        .text
        .globl  main
        .type   main, @function
main:
        pushq   %rbp
        movq    %rsp, %rbp
        addq    $-16,%rsp               # 将栈指针降低 16
        movq    $10, %r8
        movl    %r8d, -12(%rbp)         # z 在偏移量 -12 处
        movq    $20, %r8
        movl    %r8d, -8(%rbp)          # y 在偏移量 -8 处
        movq    $30, %r8
        movb    %r8b, -4(%rbp)          # x 在偏移量 -4 处
        movq    $5, %r8
        movl    %r8d, a(%rip)           # a 有全局标签
        movq    $15, %r8
        movl    %r8d, b(%rip)           # b 有全局标签
        movq    $25, %r8
        movl    %r8d, c(%rip)           # c 有全局标签
        jmp     L1
L1:
        addq    $16,%rsp                # 将栈指针升高 16
        popq    %rbp
        ret
```

最后，`$ make test` 表明编译器通过了所有之前的测试。

## 结论与下一步

我原以为实现局部变量会很棘手，但在对解决方案的设计进行了一番思考之后，结果比我想象的要容易。我总觉得下一步会是棘手的一个。

在我们编译器编写旅程的下一部分，我将尝试向编译器添加函数实参和形参。祝我好运！[下一步](../24_Function_Params/Readme_zh.md)