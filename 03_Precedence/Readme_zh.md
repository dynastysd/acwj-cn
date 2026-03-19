# 第 3 章：运算符优先级

在编译器编写旅程的前一部分中，我们看到解析器并不一定强制执行语言的所有含义，它只强制执行语法和语法规则。

我们最终得到的代码会计算出诸如 `2 * 3 + 4 * 5` 这样的表达式错误值，因为代码生成的抽象语法树是这样的：

```
     *
    / \
   2   +
      / \
     3   *
        / \
       4   5
```

而不是正确的：

```
          +
         / \
        /   \
       /     \
      *       *
     / \     / \
    2   3   4   5
```

要解决这个问题，我们必须向解析器添加代码来执行运算符优先级。实现这一点至少有两种方法：

 + 在语言的语法中显式地表达运算符优先级
 + 用运算符优先级表影响现有的解析器

## 在语法中显式地表达运算符优先级

以下是我们旅程上一部分的语法：

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

注意，四个数学运算符之间没有区别。让我们调整语法来区分它们：

```
expression: additive_expression
    ;

additive_expression:
      multiplicative_expression
    | additive_expression '+' multiplicative_expression
    | additive_expression '-' multiplicative_expression
    ;

multiplicative_expression:
      number
    | number '*' multiplicative_expression
    | number '/' multiplicative_expression
    ;

number:  T_INTLIT
         ;
```

我们现在有两种类型的表达式：*加法*表达式和*乘法*表达式。注意，语法现在强制数字只能作为乘法表达式的一部分。这使得 `*` 和 `/` 运算符与两侧的数字结合得更紧密，从而具有更高的优先级。

任何加法表达式实际上要么是一个乘法表达式本身，要么是一个加法（即乘法）表达式后跟一个 `+` 或 `-` 运算符，然后再跟一个乘法表达式。加法表达式现在的优先级比乘法表达式低很多。

## 在递归下降解析器中实现上述方法

我们如何将上述版本的语法实现到递归下降解析器中？我在 `expr2.c` 文件中完成了这个实现，下面我将介绍代码。

答案是使用一个 `multiplicative_expr()` 函数来处理 `*` 和 `/` 运算符，以及一个 `additive_expr()` 函数来处理优先级较低的 `+` 和 `-` 运算符。

两个函数都会读取某个东西和一个运算符。然后，当有相同优先级的后续运算符时，每个函数都会继续解析输入，并用第一个运算符将左右两部分组合起来。

但是，`additive_expr()` 必须调用更高优先级的 `multiplicative_expr()` 函数。下面是实现方法。

## `additive_expr()`

```c
// 返回一棵以 '+' 或 '-' 二元运算符为根节点的抽象语法树
struct ASTnode *additive_expr(void) {
  struct ASTnode *left, *right;
  int tokentype;

  // 获取比我们更高优先级的左子树
  left = multiplicative_expr();

  // 如果没有剩余的词法单元，返回只有左节点的树
  tokentype = Token.token;
  if (tokentype == T_EOF)
    return (left);

  // 在我们这个优先级的词法单元上循环工作
  while (1) {
    // 读取下一个整数字面量
    scan(&Token);

    // 获取比我们更高优先级的右子树
    right = multiplicative_expr();

    // 用我们低优先级的运算符将两棵子树连接起来
    left = mkastnode(arithop(tokentype), left, right, 0);

    // 获取下一个在我们优先级的词法单元
    tokentype = Token.token;
    if (tokentype == T_EOF)
      break;
  }

  // 返回我们创建的任何树
  return (left);
}
```

在一开始，我们立即调用 `multiplicative_expr()`，以防第一个运算符是高优先级的 `*` 或 `/`。该函数只会在遇到低优先级的 `+` 或 `-` 运算符时返回。

因此，当我们进入 `while` 循环时，我们知道遇到的是 `+` 或 `-` 运算符。我们循环直到输入中没有剩余的词法单元，即遇到 T_EOF 词法单元。

在循环内部，我们再次调用 `multiplicative_expr()`，以防后续运算符的优先级比我们高。同样，它们不会再高时返回。

一旦我们有了左右子树，就可以用上一次循环中得到的运算符将它们组合起来。这个过程会重复进行，所以如果我们有表达式 `2 + 4 + 6`，最终会得到如下抽象语法树：

```
       +
      / \
     +   6
    / \
   2   4
```

