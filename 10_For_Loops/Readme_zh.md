# 第 10 章：For 循环

在这一部分编译器编写旅程中，我要为我们的语言添加 For 循环。在讨论如何解决之前，我想先解释一下实现中的一个难点。

## For 循环语法

我假设你熟悉 For 循环的语法。一个例子是：

```c
  for (i=0; i < MAX; i++)
    printf("%d\n", i);
```

我将为我们的语言使用这个 BNF 语法：

```
 for_statement: 'for' '(' preop_statement ';'
                          true_false_expression ';'
                          postop_statement ')' compound_statement  ;

 preop_statement:  statement  ;        (目前)
 postop_statement: statement  ;        (目前)
```

`preop_statement` 在循环开始前执行。之后，我们必须限制这里可以执行的操作类型（例如，不能有 IF 语句）。然后对 `true_false_expression` 求值。如果为真，循环执行 `compound_statement`。完成后，执行 `postop_statement`，然后代码循环回去重新执行 `true_false_expression`。

## 难点

难点在于 `postop_statement` 在 `compound_statement` 之前被解析，但我们必须在 `compound_statement` 的代码之后生成 `postop_statement` 的代码。

有几种方法可以解决这个问题。当我之前写编译器时，我选择将 `compound_statement` 的汇编代码放入一个临时缓冲区，一旦生成了 `postop_statement` 的代码就"回放"缓冲区。在 SubC 编译器中，Nils 巧妙地利用标签和跳转到标签来"链接"代码的执行，以强制执行正确的顺序。

但我们在这里构建一个 AST 树。让我们用它来以正确的顺序生成汇编代码。

## 什么样的 AST 树？

你可能已经注意到 For 循环有四个结构组成部分：

 1. `preop_statement`
 2. `true_false_expression`
 3. `postop_statement`
 4. `compound_statement`

我真的不想再次更改 AST 节点结构来拥有四个子节点。但我们可以将 For 循环想象为一个增强的 While 循环：

```
   preop_statement;
   while ( true_false_expression ) {
     compound_statement;
     postop_statement;
   }
```

我们能用现有的节点类型构建一个反映这个结构的 AST 树吗？可以的：

```
          A_GLUE
         /     \
    preop     A_WHILE
             /    \
        decision  A_GLUE
                  /    \
            compound  postop
```

手动从上到下从左到右遍历这棵树，说服自己我们能按正确的顺序生成汇编代码。
我们必须将 `compound_statement` 和 `postop_statement` 粘合在一起，这样当 While 循环退出时，它会跳过 `compound_statement` 和 `postop_statement`。

这也意味着我们需要一个新的 T_FOR 词法单元，但不需要新的 AST 节点类型。因此唯一的编译器更改将是扫描和解析。

## 词法单元和扫描

有一个新的关键字 'for' 和一个关联的词法单元 T_FOR。这里没有大的变化。

## 解析语句

我们确实需要对解析器进行结构性更改。对于 For 循环语法，我只想将单个语句作为 `preop_statement` 和 `postop_statement`。目前，我们有一个 `compound_statement()` 函数，它只是循环直到遇到右花括号 '}'。我们需要将其分离出来，这样 `compound_statement()` 调用 `single_statement()` 来获取一个语句。

但还有另一个难点。以 `assignment_statement()` 中对赋值语句的现有解析为例。解析器必须在语句末尾找到分号。

这对于复合语句是好的，但对于 For 循环不行。我必须写这样的东西：

```c
  for (i=1 ; i < 10 ; i= i + 1; )
```

因为每个赋值语句必须以分号结尾。

我们需要的是单个语句解析器不扫描分号，而是将其留给复合语句解析器。我们对某些语句扫描分号（例如在赋值语句之间），而对其他语句不扫描分号（例如不在连续的 IF 语句之间）。

在解释完这些之后，让我们看看新的单个和复合语句解析代码：

