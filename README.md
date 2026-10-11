# better_vec.hpp

一个**纯头文件**的 C++20 工具库，给 `std::vector` 套上两层壳：

- **`OpAgent`** —— 向量工具：查找、合并、搬移、区间
- **`GenOpProxy`** —— 处理器管道：用 `|` 把一串函数串起来，对容器做变换

特点：**管道里每一步的返回值会自动喂给下一步**，支持中途换类型（`int → std::string`）、支持原地修改（引用返回）。

---

## 快速开始

```cpp
#include "better_vec.hpp"

int twice(int x){ return x * 2; }

std::vector<int> vec{1,2,3,4};

// 全量遍历 + 逐个 ×2
auto r = OpVec::control(vec).iterator(
             OpVec::range(OpVec::sendAll()) | twice);
// r == {2,4,6,8}
```

预览：

| 片段 | 作用 |
|---|---|
| `OpVec::control(vec)` | 把 `vec` 交给 `OpAgent` 托管 |
| `OpVec::sendAll()` | 选一个「遍历器」：全量 / 指定下标 / 区间 |
| `OpVec::range(...)` | 把遍历器包成管道起点 |
| `\| twice` | 接一个处理器（函数、lambda、函数指针、仿函数都行） |
| `.iterator(...)` | 执行整条管道 |

---

## 三个遍历器

```cpp
OpVec::sendAll()                    // 全部元素
OpVec::sendIndex({0, 3})            // 只处理下标 0 和 3
vecUtils::Controller::sendRange(b, e)  // 只处理 [b, e) 区间
```

实测：

```
sendAll()            | twice  →  2 4 6 8
sendIndex({0,3})     | twice  →  2 8
sendRange(begin+1, end-1) | twice →  4 6
```

> ⚠️ `sendRange` 没有被 `using` 进 `OpVec` 命名空间，目前得写全名
> `vecUtils::Controller::sendRange(...)`。想统一的话在 `OpVec` 里补一行 `using` 即可。

---

## 串联处理器

```cpp
auto addTen = [](int x){ return x + 10; };

OpVec::range(OpVec::sendAll()) | twice | addTen;   // → 12 14 16 18
```

**同一个函数可以重复出现**——只有第一个走 `sendAll` 的遍历范围，后续都基于上一步的结果：

```cpp
OpVec::range(OpVec::sendAll()) | twice | twice;     // → 4 8 12 16
```

### 类型可以中途变

```cpp
auto toStr = [](int x){ return "v" + std::to_string(x); };
OpVec::range(OpVec::sendAll()) | toStr;             // → vector<string>{"v1","v2","v3","v4"}
```

`operator|` 用 `first_arg_is_v` 做编译期校验：下一个处理器的**首参数类型**必须匹配上一个的**返回类型**，对不上直接编译失败（不是运行时崩）。

### 原地修改（引用返回）

处理器返回引用时，走「原地改」分支，不生成新容器：

```cpp
int& incRef(int& x){ x += 1; return x; }

std::vector<int> v2{10,20,30};
OpVec::control(v2).iterator(OpVec::range(OpVec::sendAll()) | incRef);
// v2 直接变成 {11,21,31}
```

---

## 为什么第一个处理器特殊

用户提供的遍历器（比如 `sendAll`）绑定的是**原始容器的范围**，后续处理器返回的往往是**新容器**，两者不兼容。所以库内部这么处理：

```
第 1 个处理器 → 用 loopFunc（用户遍历器）遍历原始容器
第 2..n 个   → 用默认 for 遍历上一步返回的容器
```

判断依据是**编译期索引** `createVecProcessor<I == 0>`，不是类型比较——所以同类型处理器重复出现也不会误判。

---

## OpAgent：向量工具

```cpp
auto agent = OpVec::control(vec);

agent.Copy();                       // 拷贝一份出来
agent.Move();                       // 搬走（非 const 才行）
agent.ptrSearch([](int x){ return x == 3; });   // 找元素，返回指针（没找到是 nullptr）
agent.ItSearch ([](int x){ return x == 3; });   // 找元素，返回迭代器
agent.ItBegin(1) / agent.ItEnd(1);  // 带偏移的 begin / end
agent.CopyMerge(other);             // 拷贝合并
agent.MoveMerge(other);             // 移动合并（会把 other 掏空）
agent.CopyMerge(other, pos);        // 指定插入位置
agent.findIdx(f);                   // 用 FunWrap 的标记函数，返回命中的下标集合
```

`control` 有三种重载：

```cpp
OpVec::control(vec);                    // 借用（左值）
OpVec::control(std::move(vec));         // 接管所有权
OpVec::control(constVec);               // 只读——Move/Merge 会被 requires 禁用
```

---

## FunWrap：现成的标记函数

```cpp
using FunWrap::repeatSecond;    // 只和后半段比，标记出「后面还有重复」的下标
using FunWrap::allDuplicates;   // 和全体比，标记出所有重复项的下标

auto dup = OpVec::control(vec).findIdx(
    FunWrap::allDuplicates([](int a, int b){ return a == b; }));
// dup 里是所有重复元素的下标
```

还有 `MemberFunc` 把成员函数转成普通可调用物：

```cpp
struct Foo { int bar(int x){ return x * 2; } };
Foo foo;
auto f = FunWrap::MemberFunc(&foo, &Foo::bar);
f(21);  // 42
```

---

## 编译要求

| 项目 | 要求 |
|---|---|
| 标准 | **C++20**（用了 `requires`、`auto&&` 形参、模板 lambda） |
| 依赖 | 仅标准库：`<vector> <algorithm> <ranges> <functional> <type_traits> <tuple> <memory>` |
| 测试通过 | GCC 15.2 (MinGW) |

---

## 已知小问题

1. **`sendRange` 未导出到 `OpVec`** —— 得写 `vecUtils::Controller::sendRange(...)` 另一方面是为了隐藏底层 需要可自己改
2. **`iterator()` 会移动 `loopFunc`** —— `make_next` 里用 `std::forward<RangeF>(loopFunc)`，
   如果遍历器捕获了有状态的东西，复用同一个 `range(...)` 起点两次会拿到被掏空的状态。
   需要复用就改成 `loopFunc`（拷贝）。
3. **错误信息偏长** —— 模板嵌套深，建议从最内层 `error:` 开始看 (正常使用无错误)

---

## 概括

```cpp
OpVec::control(vec).iterator(OpVec::range(OpVec::sendAll()) | f1 | f2 | f3);
```

**选容器 → 选范围 → 串处理器 → 出结果**，编译期类型检查

## 个人

窝是初学者(bush