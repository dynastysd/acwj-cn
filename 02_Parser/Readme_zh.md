# 第 2 章：解析简介

在本章中，我将介绍解析器的基础知识。正如我在第一章提到的，解析器的工作是识别输入的语法和结构元素，并确保它们符合语言的**语法**。

我们已经有了几个可以扫描的语言元素，即我们的 token：

 + 四个基本数学运算符：`*`、`/`、`+` 和 `-`
 + 十进制整数，即包含 1 个或多个数字 `0` .. `9` 的数

现在让我们为我们解析器将识别的语言定义一个语法。

## BNF：巴科斯-诺尔范式

如果你接触计算机语言，就会遇到
[BNF](https://en.wikipedia.org/wiki/Backus%E2%80%93Naur_form)
的用法。我在这里只介绍足够的 BNF 语法来表达我们想要识别的语法。

我们需要一个表示整数数学表达式的语法。以下是语法的 BNF 描述：

```
expression: number
          | expression '*' expression
          | expression '/' expression
          | expression '+' expression
          | expression '-' expression
          ;

number:  T_INTLIT
         ;
```

竖线分隔语法中的选项，所以上面说的是：

  + 一个表达式可以只是一个数字，或者
  + 一个表达式是由 '*' token 分隔的两个表达式，或者
  + 一个表达式是由 '/' token 分隔的两个表达式，或者
  + 一个表达式是由 '+' token 分隔的两个表达式，或者
  + 一个表达式是由 '-' token 分隔的两个表达式
  + 一个数字总是一个 T_INTLIT token

很明显，语法的 BNF 定义是**递归的**：表达式通过引用其他表达式来定义。但是有一种方法可以**终结**递归：当一个表达式是一个数字时，它总是一个 T_INTLIT token，因此不是递归的。

在 BNF 中，我们说 "expression" 和 "number" 是**非终结符**，因为它们是由语法中的规则产生的。然而，T_INTLIT 是一个**终结符**，因为它不是由任何规则定义的。相反，它是语言中已经识别的 token。同样，四个数学运算符 token 也是终结符号。
 
## 递归下降解析

鉴于我们语言的语法是递归的，我们尝试递归地解析它是有道理的。我们需要做的是读取一个 token，然后**向前看**下一个 token。根据下一个 token 是什么，我们可以决定需要走哪条路来解析输入。这可能需要我们递归调用一个已经被调用的函数。

在我们的情况下，任何表达式中的第一个 token 将是一个数字，后面可能跟着数学运算符。之后可能只有一个数字，或者可能是一个全新表达式的开始。我们如何递归地解析它？

我们可以写出如下伪代码：

```
function expression() {
  扫描并检查第一个 token 是一个数字。如果不是则报错
  获取下一个 token
  如果已到达输入末尾，则返回，即基本情况

  否则，调用 expression()
}
```

让我们在输入 `2 + 3 - 5 T_EOF` 上运行这个函数，其中 `T_EOF` 是反映输入结束的 token。我会给每次 `expression()` 调用编号。

```
expression0:
  扫描进 2，是一个数字
  获取下一个 token，+，不是 T_EOF
  调用 expression()

    expression1:
      扫描进 3，是一个数字
      获取下一个 token，-，不是 T_EOF
      调用 expression()

        expression2:
          扫描进 5，是一个数字
          获取下一个 token，T_EOF，所以从 expression2 返回

      从 expression1 返回
  从 expression0 返回
```

是的，这个函数能够递归解析输入 `2 + 3 - 5 T_EOF`。

当然，我们还没有对输入做任何事情，但这不是解析器的工作。解析器的工作是**识别**输入，并警告任何语法错误。其他人才会去对输入做**语义分析**，即理解和执行这个输入的含义。

> 之后你会看到，实际上这并不完全正确。将语法分析和语义分析交织在一起通常是有意义的。

## 抽象语法树

为了进行语义分析，我们需要代码来解释已识别的输入，或者将其翻译成另一种格式，例如汇编代码。在本章的旅程中，我们将为输入构建一个解释器。但要做到这一点，我们首先要将输入转换为[抽象语法树](https://en.wikipedia.org/wiki/Abstract_syntax_tree)，也称为 AST。

我强烈建议你阅读这个关于 AST 的简短解释：

 + [用 AST 提升解析水平](https://medium.com/basecs/leveling-up-ones-parsing-game-with-asts-d7a6fc2400ff)
   作者：Vaidehi Joshi

它写得很好，真的有助于解释 AST 的目的和结构。别担心，我会在你回来的时候在这里等你。

我们将要构建的 AST 中每个节点的结构在 `defs.h` 中描述：

```c
// AST 节点类型
enum {
  A_ADD, A_SUBTRACT, A_MULTIPLY, A_DIVIDE, A_INTLIT
};

// 抽象语法树结构
struct ASTnode {
  int op;                               // 在这个树上执行的"操作"
  struct ASTnode *left;                 // 左右子树
  struct ASTnode *right;
  int intvalue;                         // 对于 A_INTLIT，整数值
};
```

一些 AST 节点，如 `op` 值为 `A_ADD` 和 `A_SUBTRACT` 的节点，有两个子 AST，分别由 `left` 和 `right` 指向。稍后，我们将添加或减去子树的值。

或者，`op` 值为 A_INTLIT 的 AST 节点表示一个整数值。它没有子树子节点，只有一个在 `intvalue` 字段中的值。

## 构建 AST 节点和树

`tree.c` 中的代码有构建 AST 的函数。最通用的函数 `mkastnode()` 接受 AST 节点所有四个字段的值。它分配节点，填充字段值，并返回一个指向该节点的指针：

```c
// 构建并返回一个通用的 AST 节点
struct ASTnode *mkastnode(int op, struct ASTnode *left,
                          struct ASTnode *right, int intvalue) {
  struct ASTnode *n;

  // Malloc 一个新的 ASTnode
  n = (struct ASTnode *) malloc(sizeof(struct ASTnode));
  if (n == NULL) {
    fprintf(stderr, "Unable to malloc in mkastnode()\n");
    exit(1);
  }
  // 复制字段值并返回它
  n->op = op;
  n->left = left;
  n->right = right;
  n->intvalue = intvalue;
  return (n);
}
```

有了这个，我们可以编写更具体的函数来创建一个叶子 AST 节点（即没有子节点的节点），以及创建一个具有单个子节点的 AST 节点：

```c
// 创建一个 AST 叶子节点
struct ASTnode *mkastleaf(int op, int intvalue) {
  return (mkastnode(op, NULL, NULL, intvalue));
}

// 创建一个一元 AST 节点：只有一个子节点
struct ASTnode *mkastunary(int op, struct ASTnode *left, int intvalue) {
  return (mkastnode(op, left, NULL, intvalue));
```

## AST 的用途

我们将使用 AST 来存储我们识别的每个表达式，这样稍后我们可以递归遍历它来计算表达式的最终值。我们确实需要处理数学运算符的优先级。举个例子。

考虑表达式 `2 * 3 + 4 * 5`。现在，乘法的优先级高于加法。因此，我们希望将乘法操作数**绑定**在一起，并在做加法之前执行这些操作。

如果我们生成的 AST 树如下：

```
          +
         / \
        /   \
       /     \
      *       *
     / \     / \
    2   3   4   5
```

那么，在遍历树时，我们会首先执行 `2*3`，然后执行 `4*5`。一旦有了这些结果，我们就可以将它们传递到树的根节点来执行加法。

## 一个朴素的表达式解析器

现在，我们可以重用扫描器的 token 值作为 AST 节点操作值，但我喜欢将 token 和 AST 节点的概念分开。所以，首先，我将有一个函数将 token 值映射到 AST 节点操作值。这与解析器的其余部分一起在 `expr.c` 中：

```c
// 将一个 token 转换为一个 AST 操作。
int arithop(int tok) {
  switch (tok) {
    case T_PLUS:
      return (A_ADD);
    case T_MINUS:
      return (A_SUBTRACT);
    case T_STAR:
      return (A_MULTIPLY);
    case T_SLASH:
      return (A_DIVIDE);
    default:
      fprintf(stderr, "unknown token in arithop() on line %d\n", Line);
      exit(1);
  }
}
```

switch 语句中的 default 语句在我们无法将给定 token 转换为 AST 节点类型时触发。它将成为我们解析器中语法检查的一部分。

我们需要一个函数来检查下一个 token 是否是整数字面量，并构建一个 AST 节点来保存该字面量值。以下是它：

```c
// 解析一个主因子并返回代表它的 AST 节点。
static struct ASTnode *primary(void) {
  struct ASTnode *n;

  // 对于 INTLIT token，为它创建一个叶子 AST 节点，
  // 并扫描下一个 token。否则，对于任何其他 token 类型，
  // 这是一个语法错误。
  switch (Token.token) {
    case T_INTLIT:
      n = mkastleaf(A_INTLIT, Token.intvalue);
      scan(&Token);
      return (n);
    default:
      fprintf(stderr, "syntax error on line %d\n", Line);
      exit(1);
  }
}
```

这假定有一个全局变量 `Token`，它已经保存了从输入中扫描到的最新 token。在 `data.h` 中：

```c
extern_ struct token    Token;
```

在 `main()` 中：

```c
  scan(&Token);                 // 从输入中获取第一个 token
  n = binexpr();                // 解析文件中的表达式
```

现在我们可以编写解析器的代码了：

```c
// 返回一个以二元运算符为根的 AST 树
struct ASTnode *binexpr(void) {
  struct ASTnode *n, *left, *right;
  int nodetype;

  // 获取左边的整数字面量。
  // 同时获取下一个 token。
  left = primary();

  // 如果没有剩余 token，仅返回左节点
  if (Token.token == T_EOF)
    return (left);

  // 将 token 转换为一个节点类型
  nodetype = arithop(Token.token);

  // 获取下一个 token
  scan(&Token);

  // 递归获取右边的树
  right = binexpr();

  // 现在用两个子树构建一棵树
  n = mkastnode(nodetype, left, right, 0);
  return (n);
}
```

请注意，在这个朴素的解析器代码中，没有任何地方处理不同的运算符优先级。就目前而言，代码将所有运算符视为具有相同优先级。如果你按照代码解析表达式 `2 * 3 + 4 * 5`，你会看到它构建了这样的 AST：

```
     *
    / \
   2   +
      / \
     3   *
        / \
       4   5
```

这绝对是错误的。它会将 `4*5` 相乘得到 20，然后做 `3+20` 得到 23，而不是做 `2*3` 得到 6。

那我为什么要这样做呢？我想向你展示，编写一个简单的解析器很容易，但让它同时进行语义分析就难了。

## 解释树

现在我们有了（不正确的）AST 树，让我们编写一些代码来解释它。同样，我们将编写递归代码来遍历树。以下是伪代码：

```
interpretTree:
  首先，解释左边的子树并获取它的值
  然后，解释右边的子树并获取它的值
  对我们树根节点上的两个子树的节点执行操作，
  并返回这个值
```

回到正确的 AST 树：

```
          +
         / \
        /   \
       /     \
      *       *
     / \     / \
    2   3   4   5
```

调用结构如下：

```
interpretTree0(带有 + 的树):
  调用 interpretTree1(带有 * 的左树):
     调用 interpretTree2(带有 2 的树):
       没有数学运算，只返回 2
     调用 interpretTree3(带有 3 的树):
       没有数学运算，只返回 3
     执行 2 * 3，返回 6

  调用 interpretTree1(带有 * 的右树):
     调用 interpretTree2(带有 4 的树):
       没有数学运算，只返回 4
     调用 interpretTree3(带有 5 的树):
       没有数学运算，只返回 5
     执行 4 * 5，返回 20

  执行 6 + 20，返回 26
```

## 解释树的代码

这在 `interp.c` 中，遵循上述伪代码：

```c
// 给定一个 AST，解释其中的
// 运算符并返回
// 一个最终值。
int interpretAST(struct ASTnode *n) {
  int leftval, rightval;

  // 获取左右子树的值
  if (n->left)
    leftval = interpretAST(n->left);
  if (n->right)
    rightval = interpretAST(n->right);

  switch (n->op) {
    case A_ADD:
      return (leftval + rightval);
    case A_SUBTRACT:
      return (leftval - rightval);
    case A_MULTIPLY:
      return (leftval * rightval);
    case A_DIVIDE:
      return (leftval / rightval);
    case A_INTLIT:
      return (n->intvalue);
    default:
      fprintf(stderr, "Unknown AST operator %d\n", n->op);
      exit(1);
  }
}
```
    
同样，switch 语句中的 default 语句在我们无法解释 AST 节点类型时触发。它将成为我们解析器中语义检查的一部分。

## 构建解析器

这里还有一些其他代码，如 `main()` 中对解释器的调用：

```c
  scan(&Token);                 // 从输入中获取第一个 token
  n = binexpr();                // 解析文件中的表达式
  printf("%d\n", interpretAST(n));      // 计算最终结果
  exit(0);
```

你现在可以通过执行以下命令来构建解析器：

```
$ make
cc -o parser -g expr.c interp.c main.c scan.c tree.c
```

我为你们提供了几个输入文件来测试解析器，当然你也可以创建自己的。请记住，计算结果是错误的，但解析器应该能检测到输入错误，如连续的数字、连续的运算符，以及输入末尾缺少数字。我还在解释器中添加了一些调试代码，这样你可以看到 AST 树节点按什么顺序求值：

```
$ cat input01
2 + 3 * 5 - 8 / 3

$ ./parser input01
int 2
int 3
int 5
int 8
int 3
8 / 3
5 - 2
3 * 3
2 + 9
11

$ cat input02
13 -6+  4*
5
       +
08 / 3

$ ./parser input02
int 13
int 6
int 4
int 5
int 8
int 3
8 / 3
5 + 2
4 * 7
6 + 28
13 - 34
-21

$ cat input03
12 34 + -56 * / - - 8 + * 2

$ ./parser input03
unknown token in arithop() on line 1

$ cat input04
23 +
18 -
45.6 * 2
/ 18

$ ./parser input04
Unrecognised character . on line 3

$ cat input05
23 * 456abcdefg

$ ./parser input05
Unrecognised character a on line 1
```

## 结论与下一步

解析器识别语言的语法，并检查编译器的输入是否符合这个语法。如果不符合，解析器应该打印错误消息。由于我们的表达式语法是递归的，我们选择编写一个递归下降解析器来识别我们的表达式。

目前解析器工作正常，如上述输出所示，但它未能正确获取输入的语义。换句话说，它不能正确计算表达式的值。

在我们编译器编写的下一步中，我们将修改解析器，使其也进行表达式的语义分析，以获得正确的数学结果。[下一步](../03_Precedence/Readme_zh.md)
