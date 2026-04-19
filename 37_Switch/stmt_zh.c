#include "defs.h"
#include "data.h"
#include "decl.h"

// 语句解析
// Copyright (c) 2019 Warren Toomey, GPL3

// 原型
static struct ASTnode *single_statement(void);

// compound_statement:          // 空，即没有语句
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
// 解析IF语句包括任何
// 可选的ELSE子句并返回其AST
static struct ASTnode *if_statement(void) {
  struct ASTnode *condAST, *trueAST, *falseAST = NULL;

  // 确保我们有'if' '('
  match(T_IF, "if");
  lparen();

  // 解析后续表达式和后面的')'。
  // 强制非比较为布尔值。
  // 树的操作为比较
  condAST = binexpr(0);
  if (condAST->op < A_EQ || condAST->op > A_GE)
    condAST = mkastunary(A_TOBOOL, condAST->type, condAST, NULL, 0);
  rparen();

  // 获取复合语句的AST
  trueAST = compound_statement();

  // 如果我们有'else'，跳过它
  // 并获取复合语句的AST
  if (Token.token == T_ELSE) {
    scan(&Token);
    falseAST = compound_statement();
  }
  // 为此语句构建并返回AST
  return (mkastnode(A_IF, P_NONE, condAST, trueAST, falseAST, NULL, 0));
}


// while_statement: 'while' '(' true_false_expression ')' compound_statement  ;
//
// 解析WHILE语句并返回其AST
static struct ASTnode *while_statement(void) {
  struct ASTnode *condAST, *bodyAST;

  // 确保我们有'while' '('
  match(T_WHILE, "while");
  lparen();

  // 解析后续表达式和后面的')'。
  // 强制非比较为布尔值。
  // 树的操作为比较
  condAST = binexpr(0);
  if (condAST->op < A_EQ || condAST->op > A_GE)
    condAST = mkastunary(A_TOBOOL, condAST->type, condAST, NULL, 0);
  rparen();

  // 获取复合语句的AST
  // 同时更新循环深度
  Looplevel++;
  bodyAST = compound_statement();
  Looplevel--;

  // 为此语句构建并返回AST
  return (mkastnode(A_WHILE, P_NONE, condAST, NULL, bodyAST, NULL, 0));
}

// for_statement: 'for' '(' preop_statement ';'
//                          true_false_expression ';'
//                          postop_statement ')' compound_statement  ;
//
// preop_statement:  statement          (目前)
// postop_statement: statement          (目前)
//
// 解析FOR语句并返回其AST
static struct ASTnode *for_statement(void) {
  struct ASTnode *condAST, *bodyAST;
  struct ASTnode *preopAST, *postopAST;
  struct ASTnode *tree;

  // 确保我们有'for' '('
  match(T_FOR, "for");
  lparen();

  // 获取pre_op语句和';'
  preopAST = single_statement();
  semi();

  // 获取条件和';'
  // 强制非比较为布尔值。
  // 树的操作为比较
  condAST = binexpr(0);
  if (condAST->op < A_EQ || condAST->op > A_GE)
    condAST = mkastunary(A_TOBOOL, condAST->type, condAST, NULL, 0);
  semi();

  // 获取post_op语句和')'
  postopAST = single_statement();
  rparen();

  // 获取作为循环体的复合语句
  // 同时更新循环深度
  Looplevel++;
  bodyAST = compound_statement();
  Looplevel--;

  // 目前，所有四个子树都必须非NULL
  // 稍后，我们会改变某些缺失时的语义

  // 将复合语句和postop树粘合在一起
  tree = mkastnode(A_GLUE, P_NONE, bodyAST, NULL, postopAST, NULL, 0);

  // 用这个新身体创建一个WHILE循环
  tree = mkastnode(A_WHILE, P_NONE, condAST, NULL, tree, NULL, 0);

  // 并将preop树与A_WHILE树粘合
  return (mkastnode(A_GLUE, P_NONE, preopAST, NULL, tree, NULL, 0));
}

// return_statement: 'return' '(' expression ')'  ;
//
// 解析return语句并返回其AST
static struct ASTnode *return_statement(void) {
  struct ASTnode *tree;

  // 如果函数返回P_VOID则不能返回值
  if (Functionid->type == P_VOID)
    fatal("Can't return from a void function");

  // 确保我们有'return' '('
  match(T_RETURN, "return");
  lparen();

  // 解析后续表达式
  tree = binexpr(0);

  // 确保这与函数的类型兼容
  tree = modify_type(tree, Functionid->type, 0);
  if (tree == NULL)
    fatal("Incompatible type to return");

  // 添加A_RETURN节点
  tree = mkastunary(A_RETURN, P_NONE, tree, NULL, 0);

  // 获取')'
  rparen();
  return (tree);
}

// break_statement: 'break' ;
//
// 解析break语句并返回其AST
static struct ASTnode *break_statement(void) {

  if (Looplevel == 0 && Switchlevel == 0)
    fatal("no loop or switch to break out from");
  scan(&Token);
  return (mkastleaf(A_BREAK, 0, NULL, 0));
}

// continue_statement: 'continue' ;
//
// 解析continue语句并返回其AST
static struct ASTnode *continue_statement(void) {

