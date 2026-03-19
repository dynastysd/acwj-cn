# 第 0 章：引言

我决定踏上编译器编写之旅。回顾过去，我曾编写过一些[汇编器](https://github.com/DoctorWkt/pdp7-unix/blob/master/tools/as7)，也编写过一种[无类型语言的简单编译器](https://github.com/DoctorWkt/h-compiler)。但我从未编写过一个能够编译自身的编译器。这正是我此行的目标。

在这个过程中，我会记录下我的工作，这样其他人也可以跟随学习。这也有助于我理清自己的思想和想法。希望你觉得——以及我觉得——这会是有用的！

## 旅程的目标

以下是我在此旅程中的目标和不做的事情：

 + 编写一个**自编译编译器**。我认为，如果一个编译器能够编译自身，它就有权称自己为一个*真正的*编译器。
 + 至少面向一个真实的硬件平台。我见过一些编译器，它们只为假想的机器生成代码。我希望我的编译器能在真实硬件上运行。此外，如果可能的话，我希望将编译器设计为支持多种不同硬件平台的**后端**。
 + 实践优先于研究。编译器领域有大量的研究成果。我希望从零开始这个旅程，所以我倾向于采用实践方法，而不是理论优先的方法。即便如此，有时我仍然需要引入（和实现）一些基于理论的东西。
 + 遵循 KISS 原则：Keep it simple, stupid！（保持简单，笨蛋！）我在这里一定会用到 Ken Thompson 的原则："当你犹豫不决时，使用蛮力。"
 + 迈出许多小步来达到最终目标。我会把整个旅程分解成许多简单的步骤，而不是大步跨越。这会使编译器每次新增的内容都成为一小口、易消化的事情。

## 目标语言

目标语言的选择是困难的。如果我选择 Python、Go 等高级语言，那么我就必须实现大量的库和类，因为它们是语言的内置部分。

我可以为 Lisp 这样的语言编写编译器，但这些语言可以[很容易地实现](ftp://publications.ai.mit.edu/ai-publications/pdf/AIM-039.pdf)。

相反，我回到了老一套，我要为一个 C 的子集编写一个编译器，足够让这个编译器能够编译自身。

C 相比汇编语言只是一个台阶（对于某些 C 的子集，不是 [C18](https://en.wikipedia.org/wiki/C18_(C_standard_revision))），这将有助于把 C 代码编译成汇编的任务变得更容易一些。哦，而且我也喜欢 C。

## 编译器的基本工作

编译器的工作是将一种语言（通常是高级语言）的输入翻译成另一种输出语言（通常是比输入低级一些的语言）。主要步骤如下：

![](Figs/parsing_steps.png)

 + 进行[词法分析](https://en.wikipedia.org/wiki/Lexical_analysis)，识别词法元素。在几种语言中，`=` 和 `==` 是不同的，所以你不能只读一个 `=`。我们称这些词法元素为 **词法单元（token）**。
 + 对输入进行[解析](https://en.wikipedia.org/wiki/Parsing)，即识别输入的语法和结构元素，并确保它们符合语言的**语法**。例如，你的语言可能有这样的决策结构：

```
      if (x < 23) {
        print("x is smaller than 23\n");
      }
```

> 但在另一种语言中，你可能这样写：

```
      if (x < 23):
        print("x is smaller than 23\n")
```

> 这也是编译器可以检测语法错误的地方，例如第一个 *print* 语句末尾缺少分号。

 + 对输入进行[语义分析](https://en.wikipedia.org/wiki/Semantic_analysis_(compilers))，即理解输入的含义。这实际上与识别语法和结构不同。例如，在英语中，一个句子可能有这样的形式：`<主语> <谓语> <形容词> <宾语>`。下面两个句子有相同的结构，但含义完全不同：

```
          David ate lovely bananas.
          Jennifer hates green tomatoes.
```

 + 将输入的含义[翻译](https://en.wikipedia.org/wiki/Code_generation_(compiler))成另一种语言。在这里，我们把输入一点一点地转换成较低级的语言。
  
## 资源

网上有很多编译器资源。以下是我会参考的一些资源。

### 学习资源

如果你想从一些关于编译器的书籍、论文和工具开始，我强烈推荐这个列表：

  + [Compiler, Interpreter and Runtimes 相关资源精选列表](https://github.com/aalhour/awesome-compilers) — Ahmad Alhour

### 现有编译器

虽然我要构建自己的编译器，但我计划参考其他编译器来获取思路，也可能会借用其中一些代码。以下是我参考的编译器：

  + Nils M Holm 的 [SubC](http://www.t3x.org/subc/)
  + Robert Swierczek 的 [Swieros C 编译器](https://github.com/rswier/swieros/blob/master/root/bin/c.c)
  + Fabrice Bellard 的 [fbcc](https://github.com/DoctorWkt/fbcc)
  + Fabrice Bellard 等人的 [tcc](https://bellard.org/tcc/)
  + Yuichiro Nakada 的 [catc](https://github.com/yui0/catc)
  + Jim Huang 的 [amacc](https://github.com/jserv/amacc)
  + Ron Cain、James E. Hendrix 等人开发的 [Small C](https://en.wikipedia.org/wiki/Small-C)

特别是，我会大量使用 SubC 编译器的思想和部分代码。

## 设置开发环境

假设你想加入这个旅程，以下是你需要的准备。我将使用 Linux 开发环境，所以请下载并设置你最喜欢的 Linux 系统：我使用的是 Lubuntu 18.04。

我将面向两个硬件平台：Intel x86-64 和 32 位 ARM。我将使用运行 Lubuntu 18.04 的 PC 作为 Intel 目标平台，运行 Raspbian 的树莓派作为 ARM 目标平台。

在 Intel 平台上，我们需要一个现有的 C 编译器。所以安装这个包（我给出 Ubuntu/Debian 命令）：

```
  $ sudo apt-get install build-essential
```

如果普通的 Linux 系统还需要其他工具，请告诉我。

最后，克隆一份这个 GitHub 仓库。

## 下一步

在我们编译器编写的下一步中，我们将从扫描输入文件并找出语言中的**词法单元（token）**的代码开始。[下一步](../01_Scanner/Readme_zh.md)
