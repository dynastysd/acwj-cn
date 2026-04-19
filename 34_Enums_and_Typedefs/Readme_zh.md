# 第 34 章：枚举和类型定义

在这部分编译器编写的旅程中，我决定同时实现枚举和类型定义，因为它们都比较简单。

我们之前在第 30 章已经讨论过枚举的设计方面。简单回顾一下：枚举只是命名的整数字面量。有两个问题需要处理：

 + 我们不能重新定义一个枚举类型名
 + 我们不能重新定义一个命名的枚举值

上述情况的例子：

```c
enum fred { x, y, z };
enum fred { a, b };             // fred 被重新定义了
enum jane { x, y };             // x 和 y 被重新定义了
```

如上所示，枚举值列表只有标识符名称，没有类型：这意味着我们不能复用现有的变量声明解析代码。我们必须在这里编写自己的解析代码。

## 新的关键字和词法单元

我在语法中添加了两个新的关键字 'enum' 和 'typedef'，以及两个词法单元 T_ENUM 和 T_TYPEDEF。浏览 `scan.c` 中的代码查看详情。

## 枚举和类型定义的符号表列表

我们需要记录已声明的枚举和类型定义的详细信息，因此在 `data.h` 中有两个新的符号表列表：

```c
extern_ struct symtable *Enumhead,  *Enumtail;    // 枚举类型和值的列表
extern_ struct symtable *Typehead,  *Typetail;    // 类型定义的列表
```

在 `sym.c` 中有相关的函数来向每个列表添加条目，以及在每个列表中搜索特定的名称。这些列表中的节点被标记为以下之一（来自 `defs.h`）：

```c
  C_ENUMTYPE,                   // 命名的枚举类型
  C_ENUMVAL,                    // 命名的枚举值
  C_TYPEDEF                     // 命名的类型定义
```

好的，有两个列表但有三个节点类，这是怎么回事？事实证明，枚举值（如上面例子中的 `x` 和 `y`）不属于任何特定的枚举类型。此外，枚举类型名（如上面例子中的 `fred` 和 `jane`）实际上不起任何作用，但我们必须防止重新定义它们。

我使用一个枚举符号表列表来在同一列表中保存 C_ENUMTYPE 和 C_ENUMVAL 节点。使用顶部附近的例子，我们会有：

```
   fred           x            y            z
C_ENUMTYPE -> C_ENUMVAL -> C_ENUMVAL -> C_ENUMVAL
                  0            1            2
```

这也意味着，当我们在搜索枚举符号表列表时，需要能够搜索 C_ENUMTYPE 或 C_ENUMVAL。

## 解析枚举声明

在我给出实现代码之前，让我们先看一些需要解析的例子：

```c
enum fred { a, b, c };                  // a 是 0，b 是 1，c 是 2
enum foo  { d=2, e=6, f };              // d 是 2，e 是 6，f 是 7
enum bar  { g=2, h=6, i } var1;         // var1 实际上是 int
enum      { j, k, l }     var2;         // var2 实际上是 int
```

首先，枚举解析如何连接到我们现有的解析代码？与结构体和联合体一样，在解析类型的代码中（在 `decl.c` 中）：

```c
// 解析当前词法单元并返回
// 一个基本类型枚举值和指向
// 任何复合类型的指针。
// 还要扫描下一个词法单元
int parse_type(struct symtable **ctype) {
  int type;
  switch (Token.token) {

      // 对于以下情况，如果在解析后
      // 遇到 ';' 则没有类型，返回 -1
      ...
    case T_ENUM:
      type = P_INT;             // 枚举实际上是 int
      enum_declaration();
      if (Token.token == T_SEMI)
        type = -1;
      break;
  }
  ...
}
```

我修改了 `parse_type()` 的返回值，以帮助识别它是结构体、联合体、枚举还是类型定义的声明，而不是实际类型（后跟标识符）。

现在让我们分阶段查看 `enum_declaration()` 代码。

```c
// 解析枚举声明
static void enum_declaration(void) {
  struct symtable *etype = NULL;
  char *name;
  int intval = 0;

  // 跳过 enum 关键字
  scan(&Token);

  // 如果有后续的枚举类型名，获取
  // 任何现有枚举类型节点的指针
  if (Token.token == T_IDENT) {
    etype = findenumtype(Text);
    name = strdup(Text);        // 因为它很快会被覆盖
    scan(&Token);
  }
```

