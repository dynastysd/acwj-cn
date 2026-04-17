# 第 21 章：更多运算符

在编译器编写的旅程的这一部分，我决定挑选一些容易实现的内容，来补全表达式中仍然缺失的运算符。包括：

+ `++` 和 `--`，包括前缀和后缀的增量和减量
+ 一元运算符 `-`、`~` 和 `!`
+ 二元运算符 `^`、`&`、`|`、`<<` 和 `>>`

我还实现了隐式的"非零运算符"，它将表达式的右值作为布尔值用于选择和循环语句，例如：

```c
  for (str= "Hello"; *str; str++) ...
```

而不是写成：

```c
  for (str= "Hello"; *str != 0; str++) ...
```

## 词法单元与扫描

一如既往，我们从语言中的新词法单元开始。这次有几个新的：

| 扫描输入 | 词法单元 |
|:-------------:|-------|
|   <code>&#124;&#124;</code>        | T_LOGOR |
|   `&&`        | T_LOGAND |
|   <code>&#124;</code>         | T_OR |
|   `^`         | T_XOR |
|   `<<`        | T_LSHIFT |
|   `>>`        | T_RSHIFT |
|   `++`        | T_INC |
|   `--`        | T_DEC |
|   `~`         | T_INVERT |
|   `!`         | T_LOGNOT |

其中一些由新的单个字符组成，所以扫描比较容易。对于其他的，我们需要区分单个字符和不同字符的组合。例如 `<`、`<<` 和 `<=`。我们已经在 `scan.c` 中见过如何扫描这些字符，所以这里不再给出新代码。你可以浏览 `scan.c` 查看增加的内容。

## 向解析器添加二元运算符

现在我们需要解析这些运算符。其中一些是二元运算符：`||`、`&&`、`|`、`^`、`<<` 和 `>>`。我们已经有了二元运算符的优先级框架，只需将新运算符添加到框架中即可。

