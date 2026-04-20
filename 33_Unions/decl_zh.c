#include "defs.h"
#include "data.h"
#include "decl.h"

// 声明的解析
// Copyright (c) 2019 Warren Toomey, GPL3

static struct symtable *composite_declaration(int type);


// 解析当前标记并返回
// 一个基本类型枚举值和一个指向
// 任何复合类型的指针。
// 同时扫描下一个标记
int parse_type(struct symtable **ctype) {
  int type;
  switch (Token.token) {
  case T_VOID:
    type = P_VOID;
    scan(&Token);
    break;
  case T_CHAR:
    type = P_CHAR;
    scan(&Token);
    break;
  case T_INT:
    type = P_INT;
    scan(&Token);
    break;
  case T_LONG:
    type = P_LONG;
    scan(&Token);
    break;
  case T_STRUCT:
    type = P_STRUCT;
    *ctype = composite_declaration(P_STRUCT);
    break;
  case T_UNION:
    type = P_UNION;
    *ctype = composite_declaration(P_UNION);
    break;
  default:
    fatald("Illegal type, token", Token.token);
  }

  // 扫描一个或多个额外的 '*' 标记
  // 并确定正确的指针类型
  while (1) {
    if (Token.token != T_STAR)
      break;
    type = pointer_to(type);
    scan(&Token);
  }

  // 我们在离开时下一个标记已经被扫描
  return (type);
}

// variable_declaration: type identifier ';'
//        | type identifier '[' INTLIT ']' ';'
//        ;
//
// 解析标量变量或数组的声明
// 具有给定大小。
// 标识符已被扫描且我们得到了类型。
// class 是变量的类别
// 返回变量在符号表中的条目指针
struct symtable *var_declaration(int type, struct symtable *ctype, int class) {
  struct symtable *sym = NULL;

  // 检查是否已经声明过
  switch (class) {
  case C_GLOBAL:
    if (findglob(Text) != NULL)
      fatals("Duplicate global variable declaration", Text);
  case C_LOCAL:
  case C_PARAM:
    if (findlocl(Text) != NULL)
      fatals("Duplicate local variable declaration", Text);
  case C_MEMBER:
    if (findmember(Text) != NULL)
      fatals("Duplicate struct/union member declaration", Text);
  }

  // Text 现在包含标识符的名称。
  // 如果下一个标记是 '['
  if (Token.token == T_LBRACKET) {
    // 跳过 '['
    scan(&Token);

    // 检查我们是否有数组大小
    if (Token.token == T_INTLIT) {
      // 将其添加为已知数组并在汇编中生成其空间。
      // 我们将数组视为指向其元素类型的指针
      switch (class) {
      case C_GLOBAL:
	sym = addglob(Text, pointer_to(type), ctype, S_ARRAY, Token.intvalue);
	break;
      case C_LOCAL:
      case C_PARAM:
      case C_MEMBER:
	fatal("For now, declaration of non-global arrays is not implemented");
      }
    }
    // 确保我们有一个后续的 ']'
    scan(&Token);
    match(T_RBRACKET, "]");
  } else {
    // 将其添加为已知的标量
    // 并在汇编中生成其空间
    switch (class) {
    case C_GLOBAL:
      sym = addglob(Text, type, ctype, S_VARIABLE, 1);
      break;
    case C_LOCAL:
      sym = addlocl(Text, type, ctype, S_VARIABLE, 1);
      break;
    case C_PARAM:
      sym = addparm(Text, type, ctype, S_VARIABLE, 1);
      break;
    case C_MEMBER:
      sym = addmemb(Text, type, ctype, S_VARIABLE, 1);
      break;
    }
  }
  return (sym);
}