我们只有一个全局变量 `Text` 来保存扫描到的单词，我们必须能够解析 `enum foo var1`。如果我们扫描 `foo` 之后的词法单元，就会丢失 `foo` 字符串。所以我们需要 `strdup()` 这个。

```c
  // 如果下一个词法单元不是 LBRACE，检查
  // 我们是否有枚举类型名，然后返回
  if (Token.token != T_LBRACE) {
    if (etype == NULL)
      fatals("undeclared enum type:", name);
    return;
  }
```

我们遇到了类似 `enum foo var1` 而不是 `enum foo { ... }` 的声明。因此 `foo` 必须已经是已知的枚举类型。我们可以不带值返回，因为每个枚举的类型都是 P_INT，这是在调用 `enum_declaration()` 的代码中设置的。

```c
  // 我们确实有 LBRACE。跳过它
  scan(&Token);

  // 如果我们有枚举类型名，确保它
  // 之前没有声明过
  if (etype != NULL)
    fatals("enum type redeclared:", etype->name);
  else
    // 为这个标识符构建一个枚举类型节点
    etype = addenum(name, C_ENUMTYPE, 0);
```

现在我们正在解析类似 `enum foo { ... }` 的内容，所以我们必须检查 `foo` 是否已经作为枚举类型声明过。

```c
  // 循环获取所有枚举值
  while (1) {
    // 确保我们有一个标识符
    // 复制它以防有整数字面量跟上
    ident();
    name = strdup(Text);

    // 确保这个枚举值之前没有声明过
    etype = findenumval(name);
    if (etype != NULL)
      fatals("enum value redeclared:", Text);
```

同样，我们 `strdup()` 枚举值标识符。我们还要检查这个枚举值标识符是否已经定义过。

```c
    // 如果下一个词法单元是 '='，跳过它并
    // 获取后续的整数字面量
    if (Token.token == T_ASSIGN) {
      scan(&Token);
      if (Token.token != T_INTLIT)
        fatal("Expected int literal after '='");
      intval = Token.intvalue;
      scan(&Token);
    }
```

这就是为什么我们必须 `strdup()` 的原因，因为扫描整数字面量会覆盖 `Text` 全局变量。我们在这里扫描 '=' 和整数字面量词法单元，并将 `intval` 变量设置为整数字面量值。

```c
    // 为这个标识符构建一个枚举值节点。
    // 为下一个枚举标识符递增值。
    etype = addenum(name, C_ENUMVAL, intval++);

    // 如果是右花括号则退出，否则获取逗号
    if (Token.token == T_RBRACE)
      break;
    comma();
  }
  scan(&Token);                 // 跳过右花括号
}
```

现在我们有了枚举值的名称和它在 `intval` 中的值。我们可以用 `addenum()` 将它添加到枚举符号表列表中。我们还递增 `intval` 以便为下一个枚举值标识符做好准备。

## 访问枚举名称

我们现在有了解析枚举值名称列表并将它们的整数字面量值存储在符号表中的代码。我们如何在何时搜索和使用它们？

我们必须在可能使用变量名的表达式处执行此操作。如果我们找到一个枚举名，就将它转换为一个具有特定值的 A_INTLIT AST 节点。执行此操作的位置在 `expr.c` 的 `postfix()` 中

```c
// 解析一个后缀表达式并返回
// 表示它的 AST 节点。标识符已在 Text 中
static struct ASTnode *postfix(void) {
  struct symtable *enumptr;

  // 如果标识符匹配一个枚举值，
  // 返回一个 A_INTLIT 节点
  if ((enumptr = findenumval(Text)) != NULL) {
    scan(&Token);
    return (mkastleaf(A_INTLIT, P_INT, NULL, enumptr->posn));
  }
  ...
}
```

## 测试功能

完成了！我们有几个测试程序来确认我们能够检测到重新定义的枚举类型和名称，但 `test/input63.c` 代码展示了枚举的工作：

```c
int printf(char *fmt);

enum fred { apple=1, banana, carrot, pear=10, peach, mango, papaya };
enum jane { aple=1, bnana, crrot, par=10, pech, mago, paaya };

enum fred var1;
enum jane var2;
enum fred var3;

int main() {
  var1= carrot + pear + mango;
  printf("%d\n", var1);
  return(0);
```