在实现过程中，我意识到根据[ C 运算符优先级表](https://en.cppreference.com/w/c/language/operator_precedence)，我之前把一些现有运算符放在了错误的优先级位置。我们还需要让 AST 节点操作与二元运算符词法单元的集合保持一致。以下是 `defs.h` 和 `expr.c` 中词法单元、AST 节点类型和运算符优先级表的定义：

```c
// Token types
enum {
  T_EOF,
  // Binary operators
  T_ASSIGN, T_LOGOR, T_LOGAND,
  T_OR, T_XOR, T_AMPER,
  T_EQ, T_NE,
  T_LT, T_GT, T_LE, T_GE,
  T_LSHIFT, T_RSHIFT,
  T_PLUS, T_MINUS, T_STAR, T_SLASH,

  // Other operators
  T_INC, T_DEC, T_INVERT, T_LOGNOT,
  ...
};

// AST node types. The first few line up
// with the related tokens
enum {
  A_ASSIGN= 1, A_LOGOR, A_LOGAND, A_OR, A_XOR, A_AND,
  A_EQ, A_NE, A_LT, A_GT, A_LE, A_GE, A_LSHIFT, A_RSHIFT,
  A_ADD, A_SUBTRACT, A_MULTIPLY, A_DIVIDE,
  ...
  A_PREINC, A_PREDEC, A_POSTINC, A_POSTDEC,
  A_NEGATE, A_INVERT, A_LOGNOT,
  ...
};

// Operator precedence for each binary token. Must
// match up with the order of tokens in defs.h
static int OpPrec[] = {
  0, 10, 20, 30,                // T_EOF, T_ASSIGN, T_LOGOR, T_LOGAND
  40, 50, 60,                   // T_OR, T_XOR, T_AMPER
  70, 70,                       // T_EQ, T_NE
  80, 80, 80, 80,               // T_LT, T_GT, T_LE, T_GE
  90, 90,                       // T_LSHIFT, T_RSHIFT
  100, 100,                     // T_PLUS, T_MINUS
  110, 110                      // T_STAR, T_SLASH
};
```

## 新一元运算符

现在我们来解析新的一元运算符 `++`、`--`、`~` 和 `!`。这些都是前缀运算符（即在表达式之前），但 `++` 和 `--` 也可以作为后缀运算符。因此，我们需要解析三个前缀和两个后缀运算符，并为它们执行五个不同的语义操作。

为了准备添加这些新运算符，我回顾了[ C 的 BNF 语法](https://www.lysator.liu.se/c/ANSI-C-grammar-y.html)。由于这些新运算符无法融入现有的二元运算符框架，我们需要用递归下降解析器中的新函数来实现。以下是上述语法中与我们的词法单元名称对应的相关部分：

```
primary_expression
        : T_IDENT
        | T_INTLIT
        | T_STRLIT
        | '(' expression ')'
        ;

postfix_expression
        : primary_expression
        | postfix_expression '[' expression ']'
        | postfix_expression '(' expression ')'
        | postfix_expression '++'
        | postfix_expression '--'
        ;

prefix_expression
        : postfix_expression
        | '++' prefix_expression
        | '--' prefix_expression
        | prefix_operator prefix_expression
        ;

prefix_operator
        : '&'
        | '*'
        | '-'
        | '~'
        | '!'
        ;

multiplicative_expression
        : prefix_expression
        | multiplicative_expression '*' prefix_expression
        | multiplicative_expression '/' prefix_expression
        | multiplicative_expression '%' prefix_expression
        ;

        etc.
```

我们在 `expr.c` 的 `binexpr()` 中实现二元运算符，但它调用 `prefix()`，就像上述 BNF 语法中的 `multiplicative_expression` 引用 `prefix_expression` 一样。我们已经有一个名为 `primary()` 的函数。现在我们需要一个函数 `postfix()` 来处理后缀表达式。

## 前缀运算符

我们已经在 `prefix()` 中解析了一些词法单元：T_AMPER 和 T_STAR。我们可以在这里添加新的词法单元（T_MINUS、T_INVERT、T_LOGNOT、T_INC 和 T_DEC），方法是在 `switch (Token.token)` 语句中添加更多 case。

我不会在这里包含代码，因为所有 case 的结构都相似：

  + 用 `scan(&Token)` 跳过词法单元
  + 用 `prefix()` 解析下一个表达式
  + 进行一些语义检查
  + 扩展 `prefix()` 返回的 AST 树

但是，某些 case 之间的差异很重要。对于解析 `&`（T_AMPER）词法单元，表达式需要被视为左值：如果我们做 `&x`，我们要的是变量 `x` 的地址，而不是 `x` 值的地址。其他 case 需要将 `prefix()` 返回的 AST 树强制为右值：

  + `-`（T_MINUS）
  + `~`（T_INVERT）
  + `!`（T_LOGNOT）

而且，对于前缀增量和前缀减量运算符，我们实际上要求表达式是左值：我们可以做 `++x` 但不能做 `++3`。目前，我已经编写了代码要求一个简单的标识符，但我知道之后我们会想要解析和处理 `++b[2]` 和 `++ *ptr`。

另外，从设计角度来看，我们有两个选择：修改 `prefix()` 返回的 AST 树（不创建新的 AST 节点），或向树中添加一个或多个新 AST 节点：

  + T_AMPER 修改现有 AST 树，使根节点为 A_ADDR
  + T_STAR 在树根添加一个 A_DEREF 节点
  + T_STAR 在树根添加一个 A_NEGATE 节点，之后可能会将树宽化为 `int` 值。为什么？因为树可能是 `char` 类型（无符号的），而你不能对无符号值取负。
  + T_INVERT 在树根添加一个 A_INVERT 节点
  + T_LOGNOT 在树根添加一个 A_LOGNOT 节点
  + T_INC 在树根添加一个 A_PREINC 节点
  + T_DEC 在树根添加一个 A_PREDEC 节点

## 解析后缀运算符

如果你看上面超链接的 BNF 语法，要解析后缀表达式，我们需要先解析基本表达式。为了实现这一点，我们需要先获取基本表达式的词法单元，然后确定是否有任何后续的后缀词法单元。

尽管语法显示"postfix"调用"primary"，但我通过在 `primary()` 中扫描词法单元，然后决定调用 `postfix()` 来解析后缀词法单元。

> 这被证明是一个错误 —— 来自未来的 Warren 写道。

上面的 BNF 语法似乎允许像 `x++ ++` 这样的表达式，因为它有：

```
postfix_expression:
        postfix_expression '++'
        ;
```

但我不允许在表达式后面有多个后缀运算符。让我们看看新的代码：

`primary()` 负责识别基本表达式：整数字面量、字符串字面量和标识符。它也识别带括号的表达式。只有标识符后面可以跟后缀运算符。

```c
static struct ASTnode *primary(void) {
  ...
  switch (Token.token) {
    case T_INTLIT: ...
    case T_STRLIT: ...
    case T_LPAREN: ...
    case T_IDENT:
      return (postfix());
    ...
}
```

我已经将函数调用和数组引用的解析移到了 `postfix()` 中，这里也是我们解析后缀 `++` 和 `--` 运算符的地方：

```c
// Parse a postfix expression and return
// an AST node representing it. The
// identifier is already in Text.
static struct ASTnode *postfix(void) {
  struct ASTnode *n;
  int id;

  // Scan in the next token to see if we have a postfix expression
  scan(&Token);

  // Function call
  if (Token.token == T_LPAREN)
    return (funccall());

  // An array reference
  if (Token.token == T_LBRACKET)
    return (array_access());


  // A variable. Check that the variable exists.
  id = findglob(Text);
  if (id == -1 || Gsym[id].stype != S_VARIABLE)
    fatals("Unknown variable", Text);

  switch (Token.token) {
      // Post-increment: skip over the token
    case T_INC:
      scan(&Token);
      n = mkastleaf(A_POSTINC, Gsym[id].type, id);
      break;

      // Post-decrement: skip over the token
    case T_DEC:
      scan(&Token);
      n = mkastleaf(A_POSTDEC, Gsym[id].type, id);
      break;

      // Just a variable reference
    default:
      n = mkastleaf(A_IDENT, Gsym[id].type, id);
  }
  return (n);
}
```

另一个设计决策。对于 `++`，我们可以创建一个带有 A_POSTINC 父节点的 A_IDENT AST 节点，但考虑到我们在 `Text` 中有标识符的名称，我们可以构建一个单一的 AST 节点，同时包含节点类型和对符号表中标识符槽位的引用。

## 将整数表达式转换为布尔值

在我们离开解析方面进入代码生成方面之前，我应该提到我对允许将整数表达式视为布尔表达式的更改，例如：

```
  x= a + b;
  if (x) { printf("x is not zero\n"); }
```

BNF 语法没有提供任何明确的语法规则来限制表达式为布尔值，例如：

```
selection_statement
        : IF '(' expression ')' statement
```

因此，我们必须从语义上做这件事。在 `stmt.c` 中解析 IF、WHILE 和 FOR 循环的地方，我添加了这段代码：

```c
  // Parse the following expression
  // Force a non-comparison expression to be boolean
  condAST = binexpr(0);
  if (condAST->op < A_EQ || condAST->op > A_GE)
    condAST = mkastunary(A_TOBOOL, condAST->type, condAST, 0);
```

我引入了一个新的 AST 节点类型 A_TOBOOL。这将生成代码来获取任何整数值。如果该值为零，结果为零；否则结果为一。

## 为新运算符生成代码

现在我们将注意力转向为新运算符生成代码。实际上，新的 AST 节点类型有：A_LOGOR、A_LOGAND、A_OR、A_XOR、A_AND、A_LSHIFT、A_RSHIFT、A_PREINC、A_PREDEC、A_POSTINC、A_POSTDEC、A_NEGATE、A_INVERT、A_LOGNOT 和 A_TOBOOL。

所有这些都简单地调用 `cg.c` 中平台特定代码生成器的匹配函数。因此 `gen.c` 中 `genAST()` 的新代码如下：

```c
    case A_AND:
      return (cgand(leftreg, rightreg));
    case A_OR:
      return (cgor(leftreg, rightreg));
    case A_XOR:
      return (cgxor(leftreg, rightreg));
    case A_LSHIFT:
      return (cgshl(leftreg, rightreg));
    case A_RSHIFT:
      return (cgshr(leftreg, rightreg));
    case A_POSTINC:
      // Load the variable's value into a register,
      // then increment it
      return (cgloadglob(n->v.id, n->op));
    case A_POSTDEC:
      // Load the variable's value into a register,
      // then decrement it
      return (cgloadglob(n->v.id, n->op));
    case A_PREINC:
      // Load and increment the variable's value into a register
      return (cgloadglob(n->left->v.id, n->op));
    case A_PREDEC:
      // Load and decrement the variable's value into a register
      return (cgloadglob(n->left->v.id, n->op));
    case A_NEGATE:
      return (cgnegate(leftreg));
    case A_INVERT:
      return (cginvert(leftreg));
    case A_LOGNOT:
      return (cglognot(leftreg));
    case A_TOBOOL:
      // If the parent AST node is an A_IF or A_WHILE, generate
      // a compare followed by a jump. Otherwise, set the register
      // to 0 or 1 based on it's zeroeness or non-zeroeness
      return (cgboolean(leftreg, parentASTop, label));
```

## x86-64 特定代码生成函数

这意味着我们现在可以看看生成真正的 x86-64 汇编代码的后端函数。对于大多数位运算，x86-64 平台有汇编指令来完成它们：

```c
int cgand(int r1, int r2) {
  fprintf(Outfile, "\tandq\t%s, %s\n", reglist[r1], reglist[r2]);
  free_register(r1); return (r2);
}

int cgor(int r1, int r2) {
  fprintf(Outfile, "\torq\t%s, %s\n", reglist[r1], reglist[r2]);
  free_register(r1); return (r2);
}

int cgxor(int r1, int r2) {
  fprintf(Outfile, "\txorq\t%s, %s\n", reglist[r1], reglist[r2]);
  free_register(r1); return (r2);
}

// Negate a register's value
int cgnegate(int r) {
  fprintf(Outfile, "\tnegq\t%s\n", reglist[r]); return (r);
}

// Invert a register's value
int cginvert(int r) {
  fprintf(Outfile, "\tnotq\t%s\n", reglist[r]); return (r);
}
```

对于移位运算，据我所知，移位量必须先加载到 `%cl` 寄存器中。

```c
int cgshl(int r1, int r2) {
  fprintf(Outfile, "\tmovb\t%s, %%cl\n", breglist[r2]);
  fprintf(Outfile, "\tshlq\t%%cl, %s\n", reglist[r1]);
  free_register(r2); return (r1);
}

int cgshr(int r1, int r2) {
  fprintf(Outfile, "\tmovb\t%s, %%cl\n", breglist[r2]);
  fprintf(Outfile, "\tshrq\t%%cl, %s\n", reglist[r1]);
  free_register(r2); return (r1);
}
```

处理布尔表达式的运算（结果必须是 0 或 1）要复杂一些。

```c
// Logically negate a register's value
int cglognot(int r) {
  fprintf(Outfile, "\ttest\t%s, %s\n", reglist[r], reglist[r]);
  fprintf(Outfile, "\tsete\t%s\n", breglist[r]);
  fprintf(Outfile, "\tmovzbq\t%s, %s\n", breglist[r], reglist[r]);
  return (r);
}
```

`test` 指令本质上是将寄存器与自身进行 AND 运算，以设置零标志和负标志。然后如果寄存器等于零（`sete`），则将其置为 1。然后我们将这个 8 位结果移动到 64 位寄存器中。

这是将整数转换为布尔值的代码：

```c
// Convert an integer value to a boolean value. Jump if
// it's an IF or WHILE operation
int cgboolean(int r, int op, int label) {
  fprintf(Outfile, "\ttest\t%s, %s\n", reglist[r], reglist[r]);
  if (op == A_IF || op == A_WHILE)
    fprintf(Outfile, "\tje\tL%d\n", label);
  else {
    fprintf(Outfile, "\tsetnz\t%s\n", breglist[r]);
    fprintf(Outfile, "\tmovzbq\t%s, %s\n", breglist[r], reglist[r]);
  }
  return (r);
}
```

同样，我们做 `test` 来获取寄存器的零性或非零性。如果我们为选择或循环语句做这个操作，那么如果结果为假则 `je` 跳转到标签。否则，使用 `setnz` 如果原始值非零则将寄存器置为 1。

## 增量和减量运算

我把 `++` 和 `--` 运算留到最后。这里的微妙之处在于，我们必须同时将值从内存位置取出到寄存器中，并分别递增或递减它。而且我们必须选择是在加载寄存器之前还是之后执行此操作。

由于我们已经有了一个 `cgloadglob()` 函数来加载全局变量的值，让我们修改它以按需改变变量。代码很丑但它确实有效。

```c
// Load a value from a variable into a register.
// Return the number of the register. If the
// operation is pre- or post-increment/decrement,
// also perform this action.
int cgloadglob(int id, int op) {
  // Get a new register
  int r = alloc_register();

  // Print out the code to initialise it
  switch (Gsym[id].type) {
    case P_CHAR:
      if (op == A_PREINC)
        fprintf(Outfile, "\tincb\t%s(\%%rip)\n", Gsym[id].name);
      if (op == A_PREDEC)
        fprintf(Outfile, "\tdecb\t%s(\%%rip)\n", Gsym[id].name);
      fprintf(Outfile, "\tmovzbq\t%s(%%rip), %s\n", Gsym[id].name, reglist[r]);
      if (op == A_POSTINC)
        fprintf(Outfile, "\tincb\t%s(\%%rip)\n", Gsym[id].name);
      if (op == A_POSTDEC)
        fprintf(Outfile, "\tdecb\t%s(\%%rip)\n", Gsym[id].name);
      break;
    case P_INT:
      if (op == A_PREINC)
        fprintf(Outfile, "\tincl\t%s(\%%rip)\n", Gsym[id].name);
      if (op == A_PREDEC)
        fprintf(Outfile, "\tdecl\t%s(\%%rip)\n", Gsym[id].name);
      fprintf(Outfile, "\tmovslq\t%s(%%rip), %s\n", Gsym[id].name, reglist[r]);
      if (op == A_POSTINC)
        fprintf(Outfile, "\tincl\t%s(\%%rip)\n", Gsym[id].name);
      if (op == A_POSTDEC)
        fprintf(Outfile, "\tdecl\t%s(\%%rip)\n", Gsym[id].name);
      break;
    case P_LONG:
    case P_CHARPTR:
    case P_INTPTR:
    case P_LONGPTR:
      if (op == A_PREINC)
        fprintf(Outfile, "\tincq\t%s(\%%rip)\n", Gsym[id].name);
      if (op == A_PREDEC)
        fprintf(Outfile, "\tdecq\t%s(\%%rip)\n", Gsym[id].name);
      fprintf(Outfile, "\tmovq\t%s(%%rip), %s\n", Gsym[id].name, reglist[r]);
      if (op == A_POSTINC)
        fprintf(Outfile, "\tincq\t%s(\%%rip)\n", Gsym[id].name);
      if (op == A_POSTDEC)
        fprintf(Outfile, "\tdecq\t%s(\%%rip)\n", Gsym[id].name);
      break;
    default:
      fatald("Bad type in cgloadglob:", Gsym[id].type);
  }
  return (r);
}
```

我相当确定以后我必须重写这个来实现 `x= b[5]++`，但目前这已经够用了。毕竟，小步前进是我承诺的每一步的做法。

## 测试新功能

我不会详细讲解这一步中的新测试输入文件。它们是 `tests` 目录下的 `input22.c`、`input23.c` 和 `input24.c`。你可以浏览它们并确认编译器可以正确编译它们：

```
$ make test
...
input22.c: OK
input23.c: OK
input24.c: OK
```

## 结论与下一步

在扩展编译器功能方面，这一部分旅程添加了很多功能，但我希望额外引入的概念复杂性是最小的。

我们添加了一堆二元运算符，这是通过更新扫描器和修改运算符优先级表来完成的。

对于一元运算符，我们将它们手动添加到解析器的 `prefix()` 函数中。

对于新的后缀运算符，我们将旧的函数调用和数组索引功能分离到一个新的 `postfix()` 函数中，并用这个函数添加了后缀运算符。我们确实需要在这里考虑一点左值和右值的问题。我们还做了一些关于添加什么 AST 节点的设计决策，或者我们是否应该只重新装饰一些现有的 AST 节点。

代码生成最终相对简单，因为 x86-64 架构有指令来实现我们需要的操作。然而，我们确实需要为某些操作设置一些特定的寄存器，或者执行指令组合来完成我们想要的操作。

棘手的操作是增量和减量操作。我已经写了代码让这些操作对普通变量工作，但我们以后必须重新访问这个。

在我们编译器编写旅程的下一部分，我想解决局部变量。一旦我们能让这些工作，我们就可以将它们扩展到包括函数参数和实参。这将需要两步或更多步。[下一步](../22_Design_Locals/Readme.md)