  if (Looplevel == 0)
    fatal("no loop to continue to");
  scan(&Token);
  return (mkastleaf(A_CONTINUE, 0, NULL, 0));
}

// 解析switch语句并返回其AST
static struct ASTnode *switch_statement(void) {
  struct ASTnode *left, *n, *c, *casetree= NULL, *casetail;
  int inloop=1, casecount=0;
  int seendefault=0;
  int ASTop, casevalue;

  // 跳过'switch'和'('
  scan(&Token);
  lparen();

  // 获取switch表达式、')'和'{'
  left= binexpr(0);
  rparen();
  lbrace();

  // 确保这是int类型
  if (!inttype(left->type))
    fatal("Switch expression is not of integer type");

  // 构建一个A_SWITCH子树，表达式作为子节点
  n= mkastunary(A_SWITCH, 0, left, NULL, 0);

  // 现在解析case
  Switchlevel++;
  while (inloop) {
    switch(Token.token) {
      // 遇到'}'时退出循环
      case T_RBRACE: if (casecount==0)
			fatal("No cases in switch");
		     inloop=0; break;
      case T_CASE:
      case T_DEFAULT:
	// 确保这不是在之前的'default'之后
	if (seendefault)
	  fatal("case or default after existing default");

	// 设置AST操作。如果需要则扫描case值
	if (Token.token==T_DEFAULT) {
	  ASTop= A_DEFAULT; seendefault= 1; scan(&Token);
	} else  {
	  ASTop= A_CASE; scan(&Token);
	  left= binexpr(0);
	  // 确保case值是整数常量
	  if (left->op != A_INTLIT)
	    fatal("Expecting integer literal for case value");
	  casevalue= left->intvalue;

	  // 遍历现有case值列表以确保
	  // 没有重复的case值
	  for (c= casetree; c != NULL; c= c -> right)
	    if (casevalue == c->intvalue)
	      fatal("Duplicate case value");
        }

	// 扫描':'并获取复合表达式
	match(T_COLON, ":");
	left= compound_statement(); casecount++;

	// 构建一个以复合语句为左子节点的子树
	// 并将其链接到增长的A_CASE树
	if (casetree==NULL) {
	  casetree= casetail= mkastunary(ASTop, 0, left, NULL, casevalue);
	} else {
	  casetail->right= mkastunary(ASTop, 0, left, NULL, casevalue);
	  casetail= casetail->right;
	}
	break;
      default:
        fatald("Unexpected token in switch", Token.token);
    }
  }
  Switchlevel--;

  // 我们有一个包含case和任何default的子树。将
  // case计数放入A_SWITCH节点并附加case树
  n->intvalue= casecount;
  n->right= casetree;
  rbrace();

  return(n);
}

// 解析单个语句并返回其AST
static struct ASTnode *single_statement(void) {
  int type, class = C_LOCAL;
  struct symtable *ctype;

  switch (Token.token) {
    case T_IDENT:
      // 我们需要查看标识符是否匹配typedef
      // 如果不是，就执行此switch语句中的默认代码
      // 否则，落到parse_type()调用
      if (findtypedef(Text) == NULL)
	return (binexpr(0));
    case T_CHAR:
    case T_INT:
    case T_LONG:
    case T_STRUCT:
    case T_UNION:
    case T_ENUM:
    case T_TYPEDEF:
      // 变量声明的开始。
      // 解析类型并获取标识符。
      // 然后解析声明的其余部分
      // 并跳过冒号
      type = parse_type(&ctype, &class);
      ident();
      var_declaration(type, ctype, class);
      semi();
      return (NULL);		// 此处没有生成AST
    case T_IF:
      return (if_statement());
    case T_WHILE:
      return (while_statement());
    case T_FOR:
      return (for_statement());
    case T_RETURN:
      return (return_statement());
    case T_BREAK:
      return (break_statement());
    case T_CONTINUE:
      return (continue_statement());
    case T_SWITCH:
      return (switch_statement());
    default:
      // 目前，看看这是不是表达式
      // 这处理赋值语句
      return (binexpr(0));
  }
  return (NULL);		// 保持-Wall高兴
}

// 解析复合语句
// 并返回其AST
struct ASTnode *compound_statement(void) {
  struct ASTnode *left = NULL;
  struct ASTnode *tree;

  // 需要左花括号
  lbrace();

  while (1) {
    // 解析单个语句
    tree = single_statement();

    // 某些语句后必须跟分号
    if (tree != NULL && (tree->op == A_ASSIGN || tree->op == A_RETURN
			 || tree->op == A_FUNCCALL || tree->op == A_BREAK
			 || tree->op == A_CONTINUE))
      semi();

    // 对于每棵新树，如果left为空则保存它，
    // 否则将left和新树粘合在一起
    if (tree != NULL) {
      if (left == NULL)
	left = tree;
      else
	left = mkastnode(A_GLUE, P_NONE, left, NULL, tree, NULL, 0);
    }
    // 当遇到右花括号时，
    // 跳过它并返回AST
    if (Token.token == T_RBRACE) {
      rbrace();
      return (left);
    }
  }
}
