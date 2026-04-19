# 第 40 章：全局变量初始化

在编译器编写的旅程的这一部分，我开始了为我们的语言添加变量声明的准备工作。在编译器编写旅程的这一部分，我能够为全局标量和数组变量实现这一点。

与此同时，我意识到我没有设计好符号表结构来正确处理变量的大小和数组变量中的元素数量。因此，这一部分的一半将重写一些处理符号表的代码。

## 全局变量赋值的快速回顾

作为快速回顾，以下是我想要支持的一组全局变量赋值示例：

```c
int x= 2;
char y= 'a';
char *str= "Hello world";
int a[10];
char b[]= { 'q', 'w', 'e', 'r', 't', 'y' };
char c[10]= { 'q', 'w', 'e', 'r', 't', 'y' };   // Zero padded
char *d[]= { "apple", "banana", "peach", "pear" };
```

我不会处理全局结构体或联合体的初始化。另外，目前我也不打算处理将 NULL 放入 `char *` 变量中。如果我们需要的话，我稍后会再讨论这个问题。

## 我们要走向何方

在旅程的最后一部分，我在 `decl.c` 中写了这个：

```c
static struct symtable *symbol_declaration(...) {
  ...
  // The array or scalar variable is being initialised
  if (Token.token == T_ASSIGN) {
    ...
        // Array initialisation
    if (stype == S_ARRAY)
      array_initialisation(sym, type, ctype, class);
    else {
      fatal("Scalar variable initialisation not done yet");
      // Variable initialisation
      // if (class== C_LOCAL)
      // Local variable, parse the expression
      // expr= binexpr(0);
      // else write more code!
    }
  }
  ...
}
```

也就是说，我知道应该把代码放在哪里，但不知道该写什么代码。首先，我们需要解析一些字面量值...

## 标量变量初始化

我们需要解析整数和字符串字面量，因为这些是唯一可以赋值给全局变量的东西。我们需要确保每个字面量的类型与我们正在赋值的变量类型兼容。为此，`decl.c` 中有一个新函数：

```c
// Given a type, check that the latest token is a literal
// of that type. If an integer literal, return this value.
// If a string literal, return the label number of the string.
// Do not scan the next token.
int parse_literal(int type) {

  // We have a string literal. Store in memory and return the label
  if ((type == pointer_to(P_CHAR)) && (Token.token == T_STRLIT))
    return(genglobstr(Text));

  if (Token.token == T_INTLIT) {
    switch(type) {
      case P_CHAR: if (Token.intvalue < 0 || Token.intvalue > 255)
                     fatal("Integer literal value too big for char type");
      case P_INT:
      case P_LONG: break;
      default: fatal("Type mismatch: integer literal vs. variable");
    }
  } else
    fatal("Expecting an integer literal value");
  return(Token.intvalue);
}
```

第一个 IF 语句确保我们能够做到：

```c
char *str= "Hello world";
```

它返回存储字符串的地址的标签编号。

对于整数字面量，当赋值给 `char` 变量时，我们会检查范围。对于任何其他词法单元类型，我们会遇到致命错误。

## 符号表结构的更改

上述函数始终返回一个整数，无论它解析什么类型的字面量。现在我们需要在每个变量的符号条目中找一个位置来存储这个值。因此，我在 `defs.h` 中的符号条目结构中添加了（或修改了）以下字段：

```c
// Symbol table structure
struct symtable {
  ...
  int size;              // Total size in bytes of this symbol
  int nelems;            // Functions: # params. Arrays: # elements
  ...
  int *initlist;         // List of initial values
  ...
};
```

对于只有一个初始值的标量，或有多个初始值的数组，我们在 `nelems` 中存储元素计数，并将整数列表附加到 `initlist`。让我们看看标量变量的赋值。

## 标量变量赋值

`scalar_declaration()` 函数的修改如下：