```c
// 解析单个语句
// 并返回其 AST
static struct ASTnode *single_statement(void) {
  switch (Token.token) {
    case T_PRINT:
      return (print_statement());
    case T_INT:
      var_declaration();
      return (NULL);		// 此处没有生成 AST
    case T_IDENT:
      return (assignment_statement());
    case T_IF:
      return (if_statement());
    case T_WHILE:
      return (while_statement());
    case T_FOR:
      return (for_statement());
    default:
      fatald("语法错误，词法单元", Token.token);
  }
}

// 解析复合语句
// 并返回其 AST
struct ASTnode *compound_statement(void) {
  struct ASTnode *left = NULL;
  struct ASTnode *tree;

  // 需要一个左花括号
  lbrace();

  while (1) {
    // 解析单个语句
    tree = single_statement();

    // 某些语句后面必须跟分号
    if (tree != NULL &&
	(tree->op == A_PRINT || tree->op == A_ASSIGN))
      semi();

    // 对于每个新树，如果左树为空，
    // 则将其保存在左树中，
    // 否则将左树和新树粘合在一起
    if (tree != NULL) {
      if (left == NULL)
	left = tree;
      else
	left = mkastnode(A_GLUE, left, NULL, tree, 0);
    }
    // 当遇到右花括号时，
    // 跳过它并返回 AST
    if (Token.token == T_RBRACE) {
      rbrace();
      return (left);
    }
  }
}
```

我还从 `print_statement()` 和 `assignment_statement()` 中删除了对 `semi()` 的调用。

## 解析 For 循环

根据上面的 For 循环 BNF 语法，这很简单。考虑到我们想要的 AST 树的形状，构建这棵树的代码也很简单。代码如下：

```c
// 解析 FOR 语句
// 并返回其 AST
static struct ASTnode *for_statement(void) {
  struct ASTnode *condAST, *bodyAST;
  struct ASTnode *preopAST, *postopAST;
  struct ASTnode *tree;

  // 确保有 'for' '('
  match(T_FOR, "for");
  lparen();

  // 获取前置运算语句和 ';'
  preopAST = single_statement();
  semi();

  // 获取条件和 ';'
  condAST = binexpr(0);
  if (condAST->op < A_EQ || condAST->op > A_GE)
    fatal("错误的比较运算符");
  semi();

  // 获取后置运算语句和 ')'
  postopAST = single_statement();
  rparen();

  // 获取作为循环体的复合语句
  bodyAST = compound_statement();

  // 目前，所有四个子树都必须非空。
  // 稍后，我们会更改某些缺失时的语义

  // 将复合语句和后置运算树粘合在一起
  tree = mkastnode(A_GLUE, bodyAST, NULL, postopAST, 0);

  // 用这个新循环体创建一个 WHILE 循环
  tree = mkastnode(A_WHILE, condAST, NULL, tree, 0);

  // 将前置运算树粘合到 A_WHILE 树
  return (mkastnode(A_GLUE, preopAST, NULL, tree, 0));
}
```

## 生成汇编代码

好吧，我们所做的只是合成了一棵内部有 While 循环的树，并将一些子树粘合在一起，因此编译器生成端没有任何变化。

## 尝试一下

`tests/input07` 文件中有这个程序：

```c
{
  int i;
  for (i= 1; i <= 10; i= i + 1) {
    print i;
  }
}
```

当我们执行 `make test7` 时，得到这个输出：

```
cc -o comp1 -g cg.c decl.c expr.c gen.c main.c misc.c scan.c
    stmt.c sym.c tree.c
./comp1 tests/input07
cc -o out out.s
./out
1
2
3
4
5
6
7
8
9
10
```

这是相关的汇编输出：

```
	.comm	i,8,8
	movq	$1, %r8
	movq	%r8, i(%rip)		# i = 1
L1:
	movq	i(%rip), %r8
	movq	$10, %r9
	cmpq	%r9, %r8		# Is i < 10?
	jg	L2			# i >= 10, jump to L2
	movq	i(%rip), %r8
	movq	%r8, %rdi
	call	printint		# print i
	movq	i(%rip), %r8
	movq	$1, %r9
	addq	%r8, %r9		# i = i + 1
	movq	%r9, i(%rip)
	jmp	L1			# Jump to top of loop
L2:
```

## 结论与下一步

我们现在在语言中有了相当多的控制结构：IF 语句、While 循环和 For 循环。问题是，下一步要做什么？我们可以研究很多东西：

 + 类型
 + 局部与全局
 + 函数
 + 数组和指针
 + 结构体和联合体
 + auto、static 及其同类

我决定研究函数。所以，在编译器编写的下一步中，我们将开始为语言添加函数的几个阶段中的第一个。[下一步](../11_Functions_pt1/Readme_zh.md)