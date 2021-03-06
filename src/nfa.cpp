/*!
 * @file nfa.cpp
 * @brief NFA構造体のメソッドの実装
 */

#include "nfa.h"
#include "dfa.h"
#include <algorithm>
#include <stack>
#include <set>
#include <map>

/**
 * NFAからDFAへ変更する際に用いられる構造体。
 */
struct DFARefRecord {
    //! 遷移元のNFAの状態の集合のインデックス
    long from = NFA::REF_UNDEFINED;
    //! 遷移条件
    char rule = NFA::RULE_UNDEFINED;
    //! 遷移元のNFAの状態の集合のインデックス
    long to = NFA::REF_UNDEFINED;
};

/**
 * NFAの状態遷移の規則がまだ定義されていない状態を表す定数
 */
const char NFA::RULE_UNDEFINED = -1;

/**
 * ε遷移の規則を表す定数
 */
const char NFA::RULE_EPSILON = -2;

/**
 * 遷移先がまだ定義されていない状態を表す定数
 */
const long NFA::REF_UNDEFINED = -1;

NFASubsetRef::NFASubsetRef() : start(NFA::REF_UNDEFINED), end(NFA::REF_UNDEFINED) {};

NFAState::NFAState() : rule(NFA::RULE_UNDEFINED), refs(NFA::REF_UNDEFINED, NFA::REF_UNDEFINED) {}

NFA::NFA() : nfa(), vec(){}

NFASubsetRef NFA::ch(const char c) {
    nfa.start = vec.size();
    nfa.end = nfa.start + 1;

    vec.push_back(NFAState());
    vec.push_back(NFAState());

    vec[nfa.start].rule = c;
    vec[nfa.start].refs.first = nfa.end;

    return nfa;
}

NFASubsetRef NFA::link(const NFASubsetRef lv, const NFASubsetRef rv) {
    nfa.start = lv.start;
    nfa.end = rv.end;

    vec[lv.end].rule = NFA::RULE_EPSILON;
    vec[lv.end].refs.first = rv.start;

    return nfa;
}

NFASubsetRef NFA::select(const NFASubsetRef lv, const NFASubsetRef rv) {
    nfa.start = vec.size();
    nfa.end = nfa.start + 1;

    vec.push_back(NFAState());
    vec.push_back(NFAState());

    vec[nfa.start].rule = NFA::RULE_EPSILON;
    vec[nfa.start].refs.first = lv.start;
    vec[nfa.start].refs.second = rv.start;

    vec[lv.end].rule = NFA::RULE_EPSILON;
    vec[lv.end].refs.first = nfa.end;

    vec[rv.end].rule = NFA::RULE_EPSILON;
    vec[rv.end].refs.first = nfa.end;

    return nfa;
}

NFASubsetRef NFA::star(const NFASubsetRef v) {
    nfa.start = vec.size();
    nfa.end = nfa.start + 1;

    vec.push_back(NFAState());
    vec.push_back(NFAState());

    vec[nfa.start].rule = NFA::RULE_EPSILON;
    vec[nfa.start].refs.first = v.start;
    vec[nfa.start].refs.second = nfa.end;

    vec[v.end].rule = NFA::RULE_EPSILON;
    vec[v.end].refs.first = nfa.start;

    return nfa;
}

NFASubsetRef NFA::question(const NFASubsetRef v) {
    nfa.start = vec.size();
    nfa.end = v.end;

    vec.push_back(NFAState());

    vec[nfa.start].rule = NFA::RULE_EPSILON;
    vec[nfa.start].refs.first = v.start;
    vec[nfa.start].refs.second = v.end;

    return nfa;
}

NFASubsetRef NFA::range(const char s, const char e) {
    if(s > e) return NFASubsetRef();
    if(s == e) return ch(s);
    NFASubsetRef ns = ch(s);
    for(char c = s + 1; c <= e; c++) ns = select(ns, ch(c));
    return ns;
}

NFASubsetRef NFA::one_of(const char *list) {
    if(*list == '\0') return NFASubsetRef();
    NFASubsetRef ns = ch(*list);
    for(const char *p = list + 1; *p != '\0'; p++) ns = select(ns, ch(*p));
    return ns;
}

NFASubsetRef NFA::alnum() {
    return select(alpha(), digit());
}

NFASubsetRef NFA::alpha() {
    return select(lower(), upper());
}

NFASubsetRef NFA::blank() {
    return one_of(" \t");
}

NFASubsetRef NFA::digit() {
    return one_of("0123456789");
}

