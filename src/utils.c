#include "donkey.h"

int bufferstream(struct commands_s *data, unsigned char *buffer, int iflag){
    int pos = 0;
    if(iflag == STOPD){
        *(int *)(buffer + pos) = data->cmd; pos += sizeof(int);
        return pos;
    }
    *(int *)(buffer + pos) = data->cmd; pos += sizeof(int);
    memcpy(buffer + pos, data->args.cratename, 16); pos += 16;
    if(iflag == CREATE){
        memcpy(buffer + pos, data->args.imagename, 16); pos += 16;
    }
    return pos;
}