```c
static struct symtable *scalar_declaration(...) {
  ...
    // The variable is being initialised
  if (Token.token == T_ASSIGN) {
    // Only possible for a global or local
    if (class != C_GLOBAL && class != C_LOCAL)
      fatals("Variable can not be initialised", varname);
    scan(&Token);

    // Globals must be assigned a literal value
    if (class == C_GLOBAL) {
      // Create one initial value for the variable and
      // parse this value
      sym->initlist= (int *)malloc(sizeof(int));
      sym->initlist[0]= parse_literal(type);
      scan(&Token);
    }                           // No else code yet, soon
  }

  // Generate any global space
  if (class == C_GLOBAL)
    genglobsym(sym);

  return (sym);
}
```

我们确保赋值只能发生在全局或局部上下文中，我们跳过 '=' 词法单元。我们设置一个恰好一个元素的 `initlist`，并使用此变量的类型调用 `parse_literal()` 来获取字面量值（或字符串的标签编号）。然后我们跳过字面量值以获取下一个词法单元（要么是 ',' 要么是 ';'）。

以前，`sym` 符号表条目是用 `addglob()` 创建的，元素数量被设置为一。我将很快介绍这个变化。

现在我们将 `genglobsym()` 的调用（以前在 `addglob()` 中）移到这里，我们等到初始值存储在 `sym` 条目中后才调用。这确保了我们刚解析的字面量将被放入内存中变量的存储空间中。

### 标量初始化示例

举一个简单的例子：

```c
int x= 5;
char *y= "Hello";
```

生成：

```
        .globl  x
x:
        .long   5

L1:
        .byte   72
        .byte   101
        .byte   108
        .byte   108
        .byte   111
        .byte   0

        .globl  y
y:
        .quad   L1
```

## 符号表代码的更改

在我们讨论数组初始化的解析之前，我们需要先看看符号表代码的更改。正如我之前强调的，我的原始代码没有正确处理变量大小的存储，也没有处理数组中的元素数量。让我们看看我为此做的更改。

首先，我们有一个 bug 修复。在 `types.c` 中：

```c
// Return true if a type is an int type
// of any size, false otherwise
int inttype(int type) {
  return (((type & 0xf) == 0) && (type >= P_CHAR && type <= P_LONG));
}
```

以前，没有对 P_CHAR 的测试，所以 `void` 类型被当作整数类型处理。哎呀！

在 `sym.c` 中，我们现在处理每个变量都有以下事实：

```c
  int size;                     // Total size in bytes of this symbol
  int nelems;                   // Functions: # params. Arrays: # elements
```

稍后，我们将把 `size` 字段用于 `sizeof()` 运算符。现在我们需要在将符号添加到全局或局部符号表时设置这两个字段。

`sym.c` 中的 `newsym()` 函数和所有 `addXX()` 函数现在接受 `nelems` 参数而不是 `size` 参数。对于标量变量，这被设置为一。对于数组，这被设置为列表中的元素数量。对于函数，这被设置为函数参数的数量。对于所有其他符号表，该值未使用。

我们现在在 `newsym()` 中计算 `size` 值：

```c
  // For pointers and integer types, set the size
  // of the symbol. structs and union declarations
  // manually set this up themselves.
  if (ptrtype(type) || inttype(type))
    node->size = nelems * typesize(type, ctype);
```

`typesize()` 查看 `ctype` 指针以获取结构体或联合体的大小，或者调用 `genprimsize()`（它调用 `cgprimsize()`）来获取指针或整数类型的大小。

注意关于结构体和联合体的注释。我们不能使用结构体大小的详细信息调用 `addstruct()`（它调用 `newsym()`），因为：

```c
struct foo {            // We call addglob() here
  int x;
  int y;                // before we know the size of the structure
  int z;
};
```

所以 `decl.c` 中 `composite_declaration()` 的代码现在这样做：

```c
static struct symtable *composite_declaration(...) {
  ...
  // Build the composite type
  if (type == P_STRUCT)
    ctype = addstruct(Text);
  else
    ctype = addunion(Text);
  ...
  // Scan in the list of members
  while (1) {
    ...
  }

  // Attach to the struct type's node
  ctype->member = Membhead;
  ...

  // Set the overall size of the composite type
  ctype->size = offset;
  return (ctype);
}
```

