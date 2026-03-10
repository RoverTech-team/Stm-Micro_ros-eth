#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_FILE_SIZE (64 * 1024)
#define CORPUS_DIR "corpus"

typedef int (*fuzz_func_t)(const uint8_t *data, size_t len);

typedef struct {
    const char *name;
    fuzz_func_t func;
} fuzzer_entry_t;

extern int LLVMFuzzerTestOneInput_udp(const uint8_t *data, size_t len);
extern int LLVMFuzzerTestOneInput_ethernet(const uint8_t *data, size_t len);
extern int LLVMFuzzerTestOneInput_allocators(const uint8_t *data, size_t len);

static fuzzer_entry_t fuzzers[] = {
    {"udp", LLVMFuzzerTestOneInput_udp},
    {"ethernet", LLVMFuzzerTestOneInput_ethernet},
    {"allocators", LLVMFuzzerTestOneInput_allocators},
    {NULL, NULL}
};

static int test_count = 0;
static int pass_count = 0;
static int fail_count = 0;

static uint8_t *read_file(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (file_size < 0 || file_size > MAX_FILE_SIZE) {
        fclose(f);
        return NULL;
    }
    
    uint8_t *data = malloc(file_size);
    if (!data) {
        fclose(f);
        return NULL;
    }
    
    size_t bytes_read = fread(data, 1, file_size, f);
    fclose(f);
    
    if (bytes_read != (size_t)file_size) {
        free(data);
        return NULL;
    }
    
    *size = file_size;
    return data;
}

static int run_corpus_file(fuzzer_entry_t *fuzzer, const char *filepath) {
    size_t size;
    uint8_t *data = read_file(filepath, &size);
    
    if (!data) {
        printf("  [SKIP] %s (cannot read file)\n", filepath);
        return 0;
    }
    
    test_count++;
    
    int result = fuzzer->func(data, size);
    free(data);
    
    if (result == 0) {
        pass_count++;
        printf("  [PASS] %s (%zu bytes)\n", filepath, size);
        return 0;
    } else {
        fail_count++;
        printf("  [FAIL] %s (%zu bytes) - returned %d\n", filepath, size, result);
        return 1;
    }
}

static int run_corpus_dir(fuzzer_entry_t *fuzzer, const char *corpus_dir) {
    DIR *dir = opendir(corpus_dir);
    if (!dir) {
        printf("Warning: Cannot open corpus directory: %s\n", corpus_dir);
        return 0;
    }
    
    int failures = 0;
    struct dirent *entry;
    
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        
        char filepath[1024];
        snprintf(filepath, sizeof(filepath), "%s/%s", corpus_dir, entry->d_name);
        
        struct stat st;
        if (stat(filepath, &st) != 0) {
            continue;
        }
        
        if (S_ISREG(st.st_mode)) {
            if (run_corpus_file(fuzzer, filepath) != 0) {
                failures++;
            }
        }
    }
    
    closedir(dir);
    return failures;
}

static void print_usage(const char *prog) {
    printf("Usage: %s [options] [fuzzer]\n\n", prog);
    printf("Standalone fuzzing test harness (no libFuzzer required)\n\n");
    printf("Fuzzers:\n");
    for (int i = 0; fuzzers[i].name != NULL; i++) {
        printf("  %s\n", fuzzers[i].name);
    }
    printf("\nOptions:\n");
    printf("  -h, --help          Show this help message\n");
    printf("  -a, --all           Run all fuzzers\n");
    printf("  -c, --corpus DIR    Use corpus directory (default: corpus)\n");
    printf("  -f, --file FILE     Run single corpus file\n");
    printf("  -l, --list          List available fuzzers\n");
    printf("  -v, --verbose       Verbose output\n");
    printf("\nExamples:\n");
    printf("  %s udp              Run UDP fuzzer with corpus files\n", prog);
    printf("  %s --all            Run all fuzzers\n", prog);
    printf("  %s -f crash.bin udp Run specific file with UDP fuzzer\n", prog);
}

static void list_fuzzers(void) {
    printf("Available fuzzers:\n");
    for (int i = 0; fuzzers[i].name != NULL; i++) {
        printf("  %s\n", fuzzers[i].name);
    }
}

