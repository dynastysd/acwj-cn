# 第 35 章：C 预处理器

在编译器编写旅程的这一部分，我添加了对外部 C 预处理器的支持，同时还在我们的语言中添加了 `extern` 关键字。

我们现在已经可以为自己的程序编写
[头文件](https://www.tutorialspoint.com/cprogramming/c_header_files.htm)，
也可以在头文件中添加注释。必须承认，这感觉很好。

## C 预处理器

我不想过多地讲述 C 预处理器本身，尽管它是任何 C 环境中非常重要的一部分。我建议你阅读这两篇文章：

 + [C Preprocessor](https://en.wikipedia.org/wiki/C_preprocessor) at *Wikipedia*
 + [C Preprocessor and Macros](https://www.programiz.com/c-programming/c-preprocessor-macros) at *www.programiz.com*

## 集成 C 预处理器

在其他编译器如 [SubC](http://www.t3x.org/subc/) 中，预处理器是直接内置在语言中的。而在这里，我决定使用外部的系统 C 预处理器，通常是 [Gnu C 预处理器](https://gcc.gnu.org/onlinedocs/cpp/)。

在展示如何实现之前，我们首先需要了解预处理器在运行时会插入哪些行。

考虑这个简短的程序（带行号）：

```c
1 #include <stdio.h>
2 
3 int main() {
4   printf("Hello world\n");
5   return(0);
6 }
```

这是预处理器处理该文件后我们（编译器）可能收到的内容：

```c
# 1 "z.c"
# 1 "<built-in>"
# 1 "<command-line>"
# 1 "z.c"
# 1 "include/stdio.h" 1
# 1 "include/stddef.h" 1

typedef long size_t;
# 5 "include/stdio.h" 2

typedef char * FILE;

FILE *fopen(char *pathname, char *mode);
...
# 2 "z.c" 2

int main() {
  printf("Hello world\n");
  return(0);
}
```

每个预处理器行以 '#' 开头，后跟下一行的行号，然后是这行代码来自的文件名。有些行末尾的数字我不完全清楚它们的含义。我猜测，当一个文件包含另一个文件时，这些数字代表的是进行包含的那个文件的行号。

以下是我将预处理器与编译器集成的方法。我将使用 `popen()` 来打开一个到预处理器进程的管道，我们告诉预处理器处理我们的输入文件。然后我们将修改词法扫描器来识别预处理器行，并设置当前行号和正在处理的文件名。

## 对 `main.c` 的修改

我们在 `data.h` 中定义了一个新的全局变量 `char *Infilename`。在 `main.c` 的 `do_compile()` 函数中，我们现在这样做：

```c
// Given an input filename, compile that file
// down to assembly code. Return the new file's name
static char *do_compile(char *filename) {
  char cmd[TEXTLEN];
  ...
  // Generate the pre-processor command
  snprintf(cmd, TEXTLEN, "%s %s %s", CPPCMD, INCDIR, filename);

  // Open up the pre-processor pipe
  if ((Infile = popen(cmd, "r")) == NULL) {
    fprintf(stderr, "Unable to open %s: %s\n", filename, strerror(errno));
    exit(1);
  }
  Infilename = filename;
```

我认为这是一段直接明了的代码，除了我还没有解释 `CPPCMD` 和 `INCDIR` 是从哪里来的。

`CPPCMD` 在 `defs.h` 中定义为预处理器命令的名称：

```c
#define CPPCMD "cpp -nostdinc -isystem "
```

这告诉 Gnu 预处理器不要使用标准 include 目录 `/usr/include`：相反，`-isystem` 告诉预处理器使用命令行上的下一个内容，也就是 `INCDIR`。

`INCDIR` 实际上是在 `Makefile` 中定义的，因为这是放置可在配置时更改的内容的常用位置：

```make
# Define the location of the include directory
# and the location to install the compiler binary
INCDIR=/tmp/include
BINDIR=/tmp
```

编译器二进制文件现在使用这个 Makefile 规则编译：

```make
cwj: $(SRCS) $(HSRCS)
        cc -o cwj -g -Wall -DINCDIR=\"$(INCDIR)\" $(SRCS)
```

这会将 `/tmp/include` 的值作为 `INCDIR` 传递给编译。那么，`/tmp/include` 是什么时候创建的，又有什么内容被放在那里呢？

## 我们的第一批头文件

在这个区域的 `include/` 目录中，我已经开始制作一些我们的编译器能够处理的头文件。我们无法使用真正的系统头文件，因为它们包含这样的行：

```c
extern int _IO_feof (_IO_FILE *__fp) __attribute__ ((__nothrow__ , __leaf__));
extern int _IO_ferror (_IO_FILE *__fp) __attribute__ ((__nothrow__ , __leaf__));
```

这会导致我们的编译器出错！Makefile 中现在有一条规则将我们自己的头文件复制到 `INCDIR` 目录：

```make
install: cwj
        mkdir -p $(INCDIR)
        rsync -a include/. $(INCDIR)
        cp cwj $(BINDIR)
        chmod +x $(BINDIR)/cwj
```

## 扫描预处理器输入

现在我们从预处理器处理输入文件后的输出中读取，而不是直接读取文件。我们现在需要识别预处理器行，并设置下一行的行号以及该行来自的文件名。

我已经修改了扫描器来完成这项工作，因为它已经处理了行号递增。所以在 `scan.c` 中，我对 `scan()` 函数做了这个修改：

```c
// Get the next character from the input file.
static int next(void) {
  int c, l;

  if (Putback) {                        // Use the character put
    c = Putback;                        // back if there is one
    Putback = 0;
    return (c);
  }

  c = fgetc(Infile);                    // Read from input file

  while (c == '#') {                    // We've hit a pre-processor statement
    scan(&Token);                       // Get the line number into l
    if (Token.token != T_INTLIT)
      fatals("Expecting pre-processor line number, got:", Text);
    l = Token.intvalue;

    scan(&Token);                       // Get the filename in Text
    if (Token.token != T_STRLIT)
      fatals("Expecting pre-processor file name, got:", Text);

    if (Text[0] != '<') {               // If this is a real filename
      if (strcmp(Text, Infilename))     // and not the one we have now
        Infilename = strdup(Text);      // save it. Then update the line num
      Line = l;
    }

    while ((c = fgetc(Infile)) != '\n'); // Skip to the end of the line
    c = fgetc(Infile);                  // and get the next character
  }

  if ('\n' == c)
    Line++;                             // Increment line count
  return (c);
}
```

我们使用 'while' 循环，因为可能会有连续的预处理器行。我们很幸运地可以递归调用 `scan()` 来分别将行号扫描为 T_INTLIT 和文件名为 T_STRLIT。

代码忽略用 '<' ... '>' 包围的文件名，因为这些不是真实的文件名。我们确实需要用 `strdup()` 来复制文件名，因为它在全局变量 `Text` 中，会被覆盖。但是，如果 `Text` 中的名称已经与 `Infilename` 中的相同，就不需要复制了。

一旦我们获得了行号和文件名，就读取到该行末尾再往后读取一个字符，然后回到我们原来的字符扫描代码。

这就是将 C 预处理器与编译器集成所需的全部工作。我之前担心这会很复杂，但实际上并不是。

## 防止不想要的函数/变量重复声明

许多头文件会包含其他头文件，所以一个头文件可能会被多次包含的可能性很大。这将导致相同函数和/或全局变量的重复声明。

为了防止这种情况，我使用了正常的头文件机制，在头文件第一次被包含时定义一个头文件特定的宏。这样可以防止头文件的内容在第二次被包含。

举个例子，以下是当前 `include/stdio.h` 中的内容：

```c
#ifndef _STDIO_H_
# define _STDIO_H_

#include <stddef.h>

// This FILE definition will do for now
typedef char * FILE;

FILE *fopen(char *pathname, char *mode);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(void *ptr, size_t size, size_t nmemb, FILE *stream);
int fclose(FILE *stream);
int printf(char *format);
int fprintf(FILE *stream, char *format);

#endif  // _STDIO_H_
```

一旦 `_STDIO_H_` 被定义，它就会阻止这个文件的内容被第二次包含。

## `extern` 关键字

现在我们有了工作的预处理器，我认为现在是时候给语言添加 `extern` 关键字了。这允许我们定义一个全局变量但不为其生成存储空间：假设该变量已在另一个源文件中声明为全局的。

添加 `extern` 实际上会影响多个文件。不是大影响，但是广泛的影响。让我们看看。

### 新的词法单元和关键字

所以我们在 `scan.c` 中有一个新的关键字 `extern` 和一个新的词法单元 T_EXTERN。代码一如既往地供你阅读。

### 一个新的类别

在 `defs.h` 中我们有一个新的存储类别：

```c
// Storage classes
enum {
  C_GLOBAL = 1,                 // Globally visible symbol
  ...
  C_EXTERN,                     // External globally visible symbol
  ...
};
```

我这样做的原因是因为在 `sym.c` 中我们已经有了一段处理全局符号的代码：

```c
// Create a symbol node to be added to a symbol table list.
struct symtable *newsym(char *name, int type, struct symtable *ctype,
                        int stype, int class, int size, int posn) {
  // Get a new node
  struct symtable *node = (struct symtable *) malloc(sizeof(struct symtable));
  // Fill in the values
  ...
    // Generate any global space
  if (class == C_GLOBAL)
    genglobsym(node);
```

我们希望 `extern` 符号被添加到全局列表，但我们不想调用 `genglobsym()` 为它们创建存储。所以，我们需要用不是 C_GLOBAL 的类别来调用 `newsym()`。

### 对 `sym.c` 的修改

为此，我修改了 `addglob()` 使其接受一个 `class` 参数，这个参数被传递给 `newsym()`：

```c
// Add a symbol to the global symbol list
struct symtable *addglob(char *name, int type, struct symtable *ctype,
                         int stype, int class, int size) {
  struct symtable *sym = newsym(name, type, ctype, stype, class, size, 0);
  appendsym(&Globhead, &Globtail, sym);
  return (sym);
}
```

这意味着，在编译器中每个调用 `addglob()` 的地方，我们现在都必须传入一个 `class` 值。之前，`addglob()` 会显式地将 C_GLOBAL 传递给 `newsym()`。现在，我们必须将我们想要的 `class` 值传递给 `addglob()`。

### `extern` 关键字与我们的语法

就我们语言的语法而言，我将强制执行 `extern` 关键字必须出现在类型描述中任何其他词之前的规则。稍后，我将把 `static` 添加到词列表中。我们在前面部分看到的 [C 的 BNF 语法](https://www.lysator.liu.se/c/ANSI-C-grammar-y.html) 有这些产生式规则：

```
storage_class_specifier
        : TYPEDEF
        | EXTERN
        | STATIC
        | AUTO
        | REGISTER
        ;

type_specifier
        : VOID
        | CHAR
        | SHORT
        | INT
        | LONG
        | FLOAT
        | DOUBLE
        | SIGNED
        | UNSIGNED
        | struct_or_union_specifier
        | enum_specifier
        | TYPE_NAME
        ;

declaration_specifiers
        : storage_class_specifier
        | storage_class_specifier declaration_specifiers
        | type_specifier
        | type_specifier declaration_specifiers
        | type_qualifier
        | type_qualifier declaration_specifiers
        ;
```

我认为这允许 `extern` 出现在类型规范的任何位置。算了，我们是在构建 C 语言的一个子集！

### 解析 `extern` 关键字

与过去五六个部分一样，我再次对 `decl.c` 中的 `parse_type()` 进行了修改：

```c
int parse_type(struct symtable **ctype, int *class) {
  int type, exstatic=1;

  // See if the class has been changed to extern (later, static)
  while (exstatic) {
    switch (Token.token) {
      case T_EXTERN: *class= C_EXTERN; scan(&Token); break;
      default: exstatic= 0;
    }
  }
  ...
}
```

注意现在 `parse_type()` 有第二个参数 `int *class`。
这允许调用者传入类型的初始存储类别（可能是 C_GLOBAL、G_LOCAL 或 C_PARAM）。如果我们在 `parse_type()` 中看到 `extern` 关键字，我们可以将其更改为 T_EXTERN。另外抱歉，我想不出一个好的名字来命名控制 'while' 循环的布尔标志。

### `parse_type()` 和 `addglob()` 的调用者

所以我们已经修改了 `parse_type()` 和 `addglob()` 的参数。现在我们必须在编译器中找到所有调用这两个函数的地方，并确保向它们传递适当的 `class` 值。

在 `decl.c` 中的 `var_declaration_list()` 中，当我们解析变量或参数列表时，我们已经获得了这些变量的存储类别：

```c
static int var_declaration_list(struct symtable *funcsym, int class,
                                int separate_token, int end_token);
```

所以我们可以将 `class` 传递给 `parse_type()`，它可能会改变它，然后用实际的类别调用 `var_declaration()`：

```c
    ...
    // Get the type and identifier
    type = parse_type(&ctype, &class);
    ident();
    ...
    // Add a new parameter to the right symbol table list, based on the class
    var_declaration(type, ctype, class);
```

在 `var_declaration()` 中：

```c
      switch (class) {
        case C_EXTERN:
        case C_GLOBAL:
          sym = addglob(Text, type, ctype, S_VARIABLE, class, 1);
        ...
      }
```

对于局部变量，我们需要关注 `stmt.c` 中的 `single_statement()`。我还需要指出，我之前忘记在这里添加结构体、共用体、枚举和 typedef 的 case。

```c
// Parse a single statement and return its AST
static struct ASTnode *single_statement(void) {
  int type, class= C_LOCAL;
  struct symtable *ctype;

  switch (Token.token) {
    case T_IDENT:
      // We have to see if the identifier matches a typedef.
      // If not do the default code in this switch statement.
      // Otherwise, fall down to the parse_type() call.
      if (findtypedef(Text) == NULL)
        return (binexpr(0));
    case T_CHAR:
    case T_INT:
    case T_LONG:
    case T_STRUCT:
    case T_UNION:
    case T_ENUM:
    case T_TYPEDEF:
      // The beginning of a variable declaration.
      // Parse the type and get the identifier.
      // Then parse the rest of the declaration
      // and skip over the semicolon
      type = parse_type(&ctype, &class);
      ident();
      var_declaration(type, ctype, class);
      semi();
      return (NULL);            // No AST generated here
      ...
   }
   ...
}
```

注意我们从 `class= C_LOCAL` 开始，但它可能在传递给 `var_declaration()` 之前被 `parse_type()` 修改。这允许我们写这样的代码：

```c
int main() {
  extern int foo;
  ...
}
```

## 测试代码

我有一个测试程序 `test/input70.c`，它使用我们的一个新头文件来确认预处理器工作正常：

```c
#include <stdio.h>

typedef int FOO;

int main() {
  FOO x;
  x= 56;
  printf("%d\n", x);
  return(0);
}
```

我曾希望 `errno` 仍然是一个普通的整数，这样我就可以在 `include/errno.h` 中声明 `extern int errno;`。但显然，`errno` 现在是一个函数而不是全局整数变量。我想这告诉你 a) 我有多老，以及 b) 我上次写 C 代码是多久以前的事了。

## 结论与下一步

我觉得我们在这里又达到了一个里程碑。我们现在有了外部变量和头文件。这也意味着，*终于*，我们可以在源文件中添加注释了。这真的让我很高兴。

我们的代码已经超过 4100 行，其中大约 2800 行不是注释也不是空白。我不知道要让编译器能够自编译还需要多少行代码，但我猜测在 7000 到 9000 行之间。我们拭目以待！

在我们编译器编写旅程的下一部分，我们将向循环结构中添加 `break` 和 `continue` 关键字。[下一步](../36_Break_Continue/Readme_zh.md)
