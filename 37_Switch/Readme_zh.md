# 第 37 章：switch 语句

在编译器编写的旅程的这一部分，我们来实现 `switch` 语句。这实际上非常棘手，有几个原因，我会一一说明。让我们从一个例子开始，看看它带来的一些影响。

## 一个 switch 语句的例子

```c
  switch(x) {
    case 1:  printf("One\n");  break;
    case 2:  printf("Two\n");  break;
    case 3:  printf("Three\n");
    default: printf("More than two\n");
  }
```

这就像一个多路 `if` 语句，`x` 的值决定选择哪个分支。然而，我们需要插入 `break` 语句来跳过所有其他分支；如果我们省略 `break` 语句，当前所在的分支会"穿透"并继续执行下一个分支的代码。

`switch` 决策表达式必须是整数类型，所有 case 选项也必须是整数字面量。例如，我们不能写 `case 3*y+17`。

`default` 分支捕获所有前面 case 没有处理的值。它必须出现在 case 列表的最后。此外，我们不能有重复的 case 值，所以 `case 2: ...; case 2` 是不允许的。

## 将上述例子转换为汇编代码

将 `switch` 语句翻译成汇编代码的一种方法是将其视为多路 `if` 语句。这意味着我们需要逐一比较 `x` 与整数值，根据需要进入或跳过相应的汇编代码区段。这虽然可行，但会使汇编代码效率低下，特别是当你考虑这个例子时：

```c
  switch (2 * x - (18 +y)/z) { ... }
```

根据我们当前编译器的"[KISS](https://en.wikipedia.org/wiki/KISS_principle)"原则，我们不得不在每次与字面量值比较时重复计算表达式。