但如果 `multiplicative_expr()` 有自己的更高优先级运算符，我们就会将包含多个节点的子树组合在一起。

## `multiplicative_expr()`

```c
// 返回一棵以 '*' 或 '/' 二元运算符为根节点的抽象语法树
struct ASTnode *multiplicative_expr(void) {
  struct ASTnode *left, *right;
  int tokentype;

  // 获取左侧的整数字面量。
  // 同时读取下一个词法单元。
  left = primary();

  // 如果没有剩余的词法单元，返回只有左节点的树
  tokentype = Token.token;
  if (tokentype == T_EOF)
    return (left);

  // 当词法单元是 '*' 或 '/' 时循环
  while ((tokentype == T_STAR) || (tokentype == T_SLASH)) {
    // 读取下一个整数字面量
    scan(&Token);
    right = primary();

    // 将其与左侧的整数字面量连接起来
    left = mkastnode(arithop(tokentype), left, right, 0);

    // 更新当前词法单元的详细信息。
    // 如果没有剩余的词法单元，返回只有左节点的树
    tokentype = Token.token;
    if (tokentype == T_EOF)
      break;
  }

  // 返回我们创建的任何树
  return (left);
}
```

这段代码与 `additive_expr()` 类似，只是我们调用 `primary()` 来获取真正的整数字面量！我们也只在有高优先级运算符（即 `*` 和 `/` 运算符）时才循环。一旦遇到低优先级运算符，我们就简单地返回到目前为止构建的子树。这回到 `additive_expr()` 来处理低优先级运算符。

## 上述方法的缺点

这种用显式运算符优先级构建递归下降解析器的方式可能效率不高，因为需要大量的函数调用才能达到正确的优先级级别。还必须为每个运算符优先级级别编写函数，所以我们最终会得到大量代码行。

## 另一种方法：Pratt 解析

