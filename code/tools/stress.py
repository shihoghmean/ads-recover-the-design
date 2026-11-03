#!/usr/bin/env python3
"""对拍：小规模随机输入上，用 brute.py 的结论给主程序当标准答案。

判定规则：
  * 暴力说无解  -> 主程序必须输出 "No Solution"（或者给出一个合法解，说明暴力错了，报 bug）
  * 暴力说有解  -> 主程序必须给出合法解，不能输出 "No Solution"
另外跑一批"一定可解"的大规模输入（gen.py layout），只验证主程序给出的解合法。

用法：python stress.py [轮数] [可执行文件路径]
"""
import random
import subprocess
import sys
import os

import brute
import check
import exe as exefinder
import gen

HERE = os.path.dirname(os.path.abspath(__file__))


def to_text(n, m, k):
    return "\n".join(["%d %d" % (n, m)] + [" ".join(map(str, row)) for row in k]) + "\n"


def run(exe, case):
    p = subprocess.run([exe], input=case, capture_output=True, text=True)
    return p.stdout


def classify(n, m, k, out):
    """-> (状态, 说明)，状态取 'ok' / 'nosol' / 'invalid'。"""
    if out.strip().split()[:2] == ["No", "Solution"]:
        return "nosol", ""
    err = check.check(n, m, k, out)
    return ("ok", "") if err is None else ("invalid", err)


def main():
    rounds = int(sys.argv[1]) if len(sys.argv) > 1 else 500
    exe = sys.argv[2] if len(sys.argv) > 2 else exefinder.find_exe()
    rng = random.Random(20261103)

    bad = 0
    stats = {"solvable": 0, "unsolvable": 0}
    for i in range(rounds):
        n, m = rng.randint(3, 6), rng.randint(3, 6)
        if i % 2 == 0:
            k = gen.gen_random(n, m, rng.choice([0.4, 0.6, 0.8]), rng)
        else:
            k = gen.gen_layout(n, m, rng.choice([0.3, 0.5, 0.7]), rng)
        case = to_text(n, m, k)
        truth = brute.solve(n, m, k)
        status, why = classify(n, m, k, run(exe, case))
        stats["solvable" if truth else "unsolvable"] += 1

        if truth is None and status != "nosol":
            bad += 1
            print("[%d] 暴力无解但主程序给了 %s %s" % (i, status, why))
            print(case)
        elif truth is not None and status == "nosol":
            bad += 1
            print("[%d] 暴力有解但主程序说无解" % i)
            print(case)
        elif status == "invalid":
            bad += 1
            print("[%d] 主程序输出不合法：%s" % (i, why))
            print(case)

    # 大规模"一定有解"的输入：只看解是否合法
    big_bad = 0
    for i in range(20):
        n = m = rng.choice([20, 50])
        k = gen.gen_layout(n, m, rng.choice([0.25, 0.4]), rng)
        status, why = classify(n, m, k, run(exe, to_text(n, m, k)))
        if status != "ok":
            big_bad += 1
            print("[big %d] %dx%d 构造可解但主程序 %s %s" % (i, n, m, status, why))

    print("抽查 %d 组（暴力有解 %d / 无解 %d）：%d 处不符；另 20 组大构造可解：%d 处不符"
          % (rounds, stats["solvable"], stats["unsolvable"], bad, big_bad))
    return 1 if bad or big_bad else 0


if __name__ == "__main__":
    sys.exit(main())
