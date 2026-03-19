# 第 4 章：一个真正的编译器

是时候兑现我写一个真正的编译器的承诺了。
在旅程的这一部分，我们将把程序中的解释器替换为生成 x86-64 汇编代码的代码。

## 修改解释器

在此之前，我们有必要回顾一下 `interp.c` 中的解释器代码：

```c
int interpretAST(struct ASTnode *n) {
  int leftval, rightval;

  if (n->left) leftval = interpretAST(n->left);
  if (n->right) rightval = interpretAST(n->right);

  switch (n->op) {
    case A_ADD:      return (leftval + rightval);
    case A_SUBTRACT: return (leftval - rightval);
    case A_MULTIPLY: return (leftval * rightval);
    case A_DIVIDE:   return (leftval / rightval);
    case A_INTLIT:   return (n->intvalue);

    default:
      fprintf(stderr, "Unknown AST operator %d\n", n->op);
      exit(1);
  }
}
```

`interpretAST()` 函数以深度优先的方式遍历给定的抽象语法树。
它先求值左子树，再求值右子树。最后，根据当前树根部的 `op` 值对这
些子节点进行运算。

如果 `op` 值是四个数学运算符之一，则执行相应的数学运算。如果 `op` 值表明该节点
只是一个整数字面量，则直接返回该字面量值。

该函数返回这棵树最终的求值结果。由于它是递归的，因此会
逐个子子树地计算出整棵树的最终值。

## 转向汇编代码生成

我们要写一个通用汇编代码生成器。
它又会调用一组 CPU 特定的代码生成函数。

下面是 `gen.c` 中的通用汇编代码生成器：

```c
// Given an AST, generate
// assembly code recursively
static int genAST(struct ASTnode *n) {
  int leftreg, rightreg;

  // Get the left and right sub-tree values
  if (n->left) leftreg = genAST(n->left);
  if (n->right) rightreg = genAST(n->right);

  switch (n->op) {
    case A_ADD:      return (cgadd(leftreg,rightreg));
    case A_SUBTRACT: return (cgsub(leftreg,rightreg));
    case A_MULTIPLY: return (cgmul(leftreg,rightreg));
    case A_DIVIDE:   return (cgdiv(leftreg,rightreg));
    case A_INTLIT:   return (cgload(n->intvalue));

    default:
      fprintf(stderr, "Unknown AST operator %d\n", n->op);
      exit(1);
  }
}
```

看起来很熟悉，对吧？！我们做的是同样的深度优先树遍历。
这次不同的是：

  + A_INTLIT：将字面量值加载到寄存器中
  + 其他运算符：对保存左子节点和右子节点值的两个寄存器执行数学运算

`genAST()` 中的代码传递的是寄存器标识符，而不是传递值。例如 `cgload()` 将一个值加载到寄存器中，
并返回持有该加载值的寄存器的标识。

`genAST()` 本身返回在当前树节点处保存最终值的寄存器的标识。这就是为什么顶部的代码
在获取寄存器标识：

```c
  if (n->left) leftreg = genAST(n->left);
  if (n->right) rightreg = genAST(n->right);
```

## 调用 `genAST()`

`genAST()` 只负责计算给定表达式的值。我们需要打印出这个最终计算结果。
我们还需要用一些前导代码（*前导代码*）和一些结尾代码（*结尾代码*）来包裹我们生成的汇编代码。
这由 `gen.c` 中的另一个函数完成：

```c
void generatecode(struct ASTnode *n) {
  int reg;

  cgpreamble();
  reg= genAST(n);
  cgprintint(reg);      // Print the register with the result as an int
  cgpostamble();
}
```

## x86-64 代码生成器

通用代码生成器已经搞定了。现在我们需要研究如何生成一些真正的汇编代码。
目前，我以 x86-64 CPU 为目标，因为这是最常见的 Linux 平台之一。
现在，打开 `cg.c` 让我们开始浏览。

### 分配寄存器

任何 CPU 的寄存器数量都有限。我们必须分配一个寄存器来保存整数字面量值，
以及我们对它们执行的任何运算。然而，一旦用完一个值，通常可以丢弃该值，
从而释放保存它的寄存器。然后我们就可以将该寄存器重用于另一个值。

