Recover the Design —— Research Project 3 的 code/ 说明（build & run）
================================================================

code/ 内部结构
--------------
src/          主程序源码，三个文件，C++17
  solver.h         求解器的类声明：数据结构、状态、三条传播规则、分支与回溯
  solver.cpp       求解器的实现
  main.cpp         读入、调用求解器、按题目格式输出；命令行入口
samples/      题面给的三组样例（输入 + 题面里的参考输出），以及它们现在的实际输出
tests/        七个边界用例的输入（配合 tools/edge.py 逐个校验）
tools/        辅助脚本：暴力、生成器、校验器、对拍、边界用例、基准、注释率统计
Readme.txt    本文件

浏览顺序：src/solver.h（先看类注释里那几条不变量）→ src/solver.cpp（init、propagate、
chooseVertex、dfs 四段）→ src/main.cpp（只是读写）。

环境要求
--------
g++ 支持 C++17（实测 GCC 16.2.0 / MinGW-w64，Linux 的 g++ 同样可以）。
只用 C++17 标准库，不依赖任何第三方库。

tools/ 里的脚本都是 Python 3，只用标准库，不需要 numpy、matplotlib；
主程序本身不依赖它们，只编 C++ 也能跑完所有功能。

编译
----
源码是 C++17，扩展名 .cpp 与 .h（即课程 Hand-in 要求里 Source code files (*.c, *.h) 那一项）。
在本目录（code/）内一条命令，三个文件一起编：

    g++ -O2 -std=c++17 -Wall -Wextra src/main.cpp src/solver.cpp -I src -o recover.exe

-O2 必须保留：报告第 3 章的全部时间都是在这个优化级别下测的，换级别数字不可比。

如果拿到的是完整仓库（含仓库根的 CMakeLists.txt 与 CMakePresets.json），
也可以按仓库根 README.md 的说明用 CMake 构建，产物在 build/recover.exe：

    cmake --preset default
    cmake --build --preset default

运行
----
输入格式（题面）：第一行 n m，随后 n 行每行 m 个整数，0 表示空格，1..4 表示该格
连接器要接出去的栅栏条数。程序从标准输入读，往标准输出写。

下面的 "< 文件" 是 cmd 的重定向写法；在 PowerShell 里 "<" 不可用，等价写法是
    Get-Content samples\sample1.in | .\recover.exe

1) 跑题面的三组样例（在 code/ 内执行）：

       .\recover.exe < samples\sample1.in
       .\recover.exe < samples\sample2.in
       .\recover.exe < samples\sample3.in

   sample1、sample3 都是有解实例，输出若干行 “row col up down left right”；
   sample2 必须是单独一行 No Solution。题目允许多解，所以只要求合法，
   不要求与 samples\*.out 逐字相同（实际上这两组恰好逐字相同）。

2) 看搜索规模（往 stderr 打一行统计，stdout 的答案不受影响）：

       .\recover.exe --stats < samples\sample1.in

   形如 nodes=1 propagate=14 time_ms=0.02 answer=yes，四个数分别是搜索树结点数、
   传播次数、求解耗时（不含读入输出）与是否有解。报告第 3 章的表都来自这行。

3) 换分支策略、关掉传播（做对照实验用，正常求解不需要）：

       .\recover.exe --heur 2 < samples\sample1.in
       .\recover.exe --noprop  < samples\sample1.in

   --heur 取 0..3：0 = free-need 最小（缺省，报告里的主方案），1 = 行优先第一个，
   2 = need 最大，3 = free 最大。

4) 边界用例（本文件夹内执行，七个用例一起跑并逐个对照期望）：

       cd tools
       python edge.py

   输出末行是 7/7 OK 才算过。七个用例覆盖：全空棋盘、单点要度数 1（必无解）、
   一对相邻连接器、总度数为奇数（必无解）、两条候选边在中心十字交叉（必无解）、
   中心四向全接、50x50 全空。

辅助脚本（tools/，在 code/ 或仓库根执行都可以，脚本会自己找 exe）
------------------------------------------------------------------
    python brute.py < 用例.in           暴力枚举，只用于小规模，作为对拍的标准答案
    python gen.py layout 20 20 0.3 7    生成“按构造必然有解”的实例（n m 密度 种子）
    python gen.py random 5 5 0.5 1      生成随机度数表（大多无解）
    python check.py 用例.in 答案.out     校验一个解是否合法（度数、端点、交叉、越界）
    python stress.py 400                对拍 400 组小实例 + 20 组大构造，末行给不符处数
    python bench.py                     跑报告第 3 章的五组实验（scale/heuristic/worst/hard/unsolvable）
    python bench.py prop                只跑传播开/关对照（报告 3.7 节那张表）
    python stats.py                     统计 src/ 的注释行比例（课程要求 >= 30%）

没有 Python 也不影响交作业：这些脚本只是复现测量与自查用的，主程序不需要它们。

数据
----
本题没有外部数据集，输入全部由 gen.py 现生成或题面样例给出；
bench.py 用的随机实例由固定种子生成，同样的种子每次跑出同样的输入，
所以报告里的数字可以照上面的命令复现。基准原始输出存在
documents/results/bench.md。
