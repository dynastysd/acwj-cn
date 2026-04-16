# 第 12 章：类型，第一部分

我刚刚开始为我们的编译器添加类型支持。我应该提醒你，这对我来说是新的，因为我之前的[编译器](https://github.com/DoctorWkt/h-compiler)只有`int`类型。我克制住了查看 SubC 源代码寻找思路的冲动。因此，我正在独立探索，很可能在处理更复杂的类型问题时不得不重写一些代码。

## 目前支持哪些类型？

我将首先为全局变量添加 `char` 和 `int` 类型。我们已经为函数添加了 `void` 关键字。在下一步中，我将添加函数返回值。所以目前，`void` 已存在但我还没有完全处理它。

显然，`char` 的值范围比 `int` 有限得多。与 SubC 一样，我打算对 `char` 使用 0..255 的范围，对 `int` 使用有符号值范围。

这意味着我们可以将 `char` 值扩展为 `int`，但如果开发者尝试将 `int` 值缩小到 `char` 范围，我们必须发出警告。

## 新的关键字和 Token

只有一个新的 'char' 关键字和 T_CHAR token。这里没有什么令人兴奋的。

## 表达式类型

从现在开始，每个表达式都有一个类型。这包括：

 + 整数字面量，例如 56 是 `int`
 + 数学表达式，例如 45 - 12 是 `int`
 + 变量，例如如果我们声明 `x` 为 `char`，那么它的 *rvalue* 是 `char`

我们必须在评估每个表达式时跟踪其类型，以确保在需要时可以进行扩展，或者在必要时拒绝缩小。

在 SubC 编译器中，Nils 创建了一个单一的 *lvalue* 结构。一个指向这个单一结构的指针在递归解析器中传递，以跟踪解析过程中任意点的表达式类型。

我采用了不同的方法。我修改了抽象语法树节点，使其具有一个 `type` 字段，用于保存该点的树类型。在 `defs.h` 中，这是我目前创建的类型：

```c
// 原始类型
enum {
  P_NONE, P_VOID, P_CHAR, P_INT
};
```

我称它们为*原始*类型，就像 Nils 在 SubC 中所做的那样，因为我无法想出更好的名字。数据类型，或许？P_NONE 值表示 AST 节点*不*代表表达式且没有类型。一个例子是 A_GLUE 节点类型，它将语句粘合在一起：一旦生成了左侧语句，就没有类型可言了。

如果你查看 `tree.c`，你会看到构建 AST 节点的函数已被修改，同时也为新的 AST 节点结构（在 `defs.h` 中）的 `type` 字段赋值：

```c
struct ASTnode {
  int op;                       // 在此树上执行的"操作"
  int type;                     // 此树生成的任何表达式的类型
  ...
};
```

## 变量声明及其类型

我们现在至少有两种方式声明全局变量：

```c
  int x; char y;
```

我们需要解析这个，是的。但首先，我们如何记录每个变量的类型？我们需要修改 `symtable` 结构。我还添加了符号的"结构类型"细节，我将在未来使用（在 `defs.h` 中）：

```c
// 结构类型
enum {
  S_VARIABLE, S_FUNCTION
};

// 符号表结构
struct symtable {
  char *name;                   // 符号的名称
  int type;                     // 符号的原始类型
  int stype;                    // 符号的结构类型
};
```

在 `sym.c` 的 `newglob()` 中有新代码来初始化这些新字段：

```c
int addglob(char *name, int type, int stype) {
  ...
  Gsym[y].type = type;
  Gsym[y].stype = stype;
  return (y);
}
```

## 解析变量声明

现在是时候将类型的解析与变量本身的解析分开了。所以，在 `decl.c` 中我们现在有：

```c
// 解析当前 token 并返回一个原始类型枚举值
int parse_type(int t) {
  if (t == T_CHAR) return (P_CHAR);
  if (t == T_INT)  return (P_INT);
  if (t == T_VOID) return (P_VOID);
  fatald("Illegal type, token", t);
}

// 解析变量的声明
void var_declaration(void) {
  int id, type;

  // 获取变量的类型，然后是标识符
  type = parse_type(Token.token);
  scan(&Token);
  ident();
  id = addglob(Text, type, S_VARIABLE);
  genglobsym(id);
  semi();
}
```

## 处理表达式类型

以上都是简单部分！我们现在有：

  + 一组三种类型：`char`、`int` 和 `void`，
  + 解析变量声明以找出它们的类型，
  + 在符号表中捕获每个变量的类型，以及
  + 在每个 AST 节点中存储表达式的类型

现在我们需要在我们构建的 AST 节点中实际填入类型。然后我们必须决定何时扩展类型和/或拒绝类型冲突。让我们开始工作吧！

## 解析基本终结符

我们从解析整数字面量值和变量标识符开始。有一个问题是，我们希望能够做到：

```c
  char j; j= 2;
```

但如果我们把 `2` 标记为 P_INT，那么当我们尝试将其存储在 P_CHAR `j` 变量中时，就无法缩小该值。目前，我添加了一些语义代码来将小整数字面量值保持为 P_CHAR：

```c
// 解析一个基本因子并返回一个表示它的 AST 节点。
static struct ASTnode *primary(void) {
  struct ASTnode *n;
  int id;

  switch (Token.token) {
    case T_INTLIT:
      // 对于 INTLIT token，为其创建一个叶子 AST 节点。
      // 如果它在 P_CHAR 范围内，则将其设为 P_CHAR
      if ((Token.intvalue) >= 0 && (Token.intvalue < 256))
        n = mkastleaf(A_INTLIT, P_CHAR, Token.intvalue);
      else
        n = mkastleaf(A_INTLIT, P_INT, Token.intvalue);
      break;

    case T_IDENT:
      // 检查此标识符是否存在
      id = findglob(Text);
      if (id == -1)
        fatals("Unknown variable", Text);

      // 为其创建一个叶子 AST 节点
      n = mkastleaf(A_IDENT, Gsym[id].type, id);
      break;

    default:
      fatald("Syntax error, token", Token.token);
  }

  // 扫描下一个 token 并返回叶子节点
  scan(&Token);
  return (n);
}
```

还要注意，对于标识符，我们可以很容易地从全局符号表获取它们的类型细节。

## 构建二元表达式：比较类型

当我们用二元数学运算符构建数学表达式时，会有一个来自左子节点的类型和一个来自右子节点的类型。这就是我们必须进行扩展、不做任何处理，或者如果两种类型不兼容则拒绝表达式的地方。

目前，我有一个新文件 `types.c`，其中有一个函数比较两边的类型。代码如下：

```c
// 给定两个原始类型，如果它们兼容则返回 true，否则返回 false。
// 还可以返回 A_WIDEN 操作（如果需要扩展以匹配另一个）。
// 如果 onlyright 为 true，则仅从左向右扩展。
int type_compatible(int *left, int *right, int onlyright) {

  // Void 与任何类型都不兼容
  if ((*left == P_VOID) || (*right == P_VOID)) return (0);

  // 相同类型，它们是兼容的
  if (*left == *right) { *left = *right = 0; return (1);
  }

  // 根据需要将 P_CHAR 扩展为 P_INT
  if ((*left == P_CHAR) && (*right == P_INT)) {
    *left = A_WIDEN; *right = 0; return (1);
  }
  if ((*left == P_INT) && (*right == P_CHAR)) {
    if (onlyright) return (0);
    *left = 0; *right = A_WIDEN; return (1);
  }
  // 任何剩余的类型对都是兼容的
  *left = *right = 0;
  return (1);
}
```

这里有很多内容。首先，如果两种类型相同，我们可以简单地返回 True。任何包含 P_VOID 的类型都不能与另一种类型混合。

如果一边是 P_CHAR，另一边是 P_INT，我们可以将结果扩展为 P_INT。我这样做是修改传入的类型信息，用零（什么都不做）或一个新的 AST 节点类型 A_WIDEN 替换它。这意味着：将较窄子节点的值扩展为与较宽子节点的值一样宽。我们很快就会看到它的实际运行。

有一个额外的参数 `onlyright`。当我到达 A_ASSIGN AST 节点时使用它，在那里我们将左子节点的表达式分配给右侧的变量 *lvalue*。如果设置了这个参数，则不允许将 P_INT 表达式传输到 P_CHAR 变量。

最后，目前让任何其他类型对通过。

我认为可以保证，一旦我们引入数组和指针，这需要更改。我也希望我能找到一种方法使代码更简单和更优雅。但目前这已经足够了。

## 在表达式中使用 `type_compatible()`

我在这个版本的编译器中在三个不同的地方使用了 `type_compatible()`。我们从合并带有二元运算符的表达式开始。我修改了 `expr.c` 中 `binexpr()` 的代码来做到这一点：

```c
    // 确保两种类型是兼容的。
    lefttype = left->type;
    righttype = right->type;
    if (!type_compatible(&lefttype, &righttype, 0))
      fatal("Incompatible types");

    // 根据需要扩展任一边。类型变量现在是 A_WIDEN
    if (lefttype)
      left = mkastunary(lefttype, right->type, left, 0);
    if (righttype)
      right = mkastunary(righttype, left->type, right, 0);

    // 将该子树与我们的树连接起来。同时将 token 转换为 AST 操作。
    left = mkastnode(arithop(tokentype), left->type, left, NULL, right, 0);
```

我们拒绝不兼容的类型。但是，如果 `type_compatible()` 返回非零的 `lefttype` 或 `righttype` 值，这些实际上是 A_WIDEN 值。我们可以用它来构建一个以窄子节点为子节点的单元 AST 节点。当我们到达代码生成器时，它现在将知道这个子节点的值必须被扩展。

现在，我们还需要在哪些地方扩展表达式的值？

## 使用 `type_compatible()` 打印表达式

当我们使用 `print` 关键字时，它需要一个 `int` 表达式来打印。所以我们需要修改 `stmt.c` 中的 `print_statement()`：

```c
static struct ASTnode *print_statement(void) {
  struct ASTnode *tree;
  int lefttype, righttype;
  int reg;

  ...
  // 解析接下来的表达式
  tree = binexpr(0);

  // 确保两种类型是兼容的。
  lefttype = P_INT; righttype = tree->type;
  if (!type_compatible(&lefttype, &righttype, 0))
    fatal("Incompatible types");

  // 根据需要扩展树。
  if (righttype) tree = mkastunary(righttype, P_INT, tree, 0);
```

## 使用 `type_compatible()` 赋值给变量

这是我们需要检查类型的最后一个地方。当我们给变量赋值时，需要确保可以扩展右侧表达式。我们必须拒绝将宽类型存储到窄变量的任何尝试。以下是 `stmt.c` 中 `assignment_statement()` 的新代码：

```c
static struct ASTnode *assignment_statement(void) {
  struct ASTnode *left, *right, *tree;
  int lefttype, righttype;
  int id;

  ...
  // 为变量创建一个左值节点
  right = mkastleaf(A_LVIDENT, Gsym[id].type, id);

  // 解析接下来的表达式
  left = binexpr(0);

  // 确保两种类型是兼容的。
  lefttype = left->type;
  righttype = right->type;
  if (!type_compatible(&lefttype, &righttype, 1))  // 注意这个 1
    fatal("Incompatible types");

  // 根据需要扩展左侧。
  if (lefttype)
    left = mkastunary(lefttype, right->type, left, 0);
```

注意调用 `type_compatible()` 时末尾的这个 1。这强制执行了我们不能将宽值保存到窄变量的语义。

鉴于上述所有，我们现在可以解析几种类型并强制执行一些合理的语言语义：在可能的情况下扩展值，防止类型缩小，并防止不适当的类型冲突。现在我们转向代码生成方面。

## x86-64 代码生成的变化

我们的汇编输出是基于寄存器的，本质上它们是固定大小的。我们可以影响的是：

 + 存储变量的内存位置大小，以及
 + 多少寄存器用于保存数据，例如一个字节用于字符，八个字节用于 64 位整数。

我从 `cg.c` 中的 x86-64 特定代码开始，然后展示如何在 `gen.c` 中的通用代码生成器中使用它。

让我们从为变量生成存储开始。

```c
// 生成一个全局符号
void cgglobsym(int id) {
  // 选择 P_INT 或 P_CHAR
  if (Gsym[id].type == P_INT)
    fprintf(Outfile, "\t.comm\t%s,8,8\n", Gsym[id].name);
  else
    fprintf(Outfile, "\t.comm\t%s,1,1\n", Gsym[id].name);
}
```

我们从符号表中提取变量槽的类型，并根据此类型选择为其分配 1 或 8 个字节。现在我们需要将值加载到寄存器中：

```c
// 从变量加载值到寄存器。
// 返回寄存器的编号
int cgloadglob(int id) {
  // 获取一个新寄存器
  int r = alloc_register();

  // 打印初始化它的代码：P_CHAR 或 P_INT
  if (Gsym[id].type == P_INT)
    fprintf(Outfile, "\tmovq\t%s(\%%rip), %s\n", Gsym[id].name, reglist[r]);
  else
    fprintf(Outfile, "\tmovzbq\t%s(\%%rip), %s\n", Gsym[id].name, reglist[r]);
  return (r);
```

`movq` 指令将八个字节移入 8 字节寄存器。`movzbq` 指令将 8 字节寄存器清零，然后将单个字节移入其中。这也隐式地将一个字节值扩展为八个字节。我们的存储函数类似：

```c
// 将寄存器的值存储到变量中
int cgstorglob(int r, int id) {
  // 选择 P_INT 或 P_CHAR
  if (Gsym[id].type == P_INT)
    fprintf(Outfile, "\tmovq\t%s, %s(\%%rip)\n", reglist[r], Gsym[id].name);
  else
    fprintf(Outfile, "\tmovb\t%s, %s(\%%rip)\n", breglist[r], Gsym[id].name);
  return (r);
}
```

这一次，我们必须使用寄存器的"字节"名称和 `movb` 指令来移动单个字节。

幸运的是，`cgloadglob()` 函数已经完成了 P_CHAR 变量的扩展。所以这是新的 `cgwiden()` 函数的代码：

```c
// 将寄存器中的值从旧类型扩展到新类型，
// 并返回一个具有此新值的寄存器
int cgwiden(int r, int oldtype, int newtype) {
  // 什么都不做
  return (r);
}
```
## 通用代码生成的变化

有了以上设置，通用代码生成器 `gen.c` 只有很少的更改：

  + 对 `cgloadglob()` 和 `cgstorglob()` 的调用现在接收符号的槽号而不是符号的名称。
  + 类似地，`genglobsym()` 现在接收符号的槽号并将其传递给 `cgglobsym()`

唯一的主要变化是处理新的 A_WIDEN AST 节点类型的代码。我们不需要这个节点（因为 `cgwiden()` 什么都不做），但它在这里用于其他硬件平台：

```c
    case A_WIDEN:
      // 将子节点的类型扩展到父节点的类型
      return (cgwiden(leftreg, n->left->type, n->type));
```

## 测试新的类型更改

这是我的测试输入文件 `tests/input10`：

```c
void main()
{
  int i; char j;

  j= 20; print j;
  i= 10; print i;

  for (i= 1;   i <= 5; i= i + 1) { print i; }
  for (j= 253; j != 2; j= j + 1) { print j; }
}
```

我检查我们可以从 `char` 和 `int` 类型赋值和打印。我还验证了对于 `char` 变量，我们将在值序列中溢出：253、254、255、0、1、2 等。

```
$ make test
cc -o comp1 -g cg.c decl.c expr.c gen.c main.c misc.c scan.c
   stmt.c sym.c tree.c types.c
./comp1 tests/input10
cc -o out out.s
./out
20
10
1
2
3
4
5
253
254
255
0
1
```

让我们看看生成的一些汇编代码：

```
        .comm   i,8,8                   # Eight byte i storage
        .comm   j,1,1                   # One   byte j storage
        ...
        movq    $20, %r8
        movb    %r8b, j(%rip)           # j= 20
        movzbq  j(%rip), %r8
        movq    %r8, %rdi               # print j
        call    printint

        movq    $253, %r8
        movb    %r8b, j(%rip)           # j= 253
L3:
        movzbq  j(%rip), %r8
        movq    $2, %r9
        cmpq    %r9, %r8                # while j != 2
        je      L4
        movzbq  j(%rip), %r8
        movq    %r8, %rdi               # print j
        call    printint
        movzbq  j(%rip), %r8
        movq    $1, %r9                 # j= j + 1
        addq    %r8, %r9
        movb    %r9b, j(%rip)
        jmp     L3
```

仍然不是最优雅的汇编代码，但它可以工作。另外，`$ make test` 确认所有之前的代码示例仍然有效。

## 结论与下一步

在编译器编写的下一部分中，我们将添加带一个参数的函数调用，以及从函数返回值。[下一步](../13_Functions_pt2/Readme_zh.md)