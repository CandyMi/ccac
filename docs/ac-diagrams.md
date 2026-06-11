# AC 自动机工作原理

## 1. 系统架构

```mermaid
flowchart LR
    A["UTF-8 文本"] --> B["ccunicode.h<br/>编解码器"]
    B -->|"UCS-4 码点"| C["AC 自动机"]
    D["词表"] --> E["Trie 构建"]
    E --> F["失败链接<br/>(BFS)"]
    F --> C
    C --> G["匹配结果<br/>(offset 数组)"]

    style B fill:#4a9,stroke:#333,color:#fff
    style C fill:#49a,stroke:#333,color:#fff
    style F fill:#a94,stroke:#333,color:#fff
```

[▶ 在线编辑](https://mermaid.live/edit#pako:eNp1kcFOg0AQhl9lsqdqorRaCzYe9GLiQRNN6M3EG7ssJQm7y-6gapq-el-IZW2tJ5L5Z-bLN_-MIHIEQgk5wVFX5T7Tk2UQe_4nOtxEDq3i8zUjDmdZbVZamCI3b2sdqUZw5wyVcQ6DMkC7KHSqT2KufVR8x7k4uLd0A59dS1tWw7J3d4zFgo6cEW9RBztP6RudSOsnspWX6s2hVl-YGrctd4clRg0RkYkDFmqQDFjLEnSsTwjHWqAyqKRQRMgYjxGSsYLyHnsbFROjeLZ9h7MokM9Z1S2ytDmi3LxvuFXt6_DCYfbu5TX78B4-UqQvXORbt2LuYp_0nX0__cnb3j3mmY9VzYUb6ueIn_uz5YyCCxFWxhxh3y8Nu0pH_OD3jn1mORrVBn9GAR3X_4lXcCPnL_cY0P47c5HcO4mnPgJp9ehe0YvD3KdK3dvXf9tKMKz_AfdBuCM)

## 2. Trie 构建 + 失败链接

以字典 `{he, she, his, hers}` 为例。

BFS 构建失败链接规则：对节点 `v` 的子节点 `c`（码点 `x`），沿 `v→fail` 上溯直到找到含有 `x` 子节点的节点（或 root），`c→fail` 指向该子节点。

```mermaid
flowchart TD
    R(("root"))

    R -->|"h"| H(("h"))
    R -->|"s"| S(("s"))

    H -->|"e ●"| HE(("he"))
    H -->|"i"| HI(("hi"))

    HI -->|"s ●"| HIS(("his"))

    S -->|"h"| SH(("sh"))

    SH -->|"e ●"| SHE(("she"))

    HE -->|"r"| HER(("her"))

    HER -->|"s ●"| HERS(("hers"))

    %% failure links — BFS order
    H -.->|"fail"| R
    S -.->|"fail"| R
    %% HE: h→fail=root, root 无 'e' → root
    HE -.->|"fail"| R
    %% HI: h→fail=root, root 无 'i' → root
    HI -.->|"fail"| R
    %% SH: s→fail=root, root 有 'h' → h
    SH -.->|"fail"| H
    %% SHE: sh→fail=h, h 有 'e' → he
    SHE -.->|"fail"| HE
    %% HIS: hi→fail=root, root 有 's' → s
    HIS -.->|"fail"| S
    %% HER: he→fail=root, root 无 'r' → root
    HER -.->|"fail"| R
    %% HERS: her→fail=root, root 有 's' → s
    HERS -.->|"fail"| S

    HE:::terminal
    SHE:::terminal
    HIS:::terminal
    HERS:::terminal

    classDef terminal fill:#e44,stroke:#333,color:#fff
```

[▶ 在线编辑](https://mermaid.live/edit#pako:eNqNUstrwjAU_ithrhVUtK22RWHgduPChQs3Qk9H0LbNmoesL6z47zvpQ2sXgRzad849L0KO0JIESiriD7JxSF1W3gNTRCbXG-vZGJW8GeylCqHdNtLGpQRNY5XQMiYR5dWdjVG55jLAkMXBPLIJkBo3Q2KH6xACxpM-cPzHyDHEjAgIRtgDmpMIJkS3xAKjEDGBFU0Y9hNGoCHqyzyHltBqBB8rT9CNVtIjI0d1gymqsGtyL-LVt3ZKLxhV-23aQuMspMSDz_LHquZXQ3VYnpHnC40lnGCNLlBMdWkWNk8sL2dhtbfkrvMFQn2fH3PlsTlbjLNHAq9Rnt14Ls4sL-k-FPb5lNuT_yBuN2wOo3cU3V_ba3rxMFQ7a_L-6v1QLH18IF3C4bl6Y8D3L_MkUB4bZjWDRQ7qTNU82StPSWvovLwAr4G2XA)

## 3. 搜索过程 — 文本 `"ushers"` 匹配

```mermaid
sequenceDiagram
    participant T as 文本
    participant AC as AC 自动机
    participant M as 匹配结果

    Note over T,M: 字典 = {he, she, his, hers} | 扫描 "ushers"

    T->>AC: pos=0 'u'
    AC-->>AC: root 无 'u' → 停在 root

    T->>AC: pos=1 's'
    AC-->>AC: root→s (s 节点)

    T->>AC: pos=2 'h'
    AC-->>AC: s→h (sh 节点)

    T->>AC: pos=3 'e'
    AC-->>AC: sh→e ✦ she 节点 (终端!)
    M-->>M: ✅ [1,4) = "she"
    AC-->>AC: she→fail→he (终端!)
    M-->>M: ✅ [2,4) = "he"

    T->>AC: pos=4 'r'
    AC-->>AC: 当前 he→r (her 节点)

    T->>AC: pos=5 's'
    AC-->>AC: her→s ✦ hers 节点 (终端!)
    M-->>M: ✅ [2,6) = "hers"
    AC-->>AC: hers→fail→s (非终端，停止)
```

[▶ 在线编辑](https://mermaid.live/edit#pako:eNp1U01vgkAQ_StkjmpQUA5KYxraS01NmuiRmh6IS0fYuhssW1qN8b93gYVSHE4wM2_em3kvPEEhGBIs4BdOSsqZMLE54UxSHjbyGhowxbqK9aaw4o1q8JRNTAyht0ZHT7ZFuVPaIKRjQLEgVEM6u3JK_MeIFdGaeHIFUSItjwK5csTZgyv9W74i1jAHUOKbmNze4dxmKmUe1Gz5wPL8nNRcnEOSCg4BTuH6m9jyOyy9cvI5SHB4n2oWjsxH3OoSTUsXH1-vCd13f1DPlzH-o2B7Gx40tHn4wmIdniP_YtbG6gyLvMELJ3TVnt0_RmGktoz1x7P18CSmBZHnYl0YqfvEl2fyh27kP4xPln0shFJ8fR93zEeG5XdFhY3DGRYL06HmH6bH8XszwvRvBz_IC3tx3UjiFPIavkYqL4f7J1n9JjxnH1FyH2ufkfKX4J47NM2o-ntZ0O02TTX5BtfhVnyws5Gua84UWHi--ULZz74kzh1W)

## 4. 失败链接构建 — BFS 流程

```mermaid
flowchart TD
    subgraph BFS["BFS 构建失败链接"]
        Q["queue = [h, s]"] --> P1["pop h (fail=root)"]
        P1 --> C1["h 的子节点: e, i"]
        C1 --> F1["e→fail: root 无 e → root<br/>i→fail: root 无 i → root"]
        Q --> P2["pop s (fail=root)"]
        P2 --> C2["s 的子节点: h"]
        C2 --> F2["h→fail: root 有 h → 指向 root 的 h"]
        Q --> P3["pop he → … (BFS 继续)"]
    end

    style BFS fill:#a94,stroke:#333,color:#fff
```

[▶ 在线编辑](https://mermaid.live/edit#pako:eNp1kU1PhDAQhv8KmbNkkeVFQCAk7kEve3CPJhqP1EU7pSvQQoWfu9N1QVnW43Te5-m8kyEUxBAkhPAVR2lW5GfRpEyJAl2ahmKfQpYwu8H7IK7qNAxGMUwqQEN4jhGwMclktzAEttMDh7KR4BGahScmcjwex1QUj5qGx2gHvY0C5QiFBR04Jbs6qSEaqAcJlBh8q8OnlzLBc4R3KrU3dTLsLZW5eY_s45BFNvwMRiS-SZKH-BO0_ndWD5EsIPe-x5zId_YHhu8RqleR56lRBNJ3S5PVIStIf_1Nn4hFU7awbLfKVd1ypGjI9lMopojIuEFfq-Hf2B_ckXNBtN143muwNKzJSb7Ya9GAFA)


## 5. 完整工作流 — 从词表到匹配

```mermaid
flowchart TB
    subgraph 构建阶段
        A["词表<br/>he, she, his, hers"] --> B["逐词插入 Trie"]
        B --> C["ccac_rebuild_fail()<br/>BFS 构建失败链接"]
    end

    subgraph 搜索阶段
        D["文本 ushers"] --> E["ccunicode.h<br/>逐码点解码"]
        E --> F["AC 状态转移<br/>goto + fail 链"]
        F --> G["输出链收集<br/>沿 fail 上溯"]
    end

    subgraph 结果
        H["she  [1,4)<br/>he   [2,4)<br/>hers [2,6)"]
    end

    C --> F
    G --> H

    style C fill:#a94,stroke:#333,color:#fff
    style F fill:#49a,stroke:#333,color:#fff
    style H fill:#4a4,stroke:#333,color:#fff
```

[▶ 在线编辑](https://mermaid.live/edit#pako:eNp1Uk1rg0AQ_SuyJw8EYtREKXpoCyn5CJRQiMc6alTd4O4ou2Gt_707-hHrowf9mDdv3mNmdpgnihQIfPFHCqWMG7WlM14KEbn1rY3gW6JuAvuh7K-lExYKI8RkIRXTQSqhPGiGUO3tJmSgCT4jRxrE0IAxJcI4EuSx-EGhMQgOIgLHZ0cMQCQ0IIMm9oEDg5FnJRBoUiQDyLHm55Rh2Ak2OjwT8wmlOxFTIAYjMZjW2Mf3bJUfFEKVF5ucEPPKWf-98P08fY6uE7T5fzbm8vHqPHnBqR1uyVuXbjrv9OebDzdA9v1k0beNr3OF0_qXlVp_o_3Idw_Lp8XbaQ7ZeB8aKcyn7fu5VXlrLf95Xr2MDoNvK16mYmXpvJz2iFbMdfaoL-bfIRr44sGy5b09j_4FPXlyXJ-R3X5QqXWQ9md06DM76Wn1u62pugr5IMoGJ2YjWl7jQD-C8XZ22RmBt3Gxdm3HFyDZ1y3Yr-xNkm3fSNPNHnWTaYMylV0fQ-2PdBdo02KSvYjXTR4k3V_YgLCz)

---

在 [mermaid.live](https://mermaid.live) 粘贴代码即可在线渲染。或点击每个图下方的 **▶ 在线编辑** 链接。
