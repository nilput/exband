#ifndef EXB_MSG_H
#define EXB_MSG_H
struct exb_msg {
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
        struct {
            int arg1;
            int arg2;
            size_t sz;
        } iiz;
    } u;
    
};
#endif //EXB_MSG_H