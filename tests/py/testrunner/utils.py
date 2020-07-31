def exe_name(executable_name):
    return executable_name

def format_headers(headers):
    ''.join(map(lambda x: '{}: {}\n'.format(x[0], x[1]), headers))