#ifndef SOLVER_H
#define SOLVER_H

#include<bits/stdc++.h>
using namespace std;

/*
把每个连接器看成一个点，同一行（或同一列）里相邻的两个连接器之间连一条候选边。
只连相邻的两个，是因为栅栏不能拐弯：如果一条线中途还压着第三个连接器，
那条线只能在第三个连接器那儿断开成两段，所以任何合法方案都能拆成"相邻连接器之间的一段"。

在这个模型上问题变成：从候选边里选一个子集，使得
  (1) 每个连接器上挂的选中边数恰好等于它的 k；
  (2) 一条横边和一条竖边不能在同一个空格里十字交叉。
两条约束单看都不难，叠在一起就 NP 难了——全棋盘都是连接器、k 全为 1 时，
它退化成在这个网格图上找一个 1-因子，而横竖互斥那一条又比 1-因子更紧，
所以这里用约束传播加回溯，回溯时优先挑最紧的点分支。

规模：连接器 V ≤ nm，候选边 E ≤ 2nm，每个点最多四条关联边，
所以建图和存图都是 O(nm+E)，真正决定运行时间的是搜索树结点数，不是单个结点的开销。
*/

// 一条候选边，就是连接器 u 和 v 之间的一段水平或垂直栅栏。
// 两端的格子坐标存一份而不是现算：输出方向要用，选边时要沿它扫过压住的空格，
// 坐标建图时就知道，存下来省掉后面反复的除法取模。
// st 是这个点的状态，放进边里而不是另开一张表，是因为它只属于这条边——
// 拆成两张表以后，改一处忘一处就是最容易出的 bug。
struct Edge{
    int u,v;            // 两端连接器编号，0 基，按行优先编号
    int r1,c1,r2,c2;    // 两端所在格子，0 基；建边时保证 r1<r2 或 c1<c2
    int st;             // 0 未定，1 选中，2 弃用
};

// 一个连接器。need 是"还差几条"，free 是"还有几条未定边"，
// 两个数一起决定它还能不能凑出 k 条；inq 只是传播队列的去重标记。
// 三个字段都放进结构体，就是为了让一个点的状态只在一处改、只在一处读。
struct Vert{
    int need;
    int free;
    char inq;
    vector<int> inc;    // 挂在这个点上的候选边编号
};

class Solver{
public:
    // 建图：给连接器编号、连候选边、记下每条边压住的空格。
    // 永远返回 true，无解判断统一放在 solve() 开头——那里能一次把所有点扫完，
    // 分成两处写反而容易漏掉"总度数为奇数"这种全局判定。
    bool init(int n,int m,const vector<int>& grid);

    // 从初始状态搜一个合法方案，false 表示无解。可以重复调用，状态在开头重置。
    bool solve();

    // 每个连接器的 {上,下,左,右}，第 i 项对应第 i 个连接器（行优先编号）。
    // 编号顺序就是题目要的输出顺序，所以这里不再排序。
    vector<array<int,4>> directions() const;

    // 只给实验用，不参与任何判定：搜索树结点数、传播里检查过的点数。
    long long nodes() const{ return nodeCnt; }
    long long propagations() const{ return propCnt; }

    // 分支点策略，只影响快慢不影响答案：0 自由度减需求最小（默认），
    // 1 行优先第一个，2 需求最大，3 未定边最多。第 4 章拿这四种在同一批数据上比结点数。
    void setHeuristic(int h){ heur=h; }

    // 关掉传播，只留逐边检查，用来量传播到底值多少。关掉后仍然正确，只是结点数会暴涨。
    void setPropagation(bool on){ useProp=on; }

private:
    //---------- 建图 ----------
    void addEdge(int u,int v,int r1,int c1,int r2,int c2);

    //---------- 状态改动 ----------
    // 所有改动都走这四个入口，别的函数不许直接写 need/free/st。
    // 因为 undo 只认 trail 里的记录，绕过去改一次，回滚后就还原不到原值。
    void put(int& slot,int val);     // 改一个字段，先把旧值压进 trail
    void pushVertex(int v);          // 排进待传播队列，同一个点只排一次
    void subFree(int v);             // 未定边数减一，并把该点排回队列
    bool selectEdge(int e);          // 选一条边，顺手否掉与它交叉的对向边
    void banEdge(int e);             // 弃用一条边，已经定了的边再否一次是幂等的
    void undo(size_t mark);          // 按 trail 倒着还原

    //---------- 搜索 ----------
    bool propagate();                // 反复套三条规则直到不动点
    bool dfs();                      // 回溯主体
    int chooseVertex() const;        // 挑下一个分支点，-1 表示全部满足

    //---------- 建图结果（solve 之后只读）----------
    int n,m;
    vector<int> deg;         // 每个格子的 k，0 表示没有连接器
    vector<int> cid;         // 格子 -> 连接器编号，-1 表示空
    vector<int> cellOf;      // 连接器 -> 格子编号
    vector<Edge> edges;      // 候选边表
    vector<Vert> verts;      // 连接器表
    vector<int> hEdge,vEdge; // 空格 -> 压在它上面的横边/竖边，-1 表示没有

    //---------- 搜索状态 ----------
    vector<int> que;         // 待检查的连接器

    // 回溯只记"哪个字段被改成了什么"，不整份拷贝状态。
    // 状态一共 V+E 个字段，一份拷贝几万个字节，几十万结点就是几十 GB 的挪动；
    // trail 只记真正改过的那几个，代价跟"改了多少"成正比，跟题目规模无关。
    // 代价是回滚必须严格倒序：同一个字段可能在一层里被改多次，顺序错了就还原成中间值。
    struct Change{int* p;int old;};
    vector<Change> trail;

    //---------- 实验统计 ----------
    long long nodeCnt,propCnt;
    int heur;
    bool useProp;
};

#endif
