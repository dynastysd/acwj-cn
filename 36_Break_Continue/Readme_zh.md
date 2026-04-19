# 第 36 章：`break` 和 `continue` 语句

之前我写另一个针对无类型语言的简单编译器时，没有使用抽象语法树。这使得在语言中添加 `break` 和 `continue` 关键字变得很别扭。

在这里，每个函数都有对应的 AST 树。这使得实现 `break` 和 `continue` 变得容易多了。我将在下面说明其中的原因。

## 添加 `break` 和 `continue`

毫无疑问，我们有两个新的词法单元 T_BREAK 和 T_CONTINUE，`scan.c` 中的扫描器代码能够识别 `break` 和 `continue` 关键字。和往常一样，浏览代码以了解具体实现方式。

## 新的 AST 节点类型

我们在 `defs.h` 中也有两个新的 AST 节点类型：A_BREAK 和 A_CONTINUE。当我们解析 `break` 关键字时，可以生成一个 A_BREAK AST 叶子节点；对于 `continue` 关键字同理生成 A_CONTINUE 叶子节点。

然后，当我们遍历 AST 生成汇编代码时，遇到 A_BREAK 节点时需要生成一个汇编跳转，跳转到当前所在循环末尾的标签。对于 A_CONTINUE，则跳转到循环条件求值之前的标签。

现在，我们怎么知道当前处于哪个循环之中呢？

## 追踪最近的循环

循环可以嵌套，因此在任何时刻都可能使用任意数量的循环标签。这正是我之前写编译器时遇到困难的地方。现在我们有了可以递归遍历的 AST，可以将最新循环的标签细节传递给 AST 树中的子节点。

我们已经在处理 'if' 或 'while' 语句时做过类似的传递。下面是 `gen.c` 中生成 'if' 语句汇编代码的部分代码：

```c
// Generate the code for an IF statement
// and an optional ELSE clause
static int genIF(struct ASTnode *n) {
  int Lfalse, Lend;

  // Generate two labels: one for the
  // false compound statement, and one
  // for the end of the overall IF statement.
  Lfalse = genlabel();
  Lend = genlabel();

  // Generate the condition code followed
  // by a jump to the false label.
  genAST(n->left, Lfalse, n->op);
```

左侧的 AST 子节点是求值 'if' 语句条件的那一个，因此它需要访问我们刚刚生成的标签。所以当我们用 `genAST()` 生成该子节点的汇编输出时，也传递了标签的详细信息。

对于循环，我们需要传递给 `genAST()` 循环末尾的标签，以及循环条件求值代码之前的标签。为此，我修改了 `genAST()` 的接口：

```c
int genAST(struct ASTnode *n, int iflabel, int looptoplabel,
           int loopendlabel, int parentASTop);
```

我们保留现有的 `iflabel`，并增加两个循环标签。现在我们需要将为每个循环生成的标签传递给 `genAST()`。因此，在生成 'while' 循环代码的代码中：

```c
static int genWHILE(struct ASTnode *n) {
  int Lstart, Lend;

  // Generate the start and end labels
  Lstart = genlabel();
  Lend = genlabel();

  // Generate the condition code followed
  // by a jump to the end label.
  genAST(n->left, Lend, Lstart, Lend, n->op);

  // Generate the compound statement for the body
  genAST(n->right, NOLABEL, Lstart, Lend, n->op);
  ...
}
```

## `genAST()` 是递归的

现在，嵌套循环的情况如何？考虑以下代码：

```
L1:
  while (x < 10) {
    if (x == 6) break;
L2:
    while (y < 10) {
      if (y == 6) break;
      y++;
    }
L3:
    x++;
  }
L4:
```

`if (y == 6) break` 应该跳出内层循环并跳转到 `x++` 代码处（即 L3），而 `if (x == 6) break;` 代码应该跳出外层循环并跳转到标签 L4。

这是可行的，因为 `genAST()` 为外层循环调用 `genWHILE()`。这会调用 `genAST(L1, L4)` 使得第一个 `break` 能看到这些循环标签。然后，当我们遇到第二个循环时，会再次调用 `genWHILE()`。它生成新的循环标签并调用 `genAST(L2, L3)` 来生成内层循环代码。因此，第二个 `break` 看到的是 L2 和 L3 标签，而不是 L1 和 L4。

最后，一旦内层复合语句生成完毕，内部的 `genAST()` 返回，回到看到 L1 和 L4 作为循环标签的代码。

## 上述内容的含义

就实现而言，这意味着任何调用 `genAST()` 的地方（包括它自己），只要可能处于循环中，就必须将当前的循环标签向下传播到所涉及的子节点。

我们已经看到了对 `genWHILE()` 的修改，将新的循环标签传递给 `genAST()`。让我们看看还需要在哪里传播循环标签。

当我第一次实现 `break` 时，写了这样一个测试程序

```c
int main() {
  int x;
  x = 0;
  while (x < 100) {
    printf("%d\n", x);
    if (x == 14) { break; }
    x = x + 1;
  }
  printf("Done\n");
```

