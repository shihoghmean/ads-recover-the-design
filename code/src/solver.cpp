#include"solver.h"

/*
整体思路：把"选哪些栅栏段"写成一个 0/1 约束满足问题，用约束传播加回溯去解。
传播负责把能定死的边一次定死（这个点已经够了 / 剩下的边一条都不能少 / 被交叉挡住），
回溯只负责在剩下的自由度里挑一个点枚举。所有状态改动都压进 trail，
失败时按记录倒着还原，所以递归里不用拷贝状态。

三处容易写错的地方先记在这：
  1. 候选边只连同一行或同一列里相邻的两个连接器。栅栏不能拐弯，中途压着第三个连接器
     就只能在那儿断开，所以只连相邻的不会漏解——反过来，连了不相邻的就会多出非法解。
  2. 交叉约束是边与边之间的：同一个空格里最多一条横边一条竖边，查 hEdge/vEdge 就能
     唯一定位到该否掉的那条，不用扫全图。这条是 O(1) 找边的前提。
  3. 三条传播规则都只删不可能成立的赋值，不删任何一个可行解；分支枚举又覆盖了该点
     所有可能的选法，所以搜索失败就等于真的无解。任一环节多删一次，结论就不成立了。

举题面样例 3（3×5）走一遍，说明传播凭什么比裸回溯快：
    1 1 2 0 1
    1 0 3 0 2
    1 2 1 0 1
一开始每个点按 k 算需求。右下角 (2,5) 的 k=2，而它的候选边只有上面和下面两条
（同一列里相邻的连接器只有这两个），于是 free==need==2，规则三把两条都定死；
这两条一选，(1,5) 和 (3,5) 的需求各减 1，可能又满足"剩下的边一条都不能少"，
就这样连锁推下去，很多点压根不用分支。传播停下时只剩少数几个自由度，这时才进 dfs。
*/

//---------- 建图 ----------

// 三件事：给连接器编号、连出候选边、记下每条边压住哪些空格。
// 编号按行优先，最后输出时不用再排序，编号顺序就是题目要的顺序。
bool Solver::init(int n,int m,const vector<int>& grid){
    this->n=n;
    this->m=m;
    deg=grid;

    // 第一遍编号。cid 是"格子 -> 编号"的反查表，-1 表示空格；
    // 后面扫行扫列、建边、输出都要用它，所以先建好再连边，一次遍历就够。
    cid.assign((size_t)n*m,-1);
    cellOf.clear();
    for(int r=0;r<n;r++){
        for(int c=0;c<m;c++){
            if(deg[r*m+c]==0) continue;
            cid[r*m+c]=(int)cellOf.size();
            cellOf.push_back(r*m+c);
        }
    }

    edges.clear();
    hEdge.assign((size_t)n*m,-1);
    vEdge.assign((size_t)n*m,-1);
    verts.assign(cellOf.size(),Vert());
    for(size_t v=0;v<verts.size();v++){
        verts[v].need=0;
        verts[v].free=0;
        verts[v].inq=0;
        verts[v].inc.clear();
    }

    // 第二遍铺横边：每行里相邻的两个连接器之间一条。中间没有别的连接器，
    // 所以这一段是"整段"，不会和同一行里别的候选边重叠。
    for(int r=0;r<n;r++){
        int lastC=-1;
        for(int c=0;c<m;c++){
            int id=cid[r*m+c];
            if(id<0) continue;
            if(lastC>=0) addEdge(cid[r*m+lastC],id,r,lastC,r,c);
            lastC=c;
        }
    }
    // 第三遍铺竖边，同一列竖着扫一遍，道理一样。
    for(int c=0;c<m;c++){
        int lastR=-1;
        for(int r=0;r<n;r++){
            int id=cid[r*m+c];
            if(id<0) continue;
            if(lastR>=0) addEdge(cid[lastR*m+c],id,lastR,c,r,c);
            lastR=r;
        }
    }
    return true;
}

