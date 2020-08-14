
def c_literal(s, as_array=True):
    output = ''
    assert(isinstance(s, bytes))
    first = True
    for ch in s:
        if len(output) and as_array:
            output += ', '
        if ch == b'\\':
            if as_array:
                output += "'\\\\'"
            else:
                output += '\\\\'
        elif ch == b'"':
            if as_array:
                output += "'\"'"
            else:
                output += '\\"'
        elif (ch in b'0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&^*().,<>/[]`~\';{}+-=:?'):
            if as_array:
                if ch == ord(b"'"):
                    output += "'\\''"
                else:
                    output += "'{}'".format(chr(ch))
            else:
                output += chr(ch)
        else:
            esc = '\\x{:02X}'.format(ch)
            assert(len(esc) == 4)
            if as_array:
                output += '0x{:02X}'.format(ch)
            else:
                output += esc
    return output
