#!/usr/bin/env python3
"""统计源码行数与注释行比例，交稿前自查用。

两种口径都给：
  纯注释  只数"整行都是注释"的行；
  宽口径  再加上"代码后面跟了注释"的行（一行里既有代码也有注释）。
注释行按 // 和 /* */ 块算，字符串里的 // 不管（本工程的代码里没有这种陷阱）。

用法：python stats.py [文件或目录...]（不给就统计 ../src）
"""
import os
import sys


def strip_string(line):
    """把字符串字面量挖掉，避免把 URL 里的 // 当成注释。"""
    out = []
    i = 0
    quote = None
    while i < len(line):
        ch = line[i]
        if quote:
            if ch == "\\":
                i += 2
                continue
            if ch == quote:
                quote = None
            i += 1
            continue
        if ch in "\"'":
            quote = ch
            i += 1
            continue
        out.append(ch)
        i += 1
    return "".join(out)


def count(path):
    total = pure = trailing = blank = in_block = 0
    with open(path, encoding="utf-8") as f:
        for raw in f:
            total += 1
            line = raw.strip()
            if in_block:
                pure += 1
                if "*/" in line:
                    in_block = False
                continue
            if not line:
                blank += 1
                continue
            code = strip_string(line)
            pos = code.find("//")
            block = code.find("/*")
            if pos == 0 or block == 0:
                pure += 1
                if block == 0 and "*/" not in code:
                    in_block = True
            elif pos > 0 or block > 0:
                trailing += 1
            else:
                pass
    return total, pure, trailing, blank


def collect(args):
    files = []
    for a in args:
        if os.path.isdir(a):
            for name in sorted(os.listdir(a)):
                if name.endswith((".h", ".hpp", ".cpp", ".c")):
                    files.append(os.path.join(a, name))
        else:
            files.append(a)
    return files


def main():
    args = sys.argv[1:] or [os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src")]
    files = collect(args)
    tot = [0, 0, 0, 0]
    print("| 文件 | 总行 | 纯注释 | 行尾注释 | 空行 | 纯注释率 | 宽口径 |")
    print("|---|---|---|---|---|---|---|")
    for path in files:
        t, p, tr, b = count(path)
        for i, v in enumerate((t, p, tr, b)):
            tot[i] += v
        print("| %s | %d | %d | %d | %d | %.1f%% | %.1f%% |"
              % (os.path.basename(path), t, p, tr, b, 100.0 * p / t, 100.0 * (p + tr) / t))
    t, p, tr, b = tot
    print("| **合计** | %d | %d | %d | %d | %.1f%% | %.1f%% |"
          % (t, p, tr, b, 100.0 * p / t, 100.0 * (p + tr) / t))
    code_lines = t - p - b
    print("\n代码行（含行尾注释行）= %d，纯注释行 / 代码行 = %.1f%%" % (code_lines, 100.0 * p / code_lines))


if __name__ == "__main__":
    main()
