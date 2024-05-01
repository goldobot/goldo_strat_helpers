#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <sys/select.h>
#include <errno.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <linux/if_link.h>


char *mcast_addr = "231.10.66.211";
int mcast_port = 4200;

//char *out_intf_addr = "192.168.0.76";
char *out_intf_addr = "127.0.0.1";

char * new_argv[128];

char *get_192_168_0_intf()
{
  struct ifaddrs *ifaddr;
  int family, s;
  char host[NI_MAXHOST];
  char *my_intf_addr = "127.0.0.1";

  if (getifaddrs(&ifaddr) == -1) {
    printf(" ERROR getifaddrs()\n");
    return "127.0.0.1";
  }

  /* Walk through linked list, maintaining head pointer so we
     can free list later */
  for (struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == NULL) continue;

    family = ifa->ifa_addr->sa_family;

    /* Display interface name and family (including symbolic
       form of the latter for the common families) */

#if 0
    printf("%-8s %s (%d)\n",
           ifa->ifa_name,
           (family == AF_PACKET) ? "AF_PACKET" :
           (family == AF_INET) ? "AF_INET" :
           (family == AF_INET6) ? "AF_INET6" : "???",
           family);
#endif

    /* For an AF_INET interface address, display the address */

    if (family == AF_INET)
    {
      s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                      host, NI_MAXHOST,
                      NULL, 0, NI_NUMERICHOST);
      if (s != 0)
      {
        printf(" ERROR getnameinfo()\n");
      }

#if 0
      printf("\t\taddress: <%s>\n", host);
#endif

      if (strncmp(host,"192.168.0.",10)==0)
      {
        my_intf_addr = strdup(host);
      }
    }
    else if (family == AF_PACKET && ifa->ifa_data != NULL)
    {
      struct rtnl_link_stats *stats = ifa->ifa_data;

#if 0
      printf("\t\ttx_packets = %10u; rx_packets = %10u\n"
             "\t\ttx_bytes   = %10u; rx_bytes   = %10u\n",
             stats->tx_packets, stats->rx_packets,
             stats->tx_bytes, stats->rx_bytes);
#endif
    }
  }

  freeifaddrs(ifaddr);

  return my_intf_addr;
}

