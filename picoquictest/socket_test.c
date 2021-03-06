/*
* Author: Christian Huitema
* Copyright (c) 2017, Private Octopus, Inc.
* All rights reserved.
*
* Permission to use, copy, modify, and distribute this software for any
* purpose with or without fee is hereby granted, provided that the above
* copyright notice and this permission notice appear in all copies.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL Private Octopus, Inc. BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "../picoquic/picosocks.h"
#include "../picoquic/util.h"

static int socket_ping_pong(SOCKET_TYPE fd, struct sockaddr* server_addr, int server_address_length,
    picoquic_server_sockets_t* server_sockets)
{
    int ret = 0;
    uint64_t current_time = picoquic_current_time();
    uint8_t message[1440];
    uint8_t buffer[1536];
    int bytes_sent = 0;
    int bytes_recv = 0;
    struct sockaddr_storage addr_from;
    socklen_t from_length;
    struct sockaddr_storage addr_dest;
    socklen_t dest_length;
    unsigned long dest_if;
    struct sockaddr_storage addr_back;
    socklen_t back_length;

    for (size_t i = 0; i < sizeof(message);) {
        for (int j = 0; j < 64 && i < sizeof(message); j += 8, i++) {
            message[i++] = (uint8_t)(current_time >> j);
        }
    }

    /* send from client to sever address */
    bytes_sent = sendto(fd, (const char*)&message, sizeof(message), 0, server_addr, server_address_length);

    if (bytes_sent != (int)sizeof(message)) {
        ret = -1;
    }

    /* perform select at server */
    if (ret == 0) {
        memset(buffer, 0, sizeof(buffer));
        from_length = (socklen_t)sizeof(struct sockaddr_storage);

        bytes_recv = picoquic_select(server_sockets->s_socket, PICOQUIC_NB_SERVER_SOCKETS,
            &addr_from, &from_length, &addr_dest, &dest_length, &dest_if,
            buffer, sizeof(buffer), 1000000, &current_time);

        if (bytes_recv != bytes_sent) {
            ret = -1;
        }
    }

    /* Convert message using XOR  and send to address from which the message was received */
    if (ret == 0) {
        for (int i = 0; i < bytes_recv; i++) {
            buffer[i] ^= 0xFF;
        }

        if (picoquic_send_through_server_sockets(server_sockets,
                (struct sockaddr*)&addr_from, from_length,
                (struct sockaddr*)&addr_dest, dest_length, dest_if,
                (char*)buffer, bytes_recv)
            != bytes_recv) {
            ret = -1;
        }
    }

    /* perform select at client */
    if (ret == 0) {
        memset(buffer, 0, sizeof(buffer));

        back_length = (socklen_t)sizeof(addr_back);
        bytes_recv = picoquic_select(&fd, 1,
            &addr_back, &back_length, NULL, NULL, NULL,
            buffer, sizeof(buffer), 1000000, &current_time);

        if (bytes_recv != bytes_sent) {
            ret = -1;
        } else {
            /* Check that the message matches what was sent initially */

            for (int i = 0; ret == 0 && i < bytes_recv; i++) {
                if (message[i] != (buffer[i] ^ 0xFF)) {
                    ret = -1;
                }
            }
        }
    }

    return ret;
}

static int socket_test_one(char const* addr_text, int server_port, int should_be_name,
    picoquic_server_sockets_t* server_sockets)
{
    int ret = 0;
    struct sockaddr_storage server_address;
    int server_address_length;
    int is_name;
    SOCKET_TYPE fd = INVALID_SOCKET;

    /* Resolve the server address -- check the "is_name" property */
    ret = picoquic_get_server_address(addr_text, server_port, &server_address, &server_address_length, &is_name);

    if (ret == 0) {
        if (is_name != should_be_name) {
            ret = -1;
        } else {
            fd = socket(server_address.ss_family, SOCK_DGRAM, IPPROTO_UDP);
            if (fd == INVALID_SOCKET) {
                ret = -1;
            } else {
                ret = socket_ping_pong(fd, (struct sockaddr*)&server_address, server_address_length, server_sockets);
            }

            SOCKET_CLOSE(fd);
        }
    }

    return ret;
}

int socket_test()
{
    int ret = 0;
    int test_port = 12345;
    picoquic_server_sockets_t server_sockets;
#ifdef _WINDOWS
    WSADATA wsaData;

    if (WSA_START(MAKEWORD(2, 2), &wsaData)) {
        DBG_PRINTF("Cannot init WSA\n");
        ret = -1;
    }
#endif
    /* Open server sockets */
    ret = picoquic_open_server_sockets(&server_sockets, test_port);

    if (ret == 0) {
        /* For a series of server addresses, do a ping pong test */
        if (socket_test_one("127.0.0.1", test_port, 0, &server_sockets) != 0) {
            ret = -1;
        } else if (socket_test_one("::1", test_port, 0, &server_sockets) != 0) {
            ret = -1;
        } else if (socket_test_one("localhost", test_port, 1, &server_sockets) != 0) {
            ret = -1;
        }
        /* Close the sockets */
        picoquic_close_server_sockets(&server_sockets);
    }

    return ret;
}