这会将 `carrot + pear + mango`（即 3+10+12）相加并打印出 25。

## 类型定义

枚举部分完成了。现在我们来看类型定义。类型定义声明的基本语法是：

```
typedef_declaration: 'typedef' identifier existing_type
                   | 'typedef' identifier existing_type variable_name
                   ;
```

因此，一旦我们解析了 `typedef` 关键字，就可以解析后续的类型并用该名称构建一个 C_TYPEDEF 符号节点。我们可以将实际类型的 `type` 和 `ctype` 存储在这个符号节点中。

解析代码非常简单。我们在 `decl.c` 的 `parse_type()` 中挂接它：

```c
    case T_TYPEDEF:
      type = typedef_declaration(ctype);
      if (Token.token == T_SEMI)
        type = -1;
      break;
```

这是 `typedef_declaration()` 代码。注意它返回实际的 `type` 和 `ctype`，以防声明后面跟着变量名。

```c
// 解析一个类型定义声明并返回它所代表的类型
// 和 ctype
int typedef_declaration(struct symtable **ctype) {
  int type;

  // 跳过 typedef 关键字
  scan(&Token);

  // 获取关键字后面的实际类型
  type = parse_type(ctype);

  // 查看类型定义标识符是否已存在
  if (findtypedef(Text) != NULL)
    fatals("redefinition of typedef", Text);

  // 它不存在，所以添加到类型定义列表
  addtypedef(Text, type, *ctype, 0, 0);
  scan(&Token);
  return (type);
}
```

代码应该很简单，但注意对 `parse_type()` 的递归调用：我们已经有了在类型定义名称之后解析类型定义的代码。

## 搜索和使用类型定义

我们现在在符号表列表中有了一组类型定义。我们如何使用这些定义？实际上，我们向语法中添加了新的类型关键字，例如：

```c
FILE    *zin;
int32_t cost;
```

这意味着当我们在解析类型时遇到一个不认识的关键字时，可以在该类型定义的列表中查找。所以我们需要再次修改 `parse_type()`：

```c
    case T_IDENT:
      type = type_of_typedef(Text, ctype);
      break;
```

`type` 和 `ctype` 都由 `type_of_typedef()` 返回：

```c
// 给定一个类型定义名称，返回它所代表的类型
int type_of_typedef(char *name, struct symtable **ctype) {
  struct symtable *t;

  // 在列表中查找类型定义
  t = findtypedef(name);
  if (t == NULL)
    fatals("unknown type", name);
  scan(&Token);
  *ctype = t->ctype;
  return (t->type);
}
```

请注意，到目前为止我还没有编写"递归"的代码。例如，当前的代码无法解析这个例子：

```c
typedef int FOO;
typedef FOO BAR;
BAR x;                  // x 的类型是 BAR -> 类型 FOO -> 类型 int
```

但它可以编译 `tests/input68.c`：

```c
int printf(char *fmt);

typedef int FOO;
FOO var1;

struct bar { int x; int y} ;
typedef struct bar BAR;
BAR var2;

int main() {
  var1= 5; printf("%d\n", var1);
  var2.x= 7; var2.y= 10; printf("%d\n", var2.x + var2.y);
  return(0);
}
```

其中 `int` 被重新定义为类型 `FOO`，结构体被重新定义为类型 `BAR`。

## 结论与下一步

在这部分编译器编写的旅程中，我们添加了对枚举和类型定义的支持。两者都比较容易实现，尽管我们确实需要为枚举编写相当多的解析代码。我想我之前能够为变量列表、结构体成员列表和联合体成员列表复用相同的解析代码，所以有点被宠坏了。

添加类型定义的代码非常简单。我确实需要添加代码来追踪类型定义的类型定义：这也应该很简单。

在我们编译器编写旅程的下一部分，我认为是我们引入 C 预处理器的时候了。现在我们有了结构体、联合体、枚举和类型定义，我们应该能够编写一些包含某些常见 Unix/Linux 库函数定义的*头文件*。然后我们可以在源文件中包含它们并编写一些真正有用的程序。[下一步](../35_Preprocessor/Readme_zh.md)
