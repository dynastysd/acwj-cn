#include "defs.h"
#include "data.h"
#include "decl.h"

// 语句的解析
// Copyright (c) 2019 Warren Toomey, GPL3

// 原型声明
static struct ASTnode *single_statement(void);

// compound_statement:          // empty, i.e. no statement
//      |      statement
//      |      statement statements
//      ;
//
// statement: declaration
//      |     expression_statement
//      |     function_call
//      |     if_statement
//      |     while_statement
//      |     for_statement
//      |     return_statement
//      ;


// if_statement: if_head
//      |        if_head 'else' compound_statement
//      ;
//
// if_head: 'if' '(' true_false_expression ')' compound_statement  ;
//
// 解析 IF 语句及其可选的 ELSE 子句，并返回其抽象语法树
static struct ASTnode *if_statement(void) {
  struct ASTnode *condAST, *trueAST, *falseAST = NULL;

  // 确保有 'if' '('
  match(T_IF, "if");
  lparen();

  // 解析随后的表达式
  // 以及随后的 ')'。确保
  // 树的运算是一个比较运算。
  condAST = binexpr(0);
  if (condAST->op < A_EQ || condAST->op > A_GE)
    fatal("Bad comparison operator");
  rparen();

  // 获取复合语句的抽象语法树
  trueAST = compound_statement();

  // 如果有 'else'，跳过它
  // 并获取复合语句的抽象语法树
  if (Token.token == T_ELSE) {
    scan(&Token);
    falseAST = compound_statement();
  }
  // 构建并返回该语句的抽象语法树
  return (mkastnode(A_IF, P_NONE, condAST, trueAST, falseAST, 0));
}


// while_statement: 'while' '(' true_false_expression ')' compound_statement  ;
//
// 解析 WHILE 语句并返回其抽象语法树
static struct ASTnode *while_statement(void) {
  struct ASTnode *condAST, *bodyAST;

  // 确保有 'while' '('
  match(T_WHILE, "while");
  lparen();

  // 解析随后的表达式
  // 以及随后的 ')'。确保
  // 树的运算是一个比较运算。
  condAST = binexpr(0);
  if (condAST->op < A_EQ || condAST->op > A_GE)
    fatal("Bad comparison operator");
  rparen();

  // 获取复合语句的抽象语法树
  bodyAST = compound_statement();

  // 构建并返回该语句的抽象语法树
  return (mkastnode(A_WHILE, P_NONE, condAST, NULL, bodyAST, 0));
}

// for_statement: 'for' '(' preop_statement ';'
//                          true_false_expression ';'
//                          postop_statement ')' compound_statement  ;
//
// preop_statement:  statement          (for now)
// postop_statement: statement          (for now)
//
// 解析 FOR 语句并返回其抽象语法树
static struct ASTnode *for_statement(void) {
  struct ASTnode *condAST, *bodyAST;
  struct ASTnode *preopAST, *postopAST;
  struct ASTnode *tree;

  // 确保有 'for' '('
  match(T_FOR, "for");
  lparen();

  // 获取前置运算语句和 ';'
  preopAST = single_statement();
  semi();

  // 获取条件表达式和 ';'
  condAST = binexpr(0);
  if (condAST->op < A_EQ || condAST->op > A_GE)
    fatal("Bad comparison operator");
  semi();

  // 获取后置运算语句和 ')'
  postopAST = single_statement();
  rparen();

  // 获取作为循环体的复合语句
  bodyAST = compound_statement();

  // 目前，四个子树都不能为 NULL。
  // 之后，当某些子树缺失时，我们会改变语义

  // 将复合语句和后置运算树连接起来
  tree = mkastnode(A_GLUE, P_NONE, bodyAST, NULL, postopAST, 0);

  // 使用条件和这个新循环体创建一个 WHILE 循环
  tree = mkastnode(A_WHILE, P_NONE, condAST, NULL, tree, 0);

  // 将前置运算树连接到 A_WHILE 树上
  return (mkastnode(A_GLUE, P_NONE, preopAST, NULL, tree, 0));
}

// return_statement: 'return' '(' expression ')'  ;
//
// 解析 return 语句并返回其抽象语法树
static struct ASTnode *return_statement(void) {
  struct ASTnode *tree;

  // 如果函数返回 P_VOID，则不能返回值
  if (Gsym[Functionid].type == P_VOID)
    fatal("Can't return from a void function");

  // 确保有 'return' '('
  match(T_RETURN, "return");
  lparen();

  // 解析随后的表达式
  tree = binexpr(0);

  // 确保这与函数的类型兼容
  tree = modify_type(tree, Gsym[Functionid].type, 0);
  if (tree == NULL)
    fatal("Incompatible type to return");

  // 添加 A_RETURN 节点
  tree = mkastunary(A_RETURN, P_NONE, tree, 0);

  // 获取 ')'
  rparen();
  return (tree);
}

// 解析单个语句并返回其抽象语法树
static struct ASTnode *single_statement(void) {
  int type;

  switch (Token.token) {
    case T_CHAR:
    case T_INT:
    case T_LONG:

      // 变量声明的开始。
      // 解析类型并获取标识符。
      // 然后解析声明的其余部分。
      // XXX：目前这些是全局变量。
      type = parse_type();
      ident();
      var_declaration(type);
      return (NULL);		// 此处不生成抽象语法树
    case T_IF:
      return (if_statement());
    case T_WHILE:
      return (while_statement());
    case T_FOR:
      return (for_statement());
    case T_RETURN:
      return (return_statement());
    default:
      // 目前，检查这是否是一个表达式。
      // 这可以处理赋值语句。
      return (binexpr(0));
  }
  return (NULL);		// 保持 -Wall 满意
}

// 解析复合语句
// 并返回其抽象语法树
struct ASTnode *compound_statement(void) {
  struct ASTnode *left = NULL;
  struct ASTnode *tree;

  // 需要左花括号
  lbrace();

  while (1) {
    // 解析单个语句
    tree = single_statement();

    // 某些语句后必须跟分号
    if (tree != NULL && (tree->op == A_ASSIGN ||
			 tree->op == A_RETURN || tree->op == A_FUNCCALL))
      semi();

    // 对于每个新树，如果 left 为空则保存它，
    // 否则将 left 和新树连接在一起
    if (tree != NULL) {
      if (left == NULL)
	left = tree;
      else
	left = mkastnode(A_GLUE, P_NONE, left, NULL, tree, 0);
    }

    // 当遇到右花括号时，
    // 跳过它并返回抽象语法树
    if (Token.token == T_RBRACE) {
      rbrace();
      return (left);
    }
  }
}