// var_declaration_list: <null>
//           | variable_declaration
//           | variable_declaration separate_token var_declaration_list ;
//
// 当解析函数参数时，separate_token 是 ','。
// 当解析结构体/联合体的成员时，separate_token 是 ';'。
//
// 解析变量列表。
// 将它们作为符号添加到其中一个符号表列表中，并返回
// 变量数量。如果 funcsym 不为 NULL，则存在一个现有函数
// 原型，因此将每个变量的类型与此原型进行比较。
static int var_declaration_list(struct symtable *funcsym, int class,
				int separate_token, int end_token) {
  int type;
  int paramcnt = 0;
  struct symtable *protoptr = NULL;
  struct symtable *ctype;

  // 如果存在原型，获取
  // 指向第一个原型参数的指针
  if (funcsym != NULL)
    protoptr = funcsym->member;

  // 循环直到最终的结束标记
  while (Token.token != end_token) {
    // 获取类型和标识符
    type = parse_type(&ctype);
    ident();

    // 检查此类型是否与原型匹配（如果存在）
    if (protoptr != NULL) {
      if (type != protoptr->type)
	fatald("Type doesn't match prototype for parameter", paramcnt + 1);
      protoptr = protoptr->next;
    } else {
      // 根据类别将新参数添加到正确的符号表列表
      var_declaration(type, ctype, class);
    }
    paramcnt++;

    // 此时必须有一个 separate_token 或 ')'
    if ((Token.token != separate_token) && (Token.token != end_token))
      fatald("Unexpected token in parameter list", Token.token);
    if (Token.token == separate_token)
      scan(&Token);
  }

  // 检查此列表中的参数数量是否与
  // 任何现有原型匹配
  if ((funcsym != NULL) && (paramcnt != funcsym->nelems))
    fatals("Parameter count mismatch for function", funcsym->name);

  // 返回参数计数
  return (paramcnt);
}

//
// function_declaration: type identifier '(' parameter_list ')' ;
//      | type identifier '(' parameter_list ')' compound_statement   ;
//
// 解析函数的声明。
// 标识符已被扫描且我们得到了类型。
struct ASTnode *function_declaration(int type) {
  struct ASTnode *tree, *finalstmt;
  struct symtable *oldfuncsym, *newfuncsym = NULL;
  int endlabel, paramcnt;

  // Text 包含标识符的名称。如果此函数存在且是
  // 一个函数，获取其 id。否则，将 oldfuncsym 设为 NULL。
  if ((oldfuncsym = findsymbol(Text)) != NULL)
    if (oldfuncsym->stype != S_FUNCTION)
      oldfuncsym = NULL;

  // 如果这是一个新的函数声明，获取
  // 一个用于结束标签的标签-id，并将函数
  // 添加到符号表，
  if (oldfuncsym == NULL) {
    endlabel = genlabel();
    // 假设：函数只返回标量类型，所以下面用 NULL
    newfuncsym = addglob(Text, type, NULL, S_FUNCTION, endlabel);
  }
  // 扫描 '('、任何参数和 ')'。
  // 传入任何现有函数原型指针
  lparen();
  paramcnt = var_declaration_list(oldfuncsym, C_PARAM, T_COMMA, T_RPAREN);
  rparen();

  // 如果这是一个新的函数声明，用参数数量更新
  // 函数符号条目。同时将参数列表复制到函数的节点中。
  if (newfuncsym) {
    newfuncsym->nelems = paramcnt;
    newfuncsym->member = Parmhead;
    oldfuncsym = newfuncsym;
  }
  // 清除参数列表
  Parmhead = Parmtail = NULL;

  // 声明以分号结束，只是原型。
  if (Token.token == T_SEMI) {
    scan(&Token);
    return (NULL);
  }
  // 这不仅仅是原型。
  // 将 Functionid 全局变量设置为函数的符号指针
  Functionid = oldfuncsym;

  // 获取复合语句的 AST 树
  tree = compound_statement();

