#include "http_request.h"
#include "http_response.h"
#include "exb_fileserv.h"
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>


static inline char *to_previous_slash(char *start, char *p) {
    if (p < start)
        return start;
    for (; p >= start && (*p != '/'); p--); 
    return p + 1;
}
static char *
resolve_path_1(char *start, char *ende, char *write_at) {
    unsigned short last = '/';
    char *p = start;
    while (p < ende) {
        if (*p == '/') {
            if (last == (('.' << 8) | '.')) {
                write_at = to_previous_slash(start, write_at - 4);
            }
            else if (last == (('/' << 8) | '.')) {
                write_at--;
            }
            else if ((last & 0xff) != '/'){
               *(write_at++) = '/';
            }
            last = (last << 8) | '/';
            p++;
        }
        while (p < ende && *p != '/') {
            last = (last << 8) | *p;
            *(write_at++) = *(p++);
        }
    }
    char *previous_slash = to_previous_slash(start, write_at - 1);
    if (write_at - previous_slash == 2 && previous_slash[0] == '.' && previous_slash[1] == '.')
        write_at = to_previous_slash(start, previous_slash - 2);
    else if (write_at - previous_slash == 1 && *previous_slash == '.')
        write_at -= 1;
    *write_at = 0;
    return start;
}

char *resolve_path(char *str, int len) {
    return resolve_path_1(str, str + len, str);
}


static int fileserv_handler(void *unused, struct exb_request_state *rqstate, int reason) {
	EXB_UNUSED(unused);
	struct exb_msg *u = exb_request_get_userdata(rqstate);
	size_t *remaining_in_file = &u->u.iiz.sz;
	int fd = u->u.iiz.arg1;
	char *dest = NULL;
	size_t dest_size;
	exb_response_append_body_buffer_ptr(rqstate, &dest, &dest_size);
	exb_assert_h(dest_size > *remaining_in_file, "assumption failed, buffer insufficent for read()");
	
	ssize_t read_bytes;
	again:
	errno = 0;
	read_bytes = read(fd, dest, *remaining_in_file);
	if (read_bytes >= 0) {
		(*remaining_in_file) -= read_bytes;
		if (read_bytes == 0 && (*remaining_in_file) > 0) {
			close(fd);
			exb_response_on_error_mid_transfer(rqstate);
			return EXB_READ_ERR;
		}
		exb_response_append_body_buffer_wrote(rqstate, read_bytes);
		
		if (*remaining_in_file == 0) {
			exb_response_end(rqstate);
		}
	}
	else if (errno == EINTR) {
		goto again;
	}
	else {
		close(fd);
		exb_response_on_error_mid_transfer(rqstate);
		return EXB_READ_ERR;
	}
	
	return EXB_OK;
}

//Doesn't own fs_path nor resource_path
int exb_fileserv(struct exb_request_state *rqstate, 
				char *fs_path,
				size_t fs_path_len,
				char *resource_path,
				size_t resource_path_len) 
{
	
	char buff[512];
	if (sizeof buff <= (fs_path_len + resource_path_len)) {
		return exb_response_return_error(rqstate, 400, "invalid request");
	}
	memcpy(buff, fs_path, fs_path_len);
	memcpy(buff + fs_path_len, resource_path, resource_path_len);
	buff[fs_path_len + resource_path_len] = 0;
	resolve_path(buff + fs_path_len, resource_path_len);
	errno = 0;
	int fd = open(buff, O_RDONLY);
	if (fd == -1) {
		switch (errno) {
			case ENOENT:
				return exb_response_return_error(rqstate, 404, "not found");
			case EACCES:
			case EFAULT:
				return exb_response_return_error(rqstate, 403, "forbidden");
			default:
				return exb_response_return_error(rqstate, 500, "server error");
		}
	} else {
		struct stat st;
		fstat(fd, &st);
#ifdef FD_CLOEXEC
		fcntl(fd, F_SETFD, FD_CLOEXEC);
#endif
		if (!S_ISREG(st.st_mode)) {
			close(fd);
			return exb_response_return_error(rqstate, 404, "not found");
		} else {
			struct exb_msg *u = exb_request_get_userdata(rqstate);
			u->u.iiz.sz = st.st_size;
			u->u.iiz.arg1 = fd;
			/*
			TODO: FIX THIS, EITHER USE SENDFILE OR STREAMS THE RESPONSE,
			CURRENTLY THIS STORES THE WHOLE FILE IN MEMORY
			*/
			int rv = exb_response_body_buffer_ensure(rqstate, st.st_size + 1);
			if (rv != EXB_OK) {
				close(fd);
				return exb_response_return_error(rqstate, 500, "internal error");
			}
			exb_request_state_change_handler(rqstate, fileserv_handler, NULL);
		}
	}

	return EXB_OK;
}