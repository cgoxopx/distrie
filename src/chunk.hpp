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
            chunk(){
                nextServerFd=-1;
                nextServerId=-1;
                serverId=-1;
            }
            ~chunk(){
                if(nextServerFd!=-1)
                    close(nextServerFd);
            }
            void reconnect(){
                if(nextServerId==-1)return;
            }
            
            struct backupWork{
                char c;
                GUID lastGUID;
                position newGUID;
                int backupnum;
                void setVal(char c,const GUID & l,const position & n,int backupnum){
                    this->c=c;
                    this->lastGUID=l;
                    this->newGUID=n;
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
            
            void addCharToNext(char c,const GUID & l,const position & n,int backupnum){
                if(backupnum<=0)return;
                if(nextServerFd==-1)return;
                auto id=++workIdBuf;
                backupWorks_locker.lock();
                backupWorks[id].setVal(c,l,n,backupnum);
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
                std::list<int> rms;
                for(auto it:backupWorks){
                    if(addCharToNext_work(
                        it.second.c,
                        it.second.lastGUID,
                        it.second.newGUID,
                        it.second.backupnum,
                        it.first
                    ))
                        rms.push_back(it.first);
                }
                for(auto it:rms){
                    backupWorks.erase(it);
                }
                backupWorks_locker.unlock();
            }
            
            void backupFinish(int wid){
                backupWorks_locker.lock();
                backupWorks.erase(wid);
                backupWorks_locker.unlock();
            }
            
            void onBackupChar(const GUID & lastDataId,char c,const position & newDataId,int backup){
                bool existed;
                existed=setNextNode(lastDataId,c,newDataId);
                if(!existed)
                    if(backup>0)
                        addCharToNext(c,lastDataId,newDataId,backup-1);
            }
            void onInsertChar(GUID dataId,char c,position & s,int backup){
                bool existed;
                if(c=='\0'){
                    existed=getNextNode(dataId,c,s);
                    s.first=-1;
                    s.second.init();
                }else{
                    existed=getNextNode(dataId,c,s);
                }
                if(!existed)
                    if(backup>0)
                        addCharToNext(c,dataId,s,backup-1);
            }
            bool addCharToNext_work(char c,const GUID & l,const position & n,int backupnum,int workId){
                if(nextServerFd==-1)return true;
                if(backupnum<=0)return true;
                package buf;
                
                bzero(&buf,sizeof(buf));
                
                buf.data.backup.c=c;
                buf.data.backup.guid=n.second.ton();
                buf.data.backup.serverId=htonl(n.first);
                
                buf.method=package::INSERT_BACKUP;
                buf.workId=workId;
                buf.setServerId(nextServerId);
                buf.setDataId(l);
                buf.setBackupNum(backupnum-1);
                
                ::send(nextServerFd,&buf,sizeof(buf),0);
                
                return false;
            }
            inline void onMsg_find(package * pk,int fd){
                position p;
                package buf;
                
                if(pk->getServerId()!=this->serverId)
                    return;
                
                bzero(&buf,sizeof(buf));
                
                if(findNextNode(pk->getDataId(),buf.data.arr))
                    pk->error=0x00000000;
                else
                    pk->error=0xffffffff;
                
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
            inline void onMsg_backup(package * pk,int fd){
                position p;
                package buf;
                
                if(pk->getServerId()!=this->serverId)
                    return;
                
                p.second=pk->data.backup.guid.toh();
                p.first =ntohl(pk->data.backup.serverId);
                
                onBackupChar(
                    pk->getDataId(),
                    pk->data.backup.c,
                    p,
                    pk->getBackupNum()
                );
                
                bzero(&buf,sizeof(buf));
                buf.method=package::INSERT_OK;
                buf.workId=pk->workId;
                buf.setServerId(p.first);
                
                ::send(fd,&buf,sizeof(buf),0);
            }
            void onMsg(package * pk,int fd){
                if(pk->method==package::INSERT)
                    onMsg_insert(pk,fd);
                else
                if(pk->method==package::INSERT_BACKUP)
                    onMsg_backup(pk,fd);
                else
                if(pk->method==package::INSERT_OK)
                    backupFinish(pk->workId);
                else
                if(pk->method==package::FIND)
                    onMsg_find(pk,fd);
                else
                if(pk->method==package::SETNEXT){
                    if(nextServerFd!=-1){
                        close(nextServerFd);
                        nextServerId=pk->getServerId();
                        if(nextServerId!=-1)
                            reconnect();
                    }
                }
            }
        private:
            bool isExist(GUID id,unsigned char c){
                try{
                    char buf[128];
                    std::string s;
                    getKey(buf,id.host,id.id);
                    if(!readDB(buf,s))
                        return false;
                    else{
                        if(s.at(c)=='1')
                            return true;
                        else
                            return false;
                    }
                }catch(...){
                    return false;
                }
            }
            
            void setExist(GUID id,unsigned char c){
                char buf[257];
                char key[257];
                std::string s;
                
                getKey(key,id.host,id.id);
                if(!readDB(key,s)){
                    for(register int i=0;i<256;i++){
                        buf[i]='0';
                    }
                    buf[c]='1';
                }else{
                    const char * p=s.c_str();
                    for(register int i=0;i<256;i++){
                        if(*p=='\0'){
                            buf[i]='0';
                        }else{
                            if(*p=='1')
                                buf[i]='1';
                            else
                                buf[i]='0';
                            ++p;
                        }
                        //try{
                        //    buf[i]=s.at(i);
                        //}catch(...){
                        //    buf[i]='0';
                        //}
                    }
                    buf[c]='1';
                }
                buf[256]='\0';
                writeDB(key,buf);
            }
            
            bool getNextNode(GUID dataId,char c,position & s,bool getfirst=true){
                /*
                 * return true:
                 ** next node has never existed,
                 ** push the information to next servers of this server to build backup.
                 * 
                 * if this node is exisited,parent node must be existed.
                 */
                if(findNode(dataId,c,s)){
                    return true;
                }else{
                    s.first=(++workIdBuf)%serverlist.servers.size();
                    s.second.init();
                    setNextNode(dataId,c,s);
                    return false;
                }
            }
            
            /* 
             * node operate
                struct node{
                    char[8];    //GUID:server id
                    char[16];   //GUID:count
                    char[8][];  //server ids
                    ...
                    char('\0'); //EOF
                }
            */
            bool setNextNode(const GUID & lastDataId,char c,const position & newDataId){
                if(isExist(lastDataId,c))
                    return true;
                setExist(lastDataId,c);
                
                std::string val;
                
                std::list<int> s;
                
                getNextServers(newDataId.first,s);
                
                char buf[128];
                int322hex(newDataId.second.host,buf);
                int642hex(newDataId.second.id,  buf+8);
                buf[24]='\0';
                val=buf;
                
                for(auto it:s){
                    int322hex(it,buf);
                    buf[8]='\0';
                    val+=buf;
                }
                
                getKey(buf,lastDataId.host,lastDataId.id,c);
                
                writeDB(buf,val.c_str());
                return false;
            }
            bool findNode(GUID dataId,char c,position & id,bool getfirst=true){
                std::list<int> s;
                std::string v;
                char buf[512];
                
                int ibuf;
                
                getKey(buf,dataId.host,dataId.id,c);
                
                if(readDB(buf,v)){
                    
                    const char * p=v.c_str();
                    
                    char tmp[16];
                    
                    bzero(tmp,8);
                    for(int i=0;i<8;i++){
                        if(*p=='\0')break;
                        tmp[i]=*p;
                        ++p;
                    }
                    id.second.host=hex2int32(tmp);
                    
                    bzero(tmp,16);
                    for(int i=0;i<16;i++){
                        if(*p=='\0')break;
                        tmp[i]=*p;
                        ++p;
                    }
                    id.second.id=hex2int64(tmp);
                    
                    while(1){
                        bzero(tmp,8);
                        for(int i=0;i<8;i++){
                            if(*p=='\0'){       //I would rather give up the data than read a rubbish.
                                goto finished;
                            }
                            tmp[i]=*p;
                            ++p;
                        }
                        ibuf=hex2int32(tmp);
                        s.push_back(ibuf);
                    }
                    
                    finished:
                    
                    if(s.empty())return false;
                    
                    if(getfirst)
                        id.first=*s.begin();
                    else{
                        ibuf=(++workIdBuf)%s.size();
                        auto it=s.begin();
                        for(int i=0;i<ibuf;i++){
                            if(it!=s.end())it++;
                        }
                        if(it==s.end())it--;
                        id.first=*it;
                    }
                    
                    return true;
                }else
                    return false;
            }
            bool findNextNode(GUID dataId,char * buf){
                //list children
                char key[128];
                std::string s;
                getKey(key,dataId.host,dataId.id);
                if(!readDB(key,s)){
                    return false;
                }else{
                    const char * p=s.c_str();
                    for(register int i=0;i<256;i++){
                        if(*p=='\0'){
                            buf[i]='0';
                        }else{
                            if(*p=='1')
                                buf[i]='1';
                            else
                                buf[i]='0';
                            ++p;
                        }
                    }
                }
            }
            
            bool readDB(const char * k,std::string & v){
                
            }
            bool writeDB(const char * k,const char * v){
                
            }
            
    };
}
#endif