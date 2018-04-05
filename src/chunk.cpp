#include "chunk.hpp"
#include "epoll.hpp"
#include <thread>
using namespace distrie;
chunk chk;
class server:public msg_base {
public:
    struct conn {
        char     buf[sizeof(package)];
        uint16_t ptr;
        int      fd;
        void reset() {
            bzero(buf,sizeof(package));
            ptr=0;
        }
        void append(char c) {
            char ic;

            if(ptr>=sizeof(package)-1) {
                chk.onMsg((package*)buf,fd);
                reset();
            }
        }
        void quit() {

        }
        void init(int ifd) {
            reset();
            fd=ifd;
        }
    };
    std::map<int,conn> conns;
    virtual void onConnect(int ifd) {
        conns[ifd].init(ifd);
    }
    virtual void onQuit(int connfd) {
        auto it=conns.find(connfd);
        if(it==conns.end())return;
        conn & cp=it->second;
        cp.quit();
    }
    virtual void onMessage(int connfd,char * fst,int len) {
        auto it=conns.find(connfd);
        if(it==conns.end())return;
        conn & cp=it->second;

        for(int j=0; j<len; j++)
            cp.append(fst[j]);

        while(1) {
            char buf[256];
            int slen=read(connfd,buf,256);
            if(slen<=0)return;

            for(int j=0; j<slen; j++)
                cp.append(buf[j]);

            if(slen<256)return;
        }

    }
}srv;
void backup(){
    printf("[backup] start \n");
    chk.running=true;
    while(chk.running){
        chk.backup_all();
    }
    printf("[backup] Quit \n");
}
void serve(){
    printf("[serve] start \n");
    signal(SIGPIPE,[](int){});
    signal(2,[](int){
        printf("[serve] Quit \n");
        srv.stop();
        chk.running=false;
        chk.backupWorks_cv.notify_all();
    });
    
    srv.run(13579);
}
int main(){
    std::thread t2(serve);
    std::thread t1(backup);
    t1.join();
    t2.join();
}