# 第 39 章：变量初始化，第一部分

我们可以在编译器接受的语言中声明变量，但无法同时对它们进行初始化。所以在 这一部分（以及接下来的几个部分）中，我将致力于解决这个问题。

在真正开始实现之前，现在就值得好好考虑一下这个问题，因为希望我能找到一种 方法来共享部分代码。因此，我会在下面做一些"脑暴"来帮助自己理清思路。

目前，我们可以在三个地方声明变量：

  + 全局变量声明在任何函数之外
  + 函数参数在参数列表中声明
  + 局部变量在函数内部声明

每种声明都包含变量类型的描述及其名称。

在初始化方面：

  + 我们无法初始化函数参数，因为它们的值是从函数调用者的参数中复制过来的。
  + 全局变量不能用表达式初始化，因为没有函数可以让表达式的汇编代码在其中运行。
  + 局部变量可以用表达式初始化。

我们还希望在类型定义之后有一个变量名列表。这意味着会有一些相似之处和 需要处理的差异。用半 BNF 语法表示：

```
global_declaration: type_definition global_var_list ';' ;

global_var_list: global_var
               | global_var ',' global_var_list  ;

global_var: variable_name
          | variable_name '=' literal_value ;

local_declaration: type_definition local_var_list ';' ;

local_var_list: local_var
              | local_var ',' local_var_list  ;

local_var: variable_name
         | variable_name '=' expression ;

parameter_list: parameter
              | parameter ',' parameter_list ;

parameter: type_definition variable_name ;
```

以下是一组我希望在我们编译器中支持的示例。

### 全局声明

```c
  int   x= 5;
  int   a, b= 7, c, d= 6;
  char *e, f;                           // e 是指针，f 不是！
  char  g[]= "Hello", *h= "foo";
  int   j[]= { 1, 2, 3, 4, 5 };
  char *k[]= { "fish", "cat", "ball" };
  int   l[70];
```

我添加的注释有深远的含义。我们必须解析前面的类型，并且对*每个*后续的变量， 解析任何前缀 '*' 或后缀 '[ ]' 来确定它是指针还是数组。

我只处理如上所示的单维度初始化值列表。

### 局部声明

上述示例也适用，但我们还应该能够进行这些局部声明：

```c
  int u= x + 3;
  char *v= k[0];
  char *w= k[b-6];
  int y= 2*b+c, z= l[d] + j[2*x+5];
```

我本来打算支持解析
`int list[]= { x+2, a+b, c*d, u+j[3], j[x] + j[a] };`
但这看起来处理起来简直是噩梦，所以我想我还是只支持字面量值列表，或者 甚至不允许在局部作用域中进行数组初始化。

## 现在怎么办？

现在，看了上面的示例之后，我有点害怕！我觉得我可以做全局变量初始化，但我 必须重写解析列表中每个变量类型的方式。然后才能解析 '='。

如果处于全局作用域，我会调用一个函数来解析字面量值。

如果在局部作用域，我无法使用现有的 `binexpr()` 函数，因为它在内部 解析左侧的变量名并为其生成左值 AST 节点。也许我可以手动构建这个左值 AST 节点， 然后将指针传递给它。接着我可以在 `binexpr()` 中添加代码：

```
  如果我们得到了一个左值指针 {
    将左侧设置为此指针
  } else {
    左侧 = prefix()
    处理运算符词法单元
  }
  现有代码的其余部分
```

好的，我有一个大致的计划。我会先做一些重构。第一项任务是弄清楚 如何重写类型和变量名的解析，以便我们可以解析它们的列表。

## 重构的一瞥

所以我刚刚完成了代码的重构，感觉就像只是重新排列了代码，但事实并非完全如此。 所以我会向你们展示所有新函数如何相互调用，然后概述每个函数的作用。

我画了新 `decl.c` 中代码的调用图：

![](Figs/decl_call_graph.png)

在最顶层，`global_declarations()` 被调用来解析任何全局的东西。它只是循环 调用 `declaration_list()`。或者，当我们处于函数中并遇到了类型词法单元（`int`、`char` 等）时。我们 调用 `declaration_list()` 来解析应该是一个变量的东西。

`declaration_list()` 是新的。它调用 `parse_type()` 来获取类型（例如 `int`、`char`、 结构体、联合体或 typedef 等）。这是列表的*基础类型*，但列表中的每一项 都可以修改这个类型。举个例子：

```c
  int a, *b, c[40], *d[100];
```

所以在 `declaration_list()` 中，我们循环处理列表中的每个声明。对于每个声明， 我们调用 `parse_stars()` 来查看基础类型是如何被修改的。此时我们可以解析 各个声明的标识符，这是在 `symbol_declaration()` 中完成的。根据后面跟随的词法单元， 我们调用：

  + `function_declaration()` 处理函数
  + `array_declaration` 处理数组，或
  + `scalar_delaration` 处理标量变量

在函数声明中，可能有参数，所以会调用 `parameter_declaration_list()` 来处理。 当然，参数列表也是一个声明，所以我们调用 `declaration_list()` 来处理它！

在左边我们有 `parse_type()`。它获取像 `int` 和 `char` 这样的普通类型， 但这也是解析结构体、联合体、枚举和 typedef 等新类型的地方。

在 `typedef_declaration()` 中解析 typedef 应该很容易，因为有一个已存在的 类型供我们别名。然而，我们也可以这样写：

```c
typedef char * charptr;
```

因为 `parse_type()` 不处理任何 `*` 词法单元，`typedef_declaration()` 必须 手动调用 `parse_stars()` 来查看基础类型在创建别名之前是如何被修改的。

任何枚举声明都由 `enum_declaration` 处理。对于结构体和联合体，我们调用 `composite_declaration()`。猜猜怎么着？！新结构体或联合体内部的成员形成了一个 成员声明列表，所以我们调用 `declaration_list()` 来解析它们！

## 回归测试

我很高兴现在有大约八十个单独的测试，因为如果没有方法确认新代码仍然产生 与以前相同的错误或汇编输出，我根本无法安全地重构 `decl.c`。

## 新功能

虽然这段旅程的大部分是重新设计以为变量初始化做准备，但我们现在支持 在全局和局部变量声明中使用列表。因此，我有新的测试：

```c
// tests/input84.c，局部变量
int main() {
  int x, y;
  x=2; y=3;
  ..
}

//input88.c，全局变量
struct foo {
  int x;
  int y;
} fred, mary;
```

## 结论与下一步

现在我对编译器能够解析类型后面的一系列变量（例如 `int a, *b, **c;`） 感到更高兴了。我还在代码中添加了注释，说明我必须编写哪些赋值功能 来配合声明。

在我们编译器编写旅程的下一部分，我们将尝试向编译器添加带赋值的 全局变量声明。[下一步](../40_Var_Initialisation_pt2/Readme_zh.md)
