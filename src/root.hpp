#ifndef distrie_root
#define distrie_root
#include <map>
#include <list>
#include "utils.hpp"
namespace distrie{
    class rootNode{
        public:
            struct element{
                std::list<position> next;
            };
            std::map<char,element> nodes;
            void getFirstNodeOfElm(char c,position & ret){
                auto it=nodes.find(c);
                if(it==nodes.end()){
                    auto q=nodes[c];
                    getNewPositions(q.next);
                    ret=*(q.next.begin());
                }else{
                    ret=*(it->second.next.begin());
                }
            }
            const element & getElement(char c){
                auto it=nodes.find(c);
                if(it==nodes.end()){
                    auto q=nodes[c];
                    getNewPositions(q.next);
                    return nodes[c];
                }else{
                    return it->second;
                }
            }
    };
}
#endif