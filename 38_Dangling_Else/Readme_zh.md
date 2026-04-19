# 第 38 章：悬空 Else 问题及其他

我开始这段编译器编写旅程时希望能解决 [悬空 Else 问题](https://en.wikipedia.org/wiki/Dangling_else)。但实际上，我需要做的是重构我们解析某些内容的方式，因为一开始我的解析就是错误的。

发生这种情况可能是因为我急于添加功能，但在过程中我没有退后一步好好审视我们一直在构建的内容。

那么，让我们看看编译器中有哪些错误需要修复。

## 修复 For 语法

我们从 FOR 循环结构开始。是的它能工作，但还不够通用。

到目前为止，我们 FOR 循环的 BNF 语法一直是：

```
for_statement: 'for' '(' preop_statement ';'
                         true_false_expression ';'
                         postop_statement ')' compound_statement  ;
```

然而，[C 的 BNF 语法](https://www.lysator.liu.se/c/ANSI-C-grammar-y.html) 是这样的：

```
for_statement:
        | FOR '(' expression_statement expression_statement ')' statement
        | FOR '(' expression_statement expression_statement expression ')' statement
        ;

expression_statement
        : ';'
        | expression ';'
        ;
```

而 `expression` 实际上是一个表达式列表，其中表达式由逗号分隔。

这意味着 FOR 循环的三个子句都可以是表达式列表。如果我们正在编写一个"完整的" C 编译器，这最终会变得很棘手。然而，我们只是为 C 的一个子集编写编译器，因此我不必让我们的编译器处理完整的 C 语法。

所以，我修改了 FOR 循环的解析器来识别这个：

```
for_statement: 'for' '(' expression_list ';'
                         true_false_expression ';'
                         expression_list ')' compound_statement  ;
```

中间子句是一个必须提供真或假结果的单表达式。第一和第三个子句可以是表达式列表。这允许像 `tests/input80.c` 中现在有的那种 FOR 循环：

```c
    for (x=0, y=1; x < 6; x++, y=y+2)
```

## 对 `expression_list()` 的修改

为了实现上述功能，我需要修改 `for_statement()` 解析函数，让它调用 `expression_list()` 来解析第一和第三个子句中的表达式列表。

但是，在现有的编译器中，`expression_list()` 只允许用 ')' 词法单元结束表达式列表。因此，我修改了 `expr.c` 中的 `expression_list()`，让它接收结束词法单元作为参数。在 `stmt.c` 的 `for_statement()` 中，我们现在有这样的代码：

```c
// Parse a FOR statement and return its AST
static struct ASTnode *for_statement(void) {
  ...
  // Get the pre_op expression and the ';'
  preopAST = expression_list(T_SEMI);
  semi();
  ...
  // Get the condition and the ';'.
  condAST = binexpr(0);
  semi();
  ...
  // Get the post_op expression and the ')'
  postopAST = expression_list(T_RPAREN);
  rparen();
}
```

而 `expression_list()` 中的代码现在是这样的：

```c
struct ASTnode *expression_list(int endtoken) {
  ...
  // Loop until the end token
  while (Token.token != endtoken) {

    // Parse the next expression
    child = binexpr(0);

    // Build an A_GLUE AST node ...
    tree = mkastnode(A_GLUE, P_NONE, tree, NULL, child, NULL, exprcount);

    // Stop when we reach the end token
    if (Token.token == endtoken) break;

    // Must have a ',' at this point
    match(T_COMMA, ",");
  }

  // Return the tree of expressions
  return (tree);
}
```

## 单语句和复合语句

到目前为止，我一直强制使用我们编译器的程序员在以下情况中将代码放在 '{' ... '}' 中：

 + IF 语句的真分支
 + IF 语句的假分支
 + WHILE 语句的循环体
 + FOR 语句的循环体
 + 'case' 子句后的语句体
 + 'default' 子句后的语句体

对于这个列表中的前四个语句，当只有一个语句时不需要花括号，例如：

```c
  if (x>5)
    x= x - 16;
  else
    x++;
```

但当循环体中有多个语句时，我们确实需要一个复合语句，也就是被花括号包围的一组单语句，例如：

```c
  if (x>5)
    { x= x - 16; printf("not again!\n"); }
  else
    x++;
```

但是，由于某种未知的原因，'switch' 语句中 'case' 或 'default' 子句后的代码可以是一组单语句，我们不需要花括号！！是谁觉得这样是可以的？例如：

```c
  switch (x) {
    case 1: printf("statement 1\n");
            printf("statement 2\n");
            break;
    default: ...
  }
```

更糟糕的是，这样也是合法的：

```c
  switch (x) {
    case 1: {
      printf("statement 1\n");
      printf("statement 2\n");
      break;
    }
    default: ...
  }
```

因此，我们需要能够解析：

 + 单语句
 + 被花括号包围的一组语句
 + 一组不以 '{' 开始但以 'case'、'default' 或 '}'（如果以 '{' 开头）结束的语句

为此，我修改了 `stmt.c` 中的 `compound_statement()`，让它接受一个参数：

```c
// Parse a compound statement
// and return its AST. If inswitch is true,
// we look for a '}', 'case' or 'default' token
// to end the parsing. Otherwise, look for
// just a '}' to end the parsing.
struct ASTnode *compound_statement(int inswitch) {
  struct ASTnode *left = NULL;
  struct ASTnode *tree;

  while (1) {
    // Parse a single statement
    tree = single_statement();
    ...
    // Leave if we've hit the end token
    if (Token.token == T_RBRACE) return(left);
    if (inswitch && (Token.token == T_CASE || Token.token == T_DEFAULT)) return(left);
  }
}
```

如果这个函数用 `inswitch` 设为 1 来调用，那么我们是在解析 'switch' 语句的过程中调用的，所以要查找 'case'、'default' 或 '}' 来结束复合语句。否则，我们处于更典型的 '{' ... '}' 情况。

现在，我们还需要允许：

 + IF 语句体中的单语句
 + WHILE 语句体中的单语句
 + FOR 语句体中的单语句

目前所有这些都调用 `compound_statement(0)`，但这强制要求解析一个闭合的 '}'，而对于单语句我们不会有这个。

解决方案是让 IF、WHILE 和 FOR 解析代码调用 `single_statement()` 来解析一个语句。并且，让 `single_statement()` 在看到左花括号时调用 `compound_statement()`。

因此，我在 `stmt.c` 中也做了这些修改：

```c
// Parse a single statement and return its AST.
static struct ASTnode *single_statement(void) {
  ...
  switch (Token.token) {
    case T_LBRACE:
      // We have a '{', so this is a compound statement
      lbrace();
      stmt = compound_statement(0);
      rbrace();
      return(stmt);
}
...
static struct ASTnode *if_statement(void) {
  ...
  // Get the AST for the statement
  trueAST = single_statement();
  ...
  // If we have an 'else', skip it
  // and get the AST for the statement
  if (Token.token == T_ELSE) {
    scan(&Token);
    falseAST = single_statement();
  }
  ...
}
...
static struct ASTnode *while_statement(void) {
  ...
    // Get the AST for the statement.
  // Update the loop depth in the process
  Looplevel++;
  bodyAST = single_statement();
  Looplevel--;
  ...
}
...
static struct ASTnode *for_statement(void) {
  ...
  // Get the statement which is the body
  // Update the loop depth in the process
  Looplevel++;
  bodyAST = single_statement();
  Looplevel--;
  ...
}
```

这意味着编译器现在将接受这样的代码：

```c
  if (x>5)
    x= x - 16;
  else
    x++;
```

## 是的，但"悬空 Else？"

我仍然没有解决"悬空 Else"问题，毕竟这是我开始这部分旅程的原因。好吧，事实证明这个问题已经通过我们已有的解析输入方式得到了解决。

考虑这个程序：

```c
  // Dangling else test.
  // We should not print anything for x<= 5
  for (x=0; x < 12; x++)
    if (x > 5)
      if (x > 10)
        printf("10 < %2d\n", x);
      else
        printf(" 5 < %2d <= 10\n", x);
```

我们希望 'else' 代码与最近的 'if' 语句配对。因此，上述最后一个 `printf` 语句只应在 x 在 5 到 10 之间时打印。'else' 代码不应由于 x > 5 的相反条件而被调用。

幸运的是，在我们的 `if_statement()` 解析器中，我们在 IF 语句体之后贪婪地扫描任何 'else' 词法单元：

```c
  // Get the AST for the statement
  trueAST = single_statement();

  // If we have an 'else', skip it
  // and get the AST for the statement
  if (Token.token == T_ELSE) {
    scan(&Token);
    falseAST = single_statement();
  }
```

这强制 'else' 与最近的 'if' 配对，从而解决了悬空 Else 问题。所以，一直以来，当我强制使用 '{' ... '}' 时，其实我已经解决了我担心的那个问题！唉。

## 一些更好的调试输出

最后，我修改了扫描器来改进调试。或者更准确地说，改进我们打印的调试消息。到目前为止，我们在错误消息中打印的是词法单元的数值，例如：

 + Unexpected token in parameter list: 23
 + Expecting a primary expression, got token: 19
 + Syntax error, token: 44

对于收到这些错误消息的程序员来说，它们基本上是无法使用的。在 `scan.c` 中，我添加了这个词法单元字符串列表：

```c
// List of token strings, for debugging purposes
char *Tstring[] = {
  "EOF", "=", "||", "&&", "|", "^", "&",
  "==", "!=", ",", ">", "<=", ">=", "<<", ">>",
  "+", "-", "*", "/", "++", "--", "~", "!",
  "void", "char", "int", "long",
  "if", "else", "while", "for", "return",
  "struct", "union", "enum", "typedef",
  "extern", "break", "continue", "switch",
  "case", "default",
  "intlit", "strlit", ";", "identifier",
  "{", "}", "(", ")", "[", "]", ",", ".",
  "->", ":"
};
```

在 `defs.h` 中，我向 Token 结构添加了另一个字段：

```c
// Token structure
struct token {
  int token;                    // Token type, from the enum list above
  char *tokstr;                 // String version of the token
  int intvalue;                 // For T_INTLIT, the integer value
};
```

在 `scan.c` 的 `scan()` 中，就在返回词法单元之前，我们设置它的字符串等价物：

```c
  t->tokstr = Tstring[t->token];
```

最后，我修改了一堆 `fatalXX()` 调用，用当前词法单元的 `tokstr` 字段代替 `intvalue` 字段来打印。这意味着我们现在看到的是：

 + Unexpected token in parameter list: ==
 + Expecting a primary expression, got token: ]
 + Syntax error, token: >>

这就好多了。


## 结论与下一步

我着手解决编译器中的"悬空 Else"特性问题，结果却修复了一堆其他的问题。在这个过程中，我发现实际上没有什么"悬空 Else"问题需要解决。

我们已经到了编译器开发的阶段，所有自举编译器所需的基本要素都已实现，但现在我们需要发现并修复一堆小问题。这是"收尾"阶段。

这意味着，从现在开始，关于如何编写编译器的内容会越来越少，而关于如何修复有问题的编译器的内容会越来越多。如果你想在未来的旅程中退出，我不会失望。如果这样做的话，我希望你觉得到目前为止的旅程都有用。

在我们编译器编写旅程的下一部分，我将挑选一些目前不能工作但我们需要使其能自举编译器的东西来修复。[下一步](../39_Var_Initialisation_pt1/Readme_zh.md)