  // 如果函数类型不是 P_VOID ..
  if (type != P_VOID) {

    // 如果函数中没有语句则报错
    if (tree == NULL)
      fatal("No statements in function with non-void type");

    // 检查复合语句中的最后一个 AST 操作是否是返回语句
    finalstmt = (tree->op == A_GLUE) ? tree->right : tree;
    if (finalstmt == NULL || finalstmt->op != A_RETURN)
      fatal("No return for function with non-void type");
  }
  // 返回包含函数符号指针的 A_FUNCTION 节点
  // 和复合语句子树
  return (mkastunary(A_FUNCTION, type, tree, oldfuncsym, endlabel));
}

// 解析复合类型声明：结构体或联合体。
// 要么找到现有的结构体/联合体声明，要么构建
// 一个结构体/联合体符号表条目并返回其指针。
static struct symtable *composite_declaration(int type) {
  struct symtable *ctype = NULL;
  struct symtable *m;
  int offset;

  // 跳过 struct/union 关键字
  scan(&Token);

  // 查看是否有后续的结构体/联合体名称
  if (Token.token == T_IDENT) {
    // 查找任何匹配的复合类型
    if (type == P_STRUCT)
      ctype = findstruct(Text);
    else
      ctype = findunion(Text);
    scan(&Token);
  }
  // 如果下一个标记不是 LBRACE，这意味着这是
  // 一个现有 struct/union 类型的用法。
  // 返回指向该类型的指针。
  if (Token.token != T_LBRACE) {
    if (ctype == NULL)
      fatals("unknown struct/union type", Text);
    return (ctype);
  }
  // 确保此结构体/联合体类型之前没有被
  // 定义过
  if (ctype)
    fatals("previously defined struct/union", Text);

  // 构建复合类型并跳过左大括号
  if (type == P_STRUCT)
    ctype = addstruct(Text, P_STRUCT, NULL, 0, 0);
  else
    ctype = addunion(Text, P_UNION, NULL, 0, 0);
  scan(&Token);

  // 扫描成员列表并附加
  // 到结构体类型的节点
  var_declaration_list(NULL, C_MEMBER, T_SEMI, T_RBRACE);
  rbrace();
  ctype->member = Membhead;
  Membhead = Membtail = NULL;

  // 设置初始成员的偏移量
  // 并找到其后的第一个空闲字节
  m = ctype->member;
  m->posn = 0;
  offset = typesize(m->type, m->ctype);

  // 在复合类型中设置每个后续成员的位置
  // 联合体很简单。对于结构体，需要对齐成员并找到下一个空闲字节
  for (m = m->next; m != NULL; m = m->next) {
    // 设置此成员的偏移量
    if (type == P_STRUCT)
      m->posn = genalign(m->type, offset, 1);
    else
      m->posn = 0;

    // 获取此成员之后的下一个空闲字节的偏移量
    offset += typesize(m->type, m->ctype);
  }

  // 设置复合类型的总体大小
  ctype->size = offset;
  return (ctype);
}

// 解析一个或多个全局声明，可以是
// 变量、函数或结构体
void global_declarations(void) {
  struct ASTnode *tree;
  struct symtable *ctype;
  int type;

  while (1) {
    // 当到达 EOF 时停止
    if (Token.token == T_EOF)
      break;

    // 获取类型
    type = parse_type(&ctype);

    // 我们可能刚刚解析了一个没有关联变量的
    // 结构体或联合体声明。
    // 下一个标记可能是 ';'。如果是，则循环回去。
    // XXX。我对这个实现不满意，因为它允许
    // "struct fred;" 作为接受的语句
    if ((type == P_STRUCT || type == P_UNION)
	&& Token.token == T_SEMI) {
      scan(&Token);
      continue;
    }
    // 我们必须读取标识符
    // 看看是 '(' 表示函数声明
    // 还是 ',' 或 ';' 表示变量声明。
    // Text 由 ident() 调用填充。
    ident();
    if (Token.token == T_LPAREN) {

      // 解析函数声明
      tree = function_declaration(type);

      // 只是函数原型，没有代码
      if (tree == NULL)
	continue;

      // 一个真正的函数，为其生成汇编代码
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
      var_declaration(type, ctype, C_GLOBAL);
      semi();
    }
  }
}