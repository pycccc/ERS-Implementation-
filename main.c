#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <sys/time.h>
#include <errno.h>

#define ICMP_ECHO 8
#define BUFFER_SIZE 1024

//check whether data is completed or not in ICMP packets
unsigned short checksum(void* buf, int len) {
    unsigned short* buffer = buf;
    //to store the sum of 16-bit words
    unsigned int sum = 0;
    unsigned short result;
    // add 16-bit words to the sum
    for (sum = 0; len > 1; len -= 2)
        sum += *buffer++;
    //if number of len is odd
    if (len == 1)    
        sum += *(unsigned char*)buffer;
    //fold the sum to fit into 16 bits by adding the upper and lower 16 bits
    sum = (sum >> 16) + (sum & 0xFFFF);
    // if there was a carry from the previous step, add it to the sum
    sum += (sum >> 16);
    //take complement of the sum to get the final checksum
    result = ~sum;
    return result;
}

// send ICMP echo request
int icmp_request(int sockfd, struct sockaddr_in* dest_addr, int ttl) {
    //to store ICMP packet data
    char buffer[BUFFER_SIZE];
    struct icmp* icmp_hdr = (struct icmp*)buffer;

    memset(buffer, 0, sizeof(buffer));

    icmp_hdr->icmp_type = ICMP_ECHO;
    icmp_hdr->icmp_code = 0;
    icmp_hdr->icmp_id = getpid();   //get current process id
    icmp_hdr->icmp_seq = ttl;  //set ttl for hop
    icmp_hdr->icmp_cksum = checksum(buffer, sizeof(struct icmp));
    //to control the maximum hop limit for the packet
    setsockopt(sockfd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));
    //sent ICMP packet to destination address
    if (sendto(sockfd, buffer, sizeof(struct icmp), 0, (struct sockaddr*)dest_addr, sizeof(*dest_addr)) <= 0) {
        perror("sendto failed");
        return -1;
    }

    return 0;
}

// receive ICMP reply
int icmp_reply(int sockfd, int ttl, char* router_ip) {    
    //to store ICMP reply
    char buffer[BUFFER_SIZE];
    //to store sender address
    struct sockaddr_in sender;
    socklen_t sender_len = sizeof(sender);
    //to set the timeout for receiving data
    struct timeval tv;
    tv.tv_sec = 1;  // set timeout = 1 sec
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));   // receive tv
    //receive from socket
    if (recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&sender, &sender_len) <= 0) {
        //if timeout
        if (errno == EWOULDBLOCK) {
            printf("Request timed out for TTL=%d\n", ttl);
            return -1;
        }//if another problem while receiving data
        else {
            perror("recvfrom failed");
            return -1;
        }
    }
    //convert the sender address to a string format and copy it to router_ip
    strcpy(router_ip, inet_ntoa(sender.sin_addr));
    return 0;
}

int main(int argc, char* argv[]) {
    // Check if the correct number of arguments is provided
    if (argc != 3) {
        printf("usage: %s [hop-distance] [destination IP address]\n", argv[0]);
        return EXIT_FAILURE;
    }
    // turn hop_distance into int
    int hop_distance = atoi(argv[1]);
    char* dst_addr = argv[2];
    int ttl = 1;
    int sockfd;
    char router_ip[INET_ADDRSTRLEN];

    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;   // Set address family to IPv4
    dest_addr.sin_addr.s_addr = inet_addr(dst_addr);   // Convert IP address string to binary format
    // Create a raw socket for sending ICMP packets
    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        perror("socket creation failed");
        return EXIT_FAILURE;
    }
    // Loop until the TTL reaches the specified hop distance
    while (ttl <= hop_distance) {
        // Send an ICMP request with the current TTL value
        if (icmp_request(sockfd, &dest_addr, ttl) == -1) {
            close(sockfd);
            return EXIT_FAILURE;
        }
        // Wait for and process the ICMP reply, storing the router IP if successful
        if (icmp_reply(sockfd, ttl, router_ip) == 0) {
            printf("TTL=%d, IP address=%s\n", ttl, router_ip);
            if (ttl == hop_distance) {
                printf("Found router at %d hops: %s\n", ttl, router_ip);
                break;
            }
        }
        else {
            printf("No response at TTL=%d\n", ttl);
        }
        ttl++;
    }
    close(sockfd);
    return EXIT_SUCCESS;
}