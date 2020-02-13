num = 0
def handler(request):
    global num
    request.append_body('Hello {}'.format(num))
    num += 1