// 加一条候选边，并把它压住的空格登记到 hEdge/vEdge。
// 这两张表是"格子 -> 盖住它的边"，选边时靠它 O(1) 找到该否掉的对向边，
// 不必为了找一条交叉的线去扫全图——那是每个结点都要付的代价。
void Solver::addEdge(int u,int v,int r1,int c1,int r2,int c2){
    edges.push_back({u,v,r1,c1,r2,c2,0});
    const int e=(int)edges.size()-1;
    verts[u].inc.push_back(e);
    verts[v].inc.push_back(e);

    // 只登记中间的空格：两端是连接器，那条边本身不占连接器所在的格子。
    // 建边时已经保证 r1<r2 或 c1<c2，所以这里从 +1 扫到 <，不用再判大小。
    if(r1==r2){
        for(int c=c1+1;c<c2;c++) hEdge[r1*m+c]=e;
    }else{
        for(int r=r1+1;r<r2;r++) vEdge[r*m+c1]=e;
    }
}

//---------- 状态改动 ----------

// 改一个字段，先把旧值压进 trail。这是唯一允许写状态的地方。
void Solver::put(int& slot,int val){
    trail.push_back({&slot,slot});
    slot=val;
}

// 同一个点在一次传播里只排一次队，否则队列会被重复项撑大，
// 高密度实例上传播次数能翻好几倍（这个数是报告里拿来比策略的）。
// 队列当栈用（从尾部取）：传播到不动点的结果跟处理顺序无关，顺序只影响中间过程长短，
// 所以这里没为它单独做实验，就近取尾部，省一次头指针的维护。
void Solver::pushVertex(int v){
    if(verts[v].inq) return;
    verts[v].inq=1;
    que.push_back(v);
}

// 未定边数减一。这个量不参与"能不能凑够"的判断，只用来算自由度和挑分支点，
// 但它一变，这个点的三条规则就可能适用，所以要排回队列。
void Solver::subFree(int v){
    put(verts[v].free,verts[v].free-1);
    pushVertex(v);
}

// 弃用一条边。已经定过的边再否一次直接返回：传播里可能有多条路径否到同一条边，
// 重复处理不该被当成冲突，否则一路正确的搜索会被自己否死。
void Solver::banEdge(int e){
    if(edges[e].st!=0) return;
    put(edges[e].st,2);
    subFree(edges[e].u);
    subFree(edges[e].v);
}

// 选中一条边，顺手处理两个后果：两端各消耗一个度；它压住的空格里，对向的边全部弃用。
// 后一条是双向的——那个空格里已经躺着一条对向边并且被选中了，就是十字交叉，直接失败。
// 每个空格最多一条横边一条竖边，所以这个循环的代价是这条边的长度，跟度数无关。
bool Solver::selectEdge(int e){
    if(edges[e].st==1) return true;    // 选过，幂等
    if(edges[e].st==2) return false;   // 之前被否过，不能再选

    put(edges[e].st,1);
    const int u=edges[e].u,v=edges[e].v;
    put(verts[u].need,verts[u].need-1);
    put(verts[v].need,verts[v].need-1);
    if(verts[u].need<0||verts[v].need<0) return false;
    subFree(u);
    subFree(v);

    if(edges[e].r1==edges[e].r2){
        const int r=edges[e].r1;
        for(int c=edges[e].c1+1;c<edges[e].c2;c++){
            int o=vEdge[r*m+c];
            if(o<0) continue;
            if(edges[o].st==1) return false;   // 已经有竖线横在这儿
            banEdge(o);
        }
    }else{
        const int c=edges[e].c1;
        for(int r=edges[e].r1+1;r<edges[e].r2;r++){
            int o=hEdge[r*m+c];
            if(o<0) continue;
            if(edges[o].st==1) return false;
            banEdge(o);
        }
    }
    return true;
}

