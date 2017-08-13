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

#include "../picoquic/picoquic_internal.h"
#include <stdlib.h>
#include <malloc.h>

/* 
 * Cnx creation unit test
 * - Create QUIC context
 * - Create a set of connections, with variations:
 * - IPv4 or IPv6 address
 * - Different ports
 * - either no connection ID or a connection ID.
 *
 *  - Verify that all these connections can be retrieved using their
 *    registered attributes.
 *  - Verify that a non registered connection can be retrieved.
 *
 *  - Delete connections first-middle-last.
 *  - Verify that deleted connections cannot be retrieved, and the others can.
 *
 *  - delete QUIC context.
 */

int cnxcreation_test()
{
    int ret = 0;
    picoquic_quic_t * quic = NULL;
    picoquic_cnx_t * test_cnx[5] = { NULL,  NULL, NULL, NULL, NULL };
    struct sockaddr_in test4[4];
    struct sockaddr_in6 test6[2];
    const uint8_t test_ipv4[4] = { 192, 0, 2, 0 };
    const uint8_t test_ipv6[16] = { 0x20, 0x01,0x0D, 0xB8, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0x01 };
    const uint64_t test_cnx_id[5] = { 0, 1, 2, 3, 4 };
    struct sockaddr * test_cnx_addr[5] = { 
        (struct sockaddr *) &test4[0],
        (struct sockaddr *) &test4[1], 
        (struct sockaddr *) &test4[2], 
        (struct sockaddr *) &test6[0], 
        (struct sockaddr *) &test6[1] };


    /* Create QUIC context */
    quic = picoquic_create(8, NULL, NULL);
    if (quic == NULL)
    {
        ret = -1;
    }

    /*
     * Initialize the sockaddr values
     */
    for (int i = 0; i < 4; i++)
    {
        memset(&test4[i], 0, sizeof(test4[i]));
        test4[i].sin_family = AF_INET;
        test4[i].sin_addr.S_un.S_un_b.s_b1 = test_ipv4[0];
        test4[i].sin_addr.S_un.S_un_b.s_b2 = test_ipv4[1];
        test4[i].sin_addr.S_un.S_un_b.s_b3 = test_ipv4[2];
        test4[i].sin_addr.S_un.S_un_b.s_b4 = (i == 0) ? 1 : 2;
        test4[i].sin_port = 1000 + i;
    }

    for (int i = 0; i < 2; i++)
    {
        memset(&test6[i], 0, sizeof(test6[i]));
        test6[i].sin6_family = AF_INET6;
        for (int j = 0; j < 15; j++)
        {
            test6[i].sin6_addr.u.Byte[j] = test_ipv6[j];
        }
        test6[i].sin6_addr.u.Byte[15] = i + 1;
        test6[i].sin6_port = 1000 + i;
    }

    /*
    * Create a set of connections, with variations :
    * -IPv4 or IPv6 address
    * -Different ports
    * -either no connection ID or a connection ID.
    */

    for (int i = 0; ret == 0 && i < 5; i++)
    {
        test_cnx[i] = picoquic_create_cnx(quic, test_cnx_id[i], test_cnx_addr[i], 0, 0);
        if (test_cnx[i] == NULL)
        {
            ret = -1;
        }
    }

 
    /*
     *  -Verify that all these connections can be retrieved using their
     *    registered attributes.
     */
    for (int i = 0; ret == 0 && i < 5; i++)
    {
        picoquic_cnx_t * cnx = picoquic_cnx_by_net(quic, test_cnx_addr[i]);

        if (cnx == NULL)
        {
            ret = -1;
        }
    }

    for (int i = 0; ret == 0 && i < 5; i++)
    {
        picoquic_cnx_t * cnx = picoquic_cnx_by_id(quic, test_cnx_id[i]);

        if (test_cnx_id[i] == 0 && cnx != NULL)
        {
            ret = -1;
        }
        else if (test_cnx_id[i] != 0 && cnx == NULL)
        {
            ret = -1;
        }
    }

     /*
      *  -Verify that a non registered connection cannot be retrieved.
      */

    if (ret == 0)
    {
        picoquic_cnx_t * cnx = picoquic_cnx_by_id(quic, 123456789);
        if (cnx != NULL)
        {
            ret = -1;
        }
    }

    if (ret == 0)
    {
        picoquic_cnx_t * cnx = picoquic_cnx_by_net(quic, (struct sockaddr*) &test4[3]);
        if (cnx != NULL)
        {
            ret = -1;
        }
    }

    /* Delete connections first - middle - last. */
    for (int i = 0; ret == 0 && i < 5; i += 2)
    {
        picoquic_delete_cnx(test_cnx[i]);
        test_cnx[i] = NULL;
    }

    /* Verify that deleted connections cannot be retrieved, and the others can. */
    for (int i = 0; ret == 0 && i < 5; i++)
    {
        if (test_cnx_id[i] != 0)
        {
            picoquic_cnx_t * cnx = picoquic_cnx_by_id(quic, test_cnx_id[i]);

            if (cnx != NULL && (i & 1) == 0)
            {
                ret = -1;
            }
            else if (cnx == NULL && (i & 1) != 0)
            {
                ret = -1;
            }
        }
    }

    for (int i = 0; ret == 0 && i < 5; i++)
    {
        picoquic_cnx_t * cnx = picoquic_cnx_by_net(quic, test_cnx_addr[i]);

        if (cnx != NULL && (i & 1) == 0)
        {
            ret = -1;
        }
        else if (cnx == NULL && (i & 1) != 0)
        {
            ret = -1;
        }
    }

    /* delete QUIC context. */
    if (ret == 0)
    {
        picoquic_free(quic);
    }

    return ret;
}