NFASubsetRef NFA::graph() {
    return select(alnum(), punct());
}

NFASubsetRef NFA::lower() {
    return one_of("abcdefghijklmnopqrstuvwxyz");
}

NFASubsetRef NFA::print() {
    return select(graph(), ch(' '));
}

NFASubsetRef NFA::punct() {
    return one_of("!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}");
}

NFASubsetRef NFA::upper() {
    return one_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
}

NFASubsetRef NFA::xdigit() {
    return select(digit(), one_of("ABCDEFabcdef"));
}

const NFAState& NFA::operator[](unsigned long i) const {
    return (const NFAState &)vec[i];
}

/**
 * ある状態のε閉包を求める関数。
 * 授業では、逐次ε閉包を求めているが、ε閉包は先に計算しておくことも可能なうえに、
 * そのようにしたほうが計算量を抑えることができる。
 * また、今回の実装では動的bitsetを自作するのが面倒で行っていないが、
 * 集合の管理をbitsetで行うと更に高速にDFAを求めることが可能だと考える。
 * @param nss NFAの状態一覧の参照
 * @param ecs NFAの状態に対応したε閉包一覧の参照
 */
void calculate_epsilon_closures(const std::vector<NFAState> &nss, std::vector<std::set<long>> &ecs) {
    std::vector<bool> flag(nss.size(), false);

    for(size_t i = 0; i < nss.size(); i++) {
        std::stack<long> stack;
        stack.push(i);

        while(!stack.empty()) {
            if(ecs.size() < stack.size()) throw "An epsilon infinite loop is detected.";
            long argi = stack.top();

            if(nss[argi].rule == NFA::RULE_EPSILON) {
                long s1 = nss[argi].refs.first, s2 = nss[argi].refs.second;

                if(s1 != NFA::REF_UNDEFINED) {
                    if(!flag[s1]) {
                        stack.push(s1);
                        continue;
                    }
                    else ecs[argi].insert(ecs[s1].begin(), ecs[s1].end());
                }
                if(s2 != NFA::REF_UNDEFINED) {
                    if(!flag[s2]) {
                        stack.push(s2);
                        continue;
                    }
                    else ecs[argi].insert(ecs[s2].begin(), ecs[s2].end());
                }
            }

            flag[argi] = true;
            ecs[argi].insert(argi);
            stack.pop();
        }
    }
}

DFA NFA::nfa2dfa() {
    std::vector<std::set<long>> epsilon_closures(vec.size()); // ε-closure Table
    std::vector<std::set<long>> dfa_states; // DFAState Table
    std::vector<DFARefRecord> dfa_ref_records; // DFAState Transition Table

    // at first, calculate dfa tree

    calculate_epsilon_closures(vec, epsilon_closures);

    // for all dfa_states...
    dfa_states.push_back(epsilon_closures[nfa.start]);
    for(size_t dsi = 0; dsi < dfa_states.size(); dsi++) {
        std::map<char, std::set<long>> ch_closures;

        // calculate the set of NFAState which it able to go from dsi with an character.
        std::for_each(dfa_states[dsi].begin(), dfa_states[dsi].end(), [&](long si) {
            if(vec[si].rule == NFA::RULE_EPSILON || vec[si].rule == NFA::RULE_UNDEFINED) return;

            ch_closures[vec[si].rule].insert(epsilon_closures[vec[si].refs.first].begin(), epsilon_closures[vec[si].refs.first].end());
        });

        // indexes the set of NFAState, add the record to dfa_ref_records.
        std::for_each(ch_closures.begin(), ch_closures.end(), [&](std::pair<char, std::set<long>> cs) {
            long from = dsi, to;
            char rule = cs.first;

            std::vector<std::set<long>>::iterator it = std::find(dfa_states.begin(), dfa_states.end(), cs.second);

            if(it != dfa_states.end()) { to = std::distance(dfa_states.begin(), it); }
            else { to = dfa_states.size(); dfa_states.push_back(cs.second); }

            dfa_ref_records.push_back(DFARefRecord({from, rule, to}));
        });
    }

    // second, construct the structure meaning dfa tree

    DFA dfa(dfa_states.size());
    for(size_t i = 0; i < dfa.size(); i++) dfa[i].is_end = dfa_states[i].find(nfa.end) != dfa_states[i].end();
    std::for_each(dfa_ref_records.begin(), dfa_ref_records.end(), [&](const DFARefRecord &rec) {
       dfa[rec.from].refs.emplace(rec.rule, rec.to);
    });

    return dfa;
}