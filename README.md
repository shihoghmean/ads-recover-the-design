# Recover the Design

ads（王灿）Research Project 3 —— 从"只留下连接器"的残图恢复栅栏布局：
把题面还原成网格图上的度数约束 + 交叉互斥问题，用传播 + 回溯求解，
并在 50×50 上限上量它的搜索规模与最坏情况。

题目来源：PTA `ZJUADS_王灿2025_P3 (PR)` 第 8-1 题（主观题，20 分）。

## 要求与对应

| 题面 | 要求 | 本仓库 |
| --- | --- | --- |
| Programming（10 分） | 写程序，**注释必须充分** | `code/src/` 三个文件；逐文件的行数、注释行数用 `python code/tools/stats.py` 现算 |
| Testing（3 分） | 提供一组测试用例，填进测试报告 | 三组题面样例 + 编进程序的七个边界用例（`recover test`）+ 400 组与暴力解的对拍 + 五组性能实验，全部写进报告第 3 章的测试表（目的 / 期望 / 实际 / 可能原因 / 状态） |
| Analysis（3 分） | 写分析与评论 | 报告第 4 章：传播为什么有效、随机无解为何 0 结点返回、与 f-factor / SAT / 流模型的对照、四条改进 |
| Documentation（4 分） | Chapter 1（1）、Chapter 2（2）、整体版式（1） | [`documents/report.pdf`](documents/report.pdf)（18 页）：Ch1 讲题与背景、Ch2 讲建模与算法（伪代码在正文里）、Ch3 测试、Ch4 分析；源码在 `code/src/`，报告不复制全文 |

## 快速开始

环境：Windows + MinGW-W64（g++ ≥ 8，C++17）、CMake ≥ 3.21 与 Ninja、Python 3（脚本只用标准库）。

程序从标准输入读、往标准输出写，所以下面用 `cmd` 风格的 `<` 重定向；
PowerShell 不支持 `<`，把 `xxx.exe < file` 换成 `Get-Content file | xxx.exe` 即可。

```powershell
# 1. 配置 + 编译（第一次或 CMakeLists.txt 改过后要重新配置）
cmake --preset default
cmake --build --preset default

# 2. 跑题面三组样例：前两组有解，第三组（5×6 改动一行）必须输出 No Solution
build/recover.exe < code/samples/sample1.in
build/recover.exe < code/samples/sample2.in
build/recover.exe < code/samples/sample3.in

# 3. 看搜索规模（统计行走 stderr，不影响 stdout 的答案）
build/recover.exe --stats < code/samples/sample1.in

# 4. 七个边界用例（编在程序里，末行 7/7 ok 才算过）
build/recover.exe test

# 5. 与暴力解对拍 400 组小实例 + 20 组大构造
python code/tools/stress.py 400

# 6. 复现报告第 3 章的五组实验（种子固定，数字可复现）
python code/tools/bench.py
```

## 目录结构

```
ads-recover-the-design/
├── CMakeLists.txt    一个可执行目标 recover，固定 -O2
├── CMakePresets.json 把生成器与构建目录固定成 Ninja / build
├── code/             提交包里的 code/
│   ├── Readme.txt    构建与运行说明（提交要求）
│   ├── src/          主程序源码（三个文件）
│   │   ├── solver.h      数据结构与状态、三条传播规则、分支与回溯的声明
│   │   ├── solver.cpp    实现：init → propagate → chooseVertex → dfs
│   │   └── main.cpp      读入、调用、按题面格式输出，命令行入口
│   ├── samples/      题面三组样例的输入与参考输出
│   └── tools/        brute.py、gen.py、check.py、stress.py、bench.py、stats.py
├── documents/        提交包里的 documents/
│   ├── readme.txt    目录里有什么、按什么顺序看、口径
│   ├── report.tex    报告源码（xelatex 编译两遍）
│   ├── report.pdf    报告成品，18 页
│   └── results/      bench.md：第 3 章五组实验的原始输出
└── data/             基准与调试产物（.gitignore，不提交）
```

`code/` 与 `documents/` 与 `Requirements.doc` 要求的提交压缩包一一对应，打包时只取这两个目录。
脚本放在 `code/tools/` 里是因为它们也要随包提交（复现测量、复核正确性用），主程序不依赖它们。

## 设计要点

- **题目先还原成图**：连接器是点，同一行/列里**相邻**的两个连接器之间才有一条候选边。
  这一条限制把候选边压到 O(nm) 条，也是"栅栏不能弯、中间不能穿过别的连接器"的直接翻译。
- **只有一种冲突**：横边与竖边压住同一格。每个空格最多被一条横边、一条竖边压住，
  于是查冲突是 O(1) 的表查（`hEdge`/`vEdge` 两个数组），不用两两比线段。
- **三条传播规则**：某点剩余需求 > 未定边数 → 失败；需求为 0 → 全部未定边弃用；
  需求等于未定边数 → 全部选中。选中一条边时顺带把它跨度内所有对向边弃用。
  实测关掉传播后 50×50 的同两组实例从"几十毫秒"变成"超时"，所以它是能否在时限内
  跑完的胜负手，不是常数优化。
- **分支点和取值一起选**：在 need > 0 的点里取 `free − need` 最小者，再在这个点的
  未定边里枚举"恰好选 need 条"的组合（一个点最多四条边，组合数 ≤ 6）。
- **回溯用一个 trail 数组**，不做深拷贝：每次改动压一条 `(字段指针, 旧值)`，失败时倒序写回。
- **两个根结点预检**：总度数为奇数直接判无解（每条边贡献 2 度）；某点需求 > 邻居数判无解。
  随机度数表几乎全被这两条拦住，所以"随机输入无解"是 0 结点、0.00 ms。

## 正确性怎么保证

三条独立的路子，都在报告第 3 章：

1. **样例**：三组题面样例逐行核对；有解的两组与参考解逐字相同（多解问题本来不要求）。
2. **边界**：`recover test` 里编进七组用例，覆盖全空、单点、总度奇数、十字交叉、四向全接、50×50 全空。
3. **对拍**：`code/tools/brute.py` 是另写的一份暴力枚举（不共享求解器代码），
   400 组小实例（暴力判有解 200 / 无解 200）零差异；另 20 组 50×50 构造实例只验解合法。

测试期真正抓到的四个 bug 写在报告 3.9 节：两个在测试工具里（`in` 比值不比下标、
生成器只查 `owner` 让栅栏从连接器身上穿过去），两个在求解器里（跨格子的竖边被判成"右"、
两种"越大越好"的分支策略误用了降序变量导致根结点直接判成功）。

## 数据与合规

本题不需要外部数据集：样例来自题面，其余输入由 `code/tools/gen.py` 按固定种子现生成，
`bench.py` 的实验同样固定种子，报告里的数字可以照上面的命令复现。
`data/` 是本地跑基准与调试的产物，在 `.gitignore` 里，不进库也不进提交包。
