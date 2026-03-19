# 第 1 章：词法扫描简介

我们从零开始编译器编写之旅，首先从一个简单的词法扫描器开始。正如我在上一章提到的，扫描器的工作是识别输入语言中的词法元素，即 **词法单元（token）**。

我们从一个只有五种词法元素的语言开始：

 + 四个基本数学运算符：`*`、`/`、`+` 和 `-`
 + 十进制整数，即包含 1 个或多个数字 `0` .. `9` 的数

我们扫描到的每个 token 都会存储在以下结构体中（来自 `defs.h`）：

```c
// Token 结构体
struct token {
  int token;
  int intvalue;
};
```

其中 `token` 字段可以是以下值之一（来自 `defs.h`）：

```c
// Token 类型
enum {
  T_PLUS, T_MINUS, T_STAR, T_SLASH, T_INTLIT
};
```

当 token 是 `T_INTLIT`（即整数字面量）时，`intvalue` 字段将保存我们扫描到的整数值。

## `scan.c` 中的函数

`scan.c` 文件包含词法扫描器的函数。我们将每次从输入文件读取一个字符。然而，有时候我们需要"放回"一个字符，因为我们可能在输入流中读得太前了。我们还需要跟踪当前所在的行号，以便在调试消息中打印行号。所有这些都由 `next()` 函数完成：

```c
// 从输入文件获取下一个字符。
static int next(void) {
  int c;

  if (Putback) {                // 如果有放回的字符，
    c = Putback;                // 就使用它
    Putback = 0;
    return c;
  }

  c = fgetc(Infile);            // 从输入文件读取
  if ('\n' == c)
    Line++;                     // 增加行计数
  return c;
}
```

`Putback` 和 `Line` 变量与输入文件指针一起定义在 `data.h` 中：

```c
extern_ int     Line;
extern_ int     Putback;
extern_ FILE    *Infile;
```

所有 C 文件都会包含这个定义，其中 `extern_` 会被替换为 `extern`。但 `main.c` 会去掉 `extern_`；因此，这些变量将"属于" `main.c`。

那么，如何将一个字符放回输入流呢？如下：

```c
// 放回一个不需要的字符
static void putback(int c) {
  Putback = c;
}
```

## 忽略空白符

我们需要一个函数，它读取并静默跳过空白字符，直到遇到一个非空白字符，然后返回它。如下：

```c
// 跳过我们不需要处理的输入，
// 即空白符、换行符。返回我们需要的
// 第一个字符。
static int skip(void) {
  int c;

  c = next();
  while (' ' == c || '\t' == c || '\n' == c || '\r' == c || '\f' == c) {
    c = next();
  }
  return (c);
}
```

## 扫描 Token：`scan()`

现在我们可以读取字符并跳过空白符；如果我们读得太前了，也可以放回一个字符。我们可以编写第一个词法扫描器了：

```c
// 扫描并返回在输入中找到的下一个 token。
// 如果找到有效 token 返回 1，如果没有 token 剩余返回 0。
int scan(struct token *t) {
  int c;

  // 跳过空白符
  c = skip();

  // 根据输入字符确定 token 类型
  switch (c) {
  case EOF:
    return (0);
  case '+':
    t->token = T_PLUS;
    break;
  case '-':
    t->token = T_MINUS;
    break;
  case '*':
    t->token = T_STAR;
    break;
  case '/':
    t->token = T_SLASH;
    break;
  default:
    // 更多内容即将添加
  }

  // 我们找到了一个 token
  return (1);
}
```

这就是简单单字符 token 的处理方式：对于每个识别的字符，将其转换为 token。你可能会问：为什么不直接把识别的字符放入 `struct token`？答案是，Later 我们还需要识别多字符 token，例如 `==` 和关键字如 `if` 和 `while`。所以，有一个 token 值的枚举列表会让工作更轻松。

## 整数字面量

事实上，我们已经面临这种情况，因为还需要识别整数字面量，如 `3827` 和 `87731`。下面是 `switch` 语句中缺失的 `default` 代码：

```c
  default:

    // 如果是数字，扫描
    // 字面整数值
    if (isdigit(c)) {
      t->intvalue = scanint(c);
      t->token = T_INTLIT;
      break;
    }

    printf("Unrecognised character %c on line %d\n", c, Line);
    exit(1);
```

