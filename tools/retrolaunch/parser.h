#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#define MAX_TOKEN_LEN 255

ssize_t get_token(int fd, char* token, size_t max_len);
int find_token(int fd, const char* token);
