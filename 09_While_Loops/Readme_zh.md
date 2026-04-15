# 第 9 章：While 循环

在这一部分旅程中，我们将为我们的语言添加 While 循环。从某种意义上说，While 循环非常像没有 'else' 子句的 IF 语句，只不过我们总是跳回到循环的顶部。

所以，这个：

```
  while (condition 为真) {
    statements;
  }
```

应该被翻译成：

```
Lstart: 求值条件
	如果条件为假则跳转到 Lend
	循环体语句
	跳转到 Lstart
Lend:
```

这意味着我们可以借用 IF 语句的扫描、解析和代码生成结构，并做一些小的修改来处理 While 语句。

让我们看看如何实现这一点。

## 新的词法单元

我们需要一个新的词法单元 T_WHILE 来表示新的 'while' 关键字。`defs.h` 和 `scan.c` 的更改是显而易见的，所以我在这里省略它们。

## 解析 While 语法

While 循环的 BNF 语法是：

```
// while_statement: 'while' '(' true_false_expression ')' compound_statement  ;
```

我们需要在 `stmt.c` 中编写一个函数来解析它。代码如下；注意与 IF 语句的解析相比，这里的简单性：

```c
// 解析一个 While 语句
// 并返回其 AST
struct ASTnode *while_statement(void) {
  struct ASTnode *condAST, *bodyAST;

  // 确保有 'while' '('
  match(T_WHILE, "while");
  lparen();

  // 解析后续表达式
  // 和后面的 ')'。确保
  // 树的运算是比较运算。
  condAST = binexpr(0);
  if (condAST->op < A_EQ || condAST->op > A_GE)
    fatal("Bad comparison operator");
  rparen();

  // 获取复合语句的 AST
  bodyAST = compound_statement();

  // 构造并返回该语句的 AST
  return (mkastnode(A_WHILE, condAST, NULL, bodyAST, 0));
}
```

我们需要一个新的 AST 节点类型 A_WHILE，它已被添加到 `defs.h` 中。这个节点有一个左子树来求值条件，以及一个右子树作为 While 循环体的复合语句。

## 通用代码生成

我们需要创建开始和结束标签，求值条件，并插入适当的跳转来退出循环和返回到循环顶部。同样，这比生成 IF 语句的代码要简单得多。在 `gen.c` 中：

```c
// 为 While 语句生成代码
// 和可选的 ELSE 子句
static int genWHILE(struct ASTnode *n) {
  int Lstart, Lend;

  // 生成开始和结束标签
  // 并输出开始标签
  Lstart = label();
  Lend = label();
  cglabel(Lstart);

  // 生成条件代码，后跟
  // 一个到结束标签的跳转。
  // 我们通过将 Lfalse 标签作为寄存器来骗过它。
  genAST(n->left, Lend, n->op);
  genfreeregs();

  // 生成循环体的复合语句
  genAST(n->right, NOREG, n->op);
  genfreeregs();

  // 最后输出跳回条件的跳转，
  // 和结束标签
  cgjump(Lstart);
  cglabel(Lend);
  return (NOREG);
}
```

我必须做的一件事是认识到比较运算符的父 AST 节点现在也可能是 A_WHILE，所以在 `genAST()` 中比较运算符的代码看起来像：

```c
    case A_EQ:
    case A_NE:
    case A_LT:
    case A_GT:
    case A_LE:
    case A_GE:
      // 如果父 AST 节点是 A_IF 或 A_WHILE，生成
      // 一个比较后跟一个跳转。否则，比较寄存器并
      // 根据比较结果将其中一个设置为 1 或 0。
      if (parentASTop == A_IF || parentASTop == A_WHILE)
        return (cgcompare_and_jump(n->op, leftreg, rightreg, reg));
      else
        return (cgcompare_and_set(n->op, leftreg, rightreg));
```

而这就是实现 While 循环所需的全部！

## 测试新的语言扩展

我把所有的输入文件移到了一个 `test/` 目录中。如果你现在执行 `make test`，它会进入这个目录，编译每个输入并比较输出与已知正确的输出：

```
cc -o comp1 -g cg.c decl.c expr.c gen.c main.c misc.c scan.c stmt.c
      sym.c tree.c
(cd tests; chmod +x runtests; ./runtests)
input01: OK
input02: OK
input03: OK
input04: OK
input05: OK
input06: OK
```

你也可以执行 `make test6`。这会编译 `tests/input06` 文件：

```c
{ int i;
  i=1;
  while (i <= 10) {
    print i;
    i= i + 1;
  }
}
```

这会打印出 1 到 10 的数字：

```
cc -o comp1 -g cg.c decl.c expr.c gen.c main.c misc.c scan.c
      stmt.c sym.c tree.c
./comp1 tests/input06
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

这是编译生成的汇编输出：

```
	.comm	i,8,8
	movq	$1, %r8
	movq	%r8, i(%rip)		# i= 1
L1:
	movq	i(%rip), %r8
	movq	$10, %r9
	cmpq	%r9, %r8		# Is i <= 10?
	jg	L2			# Greater than, jump to L2
	movq	i(%rip), %r8
	movq	%r8, %rdi		# Print out i
	call	printint
	movq	i(%rip), %r8
	movq	$1, %r9
	addq	%r8, %r9		# Add 1 to i
	movq	%r9, i(%rip)
	jmp	L1			# and loop back
L2:
```


## 结论与下一步

While 循环很容易添加，一旦我们完成了 IF 语句，因为它们有很多相似之处。

我认为我们现在也拥有了
一个[图灵完备](https://en.wikipedia.org/wiki/Turing_completeness)的语言：

  + 无限存储空间，即无限数量的变量
  + 基于存储值做出决策的能力，即 IF 语句
  + 改变方向的能力，即 While 循环

所以我们可以现在就停下来，我们的工作完成了！不，当然不是。我们仍在努力让编译器能够编译自己。

在我们编译器编写的下一步中，我们将为语言添加 For 循环。[下一步](../10_For_Loops/Readme_zh.md)