一旦遇到十进制数字字符，我们就用第一个字符调用辅助函数 `scanint()`。它会返回扫描到的整数值。为此，它必须依次读取每个字符，检查它是否是一个合法的数字，并构建最终的数字。代码如下：

```c
// 从输入文件扫描并返回一个整数字面量值。
static int scanint(int c) {
  int k, val = 0;

  // 将每个字符转换为一个整数值
  while ((k = chrpos("0123456789", c)) >= 0) {
    val = val * 10 + k;
    c = next();
  }

  // 遇到一个非整数字符，将其放回。
  putback(c);
  return val;
}
```

我们从零 `val` 值开始。每次在 `0` 到 `9` 的集合中得到一个字符时，我们用 `chrpos()` 将其转换为一个 `int` 值。我们将 `val` 乘以 10，然后加上这个新数字。

例如，如果我们有字符 `3`、`2`、`8`，我们这样做：

 + `val= 0 * 10 + 3`，即 3
 + `val= 3 * 10 + 2`，即 32
 + `val= 32 * 10 + 8`，即 328

就在最后，你注意到对 `putback(c)` 的调用了吗？
此时我们发现了一个非十进制数字的字符。我们不能简单地丢弃它，但幸运的是我们可以将它放回输入流，供后续消费。

你可能还会问：为什么不直接用 `c` 减去 ASCII 值 `'0'` 使其成为一个整数？答案是，以后我们能够使用 `chrpos("0123456789abcdef")` 来转换十六进制数字。

以下是 `chrpos()` 的代码：

```c
// 返回字符 c 在字符串 s 中的位置，
// 如果未找到则返回 -1
static int chrpos(char *s, int c) {
  char *p;

  p = strchr(s, c);
  return (p ? p - s : -1);
}
```

这就是 `scan.c` 中词法扫描器代码的全部内容。

## 使用扫描器

`main.c` 中的代码将上述扫描器投入使用。`main()` 函数打开一个文件，然后扫描其中的 token：

```c
void main(int argc, char *argv[]) {
  ...
  init();
  ...
  Infile = fopen(argv[1], "r");
  ...
  scanfile();
  exit(0);
}
```

`scanfile()` 循环遍历直到没有新 token，并打印每个 token 的详细信息：

```c
// 可打印的 token 列表
char *tokstr[] = { "+", "-", "*", "/", "intlit" };

// 循环扫描输入文件中的所有 token。
// 打印找到的每个 token 的详细信息。
static void scanfile() {
  struct token T;

  while (scan(&T)) {
    printf("Token %s", tokstr[T.token]);
    if (T.token == T_INTLIT)
      printf(", value %d", T.intvalue);
    printf("\n");
  }
}
```

## 一些示例输入文件

我提供了一些示例输入文件，这样你可以看到扫描器在每个文件中找到的 token，以及扫描器拒绝哪些输入文件。

```
$ make
cc -o scanner -g main.c scan.c

$ cat input01
2 + 3 * 5 - 8 / 3

$ ./scanner input01
Token intlit, value 2
Token +
Token intlit, value 3
Token *
Token intlit, value 5
Token -
Token intlit, value 8
Token /
Token intlit, value 3

$ cat input04
23 +
18 -
45.6 * 2
/ 18

$ ./scanner input04
Token intlit, value 23
Token +
Token intlit, value 18
Token -
Token intlit, value 45
Unrecognised character . on line 3
```

## 结论与下一步

我们从小处着手，有一个简单的词法扫描器，可以识别四个主要数学运算符以及整数字面量。我们看到，需要跳过空白符，并在输入中读得太前时放回字符。

单字符 token 很容易扫描，但多字符 token 就比较困难了。但最后，`scan()` 函数从输入文件中返回一个 `struct token` 变量中的下一个 token：

```c
struct token {
  int token;
  int intvalue;
};
```

在我们编译器编写的下一步中，我们将构建一个递归下降解析器来解释输入文件的语法，并计算和打印每个文件的最终值。[下一步](../02_Parser/Readme_zh.md)
