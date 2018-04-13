#include "frame.hpp"
#include "control.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

struct BrainHost {
  sockaddr_in addr;
  int socket;  
};

class Brain {
  public:
  Brain();
  BrainHost discover(); 
  int connect(BrainHost&);
  int disconnect();
  void run();
};

