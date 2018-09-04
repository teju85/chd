#include <stdio.h>
#include <string>
#include <stdexcept>
#include <string.h>
#include <vector>
#include <stdint.h>
#include <algorithm>

#define THROW(fmt, ...)                                         \
    do {                                                        \
        std::string msg;                                        \
        char errMsg[2048];                                      \
        sprintf(errMsg, "Exception occured! file=%s line=%d: ", \
                __FILE__, __LINE__);                            \
        msg += errMsg;                                          \
        sprintf(errMsg, fmt, ##__VA_ARGS__);                    \
        msg += errMsg;                                          \
        throw std::runtime_error(msg);                          \
    } while(0)

#define ASSERT(check, fmt, ...)                  \
    do {                                         \
        if(!(check))  THROW(fmt, ##__VA_ARGS__); \
    } while(0)

void printHelp() {
    printf("USAGE:\n");
    printf("  chd [-h] [-l <lambda>] [-a <alpha>] <keysFile>\n");
    printf("OPTIONS:\n");
    printf("  -h           Print this help and exit\n");
    printf("  -l <lambda>  Load factor. The same variable as in the paper.\n");
    printf("  -a <alpha>   Leeway. The same variable as in the CHD-paper.\n");
    printf("  <keysFile>   Vocabulary file [mandatory!]\n");
    printf("Format of <keysFile>:\n");
    printf("Each line contains a word that's part of the vocabulary. Currently\n"
           "only plain ascii strings are supported.\n");
}

std::string readWord(FILE* fp) {
    char str[1024];
    int len = 0;
    while(!feof(fp)) {
        char c = fgetc(fp);
        if(feof(fp) || c == '\n') {
            break;
        }
        str[len] = c;
        ++len;
    }
    str[len] = 0;
    return len > 0? std::string(str) : std::string();
}

std::vector<std::string> readAllKeys(const char* file) {
    FILE* fp = fopen(file, "rb");
    ASSERT(fp != nullptr, "Failed to open file '%s'!", file);
    std::vector<std::string> keys;
    while(!feof(fp)) {
        auto key = readWord(fp);
        if(!key.empty()) {
            keys.push_back(key);
        }
    }
    fclose(fp);
    return keys;
}

uint32_t hashsize(int n) {
    return uint32_t(1) << n;
}

uint32_t hashmask(int n) {
    return hashsize(n) - 1;
}

uint32_t rot(uint32_t x, uint32_t k) {
    return (x << k) | (x >> (32-k));
}

void mix(uint32_t& a, uint32_t& b, uint32_t& c) {
    a -= c;   a ^= rot(c,  4);   c += b;
    b -= a;   b ^= rot(a,  6);   a += c;
    c -= b;   c ^= rot(b,  8);   b += a;
    a -= c;   a ^= rot(c, 16);   c += b;
    b -= a;   b ^= rot(a, 19);   a += c;
    c -= b;   c ^= rot(b,  4);   b += a;
}

void finalMix(uint32_t& a, uint32_t& b, uint32_t& c) {
    c ^= b;   c -= rot(b, 14);
    a ^= c;   a -= rot(c, 11);
    b ^= a;   b -= rot(a, 25);
    c ^= b;   c -= rot(b, 16);
    a ^= c;   a -= rot(c,  4);
    b ^= a;   b -= rot(a, 14);
    c ^= b;   c -= rot(b, 24);
}

uint32_t get(const char* k, int len) {
    uint32_t ret = 0;
    for(int i=0;i<len;++i) {
        ret += (uint32_t)k[i] << (i * 8);
    }
    return ret;
}

void hashWord(const char* key, size_t len, uint32_t init,
              uint32_t& a, uint32_t& b, uint32_t& c) {
    a = b = c = init;
    const char* k = key;
    while(len >= 12) {
        a += get(k, 4);  b += get(k+4, 4);  c += get(k+8, 4);
        mix(a, b, c);
        len -= 12;
        k += 12;
    }
    switch(len) {
    case 11: a += get(k, 4);  b += get(k+4, 4);  c += get(k+8, 3);  break;
    case 10: a += get(k, 4);  b += get(k+4, 4);  c += get(k+8, 2);  break;
    case 9:  a += get(k, 4);  b += get(k+4, 4);  c += get(k+8, 1);  break;
    case 8:  a += get(k, 4);  b += get(k+4, 4);  break;
    case 7:  a += get(k, 4);  b += get(k+4, 3);  break;
    case 6:  a += get(k, 4);  b += get(k+4, 2);  break;
    case 5:  a += get(k, 4);  b += get(k+4, 1);  break;
    case 4:  a += get(k, 4);  break;
    case 3:  a += get(k, 3);  break;
    case 2:  a += get(k, 2);  break;
    case 1:  a += get(k, 1);  break;
    default: return;
    }
    finalMix(a, b, c);
}

int hashMod(const char* str, size_t len, uint32_t initval, int mod) {
    uint32_t a, b, c;
    hashWord(str, len, initval, a, b, c);
    return c % mod;
}

struct Bucket {
    int bId;
    std::vector<int> keyIds;
};

bool compare(const Bucket& a, const Bucket& b) {
    return a.keyIds.size() < b.keyIds.size();
}

typedef std::vector<Bucket> Buckets;

Buckets getBuckets(const std::vector<std::string>& keys, int R) {
    Buckets ret(R);
    int i = 0;
    for(const auto& k : keys) {
        int bId = hashMod(k.c_str(), k.size(), 0, R);
        ret[bId].bId = bId;
        ret[bId].keyIds.push_back(i);
        ++i;
    }
    std::sort(ret.begin(), ret.end(), compare);
    return ret;
}

int main(int argc, char** argv) {
    char* keysFile = nullptr;
    float lambda = 5.f;
    float alpha = 1.f;
    for(int i=1;i<argc;++i) {
        if(!strcmp("-h", argv[i])) {
            printHelp();
            return 0;
        } else if(!strcmp("-l", argv[i])) {
            ++i;
            ASSERT(i < argc, "'-l' expects an argument!");
            lambda = atof(argv[i]);
        } else if(!strcmp("-a", argv[i])) {
            ++i;
            ASSERT(i < argc, "'-a' expects an argument!");
            alpha = atof(argv[i]);
        } else {
            keysFile = argv[i];
        }
    }
    ASSERT(keysFile != nullptr, "<keysFile> is mandatory!");
    ASSERT(lambda >= 1.f && lambda <= 8.f, "'-l' must be between 1.0 & 8.0!");
    ASSERT(alpha >= 0.01f && alpha <= 1.f, "'-a' must be between 0.01 & 1.0!");
    auto keys = readAllKeys(keysFile);
    int n = (int)keys.size();
    printf("Read %d keys from the input file\n", n);
    int r = n / lambda;
    int m = n / alpha;
    printf("Getting buckets r=%d m=%d n=%d...\n", r, m, n);
    auto buckets = getBuckets(keys, r);
    printf("Initializing T...\n");
    bool* T = new bool[m];
    for(int i=0;i<m;++i) {
        T[i] = false;
    }
    printf("Creating CHD...\n");
    std::vector<int> hids(r);
    for(auto& itr : buckets) {
        printf("  Trying for bucket=%d ...\n", itr.bId);
        int len = (int)itr.keyIds.size();
        uint32_t l = 1;
        while(true) {
            bool found = true;
            for(auto kid : itr.keyIds) {
                int idx = hashMod(keys[kid].c_str(), keys[kid].size(), l, m);
                if(T[idx]) {
                    found = false;
                    break;
                }
            }
            if(found) {
                hids[itr.bId] = l;
                for(auto kid : itr.keyIds) {
                    int idx = hashMod(keys[kid].c_str(), keys[kid].size(), l, m);
                    T[idx] = true;
                }
                printf("    Found l=%d\n", l);
                break;
            }
            ++l;
        }
    }
    delete [] T;
    return 0;
}