有三个函数处理寄存器分配：

  + `freeall_registers()`：将所有寄存器设置为可用状态
  + `alloc_register()`：分配一个空闲寄存器
  + `free_register()`：释放已分配的寄存器

我不打算详细讲解这些代码，因为它们很简单，只是有一些错误检查。
目前，如果寄存器用完了，程序会崩溃。稍后，我会处理寄存器耗尽的情况。

代码操作的是通用寄存器：r0、r1、r2 和 r3。有一个包含实际寄存器名称的字符串表：

```c
static char *reglist[4]= { "%r8", "%r9", "%r10", "%r11" };
```

这使得这些函数与 CPU 架构基本无关。

### 加载寄存器

这在 `cgload()` 中完成：分配一个寄存器，然后用 `movq`
指令将字面量值加载到分配的寄存器中。

```c
// Load an integer literal value into a register.
// Return the number of the register
int cgload(int value) {

  // Get a new register
  int r= alloc_register();

  // Print out the code to initialise it
  fprintf(Outfile, "\tmovq\t$%d, %s\n", value, reglist[r]);
  return(r);
}
```

### 两个寄存器相加

`cgadd()` 接受两个寄存器编号并生成将它们相加的代码。
结果保存在两个寄存器中的一个，然后另一个被释放供以后使用：

```c
// Add two registers together and return
// the number of the register with the result
int cgadd(int r1, int r2) {
  fprintf(Outfile, "\taddq\t%s, %s\n", reglist[r1], reglist[r2]);
  free_register(r1);
  return(r2);
}
```

注意加法是*可交换的*，所以我也可以用 `r2` 加到 `r1` 上，而不是 `r1` 到 `r2`。
返回的是保存最终值的寄存器的标识。

### 两个寄存器相乘

这与加法非常相似，而且运算同样是*可交换的*，所以可以返回任一寄存器：

```c
// Multiply two registers together and return
// the number of the register with the result
int cgmul(int r1, int r2) {
  fprintf(Outfile, "\timulq\t%s, %s\n", reglist[r1], reglist[r2]);
  free_register(r1);
  return(r2);
}
```

### 两个寄存器相减

减法是*不可交换的*：我们必须得到正确的顺序。
第二个寄存器被从第一个寄存器中减去，所以我们返回第一个并释放第二个：

```c
// Subtract the second register from the first and
// return the number of the register with the result
int cgsub(int r1, int r2) {
  fprintf(Outfile, "\tsubq\t%s, %s\n", reglist[r2], reglist[r1]);
  free_register(r2);
  return(r1);
}
```

### 两个寄存器相除

除法也是不可交换的，所以前面的说明同样适用。在
x86-64 上，这甚至更复杂。我们需要将 `%rax`
加载上来自 `r1` 的*被除数*。这需要用 `cqo` 扩展到八
字节。然后，`idivq` 用 `r2` 中的*除数*除以 `%rax`，
将*商*留在 `%rax` 中，所以我们需要把它复制到 `r1` 或 `r2`。
然后我们可以释放另一个寄存器。

```c
// Divide the first register by the second and
// return the number of the register with the result
int cgdiv(int r1, int r2) {
  fprintf(Outfile, "\tmovq\t%s,%%rax\n", reglist[r1]);
  fprintf(Outfile, "\tcqo\n");
  fprintf(Outfile, "\tidivq\t%s\n", reglist[r2]);
  fprintf(Outfile, "\tmovq\t%%rax,%s\n", reglist[r1]);
  free_register(r2);
  return(r1);
}
```

### 打印寄存器

没有 x86-64 指令可以将寄存器作为十进制数打印出来。
为了解决这个问题，汇编前导代码包含一个名为 `printint()` 的函数，
它接受一个寄存器参数并调用 `printf()` 将其打印为十进制数。

我不打算给出 `cgpreamble()` 中的代码，但它也包含 `main()` 的开头代码，
这样我们就可以将输出文件汇编成一个完整的程序。同样，这里也不给出
`cgpostamble()` 的代码，它只是调用 `exit(0)` 来结束程序。

