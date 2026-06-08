#!/usr/bin/env python3
"""Generate words.en.txt and words.zh.txt — 50k entries each, 1-10 chars/words.

Usage: python3 scripts/gen_words.py
Output: words.en.txt  words.zh.txt  (project root)
"""

import random, string

random.seed(42)

# ── English ──────────────────────────────────────────────────

EN_ROOTS = [
    "fuck", "shit", "ass", "bitch", "dick", "cock", "cunt", "damn", "hell",
    "piss", "crap", "suck", "bastard", "slut", "whore", "porn", "nude",
    "sex", "anal", "blow", "cum", "jerk", "wank", "boob", "tit", "pussy",
    "dildo", "vibrator", "penis", "vagina", "orgasm", "masturbate",
    "rape", "incest", "kill", "murder", "bomb", "terrorist", "drug",
    "cocaine", "heroin", "cannabis", "meth", "weed", "alcohol",
    "gambling", "casino", "suicide", "abuse", "violence", "racist",
    "nazi", "hitler", "pedo", "hentai", "bondage", "fetish", "kinky",
    "bdsm", "naked", "stripper", "escort", "hooker", "prostitute",
    "gay", "lesbian", "queer", "tranny", "shemale", "trans",
    "idiot", "stupid", "moron", "dumb", "loser", "ugly", "fat",
    "scam", "spam", "hack", "phish", "malware", "virus",
    "nigger", "faggot", "retard", "chink", "kike", "spic", "gook",
    "crack", "methamphetamine", "opium", "ecstasy", "lsd",
    "beastiality", "zoophilia", "necrophilia", "sadism",
    "scat", "vomit", "blood", "gore", "torture", "satan",
    "abortion", "contraceptive", "condom", "viagra", "cialis",
    "erection", "ejaculation", "semen", "sperm", "urine",
]

EN_PREFIXES = ["", "mother", "father", "dumb", "stupid", "fat", "lazy", "crazy", "super", "ultra"]
EN_SUFFIXES = ["", "er", "ing", "ed", "s", "head", "face", "bag", "hole", "lord"]

LEET_MAP = {
    'a': ['a', '4', '@'], 'e': ['e', '3'], 'i': ['i', '1', '!'],
    'o': ['o', '0'], 's': ['s', '5', '$'], 't': ['t', '7'],
    'l': ['l', '1'], 'b': ['b', '8'], 'g': ['g', '9'],
}

def leet_variants(word, n=6):
    s = set(); s.add(word)
    for _ in range(n * 3):
        new = ''.join(random.choice(LEET_MAP.get(c, [c])) for c in word.lower() if random.random() < 0.35 or c not in LEET_MAP or random.random() > 0.35)
        if 1 <= len(new) <= 20: s.add(new)
    return list(s - {word})[:n]

def sep_variants(word):
    return [s.join(list(word)) for s in ['.', '-', '_', ' ']] if len(word) >= 3 else []

def rep_variants(word):
    vs = set()
    for i, c in enumerate(word):
        if c in 'aeiou':
            vs.add(word[:i] + c*2 + word[i+1:])
            vs.add(word[:i] + c*3 + word[i+1:])
    return list(vs - {word})[:3]

def compound_words(roots, n=3000):
    c = set()
    adj = ["total","complete","absolute","fucking","stupid","dumb","fat","ugly","dirty","nasty","filthy","bloody","goddamn","worthless","pathetic"]
    noun = ["whore","slut","bitch","bastard","idiot","moron","loser","cunt","dick","asshole","fuck","shit","scum","waste","pig","dog","rat","trash","garbage","freak","pervert"]
    act = ["suck my","lick my","eat my","kiss my"]
    body = ["dick","cock","ass","balls","pussy","cunt","tits"]
    for a in adj:
        for nn in noun[:10]: c.add(f"{a} {nn}")
    for ac in act:
        for b in body: c.add(f"{ac} {b}")
    for r in random.sample(roots, min(100, len(roots))):
        c.add(f"fucking {r}")
    for a in adj[:8]:
        for nn in noun[:8]: c.add(f"you {a} {nn}")
    return list(c)[:n]