int main(int argc, char **argv)
{
  int err=0;
  int OtoA [2];
  int EtoA [2];

  if (argc<2)
  {
    printf ("usage : %s <prog> [<args..>]\n", argv[0]);
    exit(-1);
  }

  out_intf_addr = get_192_168_0_intf();
  printf ("out_intf_addr = %s\n", out_intf_addr);

  for (int i=0; i<(argc-1); i++)
  {
    new_argv[i] = argv[i+1];
  }

  pipe(OtoA);
  pipe(EtoA);

  int fd1,fd2;
  fd1=fork();
  if(fd1>0)
  {
    /* HELPER PROCESS */

    char buffer[4096];
    char outbuf[4096];

    fd_set readfds;
    struct timeval select_timeout;

    int stop=0;

    int snd_sock=0;
    int val=0;
    ssize_t nbytes;
    int ret=0;

    struct sockaddr_in dest_addr;
    unsigned int a0,a1,a2,a3;

    snd_sock = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
    if (snd_sock<0)
    {
      printf (" ERROR: socket()\n");
    }

    val = 17;
    ret = setsockopt(snd_sock, IPPROTO_IP, IP_MULTICAST_TTL, &val, sizeof(val));
    if (ret<0)
    {
      printf (" ERROR: setsockopt(IP_MULTICAST_TTL)\n");
      close(snd_sock);
      snd_sock=-1;
    }

    /* don't fragment option */
    val = 0;
    ret = setsockopt(snd_sock, IPPROTO_IP, 10, &val, sizeof(val));
    if (ret<0)
    {
      printf (" ERROR: setsockopt(IP_DONT_FRAG(10)\n");
      close(snd_sock);
      snd_sock=-1;
    }

    /* output interface option */
    if (out_intf_addr != NULL)
    {
      unsigned int out_intf_addr_hex = 0;
      //out_intf_addr_hex = 0x0c01a8c0;
      sscanf (out_intf_addr,"%d.%d.%d.%d",&a3,&a2,&a1,&a0);
      out_intf_addr_hex = (a0<<24) | (a1<<16) | (a2<<8) | a3;
      //printf ("out_intf_addr_hex = %.8x\n", out_intf_addr_hex);
      ret = setsockopt(snd_sock, IPPROTO_IP, IP_MULTICAST_IF, &out_intf_addr_hex, sizeof(out_intf_addr_hex));
      if (ret<0)
      {
        printf (" ERROR: setsockopt(IP_MULTICAST_IF)\n");
        close(snd_sock);
        snd_sock=-1;
      }
    }
    else
    {
      printf (" no output intf. closing multicast socket\n");
      close(snd_sock);
      snd_sock=-1;
    }

    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(mcast_port);
    sscanf (mcast_addr,"%d.%d.%d.%d",&a3,&a2,&a1,&a0);
    dest_addr.sin_addr.s_addr = (a0<<24) | (a1<<16) | (a2<<8) | a3;

    //printf ("a0 = %x, a1 = %x, a2 = %x, a3 = %x\n", a0, a1, a2, a3);
    //printf ("dest_addr.sin_addr.s_addr = %.8x\n", dest_addr.sin_addr.s_addr);

    close(OtoA[1]);
    //dup2(OtoA[0], STDIN_FILENO);

    /* program running in process A (wrapper) */
    //while ((nbytes = read(STDIN_FILENO, buffer, sizeof(buffer))) > 0)
    while (!stop)
    {
      int i=0,j=0;
      int max_fd = OtoA[0]>EtoA[0]?OtoA[0]:EtoA[0];

      FD_ZERO(&readfds);
      FD_SET(OtoA[0], &readfds);
      FD_SET(EtoA[0], &readfds);

      select_timeout.tv_sec = 1;
      select_timeout.tv_usec = 100 * 1000;

      /* attente d'input.. */
      ret = select(max_fd + 1, &readfds, NULL, NULL, &select_timeout);
      if (ret == 0)
      {
        /* timeout */
        continue;
      }
      else if (ret < 0)
      {
        if (errno == EINTR)
        {
          continue;
        }
        else
        {
          printf("select error");
        }
      }

      /* lecture de l'input */
      if (FD_ISSET(OtoA[0], &readfds) )
      {
        nbytes = read(OtoA[0], buffer, sizeof(buffer));
      }
      else if (FD_ISSET(EtoA[0], &readfds) )
      {
        nbytes = read(EtoA[0], buffer, sizeof(buffer));
      }
      else
      {
        nbytes = 0;
      }

      /* traitement */
      while(i<nbytes)
      {
        outbuf[j]=buffer[i];
        if (buffer[i]==0x0a)
        {
          write(STDOUT_FILENO, outbuf, j+1);
          if (snd_sock>=0)
          {
            sendto(snd_sock,outbuf,j+1,0,(const struct sockaddr*)&dest_addr,sizeof(dest_addr));
          }
          j=0;
        }
        else if (buffer[i]==0x0d)
        {
          write(STDOUT_FILENO, outbuf, j+1);
          if (snd_sock>=0)
          {
            sendto(snd_sock,outbuf,j+1,0,(const struct sockaddr*)&dest_addr,sizeof(dest_addr));
          }
          j=0;
        }
        else if (buffer[i]==0x00)
        {
          write(STDOUT_FILENO, outbuf, j+1);
          if (snd_sock>=0)
          {
            sendto(snd_sock,outbuf,j+1,0,(const struct sockaddr*)&dest_addr,sizeof(dest_addr));
          }
          j=0;
        }
        else
        {
          j++;
        }
        i++;
      }

    }
  }
  else
  {
    /* PAYLOAD PROCESS */

    close(OtoA[0]);
    close(EtoA[0]);
    dup2(OtoA[1],STDOUT_FILENO);
    dup2(EtoA[1],STDERR_FILENO);

    err=execvp(new_argv[0], new_argv);
    if (err==-1)
    {
      printf ("Cannot execute %s\n", new_argv[0]);
    }
  }

  return 0;
}