减少代码量的一种方法是使用 [Pratt 解析器](https://en.wikipedia.org/wiki/Pratt_parser)，它有一个与每个词法单元关联的优先级值表，而不是用函数来复制语法中显式的优先级。

在这里，我强烈建议你阅读 Bob Nystrom 的[《Pratt 解析器：表达式解析变得简单》](https://journal.stuffwithstuff.com/2011/03/19/pratt-parsers-expression-parsing-made-easy/)。Pratt 解析器仍然让我头疼，所以尽可能多读，理解基本概念。

## `expr.c`：Pratt 解析

我在 `expr.c` 中实现了 Pratt 解析，它是 `expr2.c` 的直接替代品。让我们开始介绍。

首先，我们需要一些代码来确定每个词法单元的优先级：

```c
// 每个词法单元的运算符优先级
static int OpPrec[] = { 0, 10, 10, 20, 20,    0 };
//                     EOF  +   -   *   /  INTLIT

// 检查我们是否有二元运算符并返回其优先级。
static int op_precedence(int tokentype) {
  int prec = OpPrec[tokentype];
  if (prec == 0) {
    fprintf(stderr, "syntax error on line %d, token %d\n", Line, tokentype);
    exit(1);
  }
  return (prec);
}
```

较高的数字（如 20）表示比低数字（如 10）更高的优先级。

现在，你可能会问：当你有一个名为 `OpPrec[]` 的查找表时，为什么还要有一个函数？答案是：为了发现语法错误。

考虑一个像 `234 101 + 12` 这样的输入。我们可以扫描前两个词法单元。但是，如果我们只是用 `OpPrec[]` 来获取第二个 `101` 词法单元的优先级，就不会注意到它不是运算符。因此，`op_precedence()` 函数强制执行正确的语法规则。

现在，我们不再为每个优先级级别设置一个函数，而是有一个使用运算符优先级表的单一表达式函数：

```c
// 返回一棵以二元运算符为根节点的抽象语法树。
// 参数 ptp 是前一个词法单元的优先级。
struct ASTnode *binexpr(int ptp) {
  struct ASTnode *left, *right;
  int tokentype;

  // 获取左侧的整数字面量。
  // 同时读取下一个词法单元。
  left = primary();

  // 如果没有剩余的词法单元，返回只有左节点的树
  tokentype = Token.token;
  if (tokentype == T_EOF)
    return (left);

  // 当这个词法单元的优先级
  // 高于前一个词法单元的优先级时循环
  while (op_precedence(tokentype) > ptp) {
    // 读取下一个整数字面量
    scan(&Token);

    // 用我们词法单元的优先级递归调用 binexpr() 来构建子树
    right = binexpr(OpPrec[tokentype]);

    // 将该子树与我们的连接起来。同时将词法单元转换为抽象语法树操作。
    left = mkastnode(arithop(tokentype), left, right, 0);

    // 更新当前词法单元的详细信息。
    // 如果没有剩余的词法单元，返回只有左节点的树
    tokentype = Token.token;
    if (tokentype == T_EOF)
      return (left);
  }

  // 当优先级相同或更低时返回我们拥有的树
  return (left);
}
```

首先，请注意，这与之前的解析器函数一样仍然是递归的。这一次，我们接收到在我们被调用之前找到的词法单元的优先级级别。`main()` 会用最低的优先级 0 调用我们，但我们会用更高的值调用自己。

你还应该注意到，这段代码与 `multiplicative_expr()` 函数非常相似：读取一个整数字面量，获取运算符的词法单元类型，然后循环构建树。

不同的是循环条件和循环体：

```c
multiplicative_expr():
  while ((tokentype == T_STAR) || (tokentype == T_SLASH)) {
    scan(&Token); right = primary();

    left = mkastnode(arithop(tokentype), left, right, 0);

    tokentype = Token.token;
    if (tokentype == T_EOF) return (left);
  }

binexpr():
  while (op_precedence(tokentype) > ptp) {
    scan(&Token); right = binexpr(OpPrec[tokentype]);

    left = mkastnode(arithop(tokentype), left, right, 0);

    tokentype = Token.token;
    if (tokentype == T_EOF) return (left);
  }
```

使用 Pratt 解析器时，当下一个运算符的优先级高于当前词法单元时，我们不是用 `primary()` 来获取下一个整数字面量，而是用 `binexpr(OpPrec[tokentype])` 调用自己来提高运算符优先级。

一旦我们遇到与我们优先级相同或更低的词法单元，我们就简单地：

```c
  return (left);
```

这要么是一棵包含很多节点和比调用我们的运算符更高优先级运算符的子树，要么可能是一个单一整数字面量，用于与我们同优先级的运算符。

现在我们有了一个单一的函数来进行表达式解析。它使用一个小的辅助函数来强制执行运算符优先级，从而实现我们语言的语义。

## 将两种解析器付诸实践

你可以制作两个程序，每个解析器一个：

```
$ make parser                                        # Pratt 解析器
cc -o parser -g expr.c interp.c main.c scan.c tree.c

$ make parser2                                       # 优先级攀升
cc -o parser2 -g expr2.c interp.c main.c scan.c tree.c
```

你也可以用我们旅程上一部分的相同输入文件测试两个解析器：

```
$ make test
(./parser input01; \
 ./parser input02; \
 ./parser input03; \
 ./parser input04; \
 ./parser input05)
15                                       # input01 结果
29                                       # input02 结果
syntax error on line 1, token 5          # input03 结果
Unrecognised character . on line 3       # input04 结果
Unrecognised character a on line 1       # input05 结果

$ make test2
(./parser2 input01; \
 ./parser2 input02; \
 ./parser2 input03; \
 ./parser2 input04; \
 ./parser2 input05)
15                                       # input01 结果
29                                       # input02 结果
syntax error on line 1, token 5          # input03 结果
Unrecognised character . on line 3       # input04 结果
Unrecognised character a on line 1       # input05 结果

```

## 结论与下一步

也许是时候退后一步，看看我们进展到哪里了。我们现在有了：

 + 一个能识别并返回我们语言中词法单元的扫描器
 + 一个能识别我们语法、报告语法错误并构建抽象语法树的解析器
 + 一个用于解析器的优先级表，实现我们语言的语义
 + 一个能深度优先遍历抽象语法树并计算输入表达式结果的解释器

我们还没有的是一个编译器。但我们离制作我们的第一个编译器非常近了！

在我们编译器编写旅程的下一部分，我们将替换解释器。在它的位置上，我们将编写一个翻译器，为每个具有数学运算符的抽象语法树节点生成 x86-64 汇编代码。我们还将生成一些汇编序言和尾声来支持翻译器输出的汇编代码。[下一步](../04_Assembly/Readme_zh.md)

（文件结束 — 共 437 行）