不过这里是 `cgprintint()`：

```c
void cgprintint(int r) {
  fprintf(Outfile, "\tmovq\t%s, %%rdi\n", reglist[r]);
  fprintf(Outfile, "\tcall\tprintint\n");
  free_register(r);
}
```

Linux x86-64 期望函数的第一个参数放在 `%rdi` 寄存器中，
所以我们在调用 `printint` 之前将我们的寄存器移到 `%rdi`。

## 进行第一次编译

x86-64 代码生成器差不多就这些了。`main()` 中还有一些额外的代码
用于将 `out.s` 打开作为输出文件。我还保留了程序中的解释器，
这样我们就可以确认我们的汇编计算出的答案与解释器对输入表达式计算出的答案一致。

让我们编译编译器并在 `input01` 上运行：

```make
$ make
cc -o comp1 -g cg.c expr.c gen.c interp.c main.c scan.c tree.c

$ make test
./comp1 input01
15
cc -o out out.s
./out
15
```

是的！第一个 15 是解释器的输出。第二个 15 是汇编的输出。

## 检查汇编输出

那么，汇编输出到底是什么呢？输入文件是这样的：

```
2 + 3 * 5 - 8 / 3
```

这是针对此输入的 `out.s`（含注释）：

```
        .text                           # Preamble code
.LC0:
        .string "%d\n"                  # "%d\n" for printf()
printint:
        pushq   %rbp
        movq    %rsp, %rbp              # Set the frame pointer
        subq    $16, %rsp
        movl    %edi, -4(%rbp)
        movl    -4(%rbp), %eax          # Get the printint() argument
        movl    %eax, %esi
        leaq    .LC0(%rip), %rdi        # Get the pointer to "%d\n"
        movl    $0, %eax
        call    printf@PLT              # Call printf()
        nop
        leave                           # and return
        ret

        .globl  main
        .type   main, @function
main:
        pushq   %rbp
        movq    %rsp, %rbp              # Set the frame pointer
                                        # End of preamble code

        movq    $2, %r8                 # %r8 = 2
        movq    $3, %r9                 # %r9 = 3
        movq    $5, %r10                # %r10 = 5
        imulq   %r9, %r10               # %r10 = 3 * 5 = 15
        addq    %r8, %r10               # %r10 = 2 + 15 = 17
                                        # %r8 and %r9 are now free again
        movq    $8, %r8                 # %r8 = 8
        movq    $3, %r9                 # %r9 = 3
        movq    %r8,%rax
        cqo                             # Load dividend %rax with 8
        idivq   %r9                     # Divide by 3
        movq    %rax,%r8                # Store quotient in %r8, i.e. 2
        subq    %r8, %r10               # %r10 = 17 - 2 = 15
        movq    %r10, %rdi              # Copy 15 into %rdi in preparation
        call    printint                # to call printint()

        movl    $0, %eax                # Postamble: call exit(0)
        popq    %rbp
        ret
```

太棒了！我们现在有了一个真正的编译器：一个接受
一种语言的输入并生成该输入的另一种语言翻译的程序。

我们仍然需要将输出汇编成机器码并与支持库链接，
但这是我们现在可以手动执行的事情。以后，
我们会写一些代码来自动完成这项工作。

## 结论与下一步

从解释器转变为通用代码生成器很简单，但随后
我们必须编写一些代码来生成真正的汇编输出。为了做到这一点，
我们必须考虑如何分配寄存器：目前，我们有一个简单的解决方案。
我们还必须处理一些 x86-64 的特殊问题，比如 `idivq` 指令。

我还没有涉及到的一个问题是：为什么要费心为表达式生成抽象语法树？
当然，我们可以在 Pratt 解析器中遇到 '+' token 时直接调用 `cgadd()`，
其他运算符也是如此。我打算让你自己思考这个问题，
但我会在后面的步骤中回过头来讨论。

在编译器编写的下一步中，我们将为我们的语言添加一些语句，
使其开始看起来像一门真正的计算机语言。[下一步](../05_Statements/Readme_zh.md)
