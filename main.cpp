#include "server.h"
#include "epoll.h"

int main() {
    epoll er;
    server f(50000, er);
	er.execute();
    return 0;
}