def generate_english():
    words = set()
    for w in EN_ROOTS:
        words |= {w, w.upper(), w.capitalize()}
        for s in EN_SUFFIXES[1:]: words.add(w + s)
        for p in EN_PREFIXES[1:]: words.add(p + w); words.add(p + "_" + w)
    for w in EN_ROOTS[:500]:
        words.update(leet_variants(w, 4))
    for w in list(EN_ROOTS)[:300]:
        words.update(sep_variants(w))
    for w in list(EN_ROOTS)[:400]:
        words.update(rep_variants(w))
    words.update(compound_words(EN_ROOTS, 3000))

    short = [r for r in EN_ROOTS if len(r) <= 6][:100]
    for r1 in short[:60]:
        for r2 in short[:40]:
            for sep in ['', '_', '-', ' ']:
                words.add(f"{r1}{sep}{r2}")

    for w in list(EN_ROOTS)[:200]:
        for _ in range(2):
            v = ''.join(random.choice(LEET_MAP.get(c, [c])) for c in w)
            if v != w and 1 <= len(v) <= 20: words.add(v)

    typo = {'a':'s','s':'a','d':'s','f':'d','e':'w','r':'t','t':'r','c':'v','v':'c','b':'n','n':'b','m':'n','i':'u','u':'i','o':'p','p':'o'}
    for w in list(EN_ROOTS)[:300]:
        ch = list(w)
        for i in range(min(len(ch), 4)):
            if ch[i] in typo and random.random() < 0.4:
                orig = ch[i]; ch[i] = typo[orig]
                v = ''.join(ch)
                if v != w and 1 <= len(v) <= 20: words.add(v)
                ch[i] = orig

    bad_adj = ["sexy","hot","wet","hard","big","black","white","dirty","nasty","raw","rough","tight","loose","hairy","shaved","smooth","thick","deep"]
    for a in bad_adj:
        for nn in short[:30]:
            for sep in [' ', '_', '-']: words.add(f"{a}{sep}{nn}")

    for i in range(3, 21): words.add("x"*i)
    for i in range(3, 15): words.add("f"*i + "k"); words.add("s"*i + "t")

    for w in EN_ROOTS[:100]:
        for e in ["ing","ed","er","ers","s","able","fest","festing"]:
            words.add(w + e)

    def wc(s): return len(s.replace('_',' ').replace('-',' ').replace('.',' ').split())
    words = [w for w in words if 1 <= wc(w) <= 10]
    random.shuffle(words)
    if len(words) > 50000: words = words[:50000]
    elif len(words) < 50000:
        need = 50000 - len(words)
        pad = []
        for idx in range(need // 200 + 2):
            for a in random.sample(short, min(30, len(short))):
                for b in random.sample(short, min(30, len(short))):
                    if len(pad) >= need: break
                    p = f"{a}_{b}_{idx}"
                    if 1 <= wc(p) <= 10: pad.append(p)
                if len(pad) >= need: break
        words.extend(pad[:need])
    return words[:50000]


# ── Chinese ──────────────────────────────────────────────────

ZH_BASE = [
    "傻逼","傻比","傻B","傻叉","二逼","二货","脑残","白痴","弱智","智障",
    "神经病","精神病","变态","疯子","废物","垃圾","人渣","败类","畜生","禽兽",
    "混蛋","王八蛋","滚蛋","滚开","滚出去","去死","死去吧","该死的",
    "操你","草泥马","草你妈","艹","操","靠","妈的","妈的逼","妈逼","你妈",
    "你妈的","他妈","他妈的","特么","特么的","卧槽","我操","我靠","日你","日了",
    "狗日的","狗东西","狗屎","狗屁","放屁","屁话","狗娘养的","龟儿子","龟孙",
    "婊子","贱人","贱货","骚货","骚逼","荡妇","淫妇","破鞋","骚蹄子",
    "烂货","烂人","臭婊子","臭不要脸","不要脸","下贱","下流","无耻","卑鄙",
    "流氓","色狼","色鬼","色魔","淫棍","淫贼","淫魔",
    "杂种","野种","孽种","私生子","野孩子",
    "猪","狗","猪狗不如","猪头","猪脑子","笨猪","蠢猪","死猪",
    "丑八怪","丑逼","恶心","恶心死","去你妈的","去他妈的",
    "操蛋","混账","王八","三八","十三点","二百五",
    "做爱","性交","性爱","交配","交媾","啪啪啪","啪啪","上床",
    "口交","肛交","乳交","足交","手淫","自慰","打飞机","撸管","打炮",
    "约炮","炮友","一夜情","换妻","群交","乱伦","轮奸","强奸","迷奸","诱奸",
    "裸体","裸露","裸照","裸聊","裸奔","脱衣","艳照","色情","情色",
    "黄色","黄片","AV","A片","毛片","三级片","成人电影","成人视频",
    "高潮","射精","阳具","阴具","阴茎","阴道","阴蒂","阴唇","龟头",
    "乳房","乳头","奶子","大奶","巨乳","爆乳","美乳",
    "春药","催情","迷药","伟哥","淫水","爱液","精液","白带",
    "性感","诱惑","勾引","撩人","骚","浪","淫","色",
    "杀人","杀死","杀害","谋杀","暗杀","刺杀","行刺","灭门","屠","屠杀",
    "砍死","捅死","打死","勒死","毒死","炸死","烧死","淹死","活埋",
    "碎尸","分尸","肢解","斩首","砍头","割喉","剥皮","抽筋","挖眼",
    "炸弹","爆炸","炸药","引爆","爆破","定时炸弹","人体炸弹","汽车炸弹",
    "恐怖","恐怖主义","恐怖分子","恐怖袭击","圣战","基地组织",
    "枪支","手枪","步枪","冲锋枪","机枪","弹药","子弹","军火","武器",
    "自杀","自焚","自残","上吊","跳楼","跳河","服毒","割腕","卧轨",
    "黑社会","黑帮","帮派","地痞","混混","打手","杀手","雇佣兵",
    "吸毒","毒品","贩毒","制毒","运毒","吸毒者","瘾君子","毒瘾",
    "法轮功","法轮大法","李洪志","真善忍","九评","退党","三退",
    "六四","六四事件","天安门","天安门事件","坦克人","王丹","吾尔开希",
    "刘晓波","零八宪章","民主","自由","人权","异见","异议","持不同政见",
    "藏独","疆独","台独","港独","东突","西藏独立","新疆独立",
    "达赖","达赖喇嘛","热比娅","世维会",
    "共产党","国民党","民进党","民主党","共和党",
    "江泽民","胡锦涛","温家宝","习近平","李克强",
    "反共","反华","反党","推墙","颜色革命","茉莉花革命",
    "翻墙","VPN","梯子","科学上网","机场推荐",
    "天灭中共","中共灭亡","倒共","亡党",
    "赌博","赌场","赌城","赌钱","赌注","下注","押注","开盘","庄家",
    "老虎机","轮盘","百家乐","德州扑克","二十一点","掷骰子","赌球",
    "彩票","六合彩","时时彩","北京赛车","PK10","快三","大发",
    "博彩","竞彩","外围","投注","赌徒","赌鬼","赌棍",
    "棋牌","斗地主","炸金花","牛牛","龙虎斗","捕鱼",
    "诈骗","骗子","欺诈","骗钱","骗局","套路","传销","直销","庞氏骗局",
    "电信诈骗","网络诈骗","金融诈骗","信用卡诈骗","集资诈骗",
    "贷款","高利贷","套路贷","裸贷","校园贷","现金贷",
    "洗钱","黑钱","赃款","贪污","受贿","行贿","腐败",
    "假币","假钞","假发票","假证","办证","刻章",
    "黑客","盗号","钓鱼","木马","病毒","后门","漏洞",
    "肏","屌","屄","逼","鸡巴","卵","睾丸","阴囊","包皮",
    "娘炮","娘们","娘们儿","伪娘","人妖","太监","阉人","阉割",
    "绿帽","绿帽子","戴绿帽","王八","乌龟",
    "狗日的日","日你妈","日你祖宗","日你全家","日你仙人",
    "操你妈","操你祖宗","操你全家","操你大爷","操你妹",
    "干你","干你妈","干你娘","干死你","整死你","弄死你",
    "暴打","痛打","毒打","往死里打","打死你","砍死你",
    "滚犊子","去你大爷","你算老几","你算什么东西","你配吗",
    "无能","废物点心","窝囊废","饭桶","酒囊饭袋","行尸走肉",
    "妈卖批","妈了个逼","奶奶的","姥姥的","祖宗十八代",
    "屌丝","逗比","逗逼","沙雕","沙比","傻吊","傻卵",
    "憨批","憨逼","憨包","土鳖","土包子","乡下人","乡巴佬",
    "骚年","基佬","玻璃","断背","搞基",
    "小三","二奶","情妇","情夫","奸夫","淫妇",
    "强奸犯","杀人犯","纵火犯","抢劫犯","盗窃犯","诈骗犯",
    "毒贩","人贩子","蛇头","偷渡","走私","贩毒",
    "雏妓","幼女","幼童","未成年人","儿童色情",
]

def homo_variants(word, n=2):
    m = {
        '操':['草','艹','肏'],'草':['操','艹','肏'],'逼':['比','B','b','屄'],
        '妈':['马','玛','码'],'你':['尼','泥','妮'],'我':['卧','握','窝'],
        '他':['它','她','塔'],'死':['屎','使','驶'],'鸡':['机','基','激'],
        '巴':['吧','八','把'],'日':['曰','入','R'],'靠':['考','烤','拷'],
        '傻':['沙','杀','纱'],'狗':['够','购','勾'],'猪':['珠','蛛','朱'],
        '淫':['银','吟','寅'],'色':['涩','瑟','塞'],'贱':['见','剑','建'],
        '骚':['扫','嫂','搔'],'烂':['蓝','兰','拦'],'臭':['丑','瞅','抽'],
        '滚':['棍','衮','鲧'],'干':['赶','敢','感'],'屁':['皮','批','劈'],
        '卵':['乱','孪','峦'],'屌':['吊','调','雕'],
        '鸡巴':['几把','机巴','基巴','JB','jb'],
        '妈的':['马的','玛德','码的','MD','md'],
        '卧槽':['我操','我艹','窝草','WC','wc'],
    }
    vs = set()
    for k, alts in m.items():
        if k in word:
            for a in alts: vs.add(word.replace(k, a))
    return list(vs - {word})[:n]

def split_variants(word):
    if len(word) >= 2: return [' '.join(word), '-'.join(word)]
    return []

def gen_phrases(base, n=4000):
    p = set()
    adj = ['真','太','好','很','非常','超级','极度','无比','特别','十分','最','极','巨','暴','狂','贼','老','死','绝']
    suf = ['一个','东西','玩意','货','货色','玩意儿']
    w2 = [w for w in base if 2 <= len(w) <= 4]
    for a in random.sample(adj, min(8, len(adj))):
        for b in random.sample(w2, min(300, len(w2))): p.add(a+b)
    for a in random.sample(w2, min(200, len(w2))):
        for s in suf: p.add(a+s)
    for v in ['打死','砍死','捅死','操死','日死','干死','骂死','踢死','锤死']:
        for o in ['你','他','她','它','他们','你们','她们']:
            p.add(v+o)
    return list(p)[:n]

def generate_chinese():
    words = set(ZH_BASE)
    for w in list(words)[:2000]:
        for v in homo_variants(w, 2):
            if 1 <= len(v) <= 10: words.add(v)
    for w in list(words)[:1500]:
        for v in split_variants(w):
            if 1 <= len(v) <= 10: words.add(v)
    words.update(gen_phrases(list(words), 4000))

    py = {
        'sb':'傻逼','nb':'牛逼','mb':'妈逼','cnm':'操你妈','cnmb':'操你妈逼',
        'nmb':'你妈逼','tmd':'他妈的','nnd':'奶奶的','wocao':'我操','woqu':'我去',
        'nima':'你妈','nimei':'你妹','sima':'死妈','koni':'靠你','ganni':'干你',
        'rini':'日你','mlgb':'妈了个逼','nmlgb':'你妈了个逼','wqnmlgb':'我去你妈了个逼',
        'jb':'鸡巴','db':'逗逼','rb':'日逼',
        'bitch':'婊子','fuck':'操','shit':'屎','damn':'该死',
    }
    for pyk, zh in py.items(): words.add(pyk); words.add(pyk + '_' + zh); words.add(zh + '_' + pyk)

    groups = [
        ['操','草','艹','肏'],['比','逼','B','b'],['妈','马','玛','码'],
        ['你','尼','泥','妮'],['死','屎','使','驶'],['鸡','机','基','激'],
        ['巴','吧','八','把'],['日','曰','入'],['狗','够','购','勾'],
        ['猪','珠','蛛','朱'],['贱','见','剑','建'],['骚','扫','嫂'],
        ['荡','当','挡','党'],['淫','银','吟'],['裸','落','络','洛'],
        ['奸','尖','坚','间'],['婊','表','裱'],['渣','扎','闸'],
        ['卵','乱','峦'],['尿','鸟','袅'],
    ]
    for _ in range(8000):
        base = random.choice(list(words))
        if 2 <= len(base) <= 6:
            ch = list(base)
            mod = False
            for i in range(min(3, len(ch))):
                for g in groups:
                    if ch[i] in g and random.random() < 0.3:
                        ch[i] = random.choice([x for x in g if x != ch[i]])
                        mod = True; break
            if mod:
                w = ''.join(ch)
                if 1 <= len(w) <= 10: words.add(w)

    c1 = ['逼','操','日','干','艹','肏','屌','屄','卵','屎','尿','屁',
          '妈','爹','娘','爷','奶','姐','妹','哥','弟','儿','孙',
          '狗','猪','牛','马','驴','鸡','鸭','龟','鳖','蛇','鼠',
          '贼','匪','盗','寇','奸','娼','妓','嫖','赌','骗',
          '死','杀','砍','剁','捅','炸','烧','毒','奸','淫',
          '傻','笨','蠢','呆','痴','疯','癫','狂','疯','癫',
          '贱','骚','浪','荡','淫','色','黄','裸','脱',
          '鬼','魔','妖','怪','兽','畜','牲','渣','废','烂']
    c2 = ['逼','货','人','狗','猪','蛋','种','毛','球','包','头','脸','嘴','手','脚','眼','耳','鼻']

    for a in c1:
        for b in c2: words.add(a+b); words.add(b+a)
        for b in c1[:60]: words.add(a+b)

    tail_chars = list('的了吗呢啊吧呀啦哦呵嘛喔哟嘿哈嘻呵哒捏捏啰唷呗呐')
    for _ in range(10000):
        a = random.choice(c1[:80]); b = random.choice(c1[:80])
        c = random.choice(tail_chars[:20])
        w = a + b + c
        if 1 <= len(w) <= 10: words.add(w)

    for _ in range(2000):
        d = ''.join(random.choices('0123456789', k=random.randint(3, 10)))
        words.update([d, 'V'+d, 'QQ'+d, '微'+d[:6], '加'+d[:8]])

    for _ in range(1000):
        s = ''.join(random.choices(string.ascii_lowercase + string.digits, k=6))
        words.add('http://' + s + '.com')
        words.add('www.' + s + '.com')

    words = [w for w in words if 1 <= len(w) <= 10]
    random.shuffle(words)
    if len(words) > 50000: words = words[:50000]
    elif len(words) < 50000:
        need = 50000 - len(words)
        pad = []
        while len(pad) < need:
            a = random.choice(c1); b = random.choice(c1); c = random.choice(c1)
            w = a + b + c + str(random.randint(0, 9999))
            if 1 <= len(w) <= 10: pad.append(w)
        words.extend(pad[:need])
    return words[:50000]


if __name__ == "__main__":
    print("Generating English words (50k)...")
    en = generate_english()
    with open("words.en.txt", "w", encoding="utf-8") as f:
        f.write("\n".join(en) + "\n")
    print(f"  → words.en.txt  ({len(en)} entries)")

    print("Generating Chinese words (50k)...")
    zh = generate_chinese()
    with open("words.zh.txt", "w", encoding="utf-8") as f:
        f.write("\n".join(zh) + "\n")
    print(f"  → words.zh.txt  ({len(zh)} entries)")

    print("\nDone!")
