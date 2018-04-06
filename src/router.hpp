#ifndef distrie_router
#define distrie_router
#include <set>
#include <list>
#include <atomic>
#include <mutex>
#include "root.hpp"
namespace distrie{
    class router:public rootNode{
        //write
        public:
            struct addSession{
                std::string data;
                int ptr;
                void init(const char * s,int p){
                    data=s;
                    ptr=p;
                }
            };
            
            std::mutex addSessions_locker;
            std::map<int,addSession> addSessions;
            std::atomic<int> sessid;
            
            int getFdByServerId(int id){
                
            }
            void writeChar(
                char c  ,
                const position & p,
                int backup,int wid
            ){//write data into a server chain
                package buf;
                int fd=getFdByServerId(p.first);
                
                bzero(&buf,sizeof(buf));
                
                buf.method=package::INSERT;
                buf.workId=wid;
                buf.data.c=c;
                buf.setServerId(p.first);
                buf.setDataId(p.second);
                buf.setBackupNum(backup);
                
                ::send(fd,&buf,sizeof(buf),0);
            }
            void addData(const char *str){
                const char * ptr=str;
                if(ptr==NULL)return;
                if(*ptr=='\0')return;
                
                position buf;
                this->getFirstNodeOfElm(*ptr,buf); //root node is in router
                ++ptr;
                
                int sid=++sessid;
                
                if(*ptr!='\0'){ //if str[1]!='\0' ,add the string into sessions
                    addSessions_locker.lock();
                    addSessions[sid].init(str,2);
                    addSessions_locker.unlock();
                }
                
                writeChar(*ptr,buf,config.backupNum,sid);
            }
            void addDataNextStep(int id,const position & next){
                if(next.first==-1){
                    addSessions.erase(id);
                    return;
                }
                auto it=addSessions.find(id);
                if(it==addSessions.end())
                    return;
                const char * str=it->second.data.c_str();
                int ptr=it->second.ptr;
                
                writeChar(str[ptr],next,config.backupNum,id);
                
                if(str[ptr]!='\0')
                    ++(it->second.ptr);
                else
                    addSessions.erase(it);
            }
            
            inline void onMsg_addFinish(package * pk,int fd){
                position p;
                p.first=pk->getServerId();
                p.second=pk->getDataId();
                
                addSessions_locker.lock();
                addDataNextStep(pk->workId,p);
                addSessions_locker.unlock();
            }
        public:
            struct iterator{
                struct node{
                    bool child[256];
                    unsigned int offset;
                    position p;
                    char thisChar;
                    void init(char c,const bool ch[],const position & pp,unsigned int ofs){
                        thisChar=c;
                        for(register int i=0;i<256;i++){
                            child[i]=ch[i];
                        }
                        p=pp;
                        ofs=offset;
                    }
                };
                std::list<node>         nodes;
                std::list<char>         str;
                std::atomic<bool>       inuse;
                std::string             seeking;
                position                thisposition,
                                        lastposition;
                unsigned int            offset;
                bool                    eof;
                bool                    error;
                bool haveVal(){
                    if(nodes.empty())
                        return false;
                    auto it=nodes.rbegin();
                    if(it==nodes.rend())
                        return false;
                    else
                        return (it->child[0]);
                }
            };
            
            /*
            bool seek(iterator & it,char c){
                position p;
                bool buf[256];
                if(it.nodes.empty()){
                    if(findFirstNode(c,p)){
                        it.nodes.push_back(iterator::node());
                        auto it2=it.nodes.rend();
                        memcpy(it2->child,buf,256);
                        it.str.push_back(c);
                    }else
                        return false;
                }else{
                    //send request...
                }
            }
            */
            //but the function is too slow.
            //let me show the really function
            
