#ifndef EXB_FILESERV_H
#define EXB_FILESERV_H

int exb_fileserv(struct exb_request_state *rqstate, 
            char *fs_path,
            size_t fs_path_len,
            char *resource_path,
            size_t resource_path_len);
#endif