// 回滚到 trail 长度等于 mark 的那一刻，倒着还原。
// 队列里剩下的项属于刚被撤掉的那一层，状态已经回去了，不能再按它们检查，
// 所以直接清空；清的时候顺手把 inq 一起清掉，否则下次 pushVertex 以为这个点
// 还在队列里，会白白漏掉一次传播，那种 bug 只在特定数据上才现形。
void Solver::undo(size_t mark){
    while(trail.size()>mark){
        Change ch=trail.back();
        trail.pop_back();
        *ch.p=ch.old;
    }
    for(size_t i=0;i<que.size();i++) verts[que[i]].inq=0;
    que.clear();
}

//---------- 搜索 ----------

// 一直处理队列直到不动点。三条规则都在"只做必然推断"的范围内：
//   need<0 或 need>free  这个点无论如何凑不出 k 条边，整支失败；
//   need==0              已经够了，剩下的未定边一律弃用；
//   need==free           剩下的边一条都不能少，全部选上。
// 规则是链式的：选一条边会减少两端的需求，可能让邻居也满足后两条，
// 所以要用队列反复推，扫一遍是不够的。
//
// 为什么这三条不会删掉可行解——这是搜索失败能等于无解的前提，写代码时别顺手加第四条：
// 一条边要么被选要么不选，前两条把"已经不可能被选"的边定成弃用，第三条把
// "不选就凑不够"的边定成选中，两者都在给定 need/free 下是唯一可能的取值；
// 而 need/free 本身是从已经落定的边推出来的，没有引入任何假设。
// 反过来说，只要在传播里加一条"看起来合理"的启发式（比如"先挑度数大的点选"），
// 就可能把可行解删掉，那时程序只剩"找到解"的本事，"找不到"就不再是无解的证明。
bool Solver::propagate(){
    while(!que.empty()){
        const int v=que.back();
        que.pop_back();
        verts[v].inq=0;
        propCnt++;

        const int need=verts[v].need,free=verts[v].free;
        if(need<0||need>free) return false;

        if(need==0){
            for(size_t i=0;i<verts[v].inc.size();i++){
                int e=verts[v].inc[i];
                if(edges[e].st==0) banEdge(e);
            }
        }else if(need==free){
            for(size_t i=0;i<verts[v].inc.size();i++){
                int e=verts[v].inc[i];
                if(edges[e].st==0&&!selectEdge(e)) return false;
            }
        }
    }
    return true;
}

// 挑分支点。四种策略只影响搜索树宽度，不影响答案对不对（第 4 章有实测）：
//   0 自由度减需求最小：最接近被逼死的点，分支最少，默认用它；
//   1 行优先第一个：最朴素，写起来一行；
//   2 需求最大：先安排度数高的点；
//   3 未定边最多：先安排选择最多的点。
// 0 和 2/3 的优化方向相反，所以两类分数分开记：bestSlack/bestFree 越小越好，
// bestScore 越大越好。混用一个下降变量就永远选不中，这个坑真踩过。
int Solver::chooseVertex() const{
    int best=-1,bestSlack=1<<30,bestFree=1<<30,bestScore=-1;
    for(size_t v=0;v<verts.size();v++){
        if(verts[v].need==0) continue;
        if(heur==1) return (int)v;
        if(heur==2){
            if(verts[v].need>bestScore){ best=(int)v; bestScore=verts[v].need; }
            continue;
        }
        if(heur==3){
            if(verts[v].free>bestScore){ best=(int)v; bestScore=verts[v].free; }
            continue;
        }
        const int slack=verts[v].free-verts[v].need;
        if(slack<bestSlack||(slack==bestSlack&&verts[v].free<bestFree)){
            best=(int)v;
            bestSlack=slack;
            bestFree=verts[v].free;
        }
    }
    return best;
}

