# 翻译术语表 (Translation Glossary)

本文档记录 ACWJ 项目翻译过程中遇到的**核心术语**，确保全文翻译一致性。

---

## 编译器相关术语

| 英文术语 | 中文翻译 | 备注/例句 |
|---------|---------|----------|
| compiler | 编译器 | 将高级语言转换为低级语言的程序 |
| self-compiling compiler | 自举编译器 / 自编译编译器 | 能够编译自身的编译器 |
| interpreter | 解释器 | 直接执行代码的程序 |
| assembler | 汇编器 | 将汇编语言转换为机器码的程序 |
| linker | 链接器 | 链接多个目标文件生成可执行文件的程序 |
| preprocessor | 预处理器 | C语言编译前处理宏指令的程序 |
| backend | 后端 | 编译器中负责代码生成的模块 |
| frontend | 前端 | 编译器中负责词法分析、语法分析的模块 |

---

## 词法/语法分析

| 英文术语 | 中文翻译 | 备注/例句 |
|---------|---------|----------|
| lexical analysis / scanning | 词法分析 / 扫描 | 将源代码分解为 token 的过程 |
| token | 词法单元 / Token | 词法分析的基本单位 |
| parser / parsing | 解析器 / 解析 | 识别语法结构的过程 |
| scanner | 扫描器 | 进行词法分析的程序 |
| lexer | 词法分析器 | scanner 的另一种称呼 |
| grammar | 语法 / 文法 | 定义语言结构的规则 |
| BNF (Backus-Naur Form) | 巴科斯-诺尔范式 | 描述语法的表示法 |
| terminal symbol | 终结符 | 语法中不可再分的符号 |
| non-terminal symbol | 非终结符 | 由规则产生的符号 |
| recursive | 递归的 | 自引用的语法规则 |

---

## AST 相关

| 英文术语 | 中文翻译 | 备注/例句 |
|---------|---------|----------|
| AST (Abstract Syntax Tree) | 抽象语法树 | 表示程序语法结构的树形结构 |
| node | 节点 | 树中的元素 |
| leaf node / leaf | 叶子节点 | 没有子节点的节点 |
| child node / child | 子节点 | 节点的子元素 |
| root | 根节点 | 树的顶端节点 |
| subtree | 子树 | 节点的完整下层结构 |
| traverse / traversal | 遍历 | 按某种顺序访问树中所有节点 |

---

## 语义分析

| 英文术语 | 中文翻译 | 备注/例句 |
|---------|---------|----------|
| semantic analysis | 语义分析 | 理解代码含义的过程 |
| type checking | 类型检查 | 验证数据类型一致性的过程 |
| type system | 类型系统 | 语言中类型的规则集合 |
| constant folding | 常量折叠 | 编译时计算常量表达式的优化 |

---

## 运算符与优先级

| 英文术语 | 中文翻译 | 备注/例句 |
|---------|---------|----------|
| operator | 运算符 | 表示运算的符号 |
| precedence | 优先级 | 运算符的计算顺序 |
| operand | 操作数 | 运算符作用的对象 |
| unary operator | 一元运算符 | 只用一个操作数的运算符 |
| binary operator | 二元运算符 | 需要两个操作数的运算符 |
| ternary operator | 三元运算符 | 需要三个操作数的运算符 |

---

## 代码生成

| 英文术语 | 中文翻译 | 备注/例句 |
|---------|---------|----------|
| code generation | 代码生成 | 将中间表示转换为目标代码 |
| target language | 目标语言 | 编译器输出的语言 |
| assembly code | 汇编代码 | 汇编语言的源代码 |
| machine code | 机器码 | CPU可直接执行的二进制代码 |
| register | 寄存器 | CPU内部的存储单元 |
| stack | 栈 | LIFO数据结构，用于函数调用 |
| heap | 堆 | 动态内存分配区域 |
| calling convention | 调用约定 | 函数参数传递和返回值处理的规则 |
| register spilling | 寄存器溢出 | 将寄存器内容移到内存的过程 |

---

## 语法元素

| 英文术语 | 中文翻译 | 备注/例句 |
|---------|---------|----------|
| expression | 表达式 | 产生一个值的代码结构 |
| statement | 语句 | 执行一个动作的代码结构 |
| declaration | 声明 | 引入标识符的代码 |
| definition | 定义 | 创建实体的代码 |
| identifier | 标识符 | 变量、函数的名字 |
| literal | 字面量 | 固定值的表示 |
| integer literal | 整数字面量 | 如 42 |
| string literal | 字符串字面量 | 如 "hello" |
| keyword | 关键字 | 语言保留的词 |
| syntax error | 语法错误 | 违反语法规则的错误 |
| semantic error | 语义错误 | 违反语言含义规则的错误 |

---

## C 语言特定

| 英文术语 | 中文翻译 | 备注/例句 |
|---------|---------|----------|
| struct | 结构体 | C语言中复合数据类型 |
| union | 联合体 | C语言中共享内存的数据结构 |
| enum | 枚举 | 一组命名整数常量 |
| typedef | 类型定义 | 创建类型别名的关键字 |
| pointer | 指针 | 存储内存地址的变量 |
| array | 数组 | 相同类型元素的集合 |
| function | 函数 | 可复用的代码块 |
| parameter | 参数 | 函数定义中的变量 |
| argument | 实参 | 函数调用时传入的值 |
| prototype | 原型 | 函数的前向声明 |
| header file | 头文件 | 包含声明的文件 |
| source file | 源文件 | 包含定义的文件 |

---

## 编译器实现术语

| 英文术语 | 中文翻译 | 备注/例句 |
|---------|---------|----------|
| recursive descent parser | 递归下降解析器 | 一种自顶向下的解析方法 |
| Pratt parser | Pratt 解析器 | 基于运算符优先级的解析技术 |
| symbol table | 符号表 | 存储标识符信息的表 |
| abstract | 抽象的 | 忽略细节，只保留本质 |
| concrete | 具体的 | 实际存在的 |
| bind / binding | 绑定 | 建立关联关系 |
| evaluate | 求值 | 计算表达式的值 |
| parse | 解析 | 分析语法结构 |
| match | 匹配 | 使之一致 |
| consume | 消费 | 读取并处理 |
| emit | 发射/输出 | 生成代码或数据 |

---

## 进度描述

| 英文术语 | 中文翻译 |
|---------|---------|
| introduction | 引言 / 介绍 |
| conclusion | 结论 |
| what's next | 下一步 |
| next step | 下一步 |
| example | 示例 |
| implementation | 实现 |
| mopping up | 收尾工作 |

---

## 版本说明

| 英文 | 中文 |
|-----|------|
| Part 0 ~ Part 64 | 第 0 章 ~ 第 64 章 |
| Revision | 版本 |
| Updated | 已更新 |

---

*此表会在翻译过程中持续更新*
