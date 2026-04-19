#include "defs.h"
#include "data.h"
#include "decl.h"

// 声明解析
// Copyright (c) 2019 Warren Toomey, GPL3


// 解析当前词法单元并返回
// 一个基本类型枚举值。同时扫描下一个词法单元
int parse_type(void) {
  int type;
  switch (Token.token) {
    case T_VOID:
      type = P_VOID;
      break;
    case T_CHAR:
      type = P_CHAR;
      break;
    case T_INT:
      type = P_INT;
      break;
    case T_LONG:
      type = P_LONG;
      break;
    default:
      fatald("Illegal type, token", Token.token);
  }

  // 扫描一个或多个额外的 '*' 词法单元
  // 并确定正确的指针类型
  while (1) {
    scan(&Token);
    if (Token.token != T_STAR)
      break;
    type = pointer_to(type);
  }

  // 我们离开时下一个词法单元已被扫描
  return (type);
}

// variable_declaration: type identifier ';'
//        | type identifier '[' INTLIT ']' ';'
//        ;
//
// 解析具有给定大小的标量变量或数组的声明。
// 标识符已被扫描，我们有类型
// 如果是本地变量则设置 islocal
// 如果这个本地变量是函数参数则设置 isparam
void var_declaration(int type, int islocal, int isparam) {

  // Text 现在有标识符的名称。
  // 如果下一个词法单元是 '['
  if (Token.token == T_LBRACKET) {
    // 跳过 '['
    scan(&Token);

    // 检查我们是否有数组大小
    if (Token.token == T_INTLIT) {
      // 将其添加为已知数组并生成其汇编空间。
      // 我们将数组视为指向其元素类型的指针
      if (islocal) {
	fatal("For now, declaration of local arrays is not implemented");
      } else {
	addglob(Text, pointer_to(type), S_ARRAY, 0, Token.intvalue);
      }
    }
    // 确保有后续的 ']'
    scan(&Token);
    match(T_RBRACKET, "]");
  } else {
    // 将其添加为已知标量
    // 并生成其汇编空间
    if (islocal) {
      if (addlocl(Text, type, S_VARIABLE, isparam, 1)==-1)
       fatals("Duplicate local variable declaration", Text);
    } else {
      addglob(Text, type, S_VARIABLE, 0, 1);
    }
  }
}

// param_declaration: <null>
//           | variable_declaration
//           | variable_declaration ',' param_declaration
//
// 解析函数名后括号中的参数。
// 将它们作为符号添加到符号表并返回参数的数量。
static int param_declaration(void) {
  int type;
  int paramcnt=0;

  // 循环直到最终的右括号
  while (Token.token != T_RPAREN) {
    // 获取类型和标识符
    // 并将其添加到符号表
    type = parse_type();
    ident();
    var_declaration(type, 1, 1);
    paramcnt++;

    // 此时必须有 ',' 或 ')'
    switch (Token.token) {
      case T_COMMA: scan(&Token); break;
      case T_RPAREN: break;
      default:
        fatald("Unexpected token in parameter list", Token.token);
    }
  }

  // 返回参数计数
  return(paramcnt);
}

//
// function_declaration: type identifier '(' ')' compound_statement   ;
//
// 解析一个简单函数的声明。
// 标识符已被扫描，我们有类型
struct ASTnode *function_declaration(int type) {
  struct ASTnode *tree, *finalstmt;
  int nameslot, endlabel, paramcnt;

  // Text 现在有标识符的名称。
  // 获取结束标签的标签 id，将函数添加到符号表，
  // 并将 Functionid 全局变量设置为函数的符号 id
  endlabel = genlabel();
  nameslot = addglob(Text, type, S_FUNCTION, endlabel, 0);
  Functionid = nameslot;

  // 扫描括号和任何参数
  // 用参数数量更新函数符号条目
  lparen();
  paramcnt= param_declaration();
  Symtable[nameslot].nelems= paramcnt;
  rparen();

  // 获取复合语句的抽象语法树
  tree = compound_statement();

  // 如果函数类型不是 P_VOID..
  if (type != P_VOID) {

    // 如果函数中没有语句则报错
    if (tree == NULL)
      fatal("No statements in function with non-void type");

    // 检查复合语句中的最后一个
    // 抽象语法树操作是否是 return 语句
    finalstmt = (tree->op == A_GLUE) ? tree->right : tree;
    if (finalstmt == NULL || finalstmt->op != A_RETURN)
      fatal("No return for function with non-void type");
  }
  // 返回一个 A_FUNCTION 节点，其中包含函数的 nameslot
  // 和复合语句子树
  return (mkastunary(A_FUNCTION, type, tree, nameslot));
}


// 解析一个或多个全局声明，
// 即变量或函数
void global_declarations(void) {
  struct ASTnode *tree;
  int type;

  while (1) {

    // 我们必须读取类型和标识符
    // 来查看是函数声明的 '('
    // 还是变量声明的 ',' 或 ';'。
    // Text 由 ident() 调用填充。
    type = parse_type();
    ident();
    if (Token.token == T_LPAREN) {

      // 解析函数声明并
      // 为其生成汇编代码
      tree = function_declaration(type);
      if (O_dumpAST) {
	dumpAST(tree, NOLABEL, 0);
	fprintf(stdout, "\n\n");
      }
      genAST(tree, NOLABEL, 0);

      // 现在释放与
      // 此函数关联的符号
      freeloclsyms();
    } else {

      // 解析全局变量声明
      // 并跳过尾部分号
      var_declaration(type, 0, 0);
      semi();
    }

    // 当我们到达 EOF 时停止
    if (Token.token == T_EOF)
      break;
  }
}