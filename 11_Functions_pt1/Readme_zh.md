# 第 11 章：函数，第一部分

我想开始为我们的语言实现函数，但我知道这将涉及很多步骤。我们必须处理的一些事情包括：

 + 数据类型：`char`、`int`、`long` 等
 + 每个函数的返回类型
 + 每个函数的参数数量
 + 函数局部变量与全局变量

在我们的旅程中，这部分内容太多无法完成。所以我打算在这里做的是，让我们能够*声明*不同的函数。只有生成的 `main()` 函数会运行，但我们将能够为多个函数生成代码。

希望很快，我们编译器识别的语言将足够成为 C 的一个子集，我们的输入可以被"真正的"C 编译器识别。但现在还不行。

## 简化的函数语法

这绝对是一个占位符，这样我们就可以解析看起来像函数的东西。一旦完成，我们就可以添加其他重要的内容：类型、返回类型、参数等。

所以，现在我将添加一个这样的 BNF 函数语法：

```
 function_declaration: 'void' identifier '(' ')' compound_statement   ;
```

所有函数都将声明为 `void` 且没有参数。我们也不会引入调用函数的能力，所以只有 `main()` 函数会执行。

我们需要一个新的关键字 `void` 和一个新的词法单元 T_VOID，两者都很容易添加。

## 解析简化的函数语法

新的函数语法非常简单，我们可以写一个很好的小函数来解析它（在 `decl.c` 中）：

```c
// Parse the declaration of a simplistic function
struct ASTnode *function_declaration(void) {
  struct ASTnode *tree;
  int nameslot;

  // Find the 'void', the identifier, and the '(' ')'.
  // For now, do nothing with them
  match(T_VOID, "void");
  ident();
  nameslot= addglob(Text);
  lparen();
  rparen();

  // Get the AST tree for the compound statement
  tree= compound_statement();

  // Return an A_FUNCTION node which has the function's nameslot
  // and the compound statement sub-tree
  return(mkastunary(A_FUNCTION, tree, nameslot));
}
```

这将进行语法检查和 AST 构建，但这里几乎没有语义错误检查。如果一个函数被重新声明怎么办？好吧，我们还不会注意到这一点。

## 修改 `main()`

有了上面的函数，我们现在可以重写 `main()` 中的一些代码来一个接一个地解析多个函数：

```c
  scan(&Token);                 // Get the first token from the input
  genpreamble();                // Output the preamble
  while (1) {                   // Parse a function and
    tree = function_declaration();
    genAST(tree, NOREG, 0);     // generate the assembly code for it
    if (Token.token == T_EOF)   // Stop when we have reached EOF
      break;
  }
```

注意，我删除了 `genpostamble()` 函数调用。这是因为它的输出在技术上是为 `main()` 生成的汇编的后置代码。我们现在需要一些代码生成函数来生成函数的开头和函数的结尾。

## 通用代码生成函数

现在我们有了一个 A_FUNCTION AST 节点，我们最好在通用代码生成器 `gen.c` 中添加一些代码来处理它。上面看到，这是一个只有一个子节点的*一元* AST 节点：

```c
  // Return an A_FUNCTION node which has the function's nameslot
  // and the compound statement sub-tree
  return(mkastunary(A_FUNCTION, tree, nameslot));
```

子节点包含保存函数体的复合语句的子树。我们需要在生成复合语句的代码*之前*生成函数的开头。所以 `genAST()` 中的代码如下：

```c
    case A_FUNCTION:
      // Generate the function's preamble before the code
      cgfuncpreamble(Gsym[n->v.id].name);
      genAST(n->left, NOREG, n->op);
      cgfuncpostamble();
      return (NOREG);
```

## x86-64 代码生成

现在我们到了必须为每个函数生成设置栈和帧指针的代码的地方，同时在函数结束时撤销这些操作并返回到函数的调用者。

我们已经在 `cgpreamble()` 和 `cgpostamble()` 中有了这些代码，但 `cgpreamble()` 还有 `printint()` 函数的汇编代码。因此，需要将这些汇编代码片段分离到 `cg.c` 中的新函数：

```c
// Print out the assembly preamble
void cgpreamble() {
  freeall_registers();
  // Only prints out the code for printint()
}

// Print out a function preamble
void cgfuncpreamble(char *name) {
  fprintf(Outfile,
          "\t.text\n"
          "\t.globl\t%s\n"
          "\t.type\t%s, @function\n"
          "%s:\n" "\tpushq\t%%rbp\n"
          "\tmovq\t%%rsp, %%rbp\n", name, name, name);
}

// Print out a function postamble
void cgfuncpostamble() {
  fputs("\tmovl $0, %eax\n" "\tpopq     %rbp\n" "\tret\n", Outfile);
}
```

## 测试函数生成功能

我们有一个新的测试程序 `tests/input08`，它开始看起来像一个 C 程序（除了 `print` 语句）：

```c
void main()
{
  int i;
  for (i= 1; i <= 10; i= i + 1) {
    print i;
  }
}
```

要测试这个，执行 `make test8`，它会执行：

```
cc -o comp1 -g cg.c decl.c expr.c gen.c main.c misc.c scan.c
    stmt.c sym.c tree.c
./comp1 tests/input08
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

我不会查看汇编输出，因为它与上一部分中为 FOR 循环测试生成的代码相同。

但是，我在所有之前的测试输入文件中添加了 `void main()`，因为语言要求在复合语句代码之前进行函数声明。

测试程序 `tests/input09` 在其中声明了两个函数。编译器愉快地为每个函数生成有效的汇编代码，但目前我们无法运行第二个函数的代码。

## 结论与下一步

我们已经为向语言添加函数开了一个好头。目前，它只是一个简化的函数声明。

在编译器编写的下一步中，我们将开始向编译器添加类型的过程。[下一步](../12_Types_pt1/Readme_zh.md)