#include <bits/stdc++.h>
#include <sstream>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <capstone/capstone.h>
#include <elf.h>
using namespace std;
using pll = pair<long, long>;
map <unsigned long long, long> breakpoints; // breakpoints[address] = original code
struct user_regs_struct anchor_regs;
// vector <pair<pair<unsigned char*, long int>, pll>> anchor_mem; // <<buffer loc, buffer size>, <r1, r2>>

Elf64_Ehdr ehdr; // ELF header
Elf64_Shdr text_shdr; // text section header
char *code = NULL; // store the original code
int file_size = 0;
pid_t pid;
vector <pair<vector<unsigned long>, pll>> anchor_mem;

bool in_text_section(unsigned long address){
    if(text_shdr.sh_addr <= address && ((text_shdr.sh_addr + text_shdr.sh_size) > address)) return true;
    return false;
}

void Launch(string execfile){
    FILE *fp = fopen(execfile.c_str(), "rb"); // read binary
    if (fp == NULL){
        perror("Cannot Open Binary File");
        exit(0);
    }
    char str_tab[1000]; // string table
    bzero(str_tab, sizeof(str_tab)); 
    Elf64_Shdr str_shdr, str_shdr_;
    int str_shdr_off = 0; // string section header offset
    fread(&ehdr, 1, sizeof(Elf64_Ehdr), fp);
    if(ehdr.e_ident[EI_MAG0] == 0x7f && ehdr.e_ident[EI_MAG1] == 'E' && ehdr.e_ident[EI_MAG2] == 'L' && ehdr.e_ident[EI_MAG3] == 'F' && ehdr.e_ident[EI_CLASS] == ELFCLASS64){
        // ref : https://stackoverflow.com/questions/15993652/how-to-find-the-offset-of-the-section-header-string-table-of-an-elf-file
        str_shdr_off = ehdr.e_shstrndx * sizeof(Elf64_Shdr) + ehdr.e_shoff ; // section name string table
        fseek(fp, str_shdr_off, SEEK_SET); // set fp at string section header offset
        fread(&str_shdr, 1, sizeof(Elf64_Shdr), fp); // read in section header
        fseek(fp, str_shdr.sh_offset, SEEK_SET); // set fp
        fread(str_tab, str_shdr.sh_size, sizeof(char), fp); // read in string table
        fseek(fp, ehdr.e_shoff, SEEK_SET);
        for(int i = 0; i < ehdr.e_shnum; i++){
            fread(&str_shdr_, 1, sizeof(Elf64_Shdr), fp);
            if(strcmp((str_tab + str_shdr_.sh_name), ".text") == 0){ // found text section
                text_shdr = str_shdr_;
                break;
            }
        }
        printf("**program \'./%s\' loaded. entry point 0x%lx\n", execfile.c_str(), ehdr.e_entry);
        pid = fork();
        if(pid < 0){
            perror("Fork Error");
            exit(0);
        }
        if(pid == 0){ // child
            if(ptrace(PTRACE_TRACEME, 0, 0, 0) < 0){
                perror("Child ptrace Error");
                exit(0);
            }
            char* args[] = {NULL};
            execfile = "./" + execfile;
            if(execvp(execfile.c_str(), args) < 0){
                perror("Child execvp Error");
                exit(0);
            }
            
        }else{ // parent
            int state = 0;
            if(waitpid(pid, &state, 0) < 0){
                perror("Parent waitpid Error");
                exit(0);
            }
            if(ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_EXITKILL) < 0){
                perror("Parent ptrace set options Error");
                exit(0);
            }
            cout << dec << "pid = " << pid << endl;
        }
    }
}

void Disassemble(long address){
    if(!in_text_section(address)){
        printf("** the address is out of the text segment\n");
        return;
    }
    long offset = text_shdr.sh_offset + (address - text_shdr.sh_addr);
    char *cur_code;
    // bzero(cur_code, sizeof(cur_code));
    cur_code = code + offset;
    csh handle;
    cs_insn *insn;
    size_t cnt = 0;
    if(cs_open(CS_ARCH_X86, CS_MODE_64, &handle) != CS_ERR_OK){
        perror("cs_open Error");
        exit(0);
    }
    cnt = cs_disasm(handle, (uint8_t*)cur_code, (size_t)file_size, (uint64_t)address, 5, &insn);
    if(cnt <= 0){
        perror("cs_disasm Error");
        exit(0);
    }
    for(int i = 0; i < (int)cnt; i++){
        // bzero(bits, sizeof(bits));
        //cout << "insn size " << insn[i].size << " "; for(int j = 0; j < insn[i].size; j++) printf("%c", bytes[i]); cout << endl;
        if(in_text_section(insn[i].address)){
            stringstream hex_bytes;
            for (int j = 0; j < insn[i].size; ++j) {
                hex_bytes << setw(2) << right << setfill('0')
                            << hex << (unsigned int)insn[i].bytes[j] << " ";
            }
            cout << hex << right << setw(12) << insn[i].address << ": "
                     << left << setw(32) << hex_bytes.str()
                     << left << setw(9) << insn[i].mnemonic
                     << left << setw(9) << insn[i].op_str << "\n";
        }else {
            printf("** the address is out of the range of the text segment.\n");
            break;
        }
    }
    cs_free(insn, cnt);
    return;
}

