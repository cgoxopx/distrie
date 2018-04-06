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
                    position p;
                    std::list<char> wds;
                    std::list<char>::iterator it;
                };
                std::list<node> str;
                bool haveVal(){
                    auto it=str.rend();
                    if(*(it->it)=='\0')
                        return true;
                    else
                        return false;
                }
            };
    };
}
#endif