并为它生成了汇编代码。`break` 被转换成了跳转到标签 L0 的指令，也就是说循环的结束标签没有传递给处理 `break` 的代码。查看编译器的堆栈跟踪，我意识到：

  + 函数的 `genAST()` 调用了
  + 为循环生成标签并将它们传递给
  + 循环体的 `genAST()`，它又调用了
  + `genIF()`，后者没有向
  + 'if'体的 `genAST()` 传递任何标签。因此，`break` 从未看到过这些标签。

所以我还必须修改 `genIF()` 的参数列表：

```c
static int genIF(struct ASTnode *n, int looptoplabel, int loopendlabel);
```

我不会遍历 `gen.c` 中的所有代码，但请在编辑器或文本查看器中打开该文件，查找所有的 `genAST()` 调用，看看循环标签是在哪里传播的。

最后，我们确实需要为 `break` 和 `continue` 生成汇编代码。以下是 `gen.c` 中 `genAST()` 里实现这一功能的代码：

```c
    case A_BREAK:
      cgjump(loopendlabel);
      return (NOREG);
    case A_CONTINUE:
      cgjump(looptoplabel);
      return (NOREG);
```

## 解析 `break` 和 `continue`

这次我先介绍了代码生成方面再讲解析，但现在该讲解析这些新关键字了。幸运的是语法很简单，要么是 `break ;`，要么是 `continue ;`。所以看起来应该很容易解析。当然，这里有一个小问题。

我们在 `stmt.c` 的 `single_statement()` 中解析单个语句，所以改动很小：

```c
    case T_BREAK:
      return (break_statement());
    case T_CONTINUE:
      return (continue_statement());
```

在 `compound_statement()` 中做了小幅修改，以确保语句后面跟着分号：

```c
compound_statement(void) {
  struct ASTnode *left = NULL;
  struct ASTnode *tree;

  ...
  while (1) {
    // Parse a single statement
    tree = single_statement();

    // Some statements must be followed by a semicolon
    if (tree != NULL && (tree->op == A_ASSIGN || tree->op == A_RETURN
                         || tree->op == A_FUNCCALL || tree->op == A_BREAK
                         || tree->op == A_CONTINUE))
      semi();
    ...
}
```

现在来说那个小问题。下面的程序是不合法的：

```c
int main() {
  break;
}
```

因为这里没有可跳出的循环。我们需要追踪正在解析的循环深度，只在深度不为零时才允许 `break` 或 `continue` 语句。因此，解析这些关键字的函数如下：

```c
// Parse a break statement and return its AST
static struct ASTnode *break_statement(void) {

  if (Looplevel == 0)
    fatal("no loop to break out from");
  scan(&Token);
  return (mkastleaf(A_BREAK, 0, NULL, 0));
}

// continue_statement: 'continue' ;
//
// Parse a continue statement and return its AST
static struct ASTnode *continue_statement(void) {

  if (Looplevel == 0)
    fatal("no loop to continue to");
  scan(&Token);
  return (mkastleaf(A_CONTINUE, 0, NULL, 0));
}
```

## 循环层级

我们需要用一个 `Looplevel` 变量来追踪正在解析的循环层级。这在 `data.h` 中：

```c
extern_ int Looplevel;                  // Depth of nested loops
```

我们需要在需要的地方设置层级。每次开始一个新函数时，层级都设为零（在 `decl.c` 中）：

```c
// Parse the declaration of function.
struct ASTnode *function_declaration(int type) {
  ...
  // Get the AST tree for the compound statement and mark
  // that we have parsed no loops yet
  Looplevel= 0;
  tree = compound_statement();
  ...
}
```

现在，每次我们解析一个循环时，都会为循环体增加循环层级（在 `stmt.c` 中）：

```c
// Parse a WHILE statement and return its AST
static struct ASTnode *while_statement(void) {
  ...
  // Get the AST for the compound statement.
  // Update the loop depth in the process
  Looplevel++;
  bodyAST = compound_statement();
  Looplevel--;
  ...
}

// Parse a FOR statement and return its AST
static struct ASTnode *for_statement(void) {
  ...
  // Get the compound statement which is the body
  // Update the loop depth in the process
  Looplevel++;
  bodyAST = compound_statement();
  Looplevel--;
  ...
}
```

这样我们就有能力判断我们是在循环内部还是在循环外部。

## 测试代码

这是测试代码，`tests/input71.c`：

```c
#include <stdio.h>

int main() {
  int x;
  x = 0;
  while (x < 100) {
    if (x == 5) { x = x + 2; continue; }
    printf("%d\n", x);
    if (x == 14) { break; }
    x = x + 1;
  }
  printf("Done\n");
  return (0);
}
```

由于我还没有解决"悬空 else"问题，`break` 语句必须用 '{' ... '}' 括起来成为复合语句。除此之外，代码按预期工作：

```
0
1
2
3
4
7
8
9
10
11
12
13
14
Done
```

## 结论与下一步

我知道由于有 AST 的存在，添加对 `break` 和 `continue` 的支持会比之前的编译器容易。然而，在实现过程中仍然有一些小问题和细节需要处理。

既然语言中有了 `break` 关键字，我将在编译器编写旅程的下一部分尝试添加 `switch` 语句。这将需要添加 switch 跳转表，我知道这会很复杂。所以为下一个有趣的阶段做好准备吧。[下一步](../37_Switch/Readme_zh.md)