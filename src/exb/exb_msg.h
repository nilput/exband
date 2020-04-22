#ifndef CPB_MSG_H
#define CPB_MSG_H
struct cpb_msg {
    union {
        struct {
            void *argp;
            int arg1;
            int arg2;
        } iip;
        struct {
            int arg1;
            int arg2;
            int arg3;
            int arg4;
        } iiii;
        struct {
            void *argp1;
            void *argp2;
        } pp;
    } u;
    
};
#endif //CPB_MSG_H