// 回溯主体：先传播，再挑一个点分支，枚举"选其中 need 条"的所有组合。
// 完备性：任何合法解在这个点选的都恰好是某个 size 为 need 的子集，枚举不会漏解；
// 剪枝：每个组合先落子再递归，递归第一件事又是传播，冲突会尽早暴露。
// 分支宽度最多 C(4,2)=6，所以主要成本是结点数，不是单点的开销。
//
// 这里不按"这条边选不选"逐条分支，而是按"这个点的 need 条边是哪几条"成组分支，
// 因为前者会在同一个点的不同选择顺序上重复走一遍同样的子树；
// 后者一次就把这个点的需求结清，代价是每个结点最多试 6 组，换来的是树窄。
bool Solver::dfs(){
    nodeCnt++;
    if(useProp&&!propagate()) return false;

    const int v=chooseVertex();
    if(v<0) return true;    // 所有点的 need 都归零，方案成立

    int cand[4],cnt=0;
    for(size_t i=0;i<verts[v].inc.size();i++){
        int e=verts[v].inc[i];
        if(edges[e].st==0) cand[cnt++]=e;
    }
    const int needHere=verts[v].need;
    const size_t mark=trail.size();

    for(int mask=0;mask<(1<<cnt);mask++){
        if(__builtin_popcount((unsigned)mask)!=needHere) continue;
        bool ok=true;
        for(int i=0;i<cnt&&ok;i++){
            if(mask>>i&1) ok=selectEdge(cand[i]);
            else banEdge(cand[i]);
        }
        if(ok&&dfs()) return true;
        undo(mark);   // 这一组失败，退回分支前的状态试下一组
    }
    return false;
}

// 把选中的边整理成题目要的方向表。
// 这里踩过一次坑：最初拿"另一端的行差是 ±1 就判上/下"来分方向，样例 1 直接少一位——
// 候选边两端可以隔着好几个空格，行差不止 1，跨多格的竖边被判成了"右"。
// 正确的判法是先看它是横边还是竖边（两端同行即横），再比大小定左右/上下。
vector<array<int,4>> Solver::directions() const{
    vector<array<int,4>> dirs(verts.size(),{0,0,0,0});
    for(size_t v=0;v<verts.size();v++){
        const int r=cellOf[v]/m,c=cellOf[v]%m;
        for(size_t i=0;i<verts[v].inc.size();i++){
            int e=verts[v].inc[i];
            if(edges[e].st!=1) continue;
            const int orow=(edges[e].u==(int)v)?edges[e].r2:edges[e].r1;
            const int ocol=(edges[e].u==(int)v)?edges[e].c2:edges[e].c1;
            if(orow==r){ dirs[v][ocol<c?2:3]=1; }
            else{ dirs[v][orow<r?0:1]=1; }
        }
    }
    return dirs;
}

// 入口：清状态、做两条一眼能看出的无解判定、把所有点排进队列开始搜。
// 两条判定都很便宜，命中一次就省掉整棵搜索树：
//   某个点的 k 比它的关联边数还大（周围根本没有那么多邻居）；
//   总度数是奇数（每条边贡献 2 度，奇数永远凑不出来）。
//
// 建图在 init 里做一次，solve 只负责重置搜索期会写的那几个字段。凡是函数体里
// 被 put() 碰过的东西，这里都要显式清零（edges[].st、need、free、inq、que、trail），
// 漏一个，第二次调用就会从一个脏状态出发——这种错不会崩，只会算出一个错的答案，
// 所以宁可多写这几行。deg/cid/cellOf/edges 的坐标和 inc 是只读的，不重置。
//
// 运行时间的大头不在这些扫描上，在 dfs 的结点数：每个结点要花一次 O(V) 的
// chooseVertex 和若干次传播，实测一个结点约 0.5 µs，而 V 最多一千出头，
// 所以结点数上万才开始有感；真正爆掉的输入是 40 万结点那一档。
bool Solver::solve(){
    for(size_t e=0;e<edges.size();e++) edges[e].st=0;
    que.clear();
    trail.clear();
    nodeCnt=0;
    propCnt=0;

    long long total=0;
    for(size_t v=0;v<verts.size();v++){
        verts[v].need=deg[cellOf[v]];
        verts[v].free=(int)verts[v].inc.size();
        verts[v].inq=0;
        if(verts[v].need>verts[v].free) return false;
        total+=verts[v].need;
    }
    if(total%2!=0) return false;

    for(size_t v=0;v<verts.size();v++) pushVertex((int)v);
    return dfs();
}
