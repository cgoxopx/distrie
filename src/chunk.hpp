#ifndef distrie_chunk
#define distrie_chunk
#include <map>
#include <list>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include "utils.hpp"
namespace distrie{
    class chunk{
        public:
            int serverId;
            int nextServerId;
            int nextServerFd;
            std::atomic<int> workIdBuf;
            
            struct backupWork{
                char c;
                position p;
                int backupnum;
                void setVal(char c,const position & p,int backupnum){
                    this->c=c;
                    this->p=p;
                    this->backupnum=backupnum;
                }
            };
            std::mutex                  backupWorks_locker,backupWorks_wait;
            std::map<int,backupWork>    backupWorks;
            std::condition_variable     backupWorks_cv;
            
            std::atomic<bool>running;
            
            void waitBackup(){
                std::unique_lock<std::mutex> lck(backupWorks_wait);
                backupWorks_cv.wait(lck);
            }
            
            void addCharToNext(char c,const position & p,int backupnum){
                if(backupnum<=0)return;
                auto id=++workIdBuf;
                backupWorks_locker.lock();
                backupWorks[id].setVal(c,p,backupnum);
                backupWorks_locker.unlock();
                backupWorks_cv.notify_all();
            }
            
            void backup_all(){
                begin:
                if(!running)return;
                backupWorks_locker.lock();
                if(backupWorks.empty()){
                    backupWorks_locker.unlock();
                    waitBackup();
                    goto begin;
                }
                for(auto it:backupWorks){
                    addCharToNext_work(it.second.c,it.second.p,it.second.backupnum,it.first);
                }
                backupWorks_locker.unlock();
            }
            
            void backupFinish(int wid){
                backupWorks_locker.lock();
                backupWorks.erase(wid);
                backupWorks_locker.unlock();
            }
            
            void onInsertChar(int64_t dataId,char c,position & s,int backup){
                bool existed;
                if(c=='\0'){
                    existed=getNextNode(dataId,c,s);
                    s.first=-1;
                    s.second=-1;
                }else{
                    existed=getNextNode(dataId,c,s);
                }
                if(!existed)
                    if(backup>0)
                        addCharToNext(c,s,backup-1);
            }
            bool addCharToNext_work(char c,const position & p,int backupnum,int workId){
                if(nextServerFd==-1)return true;
                if(backupnum<=0)return true;
                package buf;
                
                bzero(&buf,sizeof(buf));
                buf.data.c=c;
                buf.method=package::INSERT;
                buf.workId=workId;
                buf.setServerId(p.first);
                buf.setDataId(p.second);
                buf.setBackupNum(backupnum-1);
                
                return (::send(nextServerFd,&buf,sizeof(buf),0)>0);
            }
            inline void onMsg_find(package * pk,int fd){
                position p;
                package buf;
                
                if(pk->getServerId()!=this->serverId)
                    return;
                
                bzero(&buf,sizeof(buf));
                
                findNextNode(pk->getDataId(),buf.data.arr);
                
                buf.method=package::FIND_OK;
                buf.workId=pk->workId;
                buf.setServerId(p.first);
                buf.setDataId(p.second);
                
                ::send(fd,&buf,sizeof(buf),0);
            }
            inline void onMsg_insert(package * pk,int fd){
                position p;
                package buf;
                
                if(pk->getServerId()!=this->serverId)
                    return;
                
                onInsertChar(pk->getDataId(),pk->data.c,p,pk->getBackupNum());
                
                bzero(&buf,sizeof(buf));
                buf.method=package::INSERT_OK;
                buf.workId=pk->workId;
                buf.setServerId(p.first);
                buf.setDataId(p.second);
                
                ::send(fd,&buf,sizeof(buf),0);
            }
            
            void onMsg(package * pk,int fd){
                if(pk->method==package::INSERT)
                    onMsg_insert(pk,fd);
                else
                if(pk->method==package::INSERT_OK)
                    backupFinish(pk->workId);
                else
                if(pk->method==package::FIND)
                    onMsg_find(pk,fd);
            }
            
            bool getNextNode(int64_t dataId,char c,position & s,bool getfirst=true){
                
            }
            
            
            void findNextNode(int64_t dataId,bool * c){
                
            }
            
    };
}
#endif