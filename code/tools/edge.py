#!/usr/bin/env python3
"""跑 code/tests/ 里的边界用例，逐条核对期望结果。

期望分三种：
  empty        输出必须为空（网格里根本没有连接器）
  nosolution   必须输出 No Solution
  valid        必须输出一个合法方案（交给 check.py 验）
期望值写在本文件的表里，加用例时记得同步加一行。
"""
import os
import subprocess
import sys

import check
import exe as exefinder

HERE = os.path.dirname(os.path.abspath(__file__))
TESTS = os.path.join(HERE, "..", "tests")

CASES = [
    ("edge1-empty.in", "empty"),
    ("edge2-single.in", "nosolution"),
    ("edge3-pair.in", "valid"),
    ("edge4-odd.in", "nosolution"),
    ("edge5-cross.in", "nosolution"),
    ("edge6-plus.in", "valid"),
    ("edge7-zero50.in", "empty"),
]


def main():
    exe = exefinder.find_exe()
    bad = 0
    for name, expect in CASES:
        path = os.path.join(TESTS, name)
        n, m, grid = check.read_case(path)
        with open(path, encoding="utf-8") as f:
            case = f.read()
        out = subprocess.run([exe], input=case, capture_output=True, text=True).stdout
        if expect == "empty":
            got = "empty" if not out.strip() else "非空"
        elif out.strip().split()[:2] == ["No", "Solution"]:
            got = "nosolution"
        else:
            err = check.check(n, m, grid, out)
            got = "valid" if err is None else "invalid: %s" % err
        ok = got == expect
        bad += 0 if ok else 1
        print("%-18s 期望 %-11s 实际 %-11s %s" % (name, expect, got, "OK" if ok else "FAIL"))
    print("共 %d 个边界用例，%d 个不符" % (len(CASES), bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