所以，总而言之，符号表条目中的 `size` 字段现在保存变量中所有元素的大小，而 `nelems` 是变量中的元素计数：对于标量是一，对于数组是一些非零正数。

## 数组变量初始化

我们终于可以讨论数组初始化了。我想允许三种形式：

```c
int a[10];                                      // Ten zeroed elements
char b[]= { 'q', 'w', 'e', 'r', 't', 'y' };     // Six elements
char c[10]= { 'q', 'w', 'e', 'r', 't', 'y' };   // Ten elements, zero padded
```

但要阻止声明大小为 N 的数组但初始化值超过 N 个的情况。让我们看看对 `array_declaration()` 的更改。以前，我打算调用一个 `array_initialisation()` 函数，但我决定将所有初始化代码移到 `decl.c` 中的 `array_declaration()` 中。我们将分阶段进行。

```c
// Given the type, name and class of an variable, parse
// the size of the array, if any. Then parse any initialisation
// value and allocate storage for it.
// Return the variable's symbol table entry.
static struct symtable *array_declaration(...) {
  int nelems= -1;       // Assume the number of elements won't be given
  ...
  // Skip past the '['
  scan(&Token);

  // See we have an array size
  if (Token.token == T_INTLIT) {
    if (Token.intvalue <= 0)
      fatald("Array size is illegal", Token.intvalue);
    nelems= Token.intvalue;
    scan(&Token);
  }

  // Ensure we have a following ']'
  match(T_RBRACKET, "]");
```

如果 '[' ']' 词法单元之间有数字，解析它并将 `nelems` 设置为该值。如果没有数字，我们将其保留为 -1 以表示这一点。我们还检查数字是否为正且非零。

```c
    // Array initialisation
  if (Token.token == T_ASSIGN) {
    if (class != C_GLOBAL)
      fatals("Variable can not be initialised", varname);
    scan(&Token);

    // Get the following left curly bracket
    match(T_LBRACE, "{");
```

目前我只处理全局数组。

```c
#define TABLE_INCREMENT 10

    // If the array already has nelems, allocate that many elements
    // in the list. Otherwise, start with TABLE_INCREMENT.
    if (nelems != -1)
      maxelems= nelems;
    else
      maxelems= TABLE_INCREMENT;
    initlist= (int *)malloc(maxelems *sizeof(int));
```

我们创建一个初始列表，要么是 10 个整数，要么是 `nelems`（如果数组被给定了固定大小）。但是，对于没有固定大小的数组，我们无法预测初始化列表会有多大。所以我们必须准备好扩展列表。

```c
    // Loop getting a new literal value from the list
    while (1) {

      // Check we can add the next value, then parse and add it
      if (nelems != -1 && i == maxelems)
        fatal("Too many values in initialisation list");
      initlist[i++]= parse_literal(type);
      scan(&Token);
```

获取下一个字面量值，并确保如果指定了数组大小，我们不会有比数组大小更多的初始值。

```c
      // Increase the list size if the original size was
      // not set and we have hit the end of the current list
      if (nelems == -1 && i == maxelems) {
        maxelems += TABLE_INCREMENT;
        initlist= (int *)realloc(initlist, maxelems *sizeof(int));
      }
```

这就是我们根据需要增加初始化列表大小的地方。

```c
      // Leave when we hit the right curly bracket
      if (Token.token == T_RBRACE) {
        scan(&Token);
        break;
      }

      // Next token must be a comma, then
      comma();
    }
```

解析右花括号或分隔值的逗号。一旦出了循环，我们现在有了一个包含值的 `initlist`。

```c
    // Zero any unused elements in the initlist.
    // Attach the list to the symbol table entry
    for (j=i; j < sym->nelems; j++) initlist[j]=0;
    if (i > nelems) nelems = i;
    sym->initlist= initlist;
  }
```

