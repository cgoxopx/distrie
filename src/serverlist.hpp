#ifndef distrie_serverlist
#define distrie_serverlist
#include <vector>
#include <string>
#include <sstream>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/file.h>
namespace distrie{
    class Serverlist{
        public:
            struct server{
                std::string addr;
                short port;
            };
            int myId;
            std::vector<server> servers;
            
            char buf[256];
            Serverlist(){
                FILE * fp;
                fp=fopen("config/serverid.conf","r");
                if(fp!=NULL){
                    bzero(buf,256);
                    fgets(buf,255,fp);
                    myId=atoi(buf);
                    fclose(fp);
                }
                
                fp=fopen("config/serverlist.conf","r");
                if(fp!=NULL){
                    while(!feof(fp)){
                        bzero(buf,256);
                        fgets(buf,255,fp);
                        std::istringstream iss(buf);
                        server sv;
                        iss>>sv.addr;
                        iss>>sv.port;
                        this->servers.push_back(sv);
                    }
                    fclose(fp);
                }
            }
            int connect(int id){
                try {
                    server & sv=servers.at(id);
                    struct sockaddr_in address;
                    address.sin_family = AF_INET;
                    address.sin_addr.s_addr = inet_addr(sv.addr.c_str());
                    address.sin_port = htons(sv.port);
                    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
                    if(sockfd==-1)
                        return -1;
                    ::connect(sockfd, (struct sockaddr *)&address, sizeof(address));
                    return sockfd;
                }catch (const std::out_of_range& oor) {
                    return -1;
                }
            }
    }serverlist;
}
#endif