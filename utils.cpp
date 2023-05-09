#include <string.h>
#include <vector>

std::vector <char *> tokenize(char *buff) {
    std::vector <char *> cmd;
    char *token;

    token = strtok(buff, " ");
    while (token) {
        cmd.push_back(token);
        token = strtok(NULL, " ");
    }

    return cmd;
}