void Breakpoint(unsigned long long address){
    unsigned long _code = ptrace(PTRACE_PEEKTEXT, pid, address, 0);
    // cout << "debug1\n";
    
    if((_code & 0xff) != 0xcc) { // haven't inserted int3
        // cout << "debug2\n";
        // insert int3
        if(ptrace(PTRACE_POKETEXT, pid, address, ((_code & 0xffffffffffffff00) | 0xcc)) != 0){
            perror("Set breakpoint POKETEXT Error");
            exit(0);
        }
        // cout << "debug3\n";
        if(breakpoints.find(address) == breakpoints.end()) breakpoints[address] = _code;
    }
}

void Continue()
{   
    // cout << "debug1\n";
    struct user_regs_struct regs;
    if(ptrace(PTRACE_GETREGS, pid, 0, &regs) != 0){
        perror("Get Regs Error");
        exit(0);
    }
    printf("current rip 0x%llx\n", regs.rip);
    // insert all breakpoints after current instruction
    for(auto &b : breakpoints){
        if(b.first > regs.rip) {
            Breakpoint(b.first); 
            printf("Reset bp at 0x%llx\n", b.first);
        }
    }
    // cout << "debug2\n";
    // Continue
    if (ptrace(PTRACE_CONT, pid, 0, 0) < 0){
        perror("Continue Error");
        exit(0);
    }
    int state = 0;
    if(waitpid(pid, &state, 0) < 0){
        perror("waidpid Error");
        exit(0);
    }
    // cout << "debug3\n";
    if(WIFEXITED(state)){
        printf("** the target program terminated.\n");
        pid = 0;
    }
    // cout << "debug4\n";
    if(WIFSTOPPED(state)){
        cout << "debug4.1\n";
        cout << dec << WSTOPSIG(state) << endl;
        cout << dec << SIGSEGV << endl;
        if(WSTOPSIG(state) == SIGTRAP){
            cout << "debug5\n";
            if(ptrace(PTRACE_GETREGS, pid, 0, &regs) != 0){
                perror("Get Regs Error");
                exit(0);
            }
            cout << "debug5.1\n";
            printf("stopped at 0x%llx\n", regs.rip);
            // check all breakpoints
            for(auto &b : breakpoints){
                cout << "debug6\n";
                if(b.first == regs.rip - 1 || b.first == regs.rip){
                    printf("** hit a breakpoint at 0x%llx\n", b.first);

                    if(b.first == regs.rip - 1) Disassemble(regs.rip - 1);
                    else Disassemble(regs.rip);
                    // b.first = address, b.second = original code
                    if(ptrace(PTRACE_POKETEXT, pid, b.first, b.second) != 0){
                        perror("POKETEXT Error\n");
                        exit(0);
                    }
                    if(b.first == regs.rip - 1){
                        regs.rip--;
                        if(ptrace(PTRACE_SETREGS, pid, 0, &regs) != 0){
                            perror("Set regs Error\n");
                        }
                    }
                }
            }
        }
    }
}
bool si_hit_bp = false;
void si(){
    si_hit_bp = false;
    // check for breakpoints
    struct user_regs_struct regs;
    if(ptrace(PTRACE_GETREGS, pid, 0, &regs) != 0){
        perror("Get Regs Error");
        exit(0);
    }
    for(auto &b : breakpoints){
        if(b.first > regs.rip) Breakpoint(b.first); 
    }
    if(ptrace(PTRACE_SINGLESTEP, pid, 0, 0) < 0){
        perror("ptrace SI Error");
        exit(0);
    }
    int state = 0;
    if(waitpid(pid, &state, 0) < 0){
        perror("waidpid Error");
        exit(0);
    }
    if(WIFEXITED(state)){
        printf("** the target program terminated.\n");
        pid = 0;
    }
    if(WIFSTOPPED(state)){
        if(WSTOPSIG(state) == SIGTRAP){
            if(ptrace(PTRACE_GETREGS, pid, 0, &regs) != 0){
                perror("Get Regs Error");
                exit(0);
            }
            // check all breakpoints
            for(auto &b : breakpoints){
                if(b.first == regs.rip - 1 || b.first == regs.rip){
                    for(auto &b : breakpoints){
                        if(b.first == regs.rip - 1 || b.first == regs.rip){
                            si_hit_bp = true;
                            printf("** hit a breakpoint at 0x%llx\n", b.first);
                            if(b.first == regs.rip - 1) Disassemble(regs.rip - 1);
                            else Disassemble(regs.rip);
                            if(ptrace(PTRACE_POKETEXT, pid, b.first, b.second) != 0){
                                perror("POKETEXT Error\n");
                                exit(0);
                            }
                            if(b.first == regs.rip - 1){
                                regs.rip--;
                                if(ptrace(PTRACE_SETREGS, pid, 0, &regs) != 0){
                                    perror("Set regs Error\n");
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void SetAnchor(){
    // get all registers
    if(ptrace(PTRACE_GETREGS, pid, 0, &anchor_regs) != 0){
                perror("Get Regs Error");
                exit(0);
    }
    ifstream fin;
    printf("Anchor rip = 0x%llx\n", anchor_regs.rip);
    fin.open("/proc/" + to_string(pid) + "/maps");
    string s = "";
    anchor_mem.clear();
    while (getline(fin, s)){
        if(s.find("rw") != std::string::npos){
            // printf("s = %s\n", s.c_str());
            unsigned long r1 = 0, r2 = 0;
            r1 = strtoul((s.substr(0, s.find('-'))).c_str(), nullptr, 16);
            r2 = strtoul((s.substr(s.find('-') + 1, s.find(' ') - s.find('-'))).c_str(), nullptr, 16);
            cout << dec << r1 << " " << r2 << " " << r2 - r1 << endl;
            // unsigned char *mem = (unsigned char *) malloc(sizeof(unsigned char) * (r2 - r1));
            vector <unsigned long> temp;
            for(unsigned long i = 0; i < r2 - r1; i+=8){
                unsigned long tmp = ptrace(PTRACE_PEEKTEXT, pid, r1 + i, 0);
                temp.push_back(tmp);
            }
            
            anchor_mem.emplace_back(pair<vector<unsigned long>, pll>{temp, {r1, r2}});
        }
    }
    // for(auto p : anchor_mem){
    //     printf("%p\n", p.first);
        // cout << sizeof(*p.first) << endl;
        // cout << dec << p.second << endl;
    // }
    printf("** dropped an anchor\n");
}

void TimeTravel(){
    printf("** go back to the anchor point\n");
    for(auto p : anchor_mem){
        long len = p.first.size(), r1 = p.second.first;
        
        for(int i = 0; i < len; i += 1){
            if(ptrace(PTRACE_POKETEXT, pid, r1 + i*8, p.first[i]) != 0){
                perror("Time travel POKETEXT Error");
                exit(0);
            }
        }
    }
    if(ptrace(PTRACE_SETREGS, pid, 0, &anchor_regs) != 0){
        perror("Time travel SETREGS Error");
        exit(0);
    }
    Disassemble(anchor_regs.rip);
    for(auto &b : breakpoints){
        if(anchor_regs.rip != b.first) {
            // cout << "reset bp 0x" << hex << b.first << endl;
            Breakpoint(b.first);
        }
    }
}

int main(int argc, char* argv[]){
    printf("argc = %d\n", argc);
    for (int i = 0; i < argc; i++)
    {
        printf("%s\n", argv[i]);
    }
    string execfile = string(argv[1]);
    execfile = execfile.substr(2); // ./hello -> hello
    cout << "execfile name " << execfile << endl;
    Launch(execfile);
    // read in all the codes
    ifstream fin(execfile.c_str(), ios::in | ios::binary);
    fin.seekg(0, fin.end);
    file_size = fin.tellg();
    fin.seekg(0, fin.beg);
    code = (char*)malloc(sizeof(char) * file_size);
    fin.read(code, file_size);
    fin.close();
    cout << "file size " << file_size << endl;
    Disassemble(ehdr.e_entry);
    while (1){
        cout << "(sdb) ";
        string s = "", cmd = "", s_tmp = "";
        unsigned long long address = 0;
        getline(cin, s);
        // cout << "s = " << s << endl;
        if(s.find(' ')){
            stringstream ss;
            ss << s; 
            ss >> cmd >> s_tmp;
            s_tmp.erase(0, 2);
            // cout << "Remove 0x " << s_tmp << endl;
            address = strtoul(s_tmp.c_str(), nullptr, 16);
        }else if(s.find('\n')){
            s.erase(s.find('\n'));
            cmd = s;
        }else cmd = s;

        if(cmd == "break"){
            Breakpoint(address);
            printf("** set a breakpoint at 0x%llx\n", address);
            address = 0x0;
        }else if(cmd == "cont"){
            cout << "Execute cont\n";
            Continue();
        }else if(cmd == "si"){
            si();
            struct user_regs_struct regs;
            if(ptrace(PTRACE_GETREGS, pid, 0, &regs) != 0){
                exit(0);
            }
            if(!si_hit_bp) Disassemble(regs.rip);
        }else if(cmd == "anchor"){
            SetAnchor();
        }else if(cmd == "timetravel"){
            TimeTravel();
        }
    }
    
    return 0;
}
