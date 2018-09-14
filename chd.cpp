#include <stdio.h>
#include <string>
#include <stdexcept>
#include <string.h>
#include <vector>
#include <stdint.h>
#include <algorithm>
#include <chrono>


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

enum HashAlgo {
    HashJenkins,
    HashDjb
};

void printHelp() {
    printf("USAGE:\n");
    printf("  chd [-h, v] [-l <lambda>] [-a <alpha>] [-o <outFile>] \n"
           "      [-f <hashFunc>] <keysFile>\n");
    printf("OPTIONS:\n");
    printf("  -h             Print this help and exit\n");
    printf("  -f <hashFunc>  Hash function to use. Options are: jenkins, djb\n");
    printf("  -l <lambda>    Load factor. The same variable as in the paper.\n");
    printf("  -a <alpha>     Leeway. The same variable as in the CHD-paper.\n");
    printf("  -o <outFile>   Output file containing computed hash function.\n");
    printf("  -v             Be verbose while computing hash functions.\n");
    printf("  <keysFile>     Vocabulary file [mandatory!]\n");
    printf("Format of <keysFile>:\n");
    printf("Each line contains a word that's part of the vocabulary. Currently\n"
           "only plain ascii strings are supported.\n");
    printf("Format of <outFile>:\n");
    printf("Each line contains bucket-id and it's init value for hash function\n");
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

void hashJenkins(const char* key, size_t len, uint32_t init,
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

void hashDjb(const char* key, size_t len, uint32_t init,
             uint32_t& a, uint32_t& b, uint32_t& c) {
    a = b = c = init;
    for(size_t i=0;i<len;++i)
        c += (c << 5) + key[i];
}

void hashWord(HashAlgo algo, const char* key, size_t len, uint32_t init,
              uint32_t& a, uint32_t& b, uint32_t& c) {
    if(algo == HashJenkins)
        hashJenkins(key, len, init, a, b, c);
    else if(algo == HashDjb)
        hashDjb(key, len, init, a, b, c);
    else
        ASSERT(false, "Invalid hash function!");
}

int hashMod(HashAlgo algo, const char* str, size_t len, uint32_t initval,
            int mod) {
    uint32_t a, b, c;
    hashWord(algo, str, len, initval, a, b, c);
    return c % mod;
}

struct Bucket {
    int bId;
    std::vector<int> keyIds;
    uint32_t lval;
};

bool compare(const Bucket& a, const Bucket& b) {
    return a.keyIds.size() < b.keyIds.size();
}

typedef std::vector<Bucket> Buckets;

Buckets getBuckets(const std::vector<std::string>& keys, int R, HashAlgo algo) {
    Buckets ret(R);
    int i = 0;
    for(auto& itr : ret) {
        itr.bId = i;
        ++i;
    }
    i = 0;
    for(const auto& k : keys) {
        int bId = hashMod(algo, k.c_str(), k.size(), 0, R);
        ret[bId].keyIds.push_back(i);
        ret[bId].lval = 0U;
        ++i;
    }
    std::sort(ret.begin(), ret.end(), compare);
    return ret;
}

void computePH(Buckets& buckets, bool* T, int n, int m, int r, HashAlgo algo,
               const std::vector<std::string>& keys, bool verbose) {
    for(auto& itr : buckets) {
        if(verbose) {
            printf("  Trying for bucket=%d ...", itr.bId);
            fflush(stdout);
        }
        int len = (int)itr.keyIds.size();
        uint32_t l = 1;
        for(uint32_t l=1;;++l) {
            bool found = true;
            for(auto kid : itr.keyIds) {
                int idx = hashMod(algo, keys[kid].c_str(), keys[kid].size(), l, m);
                if(T[idx]) {
                    found = false;
                    break;
                }
            }
            if(found) {
                itr.lval = l;
                for(auto kid : itr.keyIds) {
                    int idx = hashMod(algo, keys[kid].c_str(), keys[kid].size(), l, m);
                    T[idx] = true;
                }
                if(verbose) {
                    printf(" Found l=%d\n", l);
                    fflush(stdout);
                }
                break;
            }
        }
    }
}

int main(int argc, char** argv) {
    char* keysFile = nullptr;
    char* outFile = nullptr;
    float lambda = 5.f;
    float alpha = 1.f;
    bool verbose = false;
    HashAlgo algo = HashJenkins;
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
        } else if(!strcmp("-o", argv[i])) {
            ++i;
            ASSERT(i < argc, "'-o' expects an argument!");
            outFile = argv[i];
        } else if(!strcmp("-v", argv[i])) {
            verbose = true;
        } else if(!strcmp("-f", argv[i])) {
            ++i;
            ASSERT(i < argc, "'-f' expects an argument!");
            if(!strcmp("jenkins", argv[i])) {
                algo = HashJenkins;
            } else if(!strcmp("djb", argv[i])) {
                algo = HashDjb;
            } else {
                ASSERT(false, "'-f': Invalid hash function '%s'", argv[i]);
            }
        } else {
            keysFile = argv[i];
        }
    }
    ASSERT(keysFile != nullptr, "<keysFile> is mandatory!");
    ASSERT(lambda >= 1.f, "'-l' must be greater than 1.0!");
    ASSERT(alpha >= 0.01f && alpha <= 1.f, "'-a' must be between 0.01 & 1.0!");
    auto keys = readAllKeys(keysFile);
    int n = (int)keys.size();
    printf("Read %d keys from the input file\n", n);
    int r = n / lambda;
    int m = n / alpha;
    printf("Getting buckets r=%d m=%d n=%d...\n", r, m, n);
    auto buckets = getBuckets(keys, r, algo);
    printf("Initializing T array...\n");
    bool* T = new bool[m];
    for(int i=0;i<m;++i) {
        T[i] = false;
    }
    printf("Creating CHD...\n");
    auto start = std::chrono::steady_clock::now();
    computePH(buckets, T, n, m, r, algo, keys, verbose);
    auto end = std::chrono::steady_clock::now();
    auto diff = end - start;
    printf("PH generation (r,m,n=%d,%d,%d) took %lfs\n", r, m, n,
           std::chrono::duration<double>(diff).count());
    delete [] T;
    if(outFile != nullptr) {
        printf("Storing perfect hash...\n");
        FILE* fp = fopen(outFile, "w");
        ASSERT(fp != nullptr, "Failed to open out file '%s'!", outFile);
        for(auto& itr : buckets) {
            fprintf(fp, "%d %u\n", itr.bId, itr.lval);
        }
        fclose(fp);
    }
    return 0;
}
