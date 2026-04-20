# 第 33 章：实现联合体与成员访问

联合体的实现也非常简单，原因只有一个：它们与结构体类似，只是联合体的所有成员都位于联合体基址的零偏移处。此外，联合体声明的语法与结构体相同，只是关键字不同。

这意味着我们可以复用并修改现有的结构体代码来处理联合体。

## 新关键字："union"

我已经在 `scan.c` 中添加了 "union" 关键字和 T_UNION 词法单元。和往常一样，我将省略扫描相关的代码。

## 联合体的符号表

与结构体一样，有一个单向链表来存储联合体（在 `data.h` 中）：

```c
extern_ struct symtable *Unionhead, *Uniontail;   // List of struct types
```

在 `sym.c` 中，我还编写了 `addunion()` 和 `findunion()` 函数，用于向链表中添加新的联合体类型节点，以及在链表中搜索具有给定名称的联合体类型。

> 我正在考虑将结构体和联合体链表合并为一个复合类型链表，但还没有实现。等我进行更多重构时，可能会这样做。

## 解析联合体声明

我们将修改 `decl.c` 中现有的结构体解析代码来同时解析结构体和联合体。我只给出函数的更改部分，而不是完整的函数。

在 `parse_type()` 中，我们现在扫描 T_UNION 词法单元，并调用函数来解析结构体和联合体类型：

```c
  case T_STRUCT:
    type = P_STRUCT;
    *ctype = composite_declaration(P_STRUCT);
    break;
  case T_UNION:
    type = P_UNION;
    *ctype = composite_declaration(P_UNION);
    break;
```

这个函数 `composite_declaration()` 在上一部分被称为 `struct_declaration()`。它现在接受我们正在解析的类型。

## `composite_declaration()` 函数

以下是更改的内容：

```c
// Parse composite type declarations: structs or unions.
// Either find an existing struct/union declaration, or build
// a struct/union symbol table entry and return its pointer.
static struct symtable *composite_declaration(int type) {
  ...
  // Find any matching composite type
  if (type == P_STRUCT)
    ctype = findstruct(Text);
  else
    ctype = findunion(Text);
  ...
  // Build the composite type and skip the left brace
  if (type == P_STRUCT)
    ctype = addstruct(Text, P_STRUCT, NULL, 0, 0);
  else
    ctype = addunion(Text, P_UNION, NULL, 0, 0);
  ...
  // Set the position of each successive member in the composite type
  // Unions are easy. For structs, align the member and find the next free byte
  for (m = m->next; m != NULL; m = m->next) {
    // Set the offset for this member
    if (type == P_STRUCT)
      m->posn = genalign(m->type, offset, 1);
    else
      m->posn = 0;

    // Get the offset of the next free byte after this member
    offset += typesize(m->type, m->ctype);
  }
  ...
  return (ctype);
}
```

就是这样。我们只需更改正在操作的符号表链表，并始终将联合体的成员偏移设置为零。这就是我认为值得将结构体和联合体类型链表合并为一个链表的原因。

## 解析联合体表达式

与联合体声明一样，我们可以复用处理结构体的表达式代码。事实上，在 `expr.c` 中需要做的更改非常少。

```c
// Parse the member reference of a struct or union
// and return an AST tree for it. If withpointer is true,
// the access is through a pointer to the member.
static struct ASTnode *member_access(int withpointer) {
  ...
  if (withpointer && compvar->type != pointer_to(P_STRUCT)
      && compvar->type != pointer_to(P_UNION))
    fatals("Undeclared variable", Text);
  if (!withpointer && compvar->type != P_STRUCT && compvar->type != P_UNION)
    fatals("Undeclared variable", Text);
```

同样，这就是全部。代码的其他部分足够通用，可以不加修改地用于联合体。我认为还有另一个主要更改，是在 `types.c` 中的一个函数：

```c
// Given a type and a composite type pointer, return
// the size of this type in bytes
int typesize(int type, struct symtable *ctype) {
  if (type == P_STRUCT || type == P_UNION)
    return (ctype->size);
  return (genprimsize(type));
}
```

## 测试联合体代码

这是我们的测试程序，`test/input62.c`：

```c
int printf(char *fmt);

union fred {
  char w;
  int  x;
  int  y;
  long z;
};

union fred var1;
union fred *varptr;

int main() {
  var1.x= 65; printf("%d\n", var1.x);
  var1.x= 66; printf("%d\n", var1.x); printf("%d\n", var1.y);
  printf("The next two depend on the endian of the platform\n");
  printf("%d\n", var1.w); printf("%d\n", var1.z);

  varptr= &var1; varptr->x= 67;
  printf("%d\n", varptr->x); printf("%d\n", varptr->y);

  return(0);
}
```

这测试了联合体中的所有四个成员位于相同位置，因此对一个成员的更改会被视为对所有成员的相同更改。我们还检查了指针访问联合体是否也能正常工作。

## 结论与下一步

这是我们编译器编写之旅中又一个轻松简单的部分。
在我们编译器编写之旅的下一部分中，我们将添加枚举。[下一步](../34_Enums_and_Typedefs/Readme.md)