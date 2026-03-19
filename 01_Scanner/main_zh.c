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

// 可打印的 token 列表
char *tokstr[] = { "+", "-", "*", "/", "intlit" };

// 循环扫描输入文件中的所有 token。
// 打印找到的每个 token 的详细信息。
static void scanfile() {
  struct token T;

  while (scan(&T)) {
    printf("Token %s", tokstr[T.token]);
    if (T.token == T_INTLIT)
      printf(", value %d", T.intvalue);
    printf("\n");
  }
}

// 主程序：检查参数，
// 如果没有参数则打印用法说明。打开输入
// 文件并调用 scanfile() 来扫描其中的 token。
void main(int argc, char *argv[]) {
  if (argc != 2)
    usage(argv[0]);

  init();

  if ((Infile = fopen(argv[1], "r")) == NULL) {
    fprintf(stderr, "Unable to open %s: %s\n", argv[1], strerror(errno));
    exit(1);
  }

  scanfile();
  exit(0);
}
