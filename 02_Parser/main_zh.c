#include "defs.h"
#define extern_
#include "data.h"
#undef extern_
#include "decl.h"
#include <errno.h>

// 编译器设置和顶层执行
// Copyright (c) 2019 Warren Toomey, GPL3

// 初始化全局变量
static void init() {
  Line = 1;
  Putback = '\n';
}

// 如果启动方式不正确，打印用法说明
static void usage(char *prog) {
  fprintf(stderr, "Usage: %s infile\n", prog);
  exit(1);
}

// 主程序：检查参数，
// 如果没有参数则打印用法说明。打开输入
// 文件并调用 scanfile() 来扫描其中的 token。
void main(int argc, char *argv[]) {
  struct ASTnode *n;

  if (argc != 2)
    usage(argv[0]);

  init();

  if ((Infile = fopen(argv[1], "r")) == NULL) {
    fprintf(stderr, "Unable to open %s: %s\n", argv[1], strerror(errno));
    exit(1);
  }

  scan(&Token);			// 从输入中获取第一个 token
  n = binexpr();		// 解析文件中的表达式
  printf("%d\n", interpretAST(n));	// 计算最终结果
  exit(0);
}
