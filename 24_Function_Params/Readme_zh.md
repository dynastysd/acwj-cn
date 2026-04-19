# 第 24 章：函数参数

我已经实现了将函数参数从寄存器复制到函数栈上的功能，但我还没有实现用实参调用函数的功能。

作为回顾，这是 Eli Bendersky 的文章中关于
[x86-64 上的栈帧布局](https://eli.thegreenplace.net/2011/09/06/stack-frame-layout-on-x86-64/)的图片。

![](Figs/x64_frame_nonleaf.png)

最多六个"传值调用"的实参通过 '%rdi' 到 '%r9' 这些寄存器传入函数。超过六个实参时，剩下的实参会通过栈传递。

当函数被调用时，它将之前的栈基指针压入栈中，然后将栈基指针下移到与栈指针相同的位置，最后将栈指针移动到最低的局部变量处（至少如此）。

为什么是"至少"？因为我们还需要将栈指针下移到一个十六的倍数，
这样栈基指针在调用另一个函数之前才能正确对齐。

被压入栈的实参会保留在那里，它们相对于栈基指针的偏移量为正。所有通过寄存器传递的实参都会被复制到栈上，同时我们也会为局部变量在栈上分配空间。这些局部变量相对于栈基指针的偏移量为负。

这是我们的目标，但我们还需要先完成一些事情。

## 新的词法单元与扫描

首先，ANSI C 中的函数声明是由逗号分隔的类型和变量名列表，例如：

```c
int function(int x, char y, long z) { ... }
```

因此，我们需要一个新的词法单元 T_COMMA，并修改词法扫描器来识别它。我将把 `scan.c` 中 `scan()` 的修改留给你自己阅读。

## 一种新的存储类

在编译器编写旅程的最后一部分，我描述了修改符号表以支持全局变量和局部变量的变化。我们将全局变量存储在表的一端，局部变量存储在另一端。现在，我要引入函数参数。

我在 `defs.h` 中添加了一个新的存储类定义：

```c
// 存储类
enum {
        C_GLOBAL = 1,           // 全局可见的符号
        C_LOCAL,                // 局部可见的符号
        C_PARAM                 // 局部可见的函数参数
};
```

它们会出现在符号表的什么位置？实际上，同一个参数会同时出现在全局端和局部端的符号表中。

在全局符号列表中，我们首先用 C_GLOBAL 和 S_FUNCTION 条目定义函数的符号。然后，我们用连续的条目定义所有参数，这些条目被标记为 C_PARAM。这就是函数的*原型*。这意味着，当我们稍后调用该函数时，我们可以将实参列表与形参列表进行比较，确保它们匹配。

同时，相同的参数列表也存储在局部符号列表中，但被标记为 C_PARAM 而不是 C_LOCAL。这允许我们区分别人传给我们的变量和我们自己声明的变量。

## 解析器的修改

在这部分旅程中，我只处理函数声明。我们需要修改解析器来完成这项工作。一旦我们解析了函数的类型、名称和左括号'('，就可以查找参数了。每个参数都遵循正常的变量声明语法，但参数声明不是以分号结束，而是用逗号分隔。

旧的 `var_declaration()` 函数在 `decl.c` 中曾在变量声明末尾扫描 T_SEMI 词法单元。现在这已被移到了 `var_declaration()` 的调用者处。

我们现在有了一个新函数 `param_declaration()`，它的任务是读取左括号后面的（零个或多个）参数列表：

```c
// param_declaration: <null>
//           | variable_declaration
//           | variable_declaration ',' param_declaration
//
// 解析函数名后括号中的参数。
// 将它们作为符号添加到符号表并返回参数数量。
static int param_declaration(void) {
  int type;
  int paramcnt=0;

  // 循环直到遇到右括号
  while (Token.token != T_RPAREN) {
    // 获取类型和标识符
    // 并将其添加到符号表
    type = parse_type();
    ident();
    var_declaration(type, 1, 1);
    paramcnt++;

    // 此时必须是 ',' 或 ')'
    switch (Token.token) {
      case T_COMMA: scan(&Token); break;
      case T_RPAREN: break;
      default:
        fatald("Unexpected token in parameter list", Token.token);
    }
  }

  // 返回参数计数
  return(paramcnt);
}
```

`var_declaration()` 的两个 '1' 参数表示这是一个局部变量，也是一个参数声明。在 `var_declaration()` 中，我们现在这样做：

```c
    // 将其添加为已知标量
    // 并在汇编中为其分配空间
    if (islocal) {
      if (addlocl(Text, type, S_VARIABLE, isparam, 1)==-1)
       fatals("Duplicate local variable declaration", Text);
    } else {
      addglob(Text, type, S_VARIABLE, 0, 1);
    }
```

以前代码允许重复的局部变量声明，但这现在会导致栈增长超过所需的空间，所以我将任何重复声明都设为了致命错误。

## 符号表的变化

之前，我说过参数会被放在符号表的全局端和局部端，但上面的代码只显示了对 `addlocl()` 的调用。那么这是怎么回事呢？

我修改了 `addlocal()`，让它同时也将参数添加到全局端：

```c
int addlocl(char *name, int type, int stype, int isparam, int size) {
  int localslot, globalslot;
  ...
  localslot = newlocl();
  if (isparam) {
    updatesym(localslot, name, type, stype, C_PARAM, 0, size, 0);
    globalslot = newglob();
    updatesym(globalslot, name, type, stype, C_PARAM, 0, size, 0);
  } else {
    updatesym(localslot, name, type, stype, C_LOCAL, 0, size, 0);
  }
```

我们不仅为参数在符号表中获得了一个局部槽位，还获得了一个全局槽位。而且两者都被标记为 C_PARAM，而不是 C_LOCAL。

鉴于全局端现在包含了不是 C_GLOBAL 的符号，我们需要修改搜索全局符号的代码：

```c
// 判断符号 s 是否在全局符号表中。
// 返回其槽位位置，如果未找到则返回 -1。
// 跳过 C_PARAM 条目
int findglob(char *s) {
  int i;

  for (i = 0; i < Globs; i++) {
    if (Symtable[i].class == C_PARAM) continue;
    if (*s == *Symtable[i].name && !strcmp(s, Symtable[i].name))
      return (i);
  }
  return (-1);
}
```

## x86-64 代码生成器的修改

以上就是解析函数参数并将其记录在符号表中的内容。现在我们需要生成一个合适的函数序言，它能将寄存器中的实参复制到栈上的位置，同时设置新的栈基指针和栈指针。

我在上一部分写完 `cgresetlocals()` 后意识到，我可以在调用 `cgfuncpreamble()` 时重置栈偏移量，所以我删除了这个函数。另外，计算新局部变量偏移量的代码只需要在 `cg.c` 中可见，所以我给它改了名：

```c
// 下一个局部变量相对于栈基指针的位置。
// 我们将偏移量存储为正数，以使栈指针对齐更容易
static int localOffset;
static int stackOffset;

// 创建新局部变量的位置。
static int newlocaloffset(int type) {
  // 偏移量至少减少 4 个字节
  // 并在栈上分配空间
  localOffset += (cgprimsize(type) > 4) ? cgprimsize(type) : 4;
  return (-localOffset);
}
```

我还从计算负偏移量切换到计算正偏移量，因为这让我在头脑中做数学时更容易。但我仍然返回一个负偏移量，如返回值所示。

我们有六个新的用于保存实参值的寄存器，所以我们最好在某个地方给它们命名。我扩展了寄存器名称列表如下：

```c
#define NUMFREEREGS 4
#define FIRSTPARAMREG 9         // 第一个参数寄存器的位置
static int freereg[NUMFREEREGS];
static char *reglist[] =
  { "%r10", "%r11", "%r12", "%r13", "%r9", "%r8", "%rcx", "%rdx", "%rsi",
"%rdi" };
static char *breglist[] =
  { "%r10b", "%r11b", "%r12b", "%r13b", "%r9b", "%r8b", "%cl", "%dl", "%sil",
"%dil" };
static char *dreglist[] =
  { "%r10d", "%r11d", "%r12d", "%r13d", "%r9d", "%r8d", "%ecx", "%edx",
"%esi", "%edi" };
```

FIRSTPARAMREG 实际上是每个列表中的最后一个条目位置。我们将从这个末端开始，向后工作。

现在我们把注意力转向那个将为我们完成所有工作的函数 `cgfuncpreamble()`。让我们分阶段查看代码：

```c
// 输出函数序言
void cgfuncpreamble(int id) {
  char *name = Symtable[id].name;
  int i;
  int paramOffset = 16;         // 任何被压入的实参从这个栈偏移量开始
  int paramReg = FIRSTPARAMREG; // 上述寄存器列表中第一个参数寄存器的索引

  // 在文本段中输出，重置局部偏移量
  cgtextseg();
  localOffset= 0;

  // 输出函数起始，保存 %rsp 和 %rsp
  fprintf(Outfile,
          "\t.globl\t%s\n"
          "\t.type\t%s, @function\n"
          "%s:\n" "\tpushq\t%%rbp\n"
          "\tmovq\t%%rsp, %%rbp\n", name, name, name);
```

首先，声明函数，保存旧的基指针并将其下移到当前栈指针所在的位置。我们还知道任何在栈上的实参将在新基指针上方 16 字节处，我们也知道哪个寄存器将包含第一个形参。

```c
  // 将任何在寄存器中的参数复制到栈上
  // 在不超过六个参数寄存器后停止
  for (i = NSYMBOLS - 1; i > Locls; i--) {
    if (Symtable[i].class != C_PARAM)
      break;
    if (i < NSYMBOLS - 6)
      break;
    Symtable[i].posn = newlocaloffset(Symtable[i].type);
    cgstorlocal(paramReg--, i);
  }
```

这个循环最多执行六次，但一旦遇到不是 C_PARAM 的内容（即 C_LOCAL）就会退出。调用 `newlocaloffset()` 生成相对于栈上基指针的偏移量，并将寄存器实参复制到栈上的这个位置。

```c
  // 对于其余的，如果是参数那么它们已经在栈上了。
  // 如果只是局部变量，则在栈上分配一个位置。
  for (; i > Locls; i--) {
    if (Symtable[i].class == C_PARAM) {
      Symtable[i].posn = paramOffset;
      paramOffset += 8;
    } else {
      Symtable[i].posn = newlocaloffset(Symtable[i].type);
    }
  }
```

对于每个剩余的局部变量：如果是 C_PARAM，那么它已经在栈上了，所以只需在符号表中记录它现有的位置。如果是 C_LOCAL，则在栈上创建一个新位置并记录它。我们现在有了新的栈帧，其中包含了我们需要的所有局部变量。剩下的就是将栈指针对齐到十六的倍数：

```c
  // 将栈指针对齐到比其原值小 16 的倍数
  stackOffset = (localOffset + 15) & ~15;
  fprintf(Outfile, "\taddq\t$%d,%%rsp\n", -stackOffset);
}
```

`stackOffset` 是一个在整个 `cg.c` 中可见的静态变量。我们需要记住这个值，因为在函数尾声时，我们需要将栈指向上升我们之前下移的量，并恢复旧的栈基指针：

```c
// 输出函数尾声
void cgfuncpostamble(int id) {
  cglabel(Symtable[id].endlabel);
  fprintf(Outfile, "\taddq\t$%d,%%rsp\n", stackOffset);
  fputs("\tpopq %rbp\n" "\tret\n", Outfile);
}
```

## 测试修改

有了这些对编译器的修改，我们可以声明具有许多参数以及任意局部变量的函数。但编译器还没有生成通过寄存器等传递实参的代码。

因此，为了测试我们对编译器的这次修改，我们用我们的编译器编写一些带有参数的函数并编译它们（`input27a.c`）：

```c
int param8(int a, int b, int c, int d, int e, int f, int g, int h) {
  printint(a); printint(b); printint(c); printint(d);
  printint(e); printint(f); printint(g); printint(h);
  return(0);
}

int param5(int a, int b, int c, int d, int e) {
  printint(a); printint(b); printint(c); printint(d); printint(e);
  return(0);
}

int param2(int a, int b) {
  int c; int d; int e;
  c= 3; d= 4; e= 5;
  printint(a); printint(b); printint(c); printint(d); printint(e);
  return(0);
}

int param0() {
  int a; int b; int c; int d; int e;
  a= 1; b= 2; c= 3; d= 4; e= 5;
  printint(a); printint(b); printint(c); printint(d); printint(e);
  return(0);
}
```

我们编写一个单独的文件 `input27b.c`，并用 `gcc` 编译它：

```c
#include <stdio.h>
extern int param8(int a, int b, int c, int d, int e, int f, int g, int h);
extern int param5(int a, int b, int c, int d, int e);
extern int param2(int a, int b);
extern int param0();

int main() {
  param8(1,2,3,4,5,6,7,8); puts("--");
  param5(1,2,3,4,5); puts("--");
  param2(1,2); puts("--");
  param0();
  return(0);
}
```

然后我们可以将它们链接在一起，看看可执行文件是否能运行：

```
cc -o comp1 -g -Wall cg.c decl.c expr.c gen.c main.c misc.c
      scan.c stmt.c sym.c tree.c types.c
./comp1 input27a.c
cc -o out input27b.c out.s lib/printint.c
./out
1
2
3
4
5
6
7
8
--
1
2
3
4
5
--
1
2
3
4
5
--
1
2
3
4
5
```

它工作了！我加了一个感叹号，因为当事情成功时感觉仍然像魔法一样。让我们检查一下 `param8()` 的汇编代码：

```
param8:
        pushq   %rbp                    # 保存 %rbp，移动 %rsp
        movq    %rsp, %rbp
        movl    %edi, -4(%rbp)          # 将六个实参复制到栈上的局部变量
        movl    %esi, -8(%rbp)
        movl    %edx, -12(%rbp)
        movl    %ecx, -16(%rbp)
        movl    %r8d, -20(%rbp)
        movl    %r9d, -24(%rbp)
        addq    $-32,%rsp               # 将栈指针降低 32
        movslq  -4(%rbp), %r10
        movq    %r10, %rdi
        call    printint                # 打印 -4(%rbp)，即 a
        movq    %rax, %r11
        movslq  -8(%rbp), %r10
        movq    %r10, %rdi
        call    printint                # 打印 -8(%rbp)，即 b
        movq    %rax, %r11
        movslq  -12(%rbp), %r10
        movq    %r10, %rdi
        call    printint                # 打印 -12(%rbp)，即 c
        movq    %rax, %r11
        movslq  -16(%rbp), %r10
        movq    %r10, %rdi
        call    printint                # 打印 -16(%rbp)，即 d
        movq    %rax, %r11
        movslq  -20(%rbp), %r10
        movq    %r10, %rdi
        call    printint                # 打印 -20(%rbp)，即 e
        movq    %rax, %r11
        movslq  -24(%rbp), %r10
        movq    %r10, %rdi
        call    printint                # 打印 -24(%rbp)，即 f
        movq    %rax, %r11
        movslq  16(%rbp), %r10
        movq    %r10, %rdi
        call    printint                # 打印 16(%rbp)，即 g
        movq    %rax, %r11
        movslq  24(%rbp), %r10
        movq    %r10, %rdi
        call    printint                # 打印 24(%rbp)，即 h
        movq    %rax, %r11
        movq    $0, %r10
        movl    %r10d, %eax
        jmp     L1
L1:
        addq    $32,%rsp                # 将栈指针升高 32
        popq    %rbp                    # 恢复 %rbp 并返回
        ret
```

`input27a.c` 中其他一些函数既有参数变量也有本地声明的变量，所以看来生成的序言是正确的（好吧，足够通过这些测试了！）。

## 结论与下一步

我花了几次尝试才做对。第一次我在错误的方向上遍历局部符号列表，导致参数顺序错误。我还误读了 Eli Bendersky 文章中的图片，导致我的序言破坏了旧的基指针。在某种程度上，这是好事，因为重写的代码比原来的代码更清晰。

在我们编译器编写旅程的下一部分，我将修改编译器以支持任意数量实参的函数调用。然后我可以把 `input27a.c` 和 `input27b.c` 移入 `tests/` 目录。[下一步](../25_Function_Arguments/Readme_zh.md)
