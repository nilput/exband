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
    return p;
}
/*
	In all cases the input should start with a leading '/' (Precondition)
	In all cases the output should have a leading '/'
	in case the input tries to path traverse outside of root, then the '..' are removed
	in case the input tries to path traverse inside root, then the '..' are logically resolved
	in case the input contains redundant /././ they are removed
*/
static int
resolve_path_1(char *start, char *end) {
	exb_assert_h(end > start && start[0] == '/', "invalid input to resolve_path(), expected leading slash");
    unsigned short last = '/';
	char *last_slash = start; // last slash written in output
	// skip leading slash
	char *write_at = start + 1;
    char *read_at  = start + 1;
    while (read_at < end) {
        if (*read_at == '/') {
            if (last == (('.' << 8) | '.') && last_slash == (write_at - 3)) {
				// resolve path logically
				last_slash = to_previous_slash(start, last_slash - 1);
				write_at = last_slash + 1;
            }
            else if (last == (('/' << 8) | '.')) {
				// redundant "/./"
                write_at--;
            }
            else if (last_slash != (write_at - 1)){
				// if it's not "//" then add the slash
				last_slash = write_at;
               *(write_at++) = '/';
            }
            last = (last << 8) | '/';
            read_at++;
        }
        while (read_at < end && *read_at != '/') {
            last = (last << 8) | *read_at;
            *(write_at++) = *(read_at++);
        }
    }
	// Check for trailing "/.." or "/."
	if ((write_at - last_slash == 3 && last_slash[1] == '.' && last_slash[2] == '.') ||
		(write_at - last_slash == 2 && last_slash[1] == '.')						   )
	{
		write_at = last_slash + 1;
	}
    *write_at = 0;
    return write_at - start;
}

int exb_resolve_path(char *str, int len) {
    return resolve_path_1(str, str + len);
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
			close(fd);
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

/**
  * Respond to a request with a file's contents.
  * @param rqstate [in, out] - the request to serve the file to
  * @param fs_path [in] - a path to the directory, ending with no slash
  * @param resource_path [in] - a path to the file, starting with a leading slash
**/
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
	exb_resolve_path(buff + fs_path_len, resource_path_len);
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