我们可能没有给出足够的初始化值来满足初始化列表的指定大小，所以将所有未初始化的元素置零。正是在这里，我们将初始化列表附加到符号表条目。

```c
  // Set the size of the array and the number of elements
  sym->nelems= nelems;
  sym->size= sym->nelems * typesize(type, ctype);
  // Generate any global space
  if (class == C_GLOBAL)
    genglobsym(sym);
  return (sym);
}
```

我们终于可以更新符号表条目中的 `nelems` 和 `size` 了。一旦完成，我们就可以调用 `genglobsym()` 来为数组创建内存存储。

## 对 `cgglobsym()` 的更改

在我们看数组初始化的汇编输出之前，我们需要看看 `nelems` 和 `size` 的变化如何影响了生成内存存储汇编的代码。

`genglobsym()` 是一个前端函数，它只是调用 `cgglobsym()`。让我们看看 `cg.c` 中的这个函数：

```c
// Generate a global symbol but not functions
void cgglobsym(struct symtable *node) {
  int size, type;
  int initvalue;
  int i;

  if (node == NULL)
    return;
  if (node->stype == S_FUNCTION)
    return;

  // Get the size of the variable (or its elements if an array)
  // and the type of the variable
  if (node->stype == S_ARRAY) {
    size= typesize(value_at(node->type), node->ctype);
    type= value_at(node->type);
  } else {
    size = node->size;
    type= node->type;
  }
```

目前，数组的 `type` 被设置为基础元素类型的指针。这允许我们做：

```c
  char a[45];
  char *b;
  b= a;         // as they are of same type
```

在生成存储方面，我们需要知道元素的大小，所以我们调用 `value_at()` 来做到这一点。对于标量，`size` 和 `type` 按原样存储在符号表条目中。

```c
  // Generate the global identity and the label
  cgdataseg();
  fprintf(Outfile, "\t.globl\t%s\n", node->name);
  fprintf(Outfile, "%s:\n", node->name);
```

和以前一样。但现在的代码不同了：

```c
  // Output space for one or more elements
  for (i=0; i < node->nelems; i++) {

    // Get any initial value
    initvalue= 0;
    if (node->initlist != NULL)
      initvalue= node->initlist[i];

    // Generate the space for this type
    switch (size) {
      case 1:
        fprintf(Outfile, "\t.byte\t%d\n", initvalue);
        break;
      case 4:
        fprintf(Outfile, "\t.long\t%d\n", initvalue);
        break;
      case 8:
        // Generate the pointer to a string literal
        if (node->initlist != NULL && type== pointer_to(P_CHAR))
          fprintf(Outfile, "\t.quad\tL%d\n", initvalue);
        else
          fprintf(Outfile, "\t.quad\t%d\n", initvalue);
        break;
      default:
        for (int i = 0; i < size; i++)
        fprintf(Outfile, "\t.byte\t0\n");
    }
  }
}

```

对于每个元素，从 `initlist` 获取其初始值，如果没有初始化列表则使用零。根据每个元素的大小，输出字节、长整型或四字。

对于 `char *` 元素，我们在初始化列表中有字符串字面量的标签，因此输出 "L%d"（即标签）而不是整数字面量值。

### 数组初始化示例

这里是数组初始化的一个小例子：

```c
int x[4]= { 1, 4, 17 };
```

生成：

```
        .globl  x
x:
        .long   1
        .long   4
        .long   17
        .long   0
```

## 测试程序

我不会详细讲解测试程序，但 `tests/input89.c` 到 `tests/input99.c` 的程序会检查编译器是否正在生成合理的初始化代码，同时也捕获适当的致命错误。

## 结论与下一步

这真是大量的工作！俗话说，前进三步，后退一步。不过我很高兴，因为对符号表的更改比我以前的设计合理得多。

在我们编译器编写旅程的下一部分，我们将尝试向编译器添加局部变量初始化。[下一步](../41_Local_Var_Init/Readme_zh.md)