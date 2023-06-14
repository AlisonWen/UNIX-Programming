#include <bits/stdc++.h>
#include <sys/mman.h>
#include <dlfcn.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <signal.h>
#include <assert.h>
#include <elf.h>
#include <cstdlib>
#include <dlfcn.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
// #include <libunwind.h>
using namespace std;
// int __libc_start_main(int *(main) (int, char * *, char * *), 
//                       int argc, char * * ubp_av, 
//                       void (*init) (void), 
//                       void (*fini) (void), 
//                       void (*rtld_fini) (void), 
//                       void (* stack_end));
extern "C" {
    int config_fd = -1, logger_fd = -1;
    long base_num = 0x0;
    map <string, string> file_data; // <filename, data>
    map <int, string> fd_to_filename; // <file descriptor, file name>
    stringstream ss;
    vector <string> open_blacklist, read_blacklist, connect_blacklist, getaddrinfo_blacklist;
    void parse_config(){
        ifstream fin;
        printf("config path: %s\n", getenv("SANDBOX_CONFIG"));
        fin.open(getenv("SANDBOX_CONFIG"));
        string s;
        while (getline(fin, s)){
            if(s.find("BEGIN") != string::npos){ // s has space, could be BEGIN or END
                s[s.find("-")] = ' ';
                string s1, func_name, s3;
                ss.str();
                ss.clear();
                ss << s; 
                ss >> s1 >> func_name >> s3;
                cout << "func name " << func_name << endl;
                while (getline(fin, s) && s.find("END") == string::npos){
                    if (func_name == "open"){
                        open_blacklist.push_back(s);
                    }else if(func_name == "read"){
                        read_blacklist.push_back(s);
                    }else if(func_name == "connect"){
                        connect_blacklist.push_back(s);
                    }else if(func_name == "getaddrinfo"){
                        getaddrinfo_blacklist.push_back(s);
                    }
                }
                
            }
        }
        vector <string> tmp;
        for(auto h:connect_blacklist){
            cout << "host " << h << endl;
            s = h;
            s[s.find(':')] = ' ';
            if(s.size() <= 1) continue;
            stringstream ss;
            ss << s;
            string hostname, port;
            ss >> hostname >> port;
            cout << "hostname " << hostname << ", " << port<< endl;
            struct hostent *host = gethostbyname(hostname.c_str());
            struct in_addr **addr_list = (struct in_addr **)host->h_addr_list;
            for (int i = 0; addr_list[i] != NULL; i++) {
                char ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, addr_list[i], ip, sizeof(ip));
                printf("IP address %d: %s\n", i + 1, ip);
                tmp.push_back(string(ip) + ":" + port);
            }
                    
        }
        for(auto h : tmp){
            connect_blacklist.push_back(h);
        }
        // tmp.clear();
        // for(auto h:getaddrinfo_blacklist){
        //     cout << "host " << h << endl;
        //     string hostname = h;
        //     if(hostname.size() <= 1) continue;
        //     struct hostent *host = gethostbyname(hostname.c_str());
        //     struct in_addr **addr_list = (struct in_addr **)host->h_addr_list;
        //     for (int i = 0; addr_list[i] != NULL; i++) {
        //         char ip[INET_ADDRSTRLEN];
        //         inet_ntop(AF_INET, addr_list[i], ip, sizeof(ip));
        //         printf("IP address %d: %s\n", i + 1, ip);
        //         tmp.push_back(string(ip));
        //     }
                    
        // }
        // for(auto h : tmp){
        //     getaddrinfo_blacklist.push_back(h);
        // }
        printf("Each list:\n");
        cout << "open " << open_blacklist.size() << endl;
        cout << "read " << read_blacklist.size() << endl;
        cout << "connect " << connect_blacklist.size() << endl;
        cout << "getaddrinfo " << getaddrinfo_blacklist.size() << endl;
        cout << "----Connect----\n";
        for(auto h:connect_blacklist){
            cout << h << endl;
        }
        cout << "-----------------\n";
        
    }
    int fake_open(char *pathname, int flags, mode_t mode){
        // handle symbolic link
        unsetenv("LD_PRELOAD");
        string cmd = "readlink -f " + string(pathname);
        FILE *fp = popen(cmd.c_str(), "r");
        if(fp == NULL){
            perror("popen");
            exit(0);
        }
        char realpath[100];
        bzero(realpath, sizeof(realpath));
        fgets(realpath, 100, fp);
        if(fp != NULL) pclose(fp);
        string real_path = string(realpath);
        real_path.pop_back();
        cout << "In open : real path = " << real_path << endl;
        // compare blacklist
        bool found = false;
        for(auto d : open_blacklist){
            if(d == string(real_path)){
                cout << "d " << d << ", " << d.size() << endl;
                found = true;
                break;
            }
        }
        if(found){
            string log = "[logger] open(\"" + real_path + "\", " + to_string(flags) + ", " + to_string(mode) + ") = -1\n";
            write(logger_fd, log.c_str(), log.length());
            errno = EACCES;
            return -1;
        }
        // create log file
        int return_val = open(real_path.c_str(), flags, mode);
        string log = "[logger] open(\"" + real_path + "\", " + to_string(flags) + ", " + to_string(mode) + ") = " + to_string(return_val) + "\n";
        write(logger_fd, log.c_str(), log.length());
        fd_to_filename[return_val] = string(pathname);
        file_data[string(pathname)] = "";
        return return_val;
    }
    ssize_t fake_read(int fd, void *buf, size_t cnt){
    // call the true open and log
    // use getenv() to get LOGGER_FD and use write to record the logger info
        // cout << "*Reading File decriptor " << fd << endl;
        pid_t pid = getpid();
        // if a file is read seperately, how to check keyword blacklist? determine by fd & open
        ssize_t return_val = read(fd, buf, cnt);
        string filename = fd_to_filename[fd];
        file_data[filename] += string((char*)buf);
        // cout << "cur file data = " << file_data[filename] << "\n";
        if(file_data[filename].find(read_blacklist[0]) != string::npos){ // the file contains keyword
            char log[100];
            bzero(log, sizeof(log));
            sprintf(log, "[logger] read(%d, %p, %ld) = -1\n", fd, buf, cnt);
            write(logger_fd, log, sizeof(log));
            close(fd);
            errno = EIO;
            fd_to_filename[fd] = -1;
            return -1;
        }
        string logname = to_string(pid) + "-" + to_string(fd) + "-read.log";
        FILE *fp = fopen(logname.c_str(), "ab+"); // open {pid}-{fd}-read.log and create one of not exist
        fwrite(buf, sizeof(char), strlen((char*)buf), fp); // write to the log file
        char log[100];
        bzero(log, sizeof(log));
        sprintf(log, "[logger] read(%d, %p, %ld) = %ld\n", fd, buf, cnt, return_val);
        write(logger_fd, log, sizeof(log));
        return return_val;
    }

    ssize_t fake_write(int fd, const void *buf, size_t cnt){
        string logname = to_string(getpid()) + "-" + to_string(fd) + "-" + "-write.log";
        FILE *fp = fopen(logname.c_str(), "ab+");
        fwrite(buf, sizeof(char), strlen((char*)buf), fp);
        ssize_t return_val = write(fd, buf, cnt);
        char log[100];
        bzero(log, sizeof(log));
        sprintf(log, "[logger] write(%d, %p, %ld) = %ld\n", fd, buf, cnt, return_val);
        write(logger_fd, log, sizeof(log));
        return return_val;
    }

    int fake_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen){
        // char buf[NI_MAXHOST];
        // bzero(buf, sizeof(buf));
        struct sockaddr_in* ADDR = (struct sockaddr_in*)addr;
        uint16_t port = htons(ADDR->sin_port);
        char hostname[NI_MAXHOST];
        char servInfo[NI_MAXSERV];
        struct addrinfo *Addressinfo;
        char* buf = inet_ntoa(ADDR->sin_addr);
        
        string address = string(buf) + ":" + to_string(port);
        for(auto a : connect_blacklist){
            if(address == a){
                string log = "[logger] connect(" + to_string(sockfd) + ", \"" + string(buf) + "\", " + to_string(addrlen) + ") = -1\n";
                write(logger_fd, log.c_str(), log.length());
                errno = ECONNREFUSED;
                return -1;
            }
        }
        int return_val = connect(sockfd, addr, addrlen);
        string log = "[logger] connect(" + to_string(sockfd) + ", \"" + string(buf) + "\", " + to_string(addrlen) + ") = " + to_string(return_val) + "\n";
        write(logger_fd, log.c_str(), log.length());
        return return_val;
    }

    bool string_compare(const char *node, string s){
        if(strlen(node) != s.size()) return false;
        for(int i = 0; i < s.size(); i++){
            if((int)s[i] != (int)node[i]) return false;
        }
        return true;
    }

    int fake_getaddrinfo(const char *node, //e.g. "www.example.com" or IP
                    const char *service, // e.g. "http" or port number
                    const struct addrinfo *hints, // give some basic info
                    struct addrinfo **res){ // returns
        /*
        struct addrinfo {
            int              ai_flags;
            int              ai_family;// ipv4,ipv6...
            int              ai_socktype;//SOCK_STREAM SOCK_DGRAM
            int              ai_protocol;//TCP UDP...
            socklen_t        ai_addrlen;
            struct sockaddr *ai_addr;
            char            *ai_canonname;
            struct addrinfo *ai_next;
    };
    */  
        
        for(auto host:getaddrinfo_blacklist){
            if(string_compare(node, host)){
                cout << "node " << host << "gotcha\n";
                char log[100];
                bzero(log, sizeof(log));    
                sprintf(log, "[logger] getaddrinfo(\"%s\", %s, %p, %p) = %d\n",node, service, hints, res, EAI_NONAME);
                write(logger_fd, log, 100);
                errno = EAI_NONAME;
                return EAI_NONAME;
            }
        }
        struct addrinfo *Hints = (struct addrinfo*)hints;
        Hints->ai_flags=0;
        int return_val = getaddrinfo(node, service, Hints, res);
        char log[100];
        bzero(log, sizeof(log));
        sprintf(log, "[logger] getaddrinfo(\"%s\", %s, %p, %p) = %d\n",node, service, hints, res, return_val);
        write(logger_fd, log, sizeof(log));
        return return_val;
    }

    int fake_system(const char *command){
        unsetenv("LD_PRELOAD");
        string log = "[logger] system(\"" + string(command) + "\")\n";
        write(logger_fd, log.c_str(), log.length());
        int return_val = system(command);
        cout << "system return value = " << return_val << endl;
        return return_val;
    }

    Elf64_Shdr header[100];
    Elf64_Shdr *strtab;
    Elf64_Shdr got;
    Elf64_Rela relocation;
    void parse_sections(Elf64_Ehdr *hdr, int exe_fd){
        
        lseek(exe_fd, hdr->e_shoff, SEEK_SET);
        char strtab_data[65535];
        bzero(strtab_data, sizeof(strtab_data));
        cout << "debug1" << endl;
        for(int i = 0; i < hdr->e_shnum; i++){
            read(exe_fd, &header[i], sizeof(Elf64_Shdr));
            if(header[i].sh_type == SHT_STRTAB) strtab = &header[i];
            // printf("[%d]", i);
            // cout << "type = " << hex << header[i].sh_type << endl;
            // cout << "sh_name = " << header[i].sh_name << endl;
        }
        cout << "Num of sections : " << hdr->e_shnum << endl;
        cout << "debug2" << endl;
        cout << "str name table offset " << hdr->e_shstrndx << endl;
        lseek(exe_fd, hdr->e_shstrndx, SEEK_SET);
        
        cout << "read bytes = "<< read(exe_fd, strtab_data, header[hdr->e_shstrndx].sh_size) << endl;
        // printf("read %s\n", strtab_data);
        // for(int i = 0; i < hdr -> e_shnum; i++){
        //     if(string(strtab_data[header[i].sh_name]) == ".got"){ // get got table location
        //         got = header[i];
        //     }
        // }
    }
    map <string, unsigned long> GOT_offset;
    unsigned long max_off = 0x0;
    void calling_readelf_to_parse_sections(string argv){
        string cmd = "readelf -r " + argv;
        FILE *cmd_fp = popen(cmd.c_str(), "r");
        if(cmd_fp == NULL){
            perror("popen");
            exit(0);
        }
        char _cmd_path[1000];
        bzero(_cmd_path, sizeof(_cmd_path));
        vector <string> readelf_result;
        while(fgets(_cmd_path, 1000, cmd_fp) != NULL){
            readelf_result.push_back(string(_cmd_path));
            printf("%s\n", _cmd_path);
            bzero(_cmd_path, sizeof(_cmd_path));
        }
        if(cmd_fp != NULL) pclose(cmd_fp);

        GOT_offset.clear();
        for(auto s : readelf_result){
            stringstream ss;
            string s1, s2;
            ss << s;
            ss >> s1 >> s2;
            unsigned long offset = strtol(s1.c_str(), nullptr, 16);
            if(s.find("open@") == 62){
                GOT_offset["open"] = offset;
            }else if(s.find("read@") == 62){
                GOT_offset["read"] = offset;
            }else if(s.find("write@") == 62){
                GOT_offset["write"] = offset;
            }else if(s.find("connect@") == 62){
                GOT_offset["connect"] = offset;
            }else if(s.find("getaddrinfo@") == 62){
                GOT_offset["getaddrinfo"] = offset;
            }else if(s.find("system@") == 62 ){
                GOT_offset["system"] = offset;
            }
        }
        cout << "GOT offset size = "<< GOT_offset.size() << endl;
        
        for(auto &i : GOT_offset){
            max_off = max(i.second, max_off);
            cout << i.first << ", "<< hex << i.second << endl;
        }

        unsigned long min_off = max_off;
        for(auto &i : GOT_offset) min_off = min(i.second, min_off);
        

        // change the access of GOT(mprotect)
        long pagesize = sysconf(_SC_PAGE_SIZE);
        int mp = -1;
        printf("base num = %lx\n", base_num);
        if(max_off < pagesize){
            if(mp = mprotect((size_t*)base_num, pagesize, PROT_READ | PROT_WRITE | PROT_EXEC) < 0){
                perror("mprotect");
                exit(0);
            }
        }else{
            printf("page size %lx\n",pagesize);
            printf("(min off, max off) = (%lx, %lx)\n", min_off, max_off);

            unsigned long start = 0, end = 0;
            if((base_num + min_off) % pagesize != 0){
                start = base_num + min_off - ((base_num + min_off) % pagesize);
            }else start = base_num + min_off;
            if((base_num + max_off) % pagesize != 0){
                end = ((base_num + max_off)/pagesize + 1) * pagesize;
            }else end = base_num + max_off;
            printf("(start, end) = (%lx, %lx)\n", start, end);
            printf("%lx, %lx\n", end-start, (end-start)/pagesize);
            if(mp = mprotect((size_t*)start, end-start, PROT_READ | PROT_WRITE | PROT_EXEC) < 0){
                perror("mprotect");
                exit(0);
            }
        }
        cout << "mprotect returns " << mp << endl;
    }
    
    typedef int (*libc_start_main_func)(int *(main) (int, char * *, char * *), int argc, char * * ubp_av, void (*init) (void), void (*fini) (void), void (*rtld_fini) (void), void (* stack_end));
    int __libc_start_main(int *(main) (int, char * *, char * *), 
                        int argc, char ** ubp_av, 
                        void (*init) (void), 
                        void (*fini) (void), 
                        void (*rtld_fini) (void), 
                        void (* stack_end)){
        void *handle = dlopen("libc.so.6", RTLD_LAZY);
        if(!handle){
            fprintf(stderr, "%s\n", dlerror());
            exit(1);
        }
        void (*func_addr) = dlsym(handle, "__libc_start_main"); // get real function address
        if(func_addr == NULL){
            perror("dlsym");
            exit(0);
        }else{
            // printf("get func pointer\n");
        }
        // cout << "strlen of ubp_av[0] " << strlen(ubp_av[0]) << endl;
        unsetenv("LD_PRELOAD");
        string cmd = "which " + string(ubp_av[0]);
        cout << "cmd = " << cmd << endl;
        FILE *cmd_fp = popen(cmd.c_str(), "r");
        if(cmd_fp == NULL){
            perror("popen");
            exit(0);
        }
        char _cmd_path[1000];
        bzero(_cmd_path, sizeof(_cmd_path));
        fgets(_cmd_path, 1000, cmd_fp);
        printf("got :%s", _cmd_path);
        cout << "strlen " << strlen(_cmd_path) << endl;
        string cmd_path = string(_cmd_path);
        cmd_path.pop_back();
        if(cmd_fp != NULL) pclose(cmd_fp);

        int exe_fd = open(cmd_path.c_str(), S_IRUSR);
        if(exe_fd < 0){
            perror("opening exe fd");
            exit(0);
        }
        Elf64_Ehdr f_header;
        read(exe_fd, &f_header, sizeof(Elf64_Ehdr));
        parse_sections(&f_header, exe_fd);
        

        FILE* base_fp = fopen("/proc/self/maps", "r");
        
        if(base_fp){
            char base[1500];
            fseek(base_fp, 0, SEEK_SET);
            fread(base, 12, 1, base_fp);
            base[12] = '\0';
            printf("base = %s\n", base);
            base_num = strtol(base, NULL, 16);
            printf("temp = %lx\n", base_num);
        }
        calling_readelf_to_parse_sections(cmd_path);
        // get_base(cmd_path);
        // printf("main_min %ld, main_max %ld\n", main_min, main_max);
        
        // handle got
        // get sections
        // Elf32_Ehdr f_header;
        // int exe_fd = open(cmd_path, S_IRUSR);
        // if(exe_fd < 0){
        //     perror(cmd_path);
        //     exit(0);
        // }
        // read(exe_fd, &f_header, sizeof(Elf32_Ehdr));
        // parse_sections(&f_header, exe_fd);
        
        // while (!(cmd_path.back() - 'a'>= 0 && cmd_path.back() - 'a' < 26)){
        //     cmd_path.pop_back();
        // }
        // for(int i = 0; i < cmd_path.size(); i++){
        //     printf("[%d], %c, %d\n", i, cmd_path[i], (int)cmd_path[i]);
        // }
        // cout << cmd_path.size() << endl;
        // char buf[105]; bzero(buf, sizeof(buf));
        // for(int i = 0; i < cmd_path.size(); i++) buf[i] = cmd_path[i];
        // buf[cmd_path.size()] = '\0';
        
        char* Logger_fd = getenv("LOGGER_FD");
        logger_fd = strtol(Logger_fd, nullptr, 10);
        parse_config();

        for(auto &i : GOT_offset){
            long temp = base_num + i.second;
            long* addr_dest = (long*)temp;
            if(i.first == "open"){
                *addr_dest = (long)&fake_open;
            }else if(i.first == "read"){
                *addr_dest = (long)&fake_read;
            }else if(i.first == "write"){
                *addr_dest = (long)&fake_write;
            }else if(i.first == "connect"){
                *addr_dest = (long)&fake_connect;
            }else if(i.first == "getaddrinfo"){
                *addr_dest = (long)&fake_getaddrinfo;
            }else if(i.first == "system"){
                *addr_dest = (long)&fake_system;
            }
        }
        printf("*End GOT redirecting\n");
        libc_start_main_func libc_start_main = reinterpret_cast<libc_start_main_func>(func_addr);
        int return_val = libc_start_main(main, argc, ubp_av, init, fini, rtld_fini, stack_end);
        printf("*End calling __libc_start_main\n");
        return return_val;
    }
    
}