            std::atomic<int>    iterator_id;
            std::mutex          iterator_locker;
            std::map<int,iterator>  iterators;
            int create(){
                auto id=++iterator_id;
                iterator_locker.lock();
                
                auto it2=iterators.find(id);
                if(it2==iterators.end()){
                    iterator_locker.unlock();
                    return it2->first;
                }
                
                iterator & it=iterators[id];
                it.inuse=false;
                
                iterator_locker.unlock();
                return id;
            }
            void destroy(int id){
                iterator_locker.lock();
                iterators.erase(id);
                iterator_locker.unlock();
            }
            void seek_onfail(int id){
                iterator_locker.lock();
                auto it=iterators.find(id);
                if(it==iterators.end()){
                    iterator_locker.unlock();
                    return;
                }else{
                    iterator_locker.unlock();
                }
                it->second.str.pop_back();
                getAllChild(id);
            }
            void seek_onsuccess(int id,const position & p){
                iterator_locker.lock();
                auto it=iterators.find(id);
                if(it==iterators.end()){
                    iterator_locker.unlock();
                    return;
                }else{
                    iterator_locker.unlock();
                }
                auto c=it->second.seeking[(it->second.offset)++];
                it->second.lastposition=it->second.thisposition;
                it->second.thisposition=p;
                if(c=='\0'){
                    getAllChild(id);
                    return;
                }else{
                    seek(id,c);
                }
                
            }
            void getAllChild(int id){
                iterator_locker.lock();
                auto it=iterators.find(id);
                if(it==iterators.end()){
                    iterator_locker.unlock();
                    return;
                }else{
                    iterator_locker.unlock();
                }
                
                position & p=it->second.thisposition;
                
                package buf;
                bzero(&buf,sizeof(buf));
                buf.method=package::FIND;
                buf.workId=id;
                buf.setServerId(p.first);
                buf.setDataId(p.second);
                
                ::send(getFdByServerId(p.first),&buf,sizeof(buf),0);
            }
            void getAllChild_onfail(int id){
                bool bbuf[256];
                iterator_locker.lock();
                auto it=iterators.find(id);
                if(it==iterators.end()){
                    iterator_locker.unlock();
                    return;
                }else{
                    iterator_locker.unlock();
                }
                it->second.error=true;
                it->second.inuse=false;
                if(!it->second.str.empty()){
                    for(register int i=0;i<256;i++)
                        bbuf[i]=false;
                    it->second.nodes.push_back(iterator::node());
                    auto it2=it->second.nodes.begin();
                    it2->init(*(it->second.str.rend()),bbuf,it->second.thisposition,0);
                }
            }
            void getAllChild_onsuccess(int id,bool bbuf[]){
                iterator_locker.lock();
                auto it=iterators.find(id);
                if(it==iterators.end()){
                    iterator_locker.unlock();
                    return;
                }else{
                    iterator_locker.unlock();
                }
                it->second.eof=true;
                it->second.inuse=false;
                if(!it->second.str.empty()){
                    it->second.nodes.push_back(iterator::node());
                    auto it2=it->second.nodes.begin();
                    it2->init(*(it->second.str.rend()),bbuf,it->second.thisposition,0);
                }
            }
            bool seek(int id,const char * str){
                if(str==NULL)return false;
                if(str[0]=='\0')return false;
                
                iterator_locker.lock();
                auto it=iterators.find(id);
                if(it==iterators.end()){
                    iterator_locker.unlock();
                    return false;
                }else{
                    iterator_locker.unlock();
                }
                if(it->second.inuse)
                    return false;
                it->second.eof=false;
                it->second.error=false;
                it->second.inuse=true;
                it->second.seeking=str;
                it->second.offset=1;
                it->second.thisposition.first=-1;
                it->second.lastposition.first=-1;
                return seek(id,str[0]);
            }
            bool seek(int id,char tc){
                bool bbuf[256];
                position p;
                if(tc=='\0')return false;
                iterator_locker.lock();
                auto it=iterators.find(id);
                if(it==iterators.end()){
                    iterator_locker.unlock();
                    return false;
                }else{
                    iterator_locker.unlock();
                }
                //if(it->second.inuse)
                //    return false;
                it->second.inuse=true;
                
                if(it->second.str.empty()){     //is the first char
                    it->second.nodes.clear();
                    if(findFirstNode(tc,p,bbuf)){
                        it->second.thisposition=p;
                        it->second.lastposition.first=-1;
                        it->second.str.push_back(tc);
                        it->second.nodes.push_back(iterator::node());
                        auto it2=it->second.nodes.begin();
                        it2->init(tc,bbuf,p,0);
                        
                        package buf;
                        bzero(&buf,sizeof(buf));
                        buf.method=package::SEEK;
                        buf.workId=id;
                        buf.setServerId(p.first);
                        buf.setDataId(p.second);
                        buf.data.c=tc;
                        
                        ::send(getFdByServerId(p.first),&buf,sizeof(buf),0);
                    }else{
                        it->second.eof=true;
                        it->second.inuse=false;
                        return true;
                    }
                }else{
                    p=it->second.thisposition;
                    it->second.str.push_back(tc);
                    
                    package buf;
                    bzero(&buf,sizeof(buf));
                    buf.method=package::SEEK;
                    buf.workId=id;
                    buf.setServerId(p.first);
                    buf.setDataId(p.second);
                    buf.data.c=tc;
                    
                    ::send(getFdByServerId(p.first),&buf,sizeof(buf),0);
                }
                
            }
            void onMsg(package * pk,int fd){
                
                
            }
    };
}
#endif