static fuzzer_entry_t *find_fuzzer(const char *name) {
    for (int i = 0; fuzzers[i].name != NULL; i++) {
        if (strcmp(fuzzers[i].name, name) == 0) {
            return &fuzzers[i];
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    const char *corpus_dir = CORPUS_DIR;
    const char *single_file = NULL;
    int run_all = 0;
    int verbose = 0;
    
    int arg_idx = 1;
    while (arg_idx < argc) {
        if (strcmp(argv[arg_idx], "-h") == 0 || strcmp(argv[arg_idx], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[arg_idx], "-l") == 0 || strcmp(argv[arg_idx], "--list") == 0) {
            list_fuzzers();
            return 0;
        } else if (strcmp(argv[arg_idx], "-a") == 0 || strcmp(argv[arg_idx], "--all") == 0) {
            run_all = 1;
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "-v") == 0 || strcmp(argv[arg_idx], "--verbose") == 0) {
            verbose = 1;
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "-c") == 0 || strcmp(argv[arg_idx], "--corpus") == 0) {
            if (arg_idx + 1 >= argc) {
                fprintf(stderr, "Error: --corpus requires directory argument\n");
                return 1;
            }
            corpus_dir = argv[++arg_idx];
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "-f") == 0 || strcmp(argv[arg_idx], "--file") == 0) {
            if (arg_idx + 1 >= argc) {
                fprintf(stderr, "Error: --file requires file argument\n");
                return 1;
            }
            single_file = argv[++arg_idx];
            arg_idx++;
        } else if (argv[arg_idx][0] == '-') {
            fprintf(stderr, "Error: Unknown option: %s\n", argv[arg_idx]);
            print_usage(argv[0]);
            return 1;
        } else {
            break;
        }
    }
    
    printf("=== Standalone Fuzzing Test Harness ===\n\n");
    
    int total_failures = 0;
    
    if (run_all) {
        for (int i = 0; fuzzers[i].name != NULL; i++) {
            printf("Running fuzzer: %s\n", fuzzers[i].name);
            printf("Corpus directory: %s\n\n", corpus_dir);
            
            int prev_test_count = test_count;
            int prev_pass_count = pass_count;
            int prev_fail_count = fail_count;
            
            if (single_file) {
                if (run_corpus_file(&fuzzers[i], single_file) != 0) {
                    total_failures++;
                }
            } else {
                if (run_corpus_dir(&fuzzers[i], corpus_dir) != 0) {
                    total_failures++;
                }
            }
            
            printf("\n  Results for %s: %d tests, %d passed, %d failed\n\n",
                   fuzzers[i].name,
                   test_count - prev_test_count,
                   pass_count - prev_pass_count,
                   fail_count - prev_fail_count);
        }
    } else {
        const char *fuzzer_name = NULL;
        
        if (arg_idx < argc) {
            fuzzer_name = argv[arg_idx];
        } else if (single_file) {
            fprintf(stderr, "Error: Must specify fuzzer name when using --file\n");
            return 1;
        } else {
            fprintf(stderr, "Error: Must specify fuzzer name or use --all\n\n");
            print_usage(argv[0]);
            return 1;
        }
        
        fuzzer_entry_t *fuzzer = find_fuzzer(fuzzer_name);
        if (!fuzzer) {
            fprintf(stderr, "Error: Unknown fuzzer: %s\n", fuzzer_name);
            list_fuzzers();
            return 1;
        }
        
        printf("Running fuzzer: %s\n", fuzzer_name);
        printf("Corpus directory: %s\n\n", corpus_dir);
        
        if (single_file) {
            if (run_corpus_file(fuzzer, single_file) != 0) {
                total_failures++;
            }
        } else {
            total_failures = run_corpus_dir(fuzzer, corpus_dir);
        }
        
        printf("\n");
    }
    
    printf("=== Summary ===\n");
    printf("Total tests:  %d\n", test_count);
    printf("Passed:       %d\n", pass_count);
    printf("Failed:       %d\n", fail_count);
    
    if (fail_count > 0) {
        printf("\nRESULT: FAIL\n");
        return 1;
    } else {
        printf("\nRESULT: PASS\n");
        return 0;
    }
}
