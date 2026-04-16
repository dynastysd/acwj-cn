#include "defs.h"
#include "data.h"
#include "decl.h"

// 语句解析
// Copyright (c) 2019 Warren Toomey, GPL3

// 原型
static struct ASTnode *single_statement(void);

// compound_statement:          // 空，即无语句
//      |      statement
//      |      statement statements
//      ;
//
// statement: print_statement
//      |     declaration
//      |     assignment_statement
//      |     if_statement
//      |     while_statement
//      ;

// print_statement: 'print' expression ';'  ;
//
static struct ASTnode *print_statement(void) {
  struct ASTnode *tree;
  int reg;

  // 将 'print' 作为第一个词法单元进行匹配
  match(T_PRINT, "print");

  // 解析后面的表达式
  tree = binexpr(0);

  // 创建一个 print AST 树
  tree = mkastunary(A_PRINT, tree, 0);

  // 返回 AST
  return (tree);
}

// assignment_statement: identifier '=' expression ';'   ;
//
static struct ASTnode *assignment_statement(void) {
  struct ASTnode *left, *right, *tree;
  int id;

  // 确保有一个标识符
  ident();

  // 检查它是否已被定义，然后为其创建一个叶子节点
  if ((id = findglob(Text)) == -1) {
    fatals("Undeclared variable", Text);
  }
  right = mkastleaf(A_LVIDENT, id);

  // 确保有一个等号
  match(T_ASSIGN, "=");

  // 解析后面的表达式
  left = binexpr(0);

  // 创建一个赋值 AST 树
  tree = mkastnode(A_ASSIGN, left, NULL, right, 0);

  // 返回 AST
  return (tree);
}

// if_statement: if_head
//      |        if_head 'else' compound_statement
//      ;
//
// if_head: 'if' '(' true_false_expression ')' compound_statement  ;
//
// 解析 IF 语句包括
// 任何可选的 ELSE 子句
// 并返回其 AST
static struct ASTnode *if_statement(void) {
  struct ASTnode *condAST, *trueAST, *falseAST = NULL;

  // 确保有 'if' '('
  match(T_IF, "if");
  lparen();

  // 解析后面的表达式
  // 和后面的 ')'。确保
  // 树的运算是比较运算
  condAST = binexpr(0);
  if (condAST->op < A_EQ || condAST->op > A_GE)
    fatal("Bad comparison operator");
  rparen();

  // 获取复合语句的 AST
  trueAST = compound_statement();

  // 如果有 'else'，跳过它
  // 并获取复合语句的 AST
  if (Token.token == T_ELSE) {
    scan(&Token);
    falseAST = compound_statement();
  }
  // 为这个语句构建并返回 AST
  return (mkastnode(A_IF, condAST, trueAST, falseAST, 0));
}


// while_statement: 'while' '(' true_false_expression ')' compound_statement  ;
//
// 解析 WHILE 语句
// 并返回其 AST
static struct ASTnode *while_statement(void) {
  struct ASTnode *condAST, *bodyAST;

  // 确保有 'while' '('
  match(T_WHILE, "while");
  lparen();

  // 解析后面的表达式
  // 和后面的 ')'。确保
  // 树的运算是比较运算
  condAST = binexpr(0);
  if (condAST->op < A_EQ || condAST->op > A_GE)
    fatal("Bad comparison operator");
  rparen();

  // 获取复合语句的 AST
  bodyAST = compound_statement();

  // 为这个语句构建并返回 AST
  return (mkastnode(A_WHILE, condAST, NULL, bodyAST, 0));
}

// for_statement: 'for' '(' preop_statement ';'
//                          true_false_expression ';'
//                          postop_statement ')' compound_statement  ;
//
// preop_statement:  statement          (目前)
// postop_statement: statement          (目前)
//
// 解析 FOR 语句
// 并返回其 AST
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

  // 获取条件和 ';'
  condAST = binexpr(0);
  if (condAST->op < A_EQ || condAST->op > A_GE)
    fatal("Bad comparison operator");
  semi();

  // 获取后置运算语句和 ')'
  postopAST = single_statement();
  rparen();

  // 获取作为循环体的复合语句
  bodyAST = compound_statement();

  // 目前，所有四个子树都必须非空。
  // 稍后，我们会更改某些缺失时的语义

  // 将复合语句和后置运算树粘合在一起
  tree = mkastnode(A_GLUE, bodyAST, NULL, postopAST, 0);

  // 用这个新循环体创建一个 WHILE 循环
  tree = mkastnode(A_WHILE, condAST, NULL, tree, 0);

  // 将前置运算树粘合到 A_WHILE 树
  return (mkastnode(A_GLUE, preopAST, NULL, tree, 0));
}

// 解析单个语句
// 并返回其 AST
static struct ASTnode *single_statement(void) {
  switch (Token.token) {
    case T_PRINT:
      return (print_statement());
    case T_INT:
      var_declaration();
      return (NULL);		// 此处没有生成 AST
    case T_IDENT:
      return (assignment_statement());
    case T_IF:
      return (if_statement());
    case T_WHILE:
      return (while_statement());
    case T_FOR:
      return (for_statement());
    default:
      fatald("Syntax error, token", Token.token);
  }
}

// 解析复合语句
// 并返回其 AST
struct ASTnode *compound_statement(void) {
  struct ASTnode *left = NULL;
  struct ASTnode *tree;

  // 需要一个左花括号
  lbrace();

  while (1) {
    // 解析单个语句
    tree = single_statement();

    // 某些语句后面必须跟分号
    if (tree != NULL && (tree->op == A_PRINT || tree->op == A_ASSIGN))
      semi();

    // 对于每个新树，如果左树为空，
    // 则将其保存在左树中，
    // 否则将左树和新树粘合在一起
    if (tree != NULL) {
      if (left == NULL)
	left = tree;
      else
	left = mkastnode(A_GLUE, left, NULL, tree, 0);
    }
    // 当遇到右花括号时，
    // 跳过它并返回 AST
    if (Token.token == T_RBRACE) {
      rbrace();
      return (left);
    }
  }
}