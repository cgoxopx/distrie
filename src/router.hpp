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
                int                     fd;
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
                        auto it2=it.nodes.rbegin();
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
            int iterators_create(int fd){
                auto id=++iterator_id;
                iterator_locker.lock();
                
                auto it2=iterators.find(id);
                if(it2==iterators.end()){
                    iterator_locker.unlock();
                    return it2->first;
                }
                
                iterator & it=iterators[id];
                it.inuse=false;
                it.fd=fd;
                
                iterator_locker.unlock();
                return id;
            }
            void iterators_destroy(int id){
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
                    it2->init(*(it->second.str.rbegin()),bbuf,it->second.thisposition,0);
                }
                
                iterator_end(it->second.fd,id,it->second.eof,it->second.error);
            }
            inline void getAllChild_onsuccess(int id,const char * buf){
                bool bbuf[256];
                for(register int i=0;i<256;i++){
                    if(buf[i]=='1'){
                        bbuf[i]=true;
                    }else{
                        bbuf[i]=false;
                    }
                }
                getAllChild_onsuccess(id,bbuf);
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
                    it2->init(*(it->second.str.rbegin()),bbuf,it->second.thisposition,0);
                }
                
                iterator_end(it->second.fd,id,it->second.eof,it->second.error);
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
                        iterator_end(it->second.fd,id,it->second.eof,it->second.error);
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
            void iterator_goback(int id){
                iterator_locker.lock();
                auto it=iterators.find(id);
                if(it==iterators.end()){
                    iterator_locker.unlock();
                    return;
                }else{
                    iterator_locker.unlock();
                }
                it->second.nodes.pop_back();
                it->second.str.pop_back();
            }
            void iterator_end(int fd,int id,bool eof,bool err){
                package buf;
                bzero(&buf,sizeof(buf));
                
                buf.setWorkId(id);
                buf.method=package::ITERATOR_OK;
                
                if(eof)
                    buf.data.status.eof='1';
                else
                    buf.data.status.eof='0';
                
                if(err)
                    buf.data.status.error='1';
                else
                    buf.data.status.error='0';
                
                ::send(fd,&buf,sizeof(buf),0);
            }
            void iterator_fail(int fd,int id){
                package buf;
                bzero(&buf,sizeof(buf));
                
                buf.setWorkId(id);
                buf.method=package::ITERATOR_FAIL;
                
                ::send(fd,&buf,sizeof(buf),0);
            }
            void onMsg(package * pk,int fd){
                package buf;
                bzero(&buf,sizeof(buf));
                
                buf.workId=pk->workId;
                
                if(pk->method==package::ITERATOR_C){
                    buf.data.i=htonl(iterators_create(fd));
                    buf.method=package::ITERATOR_OK;
                    ::send(fd,&buf,sizeof(buf),0);
                }else
                if(pk->method==package::ITERATOR_D){
                    iterators_destroy(pk->getWorkId());
                    buf.method=package::ITERATOR_OK;
                    ::send(fd,&buf,sizeof(buf),0);
                }else
                if(pk->method==package::SEEK){
                    buf.data.arr[255]='\0';
                    seek(pk->getWorkId(),buf.data.arr);
                }else
                if(pk->method==package::FIND){
                    iterator_locker.lock();
                    auto it=iterators.find(pk->getWorkId());
                    if(it==iterators.end()){
                        iterator_locker.unlock();
                        iterator_fail(fd,pk->getWorkId());
                        return;
                    }else{
                        iterator_locker.unlock();
                    }
                    auto it2=it->second.nodes.rbegin();
                    if(it2==it->second.nodes.rend())
                        iterator_fail(fd,pk->getWorkId());
                    else{
                        buf.method=package::FIND_OK;
                        for(register int i=0;i<256;i++)
                            if(it2->child[i])
                                buf.data.arr[i]='1';
                            else
                                buf.data.arr[i]='0';
                        ::send(fd,&buf,sizeof(buf),0);
                    }
                }else
                if(pk->method==package::ITERATOR_GOBACK){
                    iterator_goback(pk->getWorkId());
                    buf.method=package::ITERATOR_OK;
                    ::send(fd,&buf,sizeof(buf),0);
                }else
                if(pk->method==package::SEEK_OK){
                    if(pk->error==0xffffffff)
                        seek_onfail(pk->getWorkId());
                    else{
                        position p;
                        p.first=ntohl(pk->data.seek.serverId);
                        p.second=pk->data.seek.guid.toh();
                        seek_onsuccess(pk->getWorkId(),p);
                    }
                }else
                if(pk->method==package::FIND_OK){
                    if(pk->error==0xffffffff)
                        getAllChild_onfail(pk->getWorkId());
                    else
                        getAllChild_onsuccess(pk->getWorkId(),pk->data.arr);
                }
            }
    };
}
#endif
