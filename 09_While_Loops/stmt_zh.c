#include "defs.h"
#include "data.h"
#include "decl.h"

// 语句的解析
// Copyright (c) 2019 Warren Toomey, GPL3

// compound_statement:          // 空，即没有语句
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

  // 匹配 'print' 作为第一个词法单元
  match(T_PRINT, "print");

  // 解析后续的表达式
  tree = binexpr(0);

  // 构造一个打印 AST 树
  tree = mkastunary(A_PRINT, tree, 0);

  // 匹配后续的分号
  // 并返回 AST
  semi();
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

  // 解析后续的表达式
  left = binexpr(0);

  // 构造一个赋值 AST 树
  tree = mkastnode(A_ASSIGN, left, NULL, right, 0);

  // 匹配后续的分号
  // 并返回 AST
  semi();
  return (tree);
}

// if_statement: if_head
//      |        if_head 'else' compound_statement
//      ;
//
// if_head: 'if' '(' true_false_expression ')' compound_statement  ;
//
// 解析一个 IF 语句包括
// 任何可选的 ELSE 子句
// 并返回其 AST
struct ASTnode *if_statement(void) {
  struct ASTnode *condAST, *trueAST, *falseAST = NULL;

  // 确保有 'if' '('
  match(T_IF, "if");
  lparen();

  // 解析后续的表达式
  // 和后面的 ')'。确保
  // 树的运算是比较运算。
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
  // 构造并返回该语句的 AST
  return (mkastnode(A_IF, condAST, trueAST, falseAST, 0));
}


// while_statement: 'while' '(' true_false_expression ')' compound_statement  ;
//
// 解析一个 While 语句
// 并返回其 AST
struct ASTnode *while_statement(void) {
  struct ASTnode *condAST, *bodyAST;

  // 确保有 'while' '('
  match(T_WHILE, "while");
  lparen();

  // 解析后续的表达式
  // 和后面的 ')'。确保
  // 树的运算是比较运算。
  condAST = binexpr(0);
  if (condAST->op < A_EQ || condAST->op > A_GE)
    fatal("Bad comparison operator");
  rparen();

  // 获取复合语句的 AST
  bodyAST = compound_statement();

  // 构造并返回该语句的 AST
  return (mkastnode(A_WHILE, condAST, NULL, bodyAST, 0));
}


// 解析一个复合语句
// 并返回其 AST
struct ASTnode *compound_statement(void) {
  struct ASTnode *left = NULL;
  struct ASTnode *tree;

  // 需要一个左花括号
  lbrace();

  while (1) {
    switch (Token.token) {
      case T_PRINT:
	tree = print_statement();
	break;
      case T_INT:
	var_declaration();
	tree = NULL;		// 此处没有生成 AST
	break;
      case T_IDENT:
	tree = assignment_statement();
	break;
      case T_IF:
	tree = if_statement();
	break;
      case T_WHILE:
	tree = while_statement();
	break;
      case T_RBRACE:
	// 当遇到右花括号时，
	// 跳过它并返回 AST
	rbrace();
	return (left);
      default:
	fatald("Syntax error, token", Token.token);
    }

    // 对于每个新树，如果左树为空，
    // 则将其保存在左树中，
    // 否则将左树和新树粘合在一起
    if (tree) {
      if (left == NULL)
	left = tree;
      else
	left = mkastnode(A_GLUE, left, NULL, tree, 0);
    }
  }
}