#include "zdtmtst.h"
#include "sysctl.h"

const char *test_doc = "static test for IP6/ICMP socket\n";
const char *test_author = "समीर सिंह Sameer Singh <lumarzeli30@gmail.com>\n";

/* Description:
 * Send a ping to localhost using IP6/ICMP socket
 */

#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/icmp6.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <netdb.h>

#define PACKET_SIZE  64
#define RECV_TIMEOUT 1

static int echo_id = 1234;

int main(int argc, char **argv)
{
	int ret, sock, seq = 0, recv_len = 0;
	char packet[PACKET_SIZE], recv_packet[PACKET_SIZE];

	struct timeval tv;
	struct icmp6_hdr icmp_header, *icmp_reply;
	struct sockaddr_in6 addr, recv_addr;
	socklen_t addr_len;

	test_init(argc, argv);

	// Allow GIDs 0-58468 to open an unprivileged ICMP socket
	if (sysctl_write_str("/proc/sys/net/ipv4/ping_group_range", "0 58468")) {
		pr_perror("sysctl_write_str() failed");
		return 1;
	}

	sock = socket(PF_INET6, SOCK_DGRAM, IPPROTO_ICMPV6);
	if (sock < 0) {
		pr_perror("Can't create socket");
		return 1;
	}

	tv.tv_sec = RECV_TIMEOUT;
	tv.tv_usec = 0;
	if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
		pr_perror("Can't set socket option");
		return 1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin6_family = AF_INET6;
	inet_pton(AF_INET6, "::1", &addr.sin6_addr);

	memset(&icmp_header, 0, sizeof(icmp_header));
	icmp_header.icmp6_type = ICMP6_ECHO_REQUEST;
	icmp_header.icmp6_code = 0;
	icmp_header.icmp6_id = echo_id;
	icmp_header.icmp6_seq = seq;

	memcpy(packet, &icmp_header, sizeof(icmp_header));
	memset(packet + sizeof(icmp_header), 0xa5,
	       PACKET_SIZE - sizeof(icmp_header));

	test_daemon();
	test_waitsig();

	ret = sendto(sock, packet, PACKET_SIZE, 0,
		     (struct sockaddr *)&addr, sizeof(addr));

	if (ret < 0) {
		pr_perror("Can't send");
		return 1;
	}

	addr_len = sizeof(recv_addr);

	recv_len = recvfrom(sock, recv_packet, sizeof(recv_packet), 0,
			    (struct sockaddr *)&recv_addr, &addr_len);

	if (recv_len < 0) {
		pr_perror("Can't recv");
		return 1;
	}

	icmp_reply = (struct icmp6_hdr *)recv_packet;

	if (icmp_reply->icmp6_type != ICMP6_ECHO_REPLY) {
		fail("Got no ICMP_ECHO_REPLY");
		return 1;
	}

	close(sock);
	pass();
	return 0;
}
