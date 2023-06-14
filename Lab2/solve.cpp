#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <cstring>
#include <unistd.h>
#include <signal.h>
#include <filesystem>
#include <dirent.h>
#include <fstream>
#include <filesystem>

using recursive_directory_iterator = std::filesystem::recursive_directory_iterator;
using namespace std;
char buf[100005];
int main(int argc, char* argv[]){
    cin.tie(0);
    cin.sync_with_stdio(0);
    
    string magic_num = string(argv[2]);
    int cnt = 0;
    bool found = false;

    for (const auto& dirEntry : recursive_directory_iterator(argv[1])){
        if(is_directory(dirEntry)) continue;
        cerr << dirEntry << endl;
        string file_path = dirEntry.path();
        ifstream fin;
        fin.open(file_path);
        string num = "";
        getline(fin, num);
        if(num == magic_num){
            cout << file_path << "\n";
            found = true;
            break;
        }
    }

    return 0;
}