更有意义的方法是只计算一次 `switch` 表达式。然后，将这个值与 case 字面量值的表格进行比较。当找到匹配时，跳转到与 case 值关联的代码分支。这被称为[跳转表](https://en.wikipedia.org/wiki/Jump_table)。

这意味着，对于每个 case 选项，我们都需要创建一个标签，放在该选项代码的开头。作为例子，上面第一个例子的跳转表可能如下所示：

| Case 值 | 标签 |
|:----------:|:-----:|
|     1      |  L18  |
|     2      |  L19  |
|     3      |  L22  |
|  default   |  L26  |

我们还需要一个标签来标记 `switch` 语句之后的代码。当某个代码分支想要执行 `break;` 时，就跳转到这个 switch 结束标签。否则，就让代码分支穿透到下一个代码分支。

## 解析的影响

以上都很不错，但我们必须从上到下解析 `switch` 语句。这意味着，在解析完所有 case 之后，我们才知道跳转表应该有多大。这也意味着，除非使用一些巧妙的技巧，否则我们会在生成跳转表之前就生成所有 case 的汇编代码。

如你所知，我编写这个编译器遵循的是"KISS 原则"：保持简单，笨蛋！所以我避免了那些巧妙的技巧，但这意味着，是的，我们将延迟跳转表的输出，直到生成完所有 case 的汇编代码之后。

从视觉上看，我们的代码布局是这样的：

![](Figs/switch_logic.png)

计算 switch 决策的代码在最上面，因为我们首先解析它。我们不想继续进入第一个 case，所以可以跳转到一个稍后输出的标签。

然后我们解析每个 case 语句并生成相应的汇编代码。我们已经生成了一个"switch 结束"标签，所以可以跳转到它。同样，我们稍后输出这个标签。

在生成每个 case 时，我们得到它的标签并输出这个标签。一旦所有 case 和 default 分支（如果有的话）都输出后，我们就可以生成跳转表了。

但是现在我们需要一些代码来遍历跳转表，将 switch 决策与每个 case 值进行比较，并相应地跳转。我们可以为每个 `switch` 语句生成这段汇编代码，但如果这段跳转处理代码很大，就会浪费内存。更好的做法是在内存中只保留一份跳转处理代码，但现在我们又必须跳转到它！更糟糕的是，这段代码不知道哪个寄存器保存着 switch 决策结果，所以我们必须将这个寄存器复制到一个已知寄存器，并将跳转表的基址复制到另一个已知寄存器。

我们在解析和代码生成中付出的复杂性代价，换来的是布满跳转的汇编代码。好在 CPU 可以处理这种跳转混乱，所以目前这是一个公平的权衡。显然，一个生产级别的编译器会采用不同的做法。

图中红线显示了执行流程：从 switch 决策到加载寄存器，再到跳转表处理，最后到具体的 case 代码。绿线表示跳转表的基址被传递给跳转表处理代码。最后，蓝线表示 case 以 `break;` 结束，跳转到 switch 汇编代码的末尾。

所以，汇编输出虽然丑陋但确实有效。现在我们已经看到了如何实现 `switch` 语句，让我们真正动手去做吧。

## 新的关键字和词法单元

我们有两个新的词法单元 T_CASE 和 T_DEFAULT，与新的 `case` 和 `default` 关键字一起使用。一如既往，浏览代码以了解这是如何实现的。

## 新的 AST 节点类型

我们需要构建 AST 树来表示 `switch` 语句。`switch` 语句的结构绝不是像表达式那样的二叉树。但这是我们自己的 AST 树，所以我们可以按照任何适合我们的方式来塑造它。所以我坐下来想了一会儿，决定采用这个结构：

![](Figs/switch_ast.png)

`switch` 树的根节点是 A_SWITCH，左边是计算 switch 条件的表达式子树。右边是一个 A_CASE 节点的链表，每个 case 一个。最后是一个可选的 A_DEFAULT 节点来捕获任何 default 分支。

每个 A_CASE 节点中的 `intvalue` 字段将保存 case 值，表达式必须与之匹配。左子子树将保存作为 case 体的复合语句的详细信息。此时，我们还没有任何跳转标签或跳转表：这些将在后面生成。

## 解析 switch 语句

有了以上基础，我们现在准备查看 `switch` 语句的解析。这里有相当多的错误检查代码，所以我将分小节来讲解。这段代码在 `stmt.c` 中，由 `single_statement()` 调用：

```c
    case T_SWITCH:
      return (switch_statement());
```

让我们开始吧。

```c
// Parse a switch statement and return its AST
static struct ASTnode *switch_statement(void) {
  struct ASTnode *left, *n, *c, *casetree= NULL, *casetail;
  int inloop=1, casecount=0;
  int seendefault=0;
  int ASTop, casevalue;

  // Skip the 'switch' and '('
  scan(&Token);
  lparen();

  // Get the switch expression, the ')' and the '{'
  left= binexpr(0);
  rparen();
  lbrace();

  // Ensure that this is of int type
  if (!inttype(left->type))
    fatal("Switch expression is not of integer type");
```

好的，函数顶部有很多局部变量，这应该让你知道我们必须在这个函数中处理一些状态。不过第一部分很简单：解析 `switch (expression) {` 语法，获取表达式的 AST 并确保它是整数类型。

```c
  // Build an A_SWITCH subtree with the expression as
  // the child
  n= mkastunary(A_SWITCH, 0, left, NULL, 0);

  // Now parse the cases
  Switchlevel++;
```

我们有了 switch 决策树，所以现在可以构建 A_SWITCH 节点并返回。你还记得，我们只能在一个至少循环内允许 `break;`。好吧，现在我们还必须在至少一个 `switch` 语句中也允许 `break;` 发生。因此，有一个新的全局变量 `Switchlevel` 来记录这一点。

```c
  // Now parse the cases
  Switchlevel++;
  while (inloop) {
    switch(Token.token) {
      // Leave the loop when we hit a '}'
      case T_RBRACE: if (casecount==0)
                        fatal("No cases in switch");
                     inloop=0; break;
  ...
  }
```

循环由 `inloop` 控制，它初始值为 1。当遇到 '}' 词法单元时，将其重置为 0 并跳出这个 `switch` 语句，从而结束循环。我们还要检查是否至少看到一个 case。

> 用一个 `switch` 语句来解析 `switch` 语句有点奇怪。

现在我们继续解析 `case` 和 `default`：

```c
      case T_CASE:
      case T_DEFAULT:
        // Ensure this isn't after a previous 'default'
        if (seendefault)
          fatal("case or default after existing default");
```

两个词法单元有很多公共代码要执行，所以它们都进入相同的代码。首先，确保我们还没有看到 default case，而且这必须是系列中的最后一个 case。

```c
        // Set the AST operation. Scan the case value if required
        if (Token.token==T_DEFAULT) {
          ASTop= A_DEFAULT; seendefault= 1; scan(&Token);
        } else ...
```

如果正在解析 `default:`，则后面没有整数。跳过关键字并记录我们已看到 default case。

```c
        } else  {
          ASTop= A_CASE; scan(&Token);
          left= binexpr(0);
          // Ensure the case value is an integer literal
          if (left->op != A_INTLIT)
            fatal("Expecting integer literal for case value");
          casevalue= left->intvalue;

          // Walk the list of existing case values to ensure
          // that there isn't a duplicate case value
          for (c= casetree; c != NULL; c= c -> right)
            if (casevalue == c->intvalue)
              fatal("Duplicate case value");
        }
```

这段代码专门处理 `case <value>:`。我们用 `binexpr()` 读取 case 后面的值。本来我可以"聪明地"调用 `primary()` 而不是 `binexpr()`，后者直接解析整数字面量。然而，`primary()` 也可以调用 `binexpr()`，所以实际上没有什么区别：我们仍然需要检查结果树以确保它只是一个 A_INTLIT 节点。

然后我们遍历已有的 A_CASE 节点链表（`casetree` 指向这个列表的头）以确保没有任何重复的 case 值。

在此过程中，我们将 `ASTop` 变量设置为 A_CASE（对于带整数字面量的 case）或 A_DEFAULT（对于 default case）。现在我们可以执行两者共同的代码。

```c
        // Scan the ':' and get the compound expression
        match(T_COLON, ":");
        left= compound_statement(); casecount++;

        // Build a sub-tree with the compound statement as the left child
        // and link it in to the growing A_CASE tree
        if (casetree==NULL) {
          casetree= casetail= mkastunary(ASTop, 0, left, NULL, casevalue);
        } else {
          casetail->right= mkastunary(ASTop, 0, left, NULL, casevalue);
          casetail= casetail->right;
        }
        break;
```

检查下一个词法单元是 ':'。获取包含复合语句的 AST 子树。用这个子树作为左子节点构建一个 A_CASE 或 A_DEFAULT 节点，并将其链接到 A_CASE/A_DEFAULT 节点的链表中：`casetree` 是这个链表的头，`casetail` 是尾。

```c
      default:
        fatald("Unexpected token in switch", Token.token);
    }
  }
```

`switch` 体中应该只有 `case` 和 `default` 关键字，所以要确保确实如此。

```c
  Switchlevel--;

  // We have a sub-tree with the cases and any default. Put the
  // case count into the A_SWITCH node and attach the case tree.
  n->intvalue= casecount;
  n->right= casetree;
  rbrace();

  return(n);
```

我们终于解析完了所有 case 和 default case，现在有了它们的计数和 `casetree` 指向的列表。将这些值添加到 A_SWITCH 节点并将其作为最终的树返回。

好的，解析工作已经相当多了。现在我们需要将注意力转向代码生成。

## switch 代码生成：一个例子

在这一点上，我认为值得看一下一个例子的汇编输出，这样你就能看到代码是如何与我上面给出的执行流程图相匹配的。以下是例子：

```c
#include <stdio.h>

int x; int y;

int main() {
  switch(x) {
    case 1:  { y= 5; break; }
    case 2:  { y= 7; break; }
    case 3:  { y= 9; }
    default: { y= 100; }
  }
  return(0);
}
```

首先，是的，我们确实需要在 case 体周围使用 '{' ... '}'。这是因为我还没有解决"悬空 else"问题，所以所有复合语句都必须用 '{' ... '}' 包围。

我现在先省略跳转表处理代码，但这里是例子的汇编输出：

![](Figs/switch_logic2.png)

将 `x` 加载到寄存器的代码在最上面，它跳转到跳转表之后。由于跳转表处理代码不知道这会是哪个寄存器，我们总是将值加载到 `%rax`，并将跳转表的基址加载到 `%rdx`。

跳转表本身的结构如下：

 + 首先是有整数值的 case 数量
 + 接下来是一组值/标签对，每个 case 一个
 + 最后是 default case 的标签。如果没有 default case，这必须是 switch 末尾的标签，这样如果没有匹配的 case 我们就不执行任何代码。

跳转表处理代码（我们稍后会看）解释跳转表并跳转到表中某个标签。假设我们跳转到 `L11` 即 `case 2:`。我们执行这个 case 选项的代码。这个选项有一个 `break;` 语句，所以会跳转到 `L9`，即 switch 语句末尾的标签。

## 跳转表处理代码

你已经知道 x86-64 汇编代码不是我的强项。因此，我直接从 [SubC](http://www.t3x.org/subc/) 借用了跳转表处理代码。我把它添加到了 `cg.c` 的 `cgpreamble()` 函数中，这样它就会在我们创建的每个汇编文件中输出。以下是带注释的代码：

```
# internal switch(expr) routine
# %rsi = switch table, %rax = expr

switch:
        pushq   %rsi            # Save %rsi
        movq    %rdx,%rsi       # Base of jump table -> %rsi
        movq    %rax,%rbx       # Switch value -> %rbx
        cld                     # Clear direction flag
        lodsq                   # Load count of cases into %rcx,
        movq    %rax,%rcx       # incrementing %rsi in the process
next:
        lodsq                   # Get the case value into %rdx
        movq    %rax,%rdx
        lodsq                   # and the label address into %rax
        cmpq    %rdx,%rbx       # Does switch value matches the case?
        jnz     no              # No, jump over this code
        popq    %rsi            # Restore %rsi
        jmp     *%rax           # and jump to the chosen case
no:
        loop    next            # Loop for the number of cases
        lodsq                   # Out of loop, load default label address
        popq    %rsi            # Restore %rsi
        jmp     *%rax           # and jump to the default case
```

我们要感谢 Nils Holm 编写了这个代码，因为我永远不可能写出这个代码！

现在我们可以看看上面的汇编代码是如何生成的。幸运的是，我们已经在 `cg.c` 中有很多有用的函数可以重用。

## 生成汇编代码

在 `gen.c` 的 `genAST()` 中，靠近顶部的地方我们识别出 A_SWITCH 节点并调用一个函数来处理这个节点及其下面的树。

```c
    case A_SWITCH:
      return (genSWITCH(n));
```

让我们分阶段看看这个新函数：

```c
// Generate the code for a SWITCH statement
static int genSWITCH(struct ASTnode *n) {
  int *caseval, *caselabel;
  int Ljumptop, Lend;
  int i, reg, defaultlabel = 0, casecount = 0;
  struct ASTnode *c;

  // Create arrays for the case values and associated labels.
  // Ensure that we have at least one position in each array.
  caseval = (int *) malloc((n->intvalue + 1) * sizeof(int));
  caselabel = (int *) malloc((n->intvalue + 1) * sizeof(int));
```

这里 `+1` 的原因是，我们可能有一个 default case 需要标签，即使它没有 case 值。

```c
  // Generate labels for the top of the jump table, and the
  // end of the switch statement. Set a default label for
  // the end of the switch, in case we don't have a default.
  Ljumptop = genlabel();
  Lend = genlabel();
  defaultlabel = Lend;
```

这些标签被创建但尚未作为汇编输出。在我们得到 default 标签之前，将其设置为 `Lend`。

```c
  // Output the code to calculate the switch condition
  reg = genAST(n->left, NOLABEL, NOLABEL, NOLABEL, 0);
  cgjump(Ljumptop);
  genfreeregs();
```

我们输出跳转到跳转表之后代码的代码，尽管跳转表还没有输出。此时我们也可以释放所有寄存器。

```c
  // Walk the right-child linked list to
  // generate the code for each case
  for (i = 0, c = n->right; c != NULL; i++, c = c->right) {

    // Get a label for this case. Store it
    // and the case value in the arrays.
    // Record if it is the default case.
    caselabel[i] = genlabel();
    caseval[i] = c->intvalue;
    cglabel(caselabel[i]);
    if (c->op == A_DEFAULT)
      defaultlabel = caselabel[i];
    else
      casecount++;

    // Generate the case code. Pass in the end label for the breaks
    genAST(c->left, NOLABEL, NOLABEL, Lend, 0);
    genfreeregs();
  }
```

这段代码既为每个 case 生成标签，也输出作为 case体的汇编代码。我们将 case 值和 case 标签存储在两个数组中。并且，如果是 default case，我们可以用正确的标签更新 `defaultlabel`。

还要注意，`genAST()` 接收到 `Lend`，这是我们 `switch` 代码之后的标签。这允许 case 体中的任何 `break;` 跳转到接下来的代码。

```c
  // Ensure the last case jumps past the switch table
  cgjump(Lend);

  // Now output the switch table and the end label.
  cgswitch(reg, casecount, Ljumptop, caselabel, caseval, defaultlabel);
  cglabel(Lend);
  return (NOREG);
}
```

我们不能依赖程序员用 `break;` 结束最后一个 case，所以我们强制最后一个 case 跳转到 switch 语句的末尾。

此时我们有了：

 + 保存 switch 值的寄存器
 + case 值数组
 + case 标签数组
 + case 数量
 + 一些有用的标签

我们将所有这些传递给 `cg.c` 中的 `cgswitch()`，并且（除了来自 SubC 的代码）这是我们在这部分需要引入的唯一新汇编代码。

## `cgswitch()`

在这里，我们需要构建跳转表并加载寄存器，以便跳转到 `switch` 汇编代码。提醒一下，跳转表结构如下：

 + 首先是有整数值的 case 数量
 + 接下来是一组值/标签对，每个 case 一个
 + 最后是 default case 的标签。如果没有 default case，这必须是 switch 末尾的标签，这样如果没有匹配的 case 我们就不执行任何代码。

对于我们的例子，跳转表如下：

```
L14:                                    # Switch jump table
        .quad   3                       # Three case values
        .quad   1, L10                  # case 1: jump to L10
        .quad   2, L11                  # case 2: jump to L11
        .quad   3, L12                  # case 3: jump to L12
        .quad   L13                     # default: jump to L13
```

这里是生成所有这些代码的方法。

```c
// Generate a switch jump table and the code to
// load the registers and call the switch() code
void cgswitch(int reg, int casecount, int toplabel,
              int *caselabel, int *caseval, int defaultlabel) {
  int i, label;

  // Get a label for the switch table
  label = genlabel();
  cglabel(label);
```

这就是上面的 `L14:`。

```c
  // Heuristic. If we have no cases, create one case
  // which points to the default case
  if (casecount == 0) {
    caseval[0] = 0;
    caselabel[0] = defaultlabel;
    casecount = 1;
  }
```

跳转表中必须至少有一个 case 值/标签对。这段代码创建一个指向 default case 的对。case 值是无关的：如果它匹配，很好。如果不匹配，我们还是会跳转到 default case。

```c
  // Generate the switch jump table.
  fprintf(Outfile, "\t.quad\t%d\n", casecount);
  for (i = 0; i < casecount; i++)
    fprintf(Outfile, "\t.quad\t%d, L%d\n", caseval[i], caselabel[i]);
  fprintf(Outfile, "\t.quad\tL%d\n", defaultlabel);
```

这里是生成跳转表的代码。简单明了。

```c
  // Load the specific registers
  cglabel(toplabel);
  fprintf(Outfile, "\tmovq\t%s, %%rax\n", reglist[reg]);
  fprintf(Outfile, "\tleaq\tL%d(%%rip), %%rdx\n", label);
  fprintf(Outfile, "\tjmp\tswitch\n");
}
```

最后，用 switch 值加载 `%rax` 寄存器，用跳转表的标签加载 `%rdx`，然后调用 `switch` 代码。

## 测试代码

我用一个循环增强了例子，以便测试 `switch` 语句中的所有 case。这是文件 `tests/input74.c`：

```c
#include <stdio.h>

int main() {
  int x;
  int y;
  y= 0;

  for (x=0; x < 5; x++) {
    switch(x) {
      case 1:  { y= 5; break; }
      case 2:  { y= 7; break; }
      case 3:  { y= 9; }
      default: { y= 100; }
    }
    printf("%d\n", y);
  }
  return(0);
}
```

程序输出如下：

```
100
5
7
100
100
```

注意值 9 没有被输出，因为当执行 case 3 时，我们穿透到了 default case。

## 结论与下一步

我们刚刚在编译器中实现了第一个真正的大型新语句：`switch` 语句。由于我以前从未这样做过，我基本上遵循了 SubC 的实现。有许多其他更高效的方法来实现 `switch`，但我在这里应用了"KISS 原则"。也就是说，这仍然是一个相当复杂的实现。

如果你还在读到这里，说明你的毅力惊人！

我开始对我们所有复合语句周围强制使用的 '{' ... '}' 感到厌烦。因此，在编译器编写旅程的下一部分，我将下定决心尝试解决"悬空 else"问题。[下一步](../38_Dangling_Else/Readme_zh.md)