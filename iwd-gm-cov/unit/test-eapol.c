/*
 *
 *  Wireless daemon for Linux
 *
 *  Copyright (C) 2013-2014  Intel Corporation. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <linux/if_ether.h>
#include <ell/ell.h>
#include <fcntl.h> //include for the open function
#include <regex.h>

#include "src/util.h"
#include "src/eapol.h"
#include "src/crypto.h"
#include "src/ie.h"
#include "src/eap.h"
#include "src/eap-private.h"
#include "src/handshake.h"

/* Our nonce to use + its size */
static const uint8_t *snonce;

/* Whether step2 was called with the right info */
static bool verify_step2_called;
/* PTK Handshake 2-of-4 frame we are expected to generate + its size */
static const uint8_t *expected_step2_frame;
static size_t expected_step2_frame_size;

/* Whether step4 was called with the right info */
static bool verify_step4_called;
/* Whether install_tk was called with the right info */
static bool verify_install_tk_called;
static bool verify_install_gtk_called;
/* PTK Handshake 4-of-4 frame we are expected to generate + its size */
static const uint8_t *expected_step4_frame;
static size_t expected_step4_frame_size;

/* Whether GTK step2 was called with the right info */
static bool verify_gtk_step2_called;
/* GTK Handshake 2-of-2 frame we are expected to generate + its size */
static const uint8_t *expected_gtk_step2_frame;
static size_t expected_gtk_step2_frame_size;

/* Authenticator Address */
static const uint8_t *aa;
/* Supplicant Address */
static const uint8_t *spa;

///XXX Global variable for data read from AFL
char * __afl_input_filename = NULL;
uint8_t * __afl_key;
uint8_t  * __afl_key1;
uint8_t  * __afl_key2;
size_t len_frame1;
size_t len_frame2;
size_t len_frame3;
#define BUF_LEN 2048




struct test_handshake_state {
    struct handshake_state super;
    const uint8_t *tk;
    bool handshake_failed;
};

static void test_handshake_state_free(struct handshake_state *hs)
{
    struct test_handshake_state *ths =
    container_of(hs, struct test_handshake_state, super);

    l_free(ths);
}

static struct handshake_state *test_handshake_state_new(uint32_t ifindex)
{
    struct test_handshake_state *ths;

    ths = l_new(struct test_handshake_state, 1);

    ths->super.ifindex = ifindex;
    ths->super.free = test_handshake_state_free;

    return &ths->super;
}



struct eapol_key_data {
    const unsigned char *frame;
    size_t frame_len;
    enum eapol_protocol_version protocol_version;
    uint16_t packet_len;
    enum eapol_descriptor_type descriptor_type;
    enum eapol_key_descriptor_version key_descriptor_version;
    bool key_type:1;
    uint8_t wpa_key_id:2;
    bool install:1;
    bool key_ack:1;
    bool key_mic:1;
    bool secure:1;
    bool error:1;
    bool request:1;
    bool encrypted_key_data:1;
    bool smk_message:1;
    uint16_t key_length;
    uint64_t key_replay_counter;
    uint8_t key_nonce[32];
    uint8_t eapol_key_iv[16];
    uint8_t key_rsc[8];
    uint8_t key_mic_data[16];
    uint16_t key_data_len;
};



typedef struct eapol_key_data ekd;
ekd *ekda;

void create_eapol_key_data_afl(){

    int i;

    ekda =(ekd *) malloc(sizeof(ekd));
    ekda->frame =  __afl_key;
    ekda->frame_len = sizeof(__afl_key);
    ekda->protocol_version = EAPOL_PROTOCOL_VERSION_2001;
    ekda->packet_len = 117;
    ekda->descriptor_type = EAPOL_DESCRIPTOR_TYPE_80211;
    ekda->key_descriptor_version = EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_SHA1_AES;
    ekda->key_type = true;
    ekda->install = false;
    ekda->key_ack = false;
    ekda->key_mic = true;
    ekda->secure = false;
    ekda->error = false;
    ekda->request = false;
    ekda->encrypted_key_data = false;
    ekda->smk_message = false;
    ekda->key_length = 0;
    ekda->key_replay_counter = 0;
    uint8_t arr1[32] ={0x32, 0x89, 0xe9, 0x15, 0x65, 0x09, 0x4f, 0x32, 0x9a,
                0x9c, 0xd5, 0x4a, 0x4a, 0x09, 0x0d, 0x2c, 0xf4, 0x34,
                0x46, 0x83, 0xbf, 0x50, 0xef, 0xee, 0x36, 0x08, 0xb6,
                0x48, 0x56, 0x80, 0x0e, 0x84 };
    for(i=0; i < sizeof(arr1); i++){
        ekda->key_nonce[i] = arr1[i];
    }
    uint8_t arr2[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    for(i=0; i < sizeof(arr2); i++){
        ekda->eapol_key_iv[i] = arr2[i];
    }

    uint8_t arr3[8]={ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    uint8_t arr4[16]={0x01, 0xc3, 0x1b, 0x82, 0xff, 0x62, 0xa3, 0x79, 0xb0,
                      0x8d, 0xd1, 0xfc, 0x82, 0xc2, 0xf7, 0x68 };

    for(i=0; i < sizeof(arr3); i++){
        ekda->key_rsc[i] = arr3[i];
    }

    for(i=0; i < sizeof(arr4); i++){
        ekda->key_mic_data [i] = arr4[i];
    }

    ekda->key_data_len = 22;

}




/* Random WPA EAPoL frame, using 2001 protocol */
static const unsigned char eapol_key_data_1[] = {
        0x01, 0x03, 0x00, 0x5f, 0xfe, 0x00, 0x89, 0x00, 0x20, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x01, 0xd5, 0xe2, 0x13, 0x9b, 0x1b, 0x1c, 0x1e,
        0xcb, 0xf4, 0xc7, 0x9d, 0xb3, 0x70, 0xcd, 0x1c, 0xea, 0x07, 0xf1, 0x61,
        0x76, 0xed, 0xa6, 0x78, 0x8a, 0xc6, 0x8c, 0x2c, 0xf4, 0xd7, 0x6f, 0x2b,
        0xf7, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00,
};

static struct eapol_key_data eapol_key_test_1 = {
        .frame = eapol_key_data_1,
        .frame_len = sizeof(eapol_key_data_1),
        .protocol_version = EAPOL_PROTOCOL_VERSION_2001,
        .packet_len = 95,
        .descriptor_type = EAPOL_DESCRIPTOR_TYPE_WPA,
        .key_descriptor_version = EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_MD5_ARC4,
        .key_type = true,
        .install = false,
        .key_ack = true,
        .key_mic = false,
        .secure = false,
        .error = false,
        .request = false,
        .encrypted_key_data = false,
        .smk_message = false,
        .key_length = 32,
        .key_replay_counter = 1,
        .key_nonce = { 0xd5, 0xe2, 0x13, 0x9b, 0x1b, 0x1c, 0x1e, 0xcb, 0xf4,
                       0xc7, 0x9d, 0xb3, 0x70, 0xcd, 0x1c, 0xea, 0x07, 0xf1,
                       0x61, 0x76, 0xed, 0xa6, 0x78, 0x8a, 0xc6, 0x8c, 0x2c,
                       0xf4, 0xd7, 0x6f, 0x2b, 0xf7 },
        .eapol_key_iv = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_rsc = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_mic_data = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_data_len = 0,
};

/* Random WPA2 EAPoL frame, using 2004 protocol */
static const unsigned char eapol_key_data_2[] = {
        0x02, 0x03, 0x00, 0x75, 0x02, 0x00, 0x8a, 0x00, 0x10, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x12, 0x6a, 0xce, 0x64, 0xc1, 0xa6, 0x44,
        0xd2, 0x7b, 0x84, 0xe0, 0x39, 0x26, 0x3b, 0x63, 0x3b, 0xc3, 0x74, 0xe3,
        0x29, 0x9d, 0x7d, 0x45, 0xe1, 0xc4, 0x25, 0x44, 0x05, 0x48, 0x05, 0xbf,
        0xe5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x16, 0xdd, 0x14, 0x00, 0x0f, 0xac, 0x04, 0x05, 0xb1, 0xb6,
        0x8b, 0x5a, 0x91, 0xfc, 0x04, 0x06, 0x83, 0x84, 0x06, 0xe8, 0xd1, 0x5f,
        0xdb,
};

static struct eapol_key_data eapol_key_test_2 = {
        .frame = eapol_key_data_2,
        .frame_len = sizeof(eapol_key_data_2),
        .protocol_version = EAPOL_PROTOCOL_VERSION_2004,
        .packet_len = 117,
        .descriptor_type = EAPOL_DESCRIPTOR_TYPE_80211,
        .key_descriptor_version = EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_SHA1_AES,
        .key_type = true,
        .install = false,
        .key_ack = true,
        .key_mic = false,
        .secure = false,
        .error = false,
        .request = false,
        .encrypted_key_data = false,
        .smk_message = false,
        .key_length = 16,
        .key_replay_counter = 0,
        .key_nonce = { 0x12, 0x6a, 0xce, 0x64, 0xc1, 0xa6, 0x44, 0xd2, 0x7b,
                       0x84, 0xe0, 0x39, 0x26, 0x3b, 0x63, 0x3b, 0xc3, 0x74,
                       0xe3, 0x29, 0x9d, 0x7d, 0x45, 0xe1, 0xc4, 0x25, 0x44,
                       0x05, 0x48, 0x05, 0xbf, 0xe5 },
        .eapol_key_iv = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_rsc = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_mic_data = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_data_len = 22,
};

/* WPA2 frame, 1 of 4.  For parameters see eapol_4way_test */
static const unsigned char eapol_key_data_3[] = {
        0x02, 0x03, 0x00, 0x5f, 0x02, 0x00, 0x8a, 0x00, 0x10, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0xc2, 0xbb, 0x57, 0xab, 0x58, 0x8f, 0x92,
        0xeb, 0xbd, 0x44, 0xe8, 0x11, 0x09, 0x4f, 0x60, 0x1c, 0x08, 0x79, 0x86,
        0x03, 0x0c, 0x3a, 0xc7, 0x49, 0xcc, 0x61, 0xd6, 0x3e, 0x33, 0x83, 0x2e,
        0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00
};



static struct eapol_key_data eapol_key_test_3 = {
        .frame = eapol_key_data_3,
        .frame_len = sizeof(eapol_key_data_3),
        .protocol_version = EAPOL_PROTOCOL_VERSION_2004,
        .packet_len = 95,
        .descriptor_type = EAPOL_DESCRIPTOR_TYPE_80211,
        .key_descriptor_version = EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_SHA1_AES,
        .key_type = true,
        .install = false,
        .key_ack = true,
        .key_mic = false,
        .secure = false,
        .error = false,
        .request = false,
        .encrypted_key_data = false,
        .smk_message = false,
        .key_length = 16,
        .key_replay_counter = 0,
        .key_nonce = { 0xc2, 0xbb, 0x57, 0xab, 0x58, 0x8f, 0x92, 0xeb, 0xbd,
                       0x44, 0xe8, 0x11, 0x09, 0x4f, 0x60, 0x1c, 0x08, 0x79,
                       0x86, 0x03, 0x0c, 0x3a, 0xc7, 0x49, 0xcc, 0x61, 0xd6,
                       0x3e, 0x33, 0x83, 0x2e, 0x50, },
        .eapol_key_iv = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_rsc = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_mic_data = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_data_len = 0,
};

/* WPA2 frame, 2 of 4.  For parameters see eapol_4way_test */
static const unsigned char eapol_key_data_4[] = {
        0x01, 0x03, 0x00, 0x75, 0x02, 0x01, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x32, 0x89, 0xe9, 0x15, 0x65, 0x09, 0x4f,
        0x32, 0x9a, 0x9c, 0xd5, 0x4a, 0x4a, 0x09, 0x0d, 0x2c, 0xf4, 0x34, 0x46,
        0x83, 0xbf, 0x50, 0xef, 0xee, 0x36, 0x08, 0xb6, 0x48, 0x56, 0x80, 0x0e,
        0x84, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xc3, 0x1b,
        0x82, 0xff, 0x62, 0xa3, 0x79, 0xb0, 0x8d, 0xd1, 0xfc, 0x82, 0xc2, 0xf7,
        0x68, 0x00, 0x16, 0x30, 0x14, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x04, 0x01,
        0x00, 0x00, 0x0f, 0xac, 0x04, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x02, 0x00,
        0x00
};

static struct eapol_key_data eapol_key_test_4 = {
        .frame = eapol_key_data_4,
        .frame_len = sizeof(eapol_key_data_4),
        .protocol_version = EAPOL_PROTOCOL_VERSION_2001,
        .packet_len = 117,
        .descriptor_type = EAPOL_DESCRIPTOR_TYPE_80211,
        .key_descriptor_version = EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_SHA1_AES,
        .key_type = true,
        .install = false,
        .key_ack = false,
        .key_mic = true,
        .secure = false,
        .error = false,
        .request = false,
        .encrypted_key_data = false,
        .smk_message = false,
        .key_length = 0,
        .key_replay_counter = 0,
        .key_nonce = { 0x32, 0x89, 0xe9, 0x15, 0x65, 0x09, 0x4f, 0x32, 0x9a,
                       0x9c, 0xd5, 0x4a, 0x4a, 0x09, 0x0d, 0x2c, 0xf4, 0x34,
                       0x46, 0x83, 0xbf, 0x50, 0xef, 0xee, 0x36, 0x08, 0xb6,
                       0x48, 0x56, 0x80, 0x0e, 0x84, },
        .eapol_key_iv = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_rsc = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_mic_data = { 0x01, 0xc3, 0x1b, 0x82, 0xff, 0x62, 0xa3, 0x79, 0xb0,
                          0x8d, 0xd1, 0xfc, 0x82, 0xc2, 0xf7, 0x68 },
        .key_data_len = 22,
};







/* WPA2 frame, 3 of 4.  For parameters see eapol_4way_test */
static const unsigned char eapol_key_data_5[] = {
        0x02, 0x03, 0x00, 0x97, 0x02, 0x13, 0xca, 0x00, 0x10, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x01, 0xc2, 0xbb, 0x57, 0xab, 0x58, 0x8f, 0x92,
        0xeb, 0xbd, 0x44, 0xe8, 0x11, 0x09, 0x4f, 0x60, 0x1c, 0x08, 0x79, 0x86,
        0x03, 0x0c, 0x3a, 0xc7, 0x49, 0xcc, 0x61, 0xd6, 0x3e, 0x33, 0x83, 0x2e,
        0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf5, 0x35, 0xd9,
        0x18, 0x09, 0x73, 0x1a, 0x1d, 0x29, 0x08, 0x94, 0x70, 0x5e, 0x91, 0x9c,
        0x8e, 0x00, 0x38, 0x19, 0x18, 0xdf, 0x1e, 0xf0, 0xe7, 0x69, 0x66, 0x52,
        0xe2, 0x57, 0x93, 0x80, 0x34, 0xe1, 0x70, 0x38, 0xb9, 0x8b, 0x4c, 0x45,
        0xa9, 0x23, 0xb7, 0xb6, 0xfa, 0x8c, 0x33, 0xe3, 0x7b, 0xdc, 0xd4, 0x7f,
        0xea, 0xb1, 0x1c, 0x22, 0x6a, 0x2c, 0x5e, 0x38, 0xd5, 0xad, 0x79, 0x94,
        0x05, 0xd6, 0x10, 0xa6, 0x95, 0x51, 0xd6, 0x0b, 0xe6, 0x0a, 0x5b,
};


static struct eapol_key_data eapol_key_test_5 = {
        .frame = eapol_key_data_5,
        .frame_len = sizeof(eapol_key_data_5),
        .protocol_version = EAPOL_PROTOCOL_VERSION_2004,
        .packet_len = 151,
        .descriptor_type = EAPOL_DESCRIPTOR_TYPE_80211,
        .key_descriptor_version = EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_SHA1_AES,
        .key_type = true,
        .install = true,
        .key_ack = true,
        .key_mic = true,
        .secure = true,
        .error = false,
        .request = false,
        .encrypted_key_data = true,
        .smk_message = false,

        .key_length = 16,
        .key_replay_counter = 1,
        .key_nonce = { 0xc2, 0xbb, 0x57, 0xab, 0x58, 0x8f, 0x92, 0xeb, 0xbd,
                       0x44, 0xe8, 0x11, 0x09, 0x4f, 0x60, 0x1c, 0x08, 0x79,
                       0x86, 0x03, 0x0c, 0x3a, 0xc7, 0x49, 0xcc, 0x61, 0xd6,
                       0x3e, 0x33, 0x83, 0x2e, 0x50, },
        .eapol_key_iv = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_rsc = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_mic_data = { 0xf5, 0x35, 0xd9, 0x18, 0x09, 0x73, 0x1a, 0x1d, 0x29,
                          0x08, 0x94, 0x70, 0x5e, 0x91, 0x9c, 0x8e },
        .key_data_len = 56,
};

/* WPA2 frame, 4 of 4.  For parameters see eapol_4way_test */
static const unsigned char eapol_key_data_6[] = {
        0x01, 0x03, 0x00, 0x5f, 0x02, 0x03, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9e, 0x57, 0xa4,
        0xc0, 0x9b, 0xaf, 0xb3, 0x37, 0x5e, 0x46, 0xd3, 0x86, 0xcf, 0x87, 0x27,
        0x53, 0x00, 0x00,
};

static struct eapol_key_data eapol_key_test_6 = {
        .frame = eapol_key_data_6,
        .frame_len = sizeof(eapol_key_data_6),
        .protocol_version = EAPOL_PROTOCOL_VERSION_2001,
        .packet_len = 95,
        .descriptor_type = EAPOL_DESCRIPTOR_TYPE_80211,
        .key_descriptor_version = EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_SHA1_AES,
        .key_type = true,
        .install = false,
        .key_ack = false,
        .key_mic = true,
        .secure = true,
        .error = false,
        .request = false,
        .encrypted_key_data = false,
        .smk_message = false,
        .key_length = 0,
        .key_replay_counter = 1,
        .key_nonce = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .eapol_key_iv = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_rsc = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_mic_data = { 0x9e, 0x57, 0xa4, 0xc0, 0x9b, 0xaf, 0xb3, 0x37, 0x5e,
                          0x46, 0xd3, 0x86, 0xcf, 0x87, 0x27, 0x53, },
        .key_data_len = 0,
};

/* WPA2 frame, 1 of 4.  For parameters see eapol_wpa2_handshake_test */
static const unsigned char eapol_key_data_7[] = {
        0x02, 0x03, 0x00, 0x5f, 0x02, 0x00, 0x8a, 0x00, 0x10, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x01, 0x2b, 0x58, 0x52, 0xb8, 0x8e, 0x4c, 0xa3,
        0x4d, 0xc5, 0x99, 0xed, 0x20, 0x2c, 0x63, 0x95, 0x7c, 0x53, 0x5e, 0x3e,
        0xfa, 0x92, 0x89, 0x87, 0x34, 0x11, 0x12, 0x7c, 0xba, 0xf3, 0x58, 0x84,
        0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00
};




//XXX Harness code: get input from file instead of
//XXX reading from the struct



/*unsigned char *  __afl_get_key_test (size_t *len){

    static unsigned char buffer[2048];
    int input = 0;
    int byte_read = 0;


    if ( __afl_input_filename != NULL ) {
        input = open(__afl_input_filename, 0);
    } else {
        perror("WWW Warning: no input file; using eapol_key_data_3. WWW\n");
        *len = sizeof(eapol_key_data_3);
        return (eapol_key_data_3);
    }

    if (input <= 0) {
        perror("XXX Error: failed file cannot be open! XXX\n");
    }
    else{
        printf("+++ File open correctly! +++\n");
    }

    byte_read = read(input, buffer, 1024);
    if(byte_read <= 0){
        perror("XXX Error: Failed to read the input file! XXX");
    }
    else{
        //  printf("Read: %i\n",byte_read);
    }

    if (input > 0) {
        close (input);
    }

    if (input > 0 &&  byte_read > 0) {
        *len = byte_read;
        return(buffer);
    }

    return(NULL);
}; */

int match(char *string, char *pattern)
{
    regex_t re;

    if (regcomp(&re, pattern, REG_EXTENDED|REG_NOSUB) != 0) return 0;
    int status = regexec(&re, string, 0, NULL, 0);
    regfree(&re);
    if (status != 0) return 0;
    return 1;
}


void * __afl_get_key_data (){
    FILE * fp;
    int i;
    int data;
    int count=0;
    int expected_byte=0;
    int sz;
    size_t ret;

    fp = fopen(__afl_input_filename , "rb");
    if (fp == NULL){
        perror("!!! Warning: no input file for AFL. Tests will be executed with static data!!!\n");
        __afl_key = eapol_key_data_3;
        __afl_key1= eapol_key_data_5;
        len_frame1= sizeof(eapol_key_data_3);
        len_frame2= sizeof(eapol_key_data_5);
        return(NULL);
    }
    else{
        printf("+++ File open correctly! +++\n");
    }

    fseek(fp, 0, SEEK_END);
    sz = ftell(fp);
    rewind(fp);




    //printf("Reading Frame 1\n");
    fread(&len_frame1, sizeof(size_t), 1, fp);
    if(len_frame1 <= 0 || len_frame1 > 1024 ){
        printf("\nLen frame1 is either < 0 or > of file size! %ld\n",len_frame1 );
        exit(EXIT_FAILURE);

    }

    printf("Frame 1 len: %ld\n\n",len_frame1);

    __afl_key = (char *)malloc(sizeof(char)* len_frame1);
    for (i=0 ; i <= len_frame1; i++){
        fread(&data, sizeof(uint8_t), 1, fp);
        __afl_key[i] = data;
        printf("%d ", __afl_key[i]);
        // count+=1;
    }


    //  printf("\nReading Frame 2\n");
    fread(&len_frame2, sizeof(size_t), 1, fp);
    if(len_frame2 <= 0 || len_frame2 > 1024){
        printf("\nLen frame2 is either < 0 or > of file size! %ld\n",len_frame2);
        exit(EXIT_FAILURE);
        //return -1;
    }
    printf("\n------------------------------------------------------\n");
    printf("\n");
    printf("Frame 2 len: %ld\n\n",len_frame2);
    __afl_key1 = (char *)malloc(sizeof(char)* len_frame2);
    for (i=0 ; i <= len_frame2; i++){
        fread(&data, sizeof(uint8_t), 1, fp);
        __afl_key1[i] = data;
        printf("%d ", __afl_key1[i]);
        //  count+=1;
    }
    printf("\n");
    printf("\n------------------------------------------------------\n");

    /*printf("\n%d %d \n",(len_frame1+len_frame2+2),(count+2));

    if((len_frame1+len_frame2+2) != (count)) {
        perror("Error. Different szie\n");
        exit(EXIT_FAILURE);
    }
     */
    fclose(fp);

    return(NULL);
};


/// WORKING SOLUTION
/*
void * __afl_get_key_data (){
    FILE * fp;
    len_frame1 =0;
    len_frame2= 0;
    int i;
    unsigned char *tmparray;
    int idx = 0;
    int data,data1;
    int count=0;
    unsigned long position;
    char del[3];
    const char *reg;


    fp = fopen(__afl_input_filename , "r");
    if (fp == NULL){
        perror("!!! Warning: no input file for AFL. Tests will be executed with static data!!!\n");
        __afl_key = eapol_key_data_4;
        __afl_key1= eapol_key_data_6;
        len_frame1= sizeof(eapol_key_data_4);
        len_frame2= sizeof(eapol_key_data_6);
        return(NULL);
    }
    else{
        printf("+++ File open correctly! +++\n");
    }

    reg= "===";


    tmparray = (char *)malloc(sizeof(char)* BUF_LEN);
    while (fscanf(fp, "%*c%*c%x,", &data) == 1) {
        tmparray[idx++] = (unsigned char)data;
        count++;
    }

    len_frame1= count;

    __afl_key = (char *)malloc(sizeof(char)* len_frame1);
    for(int i=0; i < len_frame1 ;i++){
        __afl_key[i] = tmparray[i];
        printf("%d ", __afl_key[i]);
    }
    printf("\n\n");

    //compute the position of the file
    fflush(fp);
    position = ftell(fp);

    //position the fp to the previous position (the first =) and skip the following to
    fseek(fp, position-2 , SEEK_SET);
    fread(del, sizeof(char), 3, fp);
    if( (match(del,reg) )){
        printf("Matching!\n");
    }
    else {
        perror("XXX Delimiter not found XXX");
        exit(EXIT_FAILURE);
    }

    fseek(fp, position+2 , SEEK_SET);


    count =0;
    idx=0;

    while (fscanf(fp, "%*c%*c%x,", &data) == 1) {
        tmparray[idx++] = (unsigned char)data;
        count++;
    }

    len_frame2= count;


    __afl_key1 = (char *)malloc(sizeof(char)* (len_frame2));
    for(int i=0; i < len_frame2 ;i++){
        __afl_key1[i] = tmparray[i];
        printf("%d ", __afl_key1[i]);

    }


    fclose(fp);

    return(NULL);
};
*/

 ///reading from file using delimeters "====="
/*
void * __afl_get_key_data (){
    FILE * fp;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    int count=0;
    len_frame1 =0;
    len_frame2= 0;
    regex_t regex;
    const char *reg;
    char *buffer;
    //static unsigned char buffer[2048];



    fp = fopen(__afl_input_filename , "r");
    if (fp == NULL){
        perror("!!! Warning: no input file for AFL. Tests will be executed with static data!!!\n");
        __afl_key = eapol_key_data_4;
        __afl_key1= eapol_key_data_6;
        len_frame1= sizeof(eapol_key_data_4);
        len_frame2= sizeof(eapol_key_data_6);
        return(NULL);
    }
    else{
        printf("+++ File open correctly! +++\n");
    }

    buffer = (char *) malloc(BUF_LEN *sizeof(char));

    reg= "^([0-9]|[1-8][0-9]|9[0-9]|[1-8][0-9]{2}|9[0-8][0-9]|99[0-9]|10[01][0-9]|102[0-4])$";



    if ((read = getline(&line, &len, fp)) != -1 ){
        if( (regmatch(line,reg) )){
            len_frame1= atoi(line);
      //      printf("Len read %d\n",len_frame1);
        }
        else {
            perror("XXX Format of first frame size is not correct XXX");
            exit(EXIT_FAILURE);
        }

    }

    read =0;
    //scrivere da count- read a count
    int to_write;
    __afl_key = (char *)malloc(len_frame1 * sizeof(char));

    for(;;){
        if ((read = getline(&line, &len, fp)) != -1 ){
            count+=read;
            memcpy(__afl_key,line,read);
            printf("afl :\n%s  line : %s \n %d \n", __afl_key,line,count);
            if(count >= len_frame1)
                break;
        }
    }

    printf("Frame1 :\n%s  %d \n\n", __afl_key,count);


    count =0;
    read=0;


    if ((read = getline(&line, &len, fp)) != -1 ){
        if( (regmatch(line,reg) )){
            len_frame2= atoi(line);
           // printf("Len read %d\n",len_frame2);
        }
        else {
            perror("XXX Format of first frame size is not correct XXX");
            exit(EXIT_FAILURE);
        }

    }

    buffer = (char *) malloc(BUF_LEN * sizeof(char));
    read =0;
    for(;;){
        if ((read = getline(&line, &len, fp)) != -1 ){
            count+=read;
            strcat(buffer,line);
            if(count >= len_frame2)
                break;
        }
    }

    __afl_key1 = (char *)malloc(len_frame2 * sizeof(char));
    memcpy(__afl_key1,buffer,len_frame2);
    //printf("Frame2 :\n%s  %d \n", __afl_key1,count);
    free(buffer);


    fclose(fp);
    if (line)
        free(line);

    return(NULL);
};
*/



/*
void * __afl_get_key_data (){
    int fd = 0;
    int l=0;
    int byte_read = 0;

    len_frame1 =0;
    len_frame2= 0;
    char *reg;
    char *buf, *buf1;
    char *token;
    int tmp;

    fd = open(__afl_input_filename, 0);
    if ( __afl_input_filename != NULL ) {
        fd = open(__afl_input_filename, 0);
    } else {
        perror("!!! Warning: no input file for AFL. Tests will be executed with static data!!!\n");
        __afl_key = eapol_key_data_4;
        __afl_key1= eapol_key_data_6;
        len_frame1= sizeof(eapol_key_data_4);
        len_frame2= sizeof(eapol_key_data_6);
        return(NULL);
    }

    if (fd <= 0) {
        perror("XXX Error: failed file cannot be open! XXX\n");
        exit(EXIT_FAILURE);
    }
    else{
        printf("+++ File open correctly! +++\n");
    }


    reg= "^([0-9]|[1-8][0-9]|9[0-9]|[1-8][0-9]{2}|9[0-8][0-9]|99[0-9]|10[01][0-9]|102[0-4])$";


    buf = (char *) malloc(sizeof(char)*BUF_LEN);
    buf1 = (char *) malloc(sizeof(char)*BUF_LEN);


    l = read(fd,buf, BUF_LEN);
    if (l <= 0) {
        perror("XXX Error: Failed to read the input file for frame 1! XXX");
        exit(EXIT_FAILURE);
    }

    token = strtok(buf,",");
int prova;
    while(token != NULL){
        token = strtok(NULL,",");
        printf("%s ",token);
        prova= (int) atoi(token);
        printf("%d ", prova);
        //__afl_key[i] =

    }


    if (fd > 0) {
        close (fd);
    }


    return(NULL);
};

*/
static struct eapol_key_data eapol_key_test_7 = {
        .frame = eapol_key_data_7,
        .frame_len = sizeof(eapol_key_data_7),
        .protocol_version = EAPOL_PROTOCOL_VERSION_2004,
        .packet_len = 95,
        .descriptor_type = EAPOL_DESCRIPTOR_TYPE_80211,
        .key_descriptor_version = EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_SHA1_AES,
        .key_type = true,
        .wpa_key_id = 0,
        .install = false,
        .key_ack = true,
        .key_mic = false,
        .secure = false,
        .error = false,
        .request = false,
        .encrypted_key_data = false,
        .smk_message = false,
        .key_length = 16,
        .key_replay_counter = 1,
        .key_nonce = { 0x2b, 0x58, 0x52, 0xb8, 0x8e, 0x4c, 0xa3, 0x4d, 0xc5,
                       0x99, 0xed, 0x20, 0x2c, 0x63, 0x95, 0x7c, 0x53, 0x5e,
                       0x3e, 0xfa, 0x92, 0x89, 0x87, 0x34, 0x11, 0x12, 0x7c,
                       0xba, 0xf3, 0x58, 0x84, 0x25 },
        .eapol_key_iv = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_rsc = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_mic_data = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_data_len = 0,
};

/* WPA2 frame, 2 of 4.  For parameters see eapol_wpa2_handshake_test */
static const unsigned char eapol_key_data_8[] = {
        0x02, 0x03, 0x00, 0x73, 0x02, 0x01, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x01, 0x72, 0x7c, 0x65, 0x6c, 0x5e, 0xd6, 0x42,
        0xd7, 0xf4, 0x7f, 0x48, 0x43, 0x51, 0xd8, 0x08, 0x94, 0x51, 0x18, 0xda,
        0x6a, 0x49, 0x33, 0xac, 0x7e, 0x29, 0x3f, 0x2f, 0x2a, 0xc0, 0x88, 0x34,
        0x8c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7d, 0x7c, 0x45,
        0x98, 0x9f, 0x49, 0x2b, 0x4a, 0x36, 0x58, 0x35, 0x29, 0x28, 0x2d, 0x8f,
        0xed, 0x00, 0x14, 0x30, 0x12, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x04, 0x01,
        0x00, 0x00, 0x0f, 0xac, 0x04, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x02
};

static struct eapol_key_data eapol_key_test_8 = {
        .frame = eapol_key_data_8,
        .frame_len = sizeof(eapol_key_data_8),
        .protocol_version = EAPOL_PROTOCOL_VERSION_2004,
        .packet_len = 115,
        .descriptor_type = EAPOL_DESCRIPTOR_TYPE_80211,
        .key_descriptor_version = EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_SHA1_AES,
        .key_type = true,
        .wpa_key_id = 0,
        .install = false,
        .key_ack = false,
        .key_mic = true,
        .secure = false,
        .error = false,
        .request = false,
        .encrypted_key_data = false,
        .smk_message = false,
        .key_length = 0,
        .key_replay_counter = 1,
        .key_nonce = { 0x72, 0x7c, 0x65, 0x6c, 0x5e, 0xd6, 0x42, 0xd7, 0xf4,
                       0x7f, 0x48, 0x43, 0x51, 0xd8, 0x08, 0x94, 0x51, 0x18,
                       0xda, 0x6a, 0x49, 0x33, 0xac, 0x7e, 0x29, 0x3f, 0x2f,
                       0x2a, 0xc0, 0x88, 0x34, 0x8c },
        .eapol_key_iv = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_rsc = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_mic_data = { 0x7d, 0x7c, 0x45, 0x98, 0x9f, 0x49, 0x2b, 0x4a,
                          0x36, 0x58, 0x35, 0x29, 0x28, 0x2d, 0x8f, 0xed },
        .key_data_len = 20,
};

/* WPA2 frame, 3 of 4.  For parameters see eapol_wpa2_handshake_test */
static const unsigned char eapol_key_data_9[] = {
        0x02, 0x03, 0x00, 0x97, 0x02, 0x13, 0xca, 0x00, 0x10, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x02, 0x2b, 0x58, 0x52, 0xb8, 0x8e, 0x4c, 0xa3,
        0x4d, 0xc5, 0x99, 0xed, 0x20, 0x2c, 0x63, 0x95, 0x7c, 0x53, 0x5e, 0x3e,
        0xfa, 0x92, 0x89, 0x87, 0x34, 0x11, 0x12, 0x7c, 0xba, 0xf3, 0x58, 0x84,
        0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x62, 0x6a,
        0x57, 0x76, 0xc7, 0x33, 0xbf, 0xc0, 0x71, 0xde, 0x62, 0xeb, 0xf7, 0xa7,
        0xc5, 0x00, 0x38, 0x14, 0xdd, 0x52, 0x80, 0x3d, 0xc8, 0x3d, 0xf7, 0xba,
        0x7e, 0xb6, 0x48, 0x29, 0x6d, 0x65, 0x0e, 0x88, 0xcc, 0xc8, 0xdd, 0x67,
        0x62, 0x04, 0xa2, 0xc7, 0xc1, 0xf2, 0x65, 0x0b, 0xca, 0xf9, 0x6b, 0x66,
        0xf5, 0xb4, 0x3d, 0x99, 0x26, 0xad, 0xc5, 0xce, 0x18, 0xc0, 0xa9, 0x9b,
        0xe5, 0x0e, 0x50, 0x85, 0x84, 0xb1, 0x57, 0xe3, 0x7a, 0x5e, 0x0f
};

static struct eapol_key_data eapol_key_test_9 = {
        .frame = eapol_key_data_9,
        .frame_len = sizeof(eapol_key_data_9),
        .protocol_version = EAPOL_PROTOCOL_VERSION_2004,
        .packet_len = 151,
        .descriptor_type = EAPOL_DESCRIPTOR_TYPE_80211,
        .key_descriptor_version = EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_SHA1_AES,
        .key_type = true,
        .wpa_key_id = 0,
        .install = true,
        .key_ack = true,
        .key_mic = true,
        .secure = true,
        .error = false,
        .request = false,
        .encrypted_key_data = true,
        .smk_message = false,
        .key_length = 16,
        .key_replay_counter = 2,
        .key_nonce = { 0x2b, 0x58, 0x52, 0xb8, 0x8e, 0x4c, 0xa3, 0x4d, 0xc5,
                       0x99, 0xed, 0x20, 0x2c, 0x63, 0x95, 0x7c, 0x53, 0x5e,
                       0x3e, 0xfa, 0x92, 0x89, 0x87, 0x34, 0x11, 0x12, 0x7c,
                       0xba, 0xf3, 0x58, 0x84, 0x25 },
        .eapol_key_iv = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_rsc = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_mic_data = { 0xf8, 0x62, 0x6a, 0x57, 0x76, 0xc7, 0x33, 0xbf,
                          0xc0, 0x71, 0xde, 0x62, 0xeb, 0xf7, 0xa7, 0xc5 },
        .key_data_len = 56,
};

/* WPA2 frame, 4 of 4.  For parameters see eapol_wpa2_handshake_test */
static const unsigned char eapol_key_data_10[] = {
        0x02, 0x03, 0x00, 0x5f, 0x02, 0x03, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8b, 0x0f, 0x34,
        0x23, 0x49, 0xf0, 0x85, 0x03, 0xe5, 0xfc, 0x54, 0xb7, 0x2c, 0xb5, 0x32,
        0x66, 0x00, 0x00
};

static struct eapol_key_data eapol_key_test_10 = {
        .frame = eapol_key_data_10,
        .frame_len = sizeof(eapol_key_data_10),
        .protocol_version = EAPOL_PROTOCOL_VERSION_2004,
        .packet_len = 95,
        .descriptor_type = EAPOL_DESCRIPTOR_TYPE_80211,
        .key_descriptor_version = EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_SHA1_AES,
        .key_type = true,
        .wpa_key_id = 0,
        .install = false,
        .key_ack = false,
        .key_mic = true,
        .secure = true,
        .error = false,
        .request = false,
        .encrypted_key_data = false,
        .smk_message = false,
        .key_length = 0,
        .key_replay_counter = 2,
        .key_nonce = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .eapol_key_iv = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_rsc = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_mic_data = { 0x8b, 0x0f, 0x34, 0x23, 0x49, 0xf0, 0x85, 0x03,
                          0xe5, 0xfc, 0x54, 0xb7, 0x2c, 0xb5, 0x32, 0x66 },
        .key_data_len = 0,
};

/* WPA2 frame, 1 of 2.  For parameters see eapol_wpa2_handshake_test */
static const unsigned char eapol_key_data_11[] = {
        0x02, 0x03, 0x00, 0x7f, 0x02, 0x13, 0x82, 0x00, 0x10, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x03, 0xef, 0x6d, 0xc2, 0x0c, 0x57, 0x27, 0xe8,
        0x25, 0x07, 0x75, 0x1e, 0x67, 0x36, 0x9d, 0xc9, 0x1e, 0xf1, 0x71, 0x8e,
        0xb1, 0x35, 0xb2, 0x12, 0xdf, 0x9f, 0xcb, 0x0a, 0x65, 0xa2, 0xd5, 0x67,
        0x1d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x6a, 0xb0,
        0x75, 0xbc, 0x8e, 0xd1, 0x92, 0x11, 0xcd, 0xc3, 0xb0, 0xe2, 0xd6, 0x7e,
        0xcf, 0x00, 0x20, 0xe9, 0x96, 0x5b, 0x46, 0x85, 0xd5, 0x8b, 0x88, 0xbd,
        0x57, 0x48, 0x57, 0xe4, 0xf0, 0xdf, 0xdd, 0xf7, 0x2a, 0x7a, 0xa8, 0xeb,
        0x68, 0xa9, 0xef, 0x44, 0x04, 0xa6, 0x6f, 0xaa, 0x48, 0x51, 0x1b
};

static struct eapol_key_data eapol_key_test_11 = {
        .frame = eapol_key_data_11,
        .frame_len = sizeof(eapol_key_data_11),
        .protocol_version = EAPOL_PROTOCOL_VERSION_2004,
        .packet_len = 127,
        .descriptor_type = EAPOL_DESCRIPTOR_TYPE_80211,
        .key_descriptor_version = EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_SHA1_AES,
        .key_type = false,
        .wpa_key_id = 0,
        .install = false,
        .key_ack = true,
        .key_mic = true,
        .secure = true,
        .error = false,
        .request = false,
        .encrypted_key_data = true,
        .smk_message = false,
        .key_length = 16,
        .key_replay_counter = 3,
        .key_nonce = { 0xef, 0x6d, 0xc2, 0x0c, 0x57, 0x27, 0xe8, 0x25,
                       0x07, 0x75, 0x1e, 0x67, 0x36, 0x9d, 0xc9, 0x1e,
                       0xf1, 0x71, 0x8e, 0xb1, 0x35, 0xb2, 0x12, 0xdf,
                       0x9f, 0xcb, 0x0a, 0x65, 0xa2, 0xd5, 0x67, 0x1d },
        .eapol_key_iv = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_rsc = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_mic_data = { 0x0f, 0x6a, 0xb0, 0x75, 0xbc, 0x8e, 0xd1, 0x92,
                          0x11, 0xcd, 0xc3, 0xb0, 0xe2, 0xd6, 0x7e, 0xcf },
        .key_data_len = 32,
};

/* WPA2 frame, 2 of 2.  For parameters see eapol_wpa2_handshake_test */
static const unsigned char eapol_key_data_12[] = {
        0x02, 0x03, 0x00, 0x5f, 0x02, 0x03, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc9, 0xbd, 0x17,
        0xf1, 0x54, 0xb7, 0x32, 0xcf, 0xbc, 0x01, 0xdb, 0x0c, 0x37, 0xe6, 0x33,
        0x9f, 0x00, 0x00
};

static struct eapol_key_data eapol_key_test_12 = {
        .frame = eapol_key_data_12,
        .frame_len = sizeof(eapol_key_data_12),
        .protocol_version = EAPOL_PROTOCOL_VERSION_2004,
        .packet_len = 95,
        .descriptor_type = EAPOL_DESCRIPTOR_TYPE_80211,
        .key_descriptor_version = EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_SHA1_AES,
        .key_type = false,
        .wpa_key_id = 0,
        .install = false,
        .key_ack = false,
        .key_mic = true,
        .secure = true,
        .error = false,
        .request = false,
        .encrypted_key_data = false,
        .smk_message = false,
        .key_length = 0,
        .key_replay_counter = 3,
        .key_nonce = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .eapol_key_iv = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_rsc = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_mic_data = { 0xc9, 0xbd, 0x17, 0xf1, 0x54, 0xb7, 0x32, 0xcf,
                          0xbc, 0x01, 0xdb, 0x0c, 0x37, 0xe6, 0x33, 0x9f },
        .key_data_len = 0,
};

/* WPA frame, 1 of 4.  For parameters see eapol_wpa_handshake_test */
static const unsigned char eapol_key_data_13[] = {
        0x02, 0x03, 0x00, 0x5f, 0xfe, 0x00, 0x89, 0x00, 0x20, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x01, 0x66, 0xbe, 0x99, 0x48, 0x44, 0xe0, 0xf5,
        0x40, 0x78, 0x13, 0x91, 0x37, 0x6f, 0x47, 0x99, 0x56, 0xa3, 0xec, 0x36,
        0x32, 0xe4, 0x12, 0x13, 0x64, 0xec, 0x7e, 0x75, 0x37, 0xef, 0xf6, 0x2a,
        0xc5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00
};

static struct eapol_key_data eapol_key_test_13 = {
        .frame = eapol_key_data_13,
        .frame_len = sizeof(eapol_key_data_13),
        .protocol_version = EAPOL_PROTOCOL_VERSION_2004,
        .packet_len = 95,
        .descriptor_type = EAPOL_DESCRIPTOR_TYPE_WPA,
        .key_descriptor_version = EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_MD5_ARC4,
        .key_type = true,
        .wpa_key_id = 0,
        .install = false,
        .key_ack = true,
        .key_mic = false,
        .secure = false,
        .error = false,
        .request = false,
        .encrypted_key_data = false,
        .smk_message = false,
        .key_length = 32,
        .key_replay_counter = 1,
        .key_nonce = { 0x66, 0xbe, 0x99, 0x48, 0x44, 0xe0, 0xf5, 0x40,
                       0x78, 0x13, 0x91, 0x37, 0x6f, 0x47, 0x99, 0x56,
                       0xa3, 0xec, 0x36, 0x32, 0xe4, 0x12, 0x13, 0x64,
                       0xec, 0x7e, 0x75, 0x37, 0xef, 0xf6, 0x2a, 0xc5 },
        .eapol_key_iv = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_rsc = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_mic_data = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_data_len = 0,
};

/* WPA frame, 2 of 4.  For parameters see eapol_wpa_handshake_test */
static const unsigned char eapol_key_data_14[] = {
        0x02, 0x03, 0x00, 0x77, 0xfe, 0x01, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x01, 0x3b, 0x7f, 0x85, 0x0a, 0x03, 0x9c, 0xa4,
        0x71, 0x42, 0x9d, 0x0f, 0xc3, 0xce, 0x9f, 0xff, 0x48, 0xdb, 0x89, 0x2e,
        0xf7, 0xa7, 0xff, 0x80, 0xf6, 0x22, 0xc4, 0x6e, 0x32, 0x97, 0x05, 0xc3,
        0x7d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xe6, 0x82,
        0x94, 0xdc, 0x88, 0x07, 0x18, 0xa7, 0xd3, 0x08, 0xfa, 0xb4, 0x39, 0x95,
        0x36, 0x00, 0x18, 0xdd, 0x16, 0x00, 0x50, 0xf2, 0x01, 0x01, 0x00, 0x00,
        0x50, 0xf2, 0x02, 0x01, 0x00, 0x00, 0x50, 0xf2, 0x02, 0x01, 0x00, 0x00,
        0x50, 0xf2, 0x02
};

static struct eapol_key_data eapol_key_test_14 = {
        .frame = eapol_key_data_14,
        .frame_len = sizeof(eapol_key_data_14),
        .protocol_version = EAPOL_PROTOCOL_VERSION_2004,
        .packet_len = 119,
        .descriptor_type = EAPOL_DESCRIPTOR_TYPE_WPA,
        .key_descriptor_version = EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_MD5_ARC4,
        .key_type = true,
        .wpa_key_id = 0,
        .install = false,
        .key_ack = false,
        .key_mic = true,
        .secure = false,
        .error = false,
        .request = false,
        .encrypted_key_data = false,
        .smk_message = false,
        .key_length = 0,
        .key_replay_counter = 1,
        .key_nonce = { 0x3b, 0x7f, 0x85, 0x0a, 0x03, 0x9c, 0xa4, 0x71,
                       0x42, 0x9d, 0x0f, 0xc3, 0xce, 0x9f, 0xff, 0x48,
                       0xdb, 0x89, 0x2e, 0xf7, 0xa7, 0xff, 0x80, 0xf6,
                       0x22, 0xc4, 0x6e, 0x32, 0x97, 0x05, 0xc3, 0x7d },
        .eapol_key_iv = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_rsc = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_mic_data = { 0x01, 0xe6, 0x82, 0x94, 0xdc, 0x88, 0x07, 0x18,
                          0xa7, 0xd3, 0x08, 0xfa, 0xb4, 0x39, 0x95, 0x36 },
        .key_data_len = 24,
};

/* WPA frame, 3 of 4.  For parameters see eapol_wpa_handshake_test */
static const unsigned char eapol_key_data_15[] = {
        0x02, 0x03, 0x00, 0x77, 0xfe, 0x01, 0xc9, 0x00, 0x20, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x02, 0x66, 0xbe, 0x99, 0x48, 0x44, 0xe0, 0xf5,
        0x40, 0x78, 0x13, 0x91, 0x37, 0x6f, 0x47, 0x99, 0x56, 0xa3, 0xec, 0x36,
        0x32, 0xe4, 0x12, 0x13, 0x64, 0xec, 0x7e, 0x75, 0x37, 0xef, 0xf6, 0x2a,
        0xc5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x96, 0xc2, 0x97,
        0xf6, 0xc1, 0x93, 0x72, 0x19, 0x3e, 0x40, 0xd9, 0xc8, 0xb9, 0xaa, 0x7c,
        0x94, 0x00, 0x18, 0xdd, 0x16, 0x00, 0x50, 0xf2, 0x01, 0x01, 0x00, 0x00,
        0x50, 0xf2, 0x02, 0x01, 0x00, 0x00, 0x50, 0xf2, 0x02, 0x01, 0x00, 0x00,
        0x50, 0xf2, 0x02
};

static struct eapol_key_data eapol_key_test_15 = {
        .frame = eapol_key_data_15,
        .frame_len = sizeof(eapol_key_data_15),
        .protocol_version = EAPOL_PROTOCOL_VERSION_2004,
        .packet_len = 119,
        .descriptor_type = EAPOL_DESCRIPTOR_TYPE_WPA,
        .key_descriptor_version = EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_MD5_ARC4,
        .key_type = true,
        .wpa_key_id = 0,
        .install = true,
        .key_ack = true,
        .key_mic = true,
        .secure = false,
        .error = false,
        .request = false,
        .encrypted_key_data = false,
        .smk_message = false,
        .key_length = 32,
        .key_replay_counter = 2,
        .key_nonce = { 0x66, 0xbe, 0x99, 0x48, 0x44, 0xe0, 0xf5, 0x40,
                       0x78, 0x13, 0x91, 0x37, 0x6f, 0x47, 0x99, 0x56,
                       0xa3, 0xec, 0x36, 0x32, 0xe4, 0x12, 0x13, 0x64,
                       0xec, 0x7e, 0x75, 0x37, 0xef, 0xf6, 0x2a, 0xc5 },
        .eapol_key_iv = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_rsc = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_mic_data = { 0x96, 0xc2, 0x97, 0xf6, 0xc1, 0x93, 0x72, 0x19,
                          0x3e, 0x40, 0xd9, 0xc8, 0xb9, 0xaa, 0x7c, 0x94 },
        .key_data_len = 24,
};

/* WPA frame, 4 of 4.  For parameters see eapol_wpa_handshake_test */
static const unsigned char eapol_key_data_16[] = {
        0x02, 0x03, 0x00, 0x5f, 0xfe, 0x01, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x65, 0xf3,
        0x33, 0xa3, 0x5b, 0x4f, 0xda, 0xc3, 0x66, 0xb3, 0x1a, 0x43, 0xb5, 0x31,
        0x95, 0x00, 0x00
};

static struct eapol_key_data eapol_key_test_16 = {
        .frame = eapol_key_data_16,
        .frame_len = sizeof(eapol_key_data_16),
        .protocol_version = EAPOL_PROTOCOL_VERSION_2004,
        .packet_len = 95,
        .descriptor_type = EAPOL_DESCRIPTOR_TYPE_WPA,
        .key_descriptor_version = EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_MD5_ARC4,
        .key_type = true,
        .wpa_key_id = 0,
        .install = false,
        .key_ack = false,
        .key_mic = true,
        .secure = false,
        .error = false,
        .request = false,
        .encrypted_key_data = false,
        .smk_message = false,
        .key_length = 0,
        .key_replay_counter = 2,
        .key_nonce = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .eapol_key_iv = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_rsc = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_mic_data = { 0xe0, 0x65, 0xf3, 0x33, 0xa3, 0x5b, 0x4f, 0xda,
                          0xc3, 0x66, 0xb3, 0x1a, 0x43, 0xb5, 0x31, 0x95 },
        .key_data_len = 0,
};

/* WPA frame, 1 of 2.  For parameters see eapol_wpa_handshake_test */
static const unsigned char eapol_key_data_17[] = {
        0x02, 0x03, 0x00, 0x7f, 0xfe, 0x03, 0xa1, 0x00, 0x20, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x03, 0xc3, 0xa7, 0xe0, 0x14, 0x10, 0xea, 0xe6,
        0xe5, 0xfb, 0x79, 0xfc, 0xf5, 0xe5, 0x55, 0x08, 0x44, 0xd9, 0xbd, 0xd2,
        0x80, 0x5d, 0x81, 0x1c, 0x0a, 0x9c, 0x48, 0x0a, 0xe9, 0x86, 0xca, 0x87,
        0xa1, 0xd9, 0xbd, 0xd2, 0x80, 0x5d, 0x81, 0x1c, 0x0a, 0x9c, 0x48, 0x0a,
        0xe9, 0x86, 0xca, 0x87, 0xa2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfb, 0x78, 0xf8,
        0xf3, 0xff, 0x1f, 0xec, 0x97, 0x98, 0x67, 0xe8, 0x25, 0x0a, 0xf2, 0x9d,
        0x61, 0x00, 0x20, 0x9f, 0x46, 0x72, 0x04, 0x83, 0xd7, 0xf6, 0xa3, 0x5b,
        0xbd, 0xa6, 0x80, 0x32, 0xe0, 0x44, 0x92, 0x5e, 0x90, 0xe5, 0x7f, 0xd8,
        0x5d, 0xfc, 0xd0, 0xdb, 0xcd, 0x7f, 0xf7, 0x48, 0xdf, 0x33, 0x75
};

static struct eapol_key_data eapol_key_test_17 = {
        .frame = eapol_key_data_17,
        .frame_len = sizeof(eapol_key_data_17),
        .protocol_version = EAPOL_PROTOCOL_VERSION_2004,
        .packet_len = 127,
        .descriptor_type = EAPOL_DESCRIPTOR_TYPE_WPA,
        .key_descriptor_version = EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_MD5_ARC4,
        .key_type = false,
        .wpa_key_id = 2,
        .install = false,
        .key_ack = true,
        .key_mic = true,
        .secure = true,
        .error = false,
        .request = false,
        .encrypted_key_data = false,
        .smk_message = false,
        .key_length = 32,
        .key_replay_counter = 3,
        .key_nonce = { 0xc3, 0xa7, 0xe0, 0x14, 0x10, 0xea, 0xe6, 0xe5,
                       0xfb, 0x79, 0xfc, 0xf5, 0xe5, 0x55, 0x08, 0x44,
                       0xd9, 0xbd, 0xd2, 0x80, 0x5d, 0x81, 0x1c, 0x0a,
                       0x9c, 0x48, 0x0a, 0xe9, 0x86, 0xca, 0x87, 0xa1 },
        .eapol_key_iv = { 0xd9, 0xbd, 0xd2, 0x80, 0x5d, 0x81, 0x1c, 0x0a,
                          0x9c, 0x48, 0x0a, 0xe9, 0x86, 0xca, 0x87, 0xa2 },
        .key_rsc = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_mic_data = { 0xfb, 0x78, 0xf8, 0xf3, 0xff, 0x1f, 0xec, 0x97,
                          0x98, 0x67, 0xe8, 0x25, 0x0a, 0xf2, 0x9d, 0x61 },
        .key_data_len = 32,
};

/* WPA frame, 2 of 2.  For parameters see eapol_wpa_handshake_test */
static const unsigned char eapol_key_data_18[] = {
        0x02, 0x03, 0x00, 0x5f, 0xfe, 0x03, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe9, 0x92, 0xb9,
        0x33, 0x25, 0xa9, 0xdc, 0x46, 0xe3, 0xb2, 0xa1, 0x5c, 0xf7, 0x2f, 0x82,
        0x52, 0x00, 0x00,
};

static struct eapol_key_data eapol_key_test_18 = {
        .frame = eapol_key_data_18,
        .frame_len = sizeof(eapol_key_data_18),
        .protocol_version = EAPOL_PROTOCOL_VERSION_2004,
        .packet_len = 95,
        .descriptor_type = EAPOL_DESCRIPTOR_TYPE_WPA,
        .key_descriptor_version = EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_MD5_ARC4,
        .key_type = false,
        .wpa_key_id = 2,
        .install = false,
        .key_ack = false,
        .key_mic = true,
        .secure = true,
        .error = false,
        .request = false,
        .encrypted_key_data = false,
        .smk_message = false,
        .key_length = 0,
        .key_replay_counter = 3,
        .key_nonce = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .eapol_key_iv = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_rsc = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_mic_data = { 0xe9, 0x92, 0xb9, 0x33, 0x25, 0xa9, 0xdc, 0x46, 0xe3,
                          0xb2, 0xa1, 0x5c, 0xf7, 0x2f, 0x82, 0x52 },
        .key_data_len = 0,
};

/* WPA frame, 1 of 4.  For parameters see eapol_wpa2_handshake_test */
static const unsigned char eapol_key_data_19[] = {
        0x01, 0x03, 0x00, 0x5f, 0xfe, 0x00, 0x89, 0x00, 0x20, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x26, 0x56, 0x9b, 0x2a, 0x6e, 0x90, 0xad,
        0x21, 0x91, 0x86, 0x6d, 0x86, 0xe9, 0xfd, 0xf8, 0xb7, 0x9a, 0x12, 0xcb,
        0xab, 0x1a, 0xc3, 0xe0, 0x2d, 0xa6, 0xa1, 0x22, 0x43, 0x4e, 0x76, 0x9d,
        0x75, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00,
};

static struct eapol_key_data eapol_key_test_19 = {
        .frame = eapol_key_data_19,
        .frame_len = sizeof(eapol_key_data_19),
        .protocol_version = EAPOL_PROTOCOL_VERSION_2001,
        .packet_len = 95,
        .descriptor_type = EAPOL_DESCRIPTOR_TYPE_WPA,
        .key_descriptor_version = EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_MD5_ARC4,
        .key_type = true,
        .wpa_key_id = 0,
        .install = false,
        .key_ack = true,
        .key_mic = false,
        .secure = false,
        .error = false,
        .request = false,
        .encrypted_key_data = false,
        .smk_message = false,
        .key_length = 32,
        .key_replay_counter = 0,
        .key_nonce = { 0x26, 0x56, 0x9b, 0x2a, 0x6e, 0x90, 0xad, 0x21, 0x91,
                       0x86, 0x6d, 0x86, 0xe9, 0xfd, 0xf8, 0xb7, 0x9a, 0x12,
                       0xcb, 0xab, 0x1a, 0xc3, 0xe0, 0x2d, 0xa6, 0xa1, 0x22,
                       0x43, 0x4e, 0x76, 0x9d, 0x75 },
        .eapol_key_iv = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_rsc = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_mic_data = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_data_len = 0,
};

/* WPA frame, 2 of 4.  For parameters see eapol_wpa2_handshake_test */
static const unsigned char eapol_key_data_20[] = {
        0x01, 0x03, 0x00, 0x77, 0xfe, 0x01, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x88, 0xe2, 0x61, 0x2d, 0xf3, 0xe3,
        0x66, 0x3d, 0x35, 0xf7, 0xd3, 0x70, 0x17, 0x87, 0x92, 0x92, 0x4a, 0x35,
        0xc8, 0x2d, 0x67, 0x45, 0x56, 0xe6, 0x98, 0x9e, 0x75, 0x79, 0x9e, 0x25,
        0x6b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x32, 0x5c, 0x17,
        0xb7, 0xbc, 0x6d, 0x2c, 0x0f, 0x7c, 0xc0, 0x32, 0xec, 0xa6, 0x31, 0xbd,
        0x8d, 0x00, 0x18, 0xdd, 0x16, 0x00, 0x50, 0xf2, 0x01, 0x01, 0x00, 0x00,
        0x50, 0xf2, 0x02, 0x01, 0x00, 0x00, 0x50, 0xf2, 0x02, 0x01, 0x00, 0x00,
        0x50, 0xf2, 0x02,
};

static struct eapol_key_data eapol_key_test_20 = {
        .frame = eapol_key_data_20,
        .frame_len = sizeof(eapol_key_data_20),
        .protocol_version = EAPOL_PROTOCOL_VERSION_2001,
        .packet_len = 119,
        .descriptor_type = EAPOL_DESCRIPTOR_TYPE_WPA,
        .key_descriptor_version = EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_MD5_ARC4,
        .key_type = true,
        .wpa_key_id = 0,
        .install = false,
        .key_ack = false,
        .key_mic = true,
        .secure = false,
        .error = false,
        .request = false,
        .encrypted_key_data = false,
        .smk_message = false,
        .key_length = 0,
        .key_replay_counter = 0,
        .key_nonce = { 0x04, 0x88, 0xe2, 0x61, 0x2d, 0xf3, 0xe3, 0x66, 0x3d,
                       0x35, 0xf7, 0xd3, 0x70, 0x17, 0x87, 0x92, 0x92, 0x4a,
                       0x35, 0xc8, 0x2d, 0x67, 0x45, 0x56, 0xe6, 0x98, 0x9e,
                       0x75, 0x79, 0x9e, 0x25, 0x6b },
        .eapol_key_iv = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_rsc = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_mic_data = { 0x32, 0x5c, 0x17, 0xb7, 0xbc, 0x6d, 0x2c, 0x0f, 0x7c,
                          0xc0, 0x32, 0xec, 0xa6, 0x31, 0xbd, 0x8d },
        .key_data_len = 24,
};

/* WPA frame, 3 of 4.  For parameters see eapol_wpa2_handshake_test */
static const unsigned char eapol_key_data_21[] = {
        0x01, 0x03, 0x00, 0x77, 0xfe, 0x01, 0xc9, 0x00, 0x20, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x01, 0x26, 0x56, 0x9b, 0x2a, 0x6e, 0x90, 0xad,
        0x21, 0x91, 0x86, 0x6d, 0x86, 0xe9, 0xfd, 0xf8, 0xb7, 0x9a, 0x12, 0xcb,
        0xab, 0x1a, 0xc3, 0xe0, 0x2d, 0xa6, 0xa1, 0x22, 0x43, 0x4e, 0x76, 0x9d,
        0x75, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xcd, 0x76, 0x63,
        0x43, 0xe1, 0xc7, 0x6b, 0x59, 0x34, 0xa4, 0x23, 0x42, 0xc0, 0xb9, 0x98,
        0x02, 0x00, 0x18, 0xdd, 0x16, 0x00, 0x50, 0xf2, 0x01, 0x01, 0x00, 0x00,
        0x50, 0xf2, 0x02, 0x01, 0x00, 0x00, 0x50, 0xf2, 0x02, 0x01, 0x00, 0x00,
        0x50, 0xf2, 0x02,
};

static struct eapol_key_data eapol_key_test_21 = {
        .frame = eapol_key_data_21,
        .frame_len = sizeof(eapol_key_data_21),
        .protocol_version = EAPOL_PROTOCOL_VERSION_2001,
        .packet_len = 119,
        .descriptor_type = EAPOL_DESCRIPTOR_TYPE_WPA,
        .key_descriptor_version = EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_MD5_ARC4,
        .key_type = true,
        .wpa_key_id = 0,
        .install = true,
        .key_ack = true,
        .key_mic = true,
        .secure = false,
        .error = false,
        .request = false,
        .encrypted_key_data = false,
        .smk_message = false,
        .key_length = 32,
        .key_replay_counter = 1,
        .key_nonce = { 0x26, 0x56, 0x9b, 0x2a, 0x6e, 0x90, 0xad, 0x21, 0x91,
                       0x86, 0x6d, 0x86, 0xe9, 0xfd, 0xf8, 0xb7, 0x9a, 0x12,
                       0xcb, 0xab, 0x1a, 0xc3, 0xe0, 0x2d, 0xa6, 0xa1, 0x22,
                       0x43, 0x4e, 0x76, 0x9d, 0x75 },
        .eapol_key_iv = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_rsc = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_mic_data = { 0xcd, 0x76, 0x63, 0x43, 0xe1, 0xc7, 0x6b, 0x59, 0x34,
                          0xa4, 0x23, 0x42, 0xc0, 0xb9, 0x98, 0x02 },
        .key_data_len = 24,
};

/* WPA frame, 4 of 4.  For parameters see eapol_wpa2_handshake_test */
static const unsigned char eapol_key_data_22[] = {
        0x01, 0x03, 0x00, 0x5f, 0xfe, 0x01, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x37, 0xbc, 0x08,
        0x98, 0x99, 0x33, 0x1a, 0x70, 0xed, 0xa5, 0xb5, 0xb7, 0x54, 0xf5, 0x5f,
        0x06, 0x00, 0x00,
};

static struct eapol_key_data eapol_key_test_22 = {
        .frame = eapol_key_data_22,
        .frame_len = sizeof(eapol_key_data_22),
        .protocol_version = EAPOL_PROTOCOL_VERSION_2001,
        .packet_len = 95,
        .descriptor_type = EAPOL_DESCRIPTOR_TYPE_WPA,
        .key_descriptor_version = EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_MD5_ARC4,
        .key_type = true,
        .wpa_key_id = 0,
        .install = false,
        .key_ack = false,
        .key_mic = true,
        .secure = false,
        .error = false,
        .request = false,
        .encrypted_key_data = false,
        .smk_message = false,
        .key_length = 0,
        .key_replay_counter = 1,
        .key_nonce = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .eapol_key_iv = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_rsc = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_mic_data = { 0x37, 0xbc, 0x08, 0x98, 0x99, 0x33, 0x1a, 0x70, 0xed,
                          0xa5, 0xb5, 0xb7, 0x54, 0xf5, 0x5f, 0x06 },
        .key_data_len = 0,
};

/*
 * WPA Group Handshake frame, 1 of 2.  For parameters
 * see eapol_wpa2_handshake_test
 */
static const unsigned char eapol_key_data_23[] = {
        0x01, 0x03, 0x00, 0x7f, 0xfe, 0x03, 0xa1, 0x00, 0x20, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x02, 0x6a, 0x65, 0xcf, 0x21, 0x0d, 0x51, 0xeb,
        0xd6, 0xca, 0x3e, 0xc9, 0xee, 0xc9, 0xd9, 0xad, 0x2f, 0x14, 0x13, 0x8d,
        0xb9, 0x33, 0xa1, 0xbb, 0xed, 0x94, 0x26, 0xa1, 0x73, 0x48, 0xc8, 0x4d,
        0x4a, 0x0d, 0x8c, 0xe1, 0x3f, 0x1c, 0x0c, 0x52, 0x55, 0x00, 0xe5, 0x04,
        0xe5, 0xbd, 0x46, 0x94, 0xdb, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe5, 0xa3, 0x27,
        0x7c, 0x85, 0x47, 0xd5, 0x3f, 0x35, 0xaf, 0x86, 0xc2, 0x2c, 0xcb, 0xb3,
        0x87, 0x00, 0x20, 0xc8, 0xb3, 0x61, 0xc0, 0x71, 0x5d, 0x2e, 0x9a, 0x6a,
        0x78, 0xdf, 0xfe, 0x0d, 0x64, 0x11, 0x23, 0xd9, 0xcc, 0xc6, 0x44, 0x65,
        0x76, 0xba, 0x82, 0xd5, 0x08, 0x4b, 0x4e, 0x51, 0xc2, 0x6a, 0x3f,
};

static struct eapol_key_data eapol_key_test_23 = {
        .frame = eapol_key_data_23,
        .frame_len = sizeof(eapol_key_data_23),
        .protocol_version = EAPOL_PROTOCOL_VERSION_2001,
        .packet_len = 127,
        .descriptor_type = EAPOL_DESCRIPTOR_TYPE_WPA,
        .key_descriptor_version = EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_MD5_ARC4,
        .key_type = false,
        .wpa_key_id = 2,
        .install = false,
        .key_ack = true,
        .key_mic = true,
        .secure = true,
        .error = false,
        .request = false,
        .encrypted_key_data = false,
        .smk_message = false,
        .key_length = 32,
        .key_replay_counter = 2,
        .key_nonce = { 0x6a, 0x65, 0xcf, 0x21, 0x0d, 0x51, 0xeb, 0xd6, 0xca,
                       0x3e, 0xc9, 0xee, 0xc9, 0xd9, 0xad, 0x2f, 0x14, 0x13,
                       0x8d, 0xb9, 0x33, 0xa1, 0xbb, 0xed, 0x94, 0x26, 0xa1,
                       0x73, 0x48, 0xc8, 0x4d, 0x4a },
        .eapol_key_iv = { 0x0d, 0x8c, 0xe1, 0x3f, 0x1c, 0x0c, 0x52, 0x55, 0x00,
                          0xe5, 0x04, 0xe5, 0xbd, 0x46, 0x94, 0xdb },
        .key_rsc = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_mic_data = { 0xe5, 0xa3, 0x27, 0x7c, 0x85, 0x47, 0xd5, 0x3f, 0x35,
                          0xaf, 0x86, 0xc2, 0x2c, 0xcb, 0xb3, 0x87 },
        .key_data_len = 32,
};

/*
 * WPA Group Handshake frame, 2 of 2.  For parameters
 * see eapol_wpa2_handshake_test
 */
static const unsigned char eapol_key_data_24[] = {
        0x01, 0x03, 0x00, 0x5f, 0xfe, 0x03, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4f, 0x06, 0x47,
        0x53, 0x9a, 0x1d, 0x3f, 0x9c, 0x02, 0x50, 0xb5, 0xc0, 0x40, 0x6b, 0x97,
        0xf8, 0x00, 0x00,
};

static struct eapol_key_data eapol_key_test_24 = {
        .frame = eapol_key_data_24,
        .frame_len = sizeof(eapol_key_data_24),
        .protocol_version = EAPOL_PROTOCOL_VERSION_2001,
        .packet_len = 95,
        .descriptor_type = EAPOL_DESCRIPTOR_TYPE_WPA,
        .key_descriptor_version = EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_MD5_ARC4,
        .key_type = false,
        .wpa_key_id = 2,
        .install = false,
        .key_ack = false,
        .key_mic = true,
        .secure = true,
        .error = false,
        .request = false,
        .encrypted_key_data = false,
        .smk_message = false,
        .key_length = 0,
        .key_replay_counter = 2,
        .key_nonce = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .eapol_key_iv = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_rsc = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_mic_data = { 0x4f, 0x06, 0x47, 0x53, 0x9a, 0x1d, 0x3f, 0x9c, 0x02,
                          0x50, 0xb5, 0xc0, 0x40, 0x6b, 0x97, 0xf8 },
        .key_data_len = 0,
};

/* FT frame, 1 of 4.  For parameters see eapol_ft_handshake_test */
static const unsigned char eapol_key_data_25[] = {
        0x02, 0x03, 0x00, 0x5f, 0x02, 0x00, 0x8b, 0x00, 0x10, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x01, 0x33, 0xb2, 0x74, 0xa0, 0xae, 0xc9, 0xe8,
        0x5d, 0x61, 0x11, 0x8f, 0x1b, 0x6b, 0x97, 0x77, 0x4e, 0x5b, 0x75, 0x08,
        0x37, 0x45, 0x77, 0xdc, 0x14, 0x08, 0xa5, 0xf1, 0x80, 0xc5, 0xd2, 0xe9,
        0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00,
};

static struct eapol_key_data eapol_key_test_25 = {
        .frame = eapol_key_data_25,
        .frame_len = sizeof(eapol_key_data_25),
        .protocol_version = EAPOL_PROTOCOL_VERSION_2004,
        .packet_len = 95,
        .descriptor_type = EAPOL_DESCRIPTOR_TYPE_80211,
        .key_descriptor_version = EAPOL_KEY_DESCRIPTOR_VERSION_AES_128_CMAC_AES,
        .key_type = true,
        .install = false,
        .key_ack = true,
        .key_mic = false,
        .secure = false,
        .error = false,
        .request = false,
        .encrypted_key_data = false,
        .smk_message = false,
        .key_length = 16,
        .key_replay_counter = 1,
        .key_nonce = { 0x33, 0xb2, 0x74, 0xa0, 0xae, 0xc9, 0xe8, 0x5d, 0x61,
                       0x11, 0x8f, 0x1b, 0x6b, 0x97, 0x77, 0x4e, 0x5b, 0x75,
                       0x08, 0x37, 0x45, 0x77, 0xdc, 0x14, 0x08, 0xa5, 0xf1,
                       0x80, 0xc5, 0xd2, 0xe9, 0xfd },
        .eapol_key_iv = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_rsc = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_mic_data = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_data_len = 0,
};

/* FT frame, 2 of 4.  For parameters see eapol_ft_handshake_test */
static const unsigned char eapol_key_data_26[] = {
        0x02, 0x03, 0x00, 0xf0, 0x02, 0x01, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x01, 0xac, 0x1e, 0xb2, 0xc7, 0x0b, 0x20, 0x8c,
        0xe6, 0x0a, 0xe2, 0x07, 0xb2, 0x38, 0x9e, 0x44, 0x1f, 0xff, 0x39, 0x86,
        0x3d, 0x44, 0x9f, 0x81, 0x24, 0x6f, 0xe3, 0x6e, 0xde, 0x0f, 0x1f, 0x56,
        0xce, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79, 0xa1, 0x95,
        0x19, 0xe8, 0x5f, 0xcf, 0x2e, 0x60, 0x7d, 0x6f, 0xfb, 0xd9, 0xe8, 0x04,
        0xb0, 0x00, 0x91, 0x30, 0x26, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x04, 0x01,
        0x00, 0x00, 0x0f, 0xac, 0x04, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x04, 0x00,
        0x00, 0x01, 0x00, 0xde, 0xce, 0x50, 0xa0, 0x9e, 0xf0, 0x8c, 0x4e, 0xbe,
        0xf2, 0xf1, 0xdb, 0xe9, 0x67, 0xb4, 0xd4, 0x36, 0x03, 0x12, 0x34, 0x01,
        0x37, 0x62, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x01, 0x06, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x06, 0x64, 0x75,
        0x6d, 0x6d, 0x79, 0x30,
};

static struct eapol_key_data eapol_key_test_26 = {
        .frame = eapol_key_data_26,
        .frame_len = sizeof(eapol_key_data_26),
        .protocol_version = EAPOL_PROTOCOL_VERSION_2004,
        .packet_len = 240,
        .descriptor_type = EAPOL_DESCRIPTOR_TYPE_80211,
        .key_descriptor_version = EAPOL_KEY_DESCRIPTOR_VERSION_AES_128_CMAC_AES,
        .key_type = true,
        .install = false,
        .key_ack = false,
        .key_mic = true,
        .secure = false,
        .error = false,
        .request = false,
        .encrypted_key_data = false,
        .smk_message = false,
        .key_length = 0,
        .key_replay_counter = 1,
        .key_nonce = { 0xac, 0x1e, 0xb2, 0xc7, 0x0b, 0x20, 0x8c, 0xe6, 0x0a,
                       0xe2, 0x07, 0xb2, 0x38, 0x9e, 0x44, 0x1f, 0xff, 0x39,
                       0x86, 0x3d, 0x44, 0x9f, 0x81, 0x24, 0x6f, 0xe3, 0x6e,
                       0xde, 0x0f, 0x1f, 0x56, 0xce },
        .eapol_key_iv = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_rsc = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_mic_data = { 0x79, 0xa1, 0x95, 0x19, 0xe8, 0x5f, 0xcf, 0x2e, 0x60,
                          0x7d, 0x6f, 0xfb, 0xd9, 0xe8, 0x04, 0xb0 },
        .key_data_len = 145,
};

/* FT frame, 3 of 4.  For parameters see eapol_ft_handshake_test */
static const unsigned char eapol_key_data_27[] = {
        0x02, 0x03, 0x01, 0x1f, 0x02, 0x13, 0xcb, 0x00, 0x10, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x02, 0x33, 0xb2, 0x74, 0xa0, 0xae, 0xc9, 0xe8,
        0x5d, 0x61, 0x11, 0x8f, 0x1b, 0x6b, 0x97, 0x77, 0x4e, 0x5b, 0x75, 0x08,
        0x37, 0x45, 0x77, 0xdc, 0x14, 0x08, 0xa5, 0xf1, 0x80, 0xc5, 0xd2, 0xe9,
        0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x12, 0xe3, 0x6d,
        0xef, 0x96, 0x5b, 0x07, 0xaa, 0x58, 0xce, 0xed, 0xb1, 0x57, 0x83, 0x51,
        0x7e, 0x00, 0xc0, 0x1d, 0x41, 0x1c, 0xf6, 0x35, 0x4e, 0x5c, 0x0f, 0x4d,
        0x03, 0x08, 0xe7, 0x3b, 0x33, 0x04, 0xde, 0x5e, 0x01, 0x77, 0x1c, 0xb8,
        0x91, 0xef, 0x4f, 0x24, 0x64, 0x84, 0xaa, 0x94, 0x42, 0xc3, 0xcb, 0x63,
        0x2d, 0xed, 0x5e, 0x5f, 0xcb, 0x1a, 0xff, 0x53, 0x58, 0x45, 0xd5, 0xc2,
        0x11, 0x52, 0x40, 0x06, 0x08, 0x70, 0xd8, 0xd1, 0xf7, 0x79, 0xfe, 0x37,
        0xb8, 0x6d, 0xd6, 0x27, 0x45, 0xbc, 0x11, 0x2a, 0x69, 0x05, 0x2c, 0x40,
        0x3a, 0x06, 0x24, 0x5a, 0x7a, 0xa6, 0x79, 0x4b, 0x18, 0xc8, 0xbf, 0x21,
        0x10, 0xa0, 0x7d, 0x80, 0x89, 0x77, 0x85, 0x27, 0x88, 0x74, 0x07, 0x00,
        0x14, 0xe0, 0x91, 0xa2, 0xe0, 0xa2, 0x25, 0x3c, 0x26, 0xce, 0xd6, 0x9f,
        0x33, 0x20, 0x99, 0xc6, 0xc0, 0x46, 0x91, 0x8a, 0x68, 0x5b, 0xa0, 0xa1,
        0x30, 0xcd, 0x76, 0x58, 0x26, 0x81, 0x03, 0xe6, 0x79, 0x3b, 0xf6, 0x4b,
        0x2f, 0x1d, 0x73, 0x8f, 0x62, 0xda, 0x22, 0xa9, 0xb4, 0xac, 0x77, 0xcc,
        0xd4, 0xd4, 0x6f, 0x80, 0x31, 0xfb, 0x3d, 0x21, 0xde, 0x41, 0x09, 0x70,
        0xcc, 0x18, 0xa6, 0xa5, 0x3e, 0xda, 0x1b, 0x6b, 0x30, 0xce, 0xd4, 0xde,
        0xd4, 0x86, 0x7e, 0x71, 0xd0, 0x97, 0x85, 0xcc, 0xa2, 0xd3, 0x4c, 0x56,
        0xcf, 0x1a, 0xd3, 0xff, 0xf8, 0x18, 0xfd, 0x52, 0x77, 0x21, 0xe9, 0x87,
        0x3f, 0x14, 0x30,
};

static struct eapol_key_data eapol_key_test_27 = {
        .frame = eapol_key_data_27,
        .frame_len = sizeof(eapol_key_data_27),
        .protocol_version = EAPOL_PROTOCOL_VERSION_2004,
        .packet_len = 287,
        .descriptor_type = EAPOL_DESCRIPTOR_TYPE_80211,
        .key_descriptor_version = EAPOL_KEY_DESCRIPTOR_VERSION_AES_128_CMAC_AES,
        .key_type = true,
        .install = true,
        .key_ack = true,
        .key_mic = true,
        .secure = true,
        .error = false,
        .request = false,
        .encrypted_key_data = true,
        .smk_message = false,
        .key_length = 16,
        .key_replay_counter = 2,
        .key_nonce = { 0x33, 0xb2, 0x74, 0xa0, 0xae, 0xc9, 0xe8, 0x5d, 0x61,
                       0x11, 0x8f, 0x1b, 0x6b, 0x97, 0x77, 0x4e, 0x5b, 0x75,
                       0x08, 0x37, 0x45, 0x77, 0xdc, 0x14, 0x08, 0xa5, 0xf1,
                       0x80, 0xc5, 0xd2, 0xe9, 0xfd },
        .eapol_key_iv = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_rsc = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_mic_data = { 0x12, 0xe3, 0x6d, 0xef, 0x96, 0x5b, 0x07, 0xaa, 0x58,
                          0xce, 0xed, 0xb1, 0x57, 0x83, 0x51, 0x7e },
        .key_data_len = 192,
};

/* FT frame, 4 of 4.  For parameters see eapol_ft_handshake_test */
static const unsigned char eapol_key_data_28[] = {
        0x02, 0x03, 0x00, 0x5f, 0x02, 0x03, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x85, 0x3a,
        0x3c, 0x83, 0x71, 0x4a, 0x94, 0xe4, 0x30, 0xdf, 0xb2, 0x06, 0x65, 0xa1,
        0xd4, 0x00, 0x00,
};

static struct eapol_key_data eapol_key_test_28 = {
        .frame = eapol_key_data_28,
        .frame_len = sizeof(eapol_key_data_28),
        .protocol_version = EAPOL_PROTOCOL_VERSION_2004,
        .packet_len = 95,
        .descriptor_type = EAPOL_DESCRIPTOR_TYPE_80211,
        .key_descriptor_version = EAPOL_KEY_DESCRIPTOR_VERSION_AES_128_CMAC_AES,
        .key_type = true,
        .install = false,
        .key_ack = false,
        .key_mic = true,
        .secure = true,
        .error = false,
        .request = false,
        .encrypted_key_data = false,
        .smk_message = false,
        .key_length = 0,
        .key_replay_counter = 2,
        .key_nonce = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .eapol_key_iv = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_rsc = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_mic_data = { 0x3e, 0x85, 0x3a, 0x3c, 0x83, 0x71, 0x4a, 0x94, 0xe4,
                          0x30, 0xdf, 0xb2, 0x06, 0x65, 0xa1, 0xd4 },
        .key_data_len = 0,
};

/* WPA frame, 1 of 4.  For parameters see eapol_sm_test_igtk */
static const unsigned char eapol_key_data_29[] = {
        0x02, 0x03, 0x00, 0x5f, 0x02, 0x00, 0x89, 0x00, 0x20, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x30, 0x37, 0x86,
        0x8d, 0x6c, 0xd2, 0x38, 0xb2, 0xfe, 0xb4, 0x5b, 0xd3, 0xc6,
        0x4b, 0xa1, 0x3e, 0x26, 0xd9, 0xa4, 0x89, 0x8b, 0x43, 0xf6,
        0x66, 0x51, 0x26, 0x99, 0x5e, 0x62, 0xce, 0x8e, 0x9d, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static struct eapol_key_data eapol_key_test_29 = {
        .frame = eapol_key_data_29,
        .frame_len = sizeof(eapol_key_data_29),
        .protocol_version = EAPOL_PROTOCOL_VERSION_2004,
        .packet_len = 95,
        .descriptor_type = EAPOL_DESCRIPTOR_TYPE_80211,
        .key_descriptor_version = EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_MD5_ARC4,
        .key_type = true,
        .install = false,
        .key_ack = true,
        .key_mic = false,
        .secure = false,
        .error = false,
        .request = false,
        .encrypted_key_data = false,
        .smk_message = false,
        .key_length = 32,
        .key_replay_counter = 1,
        .key_nonce = { 0x30, 0x37, 0x86, 0x8d, 0x6c, 0xd2, 0x38, 0xb2, 0xfe,
                       0xb4, 0x5b, 0xd3, 0xc6, 0x4b, 0xa1, 0x3e, 0x26, 0xd9,
                       0xa4, 0x89, 0x8b, 0x43, 0xf6, 0x66, 0x51, 0x26, 0x99,
                       0x5e, 0x62, 0xce, 0x8e, 0x9d},
        .eapol_key_iv = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_rsc = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_mic_data = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, },
        .key_data_len = 0,
};

/* WPA frame, 2 of 4.  For parameters see eapol_sm_test_igtk */
static const unsigned char eapol_key_data_30[] = {
        0x01, 0x03, 0x00, 0x7b, 0x02, 0x01, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x01, 0x3e, 0x5e, 0xb7, 0x47, 0x91, 0xf4, 0x2a,
        0x39, 0x3a, 0x6a, 0xbc, 0xeb, 0x9c, 0x25, 0x27, 0x0f, 0x61, 0xb4, 0x24,
        0x8c, 0xf2, 0x97, 0xdf, 0x22, 0xef, 0x67, 0x15, 0x87, 0xad, 0x22, 0xc3,
        0xd8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x92, 0x76, 0xec,
        0x87, 0x1e, 0x42, 0x7a, 0x66, 0x3f, 0x45, 0xb2, 0x7f, 0x7c, 0xd7, 0xe3,
        0xb9, 0x00, 0x1c, 0x30, 0x1a, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x02, 0x01,
        0x00, 0x00, 0x0f, 0xac, 0x02, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x02, 0x80,
        0x00, 0x00, 0x00, 0x00, 0x0f, 0xac, 0x06,
};

static struct eapol_key_data eapol_key_test_30 = {
        .frame = eapol_key_data_30,
        .frame_len = sizeof(eapol_key_data_30),
        .protocol_version = EAPOL_PROTOCOL_VERSION_2001,
        .packet_len = 123,
        .descriptor_type = EAPOL_DESCRIPTOR_TYPE_80211,
        .key_descriptor_version = EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_MD5_ARC4,
        .key_type = true,
        .install = false,
        .key_ack = false,
        .key_mic = true,
        .secure = false,
        .error = false,
        .request = false,
        .encrypted_key_data = false,
        .smk_message = false,
        .key_length = 0,
        .key_replay_counter = 1,
        .key_nonce = { 0x3e, 0x5e, 0xb7, 0x47, 0x91, 0xf4, 0x2a, 0x39, 0x3a,
                       0x6a, 0xbc, 0xeb, 0x9c, 0x25, 0x27, 0x0f, 0x61, 0xb4,
                       0x24, 0x8c, 0xf2, 0x97, 0xdf, 0x22, 0xef, 0x67, 0x15,
                       0x87, 0xad, 0x22, 0xc3, 0xd8 },
        .eapol_key_iv = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_rsc = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_mic_data = { 0x92, 0x76, 0xec, 0x87, 0x1e, 0x42, 0x7a, 0x66, 0x3f,
                          0x45, 0xb2, 0x7f, 0x7c, 0xd7, 0xe3, 0xb9 },
        .key_data_len = 28,
};

/* WPA frame, 3 of 4.  For parameters see eapol_sm_test_igtk */
static const unsigned char eapol_key_data_31[] = {
        0x02, 0x03, 0x00, 0xd3, 0x02, 0x13, 0xc9, 0x00, 0x20, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x02, 0x30, 0x37, 0x86, 0x8d, 0x6c, 0xd2, 0x38,
        0xb2, 0xfe, 0xb4, 0x5b, 0xd3, 0xc6, 0x4b, 0xa1, 0x3e, 0x26, 0xd9, 0xa4,
        0x89, 0x8b, 0x43, 0xf6, 0x66, 0x51, 0x26, 0x99, 0x5e, 0x62, 0xce, 0x8e,
        0x9d, 0x92, 0xcf, 0x64, 0xa6, 0xf5, 0xea, 0x95, 0xf7, 0xf9, 0xeb, 0x6a,
        0x54, 0x8a, 0x85, 0x6c, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x80, 0xb3,
        0x14, 0x1f, 0xfa, 0x11, 0x47, 0xcd, 0x6d, 0xd0, 0x20, 0x7e, 0x9e, 0x68,
        0x65, 0x00, 0x74, 0x39, 0xf4, 0xc9, 0x3a, 0xf3, 0xac, 0xf5, 0xd3, 0x98,
        0xeb, 0xaf, 0x3c, 0x0f, 0xf1, 0xb5, 0x33, 0xff, 0xb2, 0x00, 0x1b, 0xe4,
        0x2c, 0x61, 0xaf, 0xff, 0x1c, 0x22, 0x76, 0x07, 0x3b, 0xbc, 0x0d, 0x0c,
        0xeb, 0x8a, 0xdc, 0xcd, 0x47, 0x01, 0xa5, 0x6e, 0x76, 0x77, 0x85, 0x6f,
        0x09, 0x43, 0x83, 0xee, 0x50, 0x6e, 0x5e, 0xb1, 0x24, 0xe3, 0x47, 0xef,
        0x20, 0x5e, 0x5c, 0x10, 0x7a, 0xe3, 0x61, 0x69, 0x7b, 0xb0, 0xf6, 0xdd,
        0x42, 0x1a, 0xe1, 0xc9, 0x33, 0xd6, 0xd3, 0x88, 0x40, 0xcc, 0x72, 0x28,
        0x86, 0xce, 0xec, 0xea, 0xc0, 0xea, 0xc9, 0xcf, 0xe1, 0x93, 0x8b, 0x15,
        0x5e, 0xbb, 0x1f, 0xf9, 0x6f, 0x10, 0x34, 0xa5, 0xfc, 0x61, 0x78, 0x77,
        0xa7, 0xb1, 0x4d, 0xc4, 0x36, 0xea, 0x2f, 0x1d, 0xda, 0x31, 0xa1,
};

static struct eapol_key_data eapol_key_test_31 = {
        .frame = eapol_key_data_31,
        .frame_len = sizeof(eapol_key_data_31),
        .protocol_version = EAPOL_PROTOCOL_VERSION_2004,
        .packet_len = 211,
        .descriptor_type = EAPOL_DESCRIPTOR_TYPE_80211,
        .key_descriptor_version = EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_MD5_ARC4,
        .key_type = true,
        .install = true,
        .key_ack = true,
        .key_mic = true,
        .secure = true,
        .error = false,
        .request = false,
        .encrypted_key_data = true,
        .smk_message = false,
        .key_length = 32,
        .key_replay_counter = 2,
        .key_nonce = { 0x30, 0x37, 0x86, 0x8d, 0x6c, 0xd2, 0x38, 0xb2, 0xfe,
                       0xb4, 0x5b, 0xd3, 0xc6, 0x4b, 0xa1, 0x3e, 0x26, 0xd9,
                       0xa4, 0x89, 0x8b, 0x43, 0xf6, 0x66, 0x51, 0x26, 0x99,
                       0x5e, 0x62, 0xce, 0x8e, 0x9d },
        .eapol_key_iv = { 0x92, 0xcf, 0x64, 0xa6, 0xf5, 0xea, 0x95, 0xf7, 0xf9,
                          0xeb, 0x6a, 0x54, 0x8a, 0x85, 0x6c, 0x1c},
        .key_rsc = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_mic_data = { 0x1f, 0x80, 0xb3, 0x14, 0x1f, 0xfa, 0x11, 0x47, 0xcd,
                          0x6d, 0xd0, 0x20, 0x7e, 0x9e, 0x68, 0x65 },
        .key_data_len = 116,
};

/* WPA frame, 4 of 4.  For parameters see eapol_sm_test_igtk */
static const unsigned char eapol_key_data_32[] = {
        0x01, 0x03, 0x00, 0x5f, 0x02, 0x03, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x66, 0x2c, 0x07,
        0x9b, 0x73, 0xb6, 0x94, 0xb9, 0x4b, 0x7f, 0xa1, 0x99, 0x2f, 0x7a, 0x92,
        0xe2, 0x00, 0x00
};

static struct eapol_key_data eapol_key_test_32 = {
        .frame = eapol_key_data_32,
        .frame_len = sizeof(eapol_key_data_32),
        .protocol_version = EAPOL_PROTOCOL_VERSION_2001,
        .packet_len = 95,
        .descriptor_type = EAPOL_DESCRIPTOR_TYPE_80211,
        .key_descriptor_version = EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_MD5_ARC4,
        .key_type = true,
        .install = false,
        .key_ack = false,
        .key_mic = true,
        .secure = true,
        .error = false,
        .request = false,
        .encrypted_key_data = false,
        .smk_message = false,
        .key_length = 0,
        .key_replay_counter = 2,
        .key_nonce = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .eapol_key_iv = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_rsc = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        .key_mic_data = { 0x66, 0x2c, 0x07, 0x9b, 0x73, 0xb6, 0x94, 0xb9,
                          0x4b, 0x7f, 0xa1, 0x99, 0x2f, 0x7a, 0x92, 0xe2  },
        .key_data_len = 0,
};

static void eapol_key_test(const void *data)
{
    const struct eapol_key_data *test = data;
    const struct eapol_key *packet;

    packet = eapol_key_validate(test->frame, test->frame_len);
    assert(packet);

    assert(packet->header.protocol_version == test->protocol_version);
    assert(packet->header.packet_type == 0x03);
    assert(L_BE16_TO_CPU(packet->header.packet_len) == test->packet_len);
    assert(packet->descriptor_type == test->descriptor_type);
    assert(packet->key_descriptor_version == test->key_descriptor_version);
    assert(packet->key_type == test->key_type);
    assert(packet->wpa_key_id == test->wpa_key_id);
    assert(packet->install == test->install);
    assert(packet->key_ack == test->key_ack);
    assert(packet->key_mic == test->key_mic);
    assert(packet->secure == test->secure);
    assert(packet->error == test->error);
    assert(packet->request == test->request);
    assert(packet->encrypted_key_data == test->encrypted_key_data);
    assert(packet->smk_message == test->smk_message);
    assert(L_BE16_TO_CPU(packet->key_length) == test->key_length);
    assert(L_BE64_TO_CPU(packet->key_replay_counter) ==
           test->key_replay_counter);
    assert(!memcmp(packet->key_nonce, test->key_nonce,
                   sizeof(packet->key_nonce)));
    assert(!memcmp(packet->eapol_key_iv, test->eapol_key_iv,
                   sizeof(packet->eapol_key_iv)));
    assert(!memcmp(packet->key_mic_data, test->key_mic_data,
                   sizeof(packet->key_mic_data)));
    assert(!memcmp(packet->key_rsc, test->key_rsc,
                   sizeof(packet->key_rsc)));
    assert(L_BE16_TO_CPU(packet->key_data_len) == test->key_data_len);
}

struct eapol_key_mic_test {
    const uint8_t *frame;
    size_t frame_len;
    enum eapol_key_descriptor_version version;
    const uint8_t *kck;
    const uint8_t *mic;
};

static const uint8_t eapol_key_mic_data_1[] = {
        0x01, 0x03, 0x00, 0x75, 0x02, 0x01, 0x0a, 0x00,
        0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x01, 0x59, 0x16, 0x8b, 0xc3, 0xa5, 0xdf, 0x18,
        0xd7, 0x1e, 0xfb, 0x64, 0x23, 0xf3, 0x40, 0x08,
        0x8d, 0xab, 0x9e, 0x1b, 0xa2, 0xbb, 0xc5, 0x86,
        0x59, 0xe0, 0x7b, 0x37, 0x64, 0xb0, 0xde, 0x85,
        0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x16, 0x30, 0x14, 0x01, 0x00, 0x00,
        0x0f, 0xac, 0x04, 0x01, 0x00, 0x00, 0x0f, 0xac,
        0x04, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x02, 0x01,
        0x00,
};

static const uint8_t eapol_key_mic_1[] = {
        0x9c, 0xc3, 0xfa, 0xa0, 0xc6, 0x85, 0x96, 0x1d,
        0x84, 0x06, 0xbb, 0x65, 0x77, 0x45, 0x13, 0x5d,
};

static unsigned char kck_data_1[] = {
        0x9a, 0x75, 0xef, 0x0b, 0xde, 0x7c, 0x20, 0x9c,
        0xca, 0xe1, 0x3f, 0x54, 0xb1, 0xb3, 0x3e, 0xa3,
};

static const struct eapol_key_mic_test eapol_key_mic_test_1 = {
        .frame = eapol_key_mic_data_1,
        .frame_len = sizeof(eapol_key_mic_data_1),
        .version = EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_MD5_ARC4,
        .kck = kck_data_1,
        .mic = eapol_key_mic_1,
};

static const uint8_t eapol_key_mic_2[] = {
        0x6f, 0x04, 0x89, 0xcf, 0x74, 0x06, 0xac, 0xf0,
        0xae, 0x8f, 0xcb, 0x32, 0xbc, 0xe5, 0x7c, 0x37,
};

static const struct eapol_key_mic_test eapol_key_mic_test_2 = {
        .frame = eapol_key_mic_data_1,
        .frame_len = sizeof(eapol_key_mic_data_1),
        .version = EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_SHA1_AES,
        .kck = kck_data_1,
        .mic = eapol_key_mic_2,
};

static void eapol_key_mic_test(const void *data)
{
    const struct eapol_key_mic_test *test = data;
    uint8_t mic[16];

    memset(mic, 0, sizeof(mic));

    switch (test->version) {
        case EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_MD5_ARC4:
            assert(hmac_md5(test->kck, 16, test->frame, test->frame_len,
                            mic, 16));
            break;
        case EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_SHA1_AES:
            assert(hmac_sha1(test->kck, 16, test->frame, test->frame_len,
                             mic, 16));
            break;
        default:
            assert(false);
    }

    assert(!memcmp(test->mic, mic, sizeof(mic)));
}

struct eapol_calculate_mic_test {
    const uint8_t *frame;
    size_t frame_len;
    const uint8_t *kck;
    const uint8_t *mic;
};

static const struct eapol_calculate_mic_test eapol_calculate_mic_test_1 = {
        .frame = eapol_key_mic_data_1,
        .frame_len = sizeof(eapol_key_mic_data_1),
        .kck = kck_data_1,
        .mic = eapol_key_mic_2,
};

static void eapol_calculate_mic_test(const void *data)
{
    const struct eapol_calculate_mic_test *test = data;
    struct eapol_key *frame;
    uint8_t mic[16];
    bool ret;

    memset(mic, 0, sizeof(mic));
    frame = (struct eapol_key *) test->frame;

    ret = eapol_calculate_mic(IE_RSN_AKM_SUITE_PSK, test->kck, frame, mic);
    assert(ret);
    assert(!memcmp(test->mic, mic, sizeof(mic)));
}

static void eapol_4way_test(const void *data)
{
    uint8_t anonce[32];
    uint8_t snonce[32];
    uint8_t mic[16];
    struct eapol_key *frame;
    uint8_t aa[] = { 0x24, 0xa2, 0xe1, 0xec, 0x17, 0x04 };
    uint8_t spa[] = { 0xa0, 0xa8, 0xcd, 0x1c, 0x7e, 0xc9 };
    const char *passphrase = "EasilyGuessedPassword";
    const char *ssid = "TestWPA";
    const unsigned char expected_psk[] = {
            0xbf, 0x9a, 0xa3, 0x15, 0x53, 0x00, 0x12, 0x5e,
            0x7a, 0x5e, 0xbb, 0x2a, 0x54, 0x9f, 0x8c, 0xd4,
            0xed, 0xab, 0x8e, 0xe1, 0x2e, 0x94, 0xbf, 0xc2,
            0x4b, 0x33, 0x57, 0xad, 0x04, 0x96, 0x65, 0xd9 };
    unsigned char psk[32];
    struct crypto_ptk *ptk;
    size_t ptk_len;
    bool ret;
    const struct eapol_key *step1;
    const struct eapol_key *step2;
    const struct eapol_key *step3;
    const struct eapol_key *step4;
    uint8_t *decrypted_key_data;
    size_t decrypted_key_data_len;

    step1 = eapol_key_validate(eapol_key_data_3,
                               sizeof(eapol_key_data_3));
    assert(step1);
    assert(eapol_verify_ptk_1_of_4(step1));
    memcpy(anonce, step1->key_nonce, sizeof(step1->key_nonce));

    step2 = eapol_key_validate(eapol_key_data_4,
                               sizeof(eapol_key_data_4));
    assert(step2);
    assert(eapol_verify_ptk_2_of_4(step2));
    memcpy(snonce, step2->key_nonce, sizeof(step2->key_nonce));

    assert(!crypto_psk_from_passphrase(passphrase, (uint8_t *) ssid,
                                       strlen(ssid), psk));
    assert(!memcmp(expected_psk, psk, sizeof(psk)));

    ptk_len = sizeof(struct crypto_ptk) +
              crypto_cipher_key_len(CRYPTO_CIPHER_CCMP);
    ptk = l_malloc(ptk_len);
    ret = crypto_derive_pairwise_ptk(psk, aa, spa, anonce, snonce,
                                     ptk, ptk_len, false);
    assert(ret);

    frame = eapol_create_ptk_2_of_4(EAPOL_PROTOCOL_VERSION_2001,
                                    EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_SHA1_AES,
                                    eapol_key_test_4.key_replay_counter,
                                    snonce, eapol_key_test_4.key_data_len,
                                    eapol_key_data_4 + sizeof(struct eapol_key),
                                    false);
    assert(frame);
    assert(eapol_calculate_mic(IE_RSN_AKM_SUITE_PSK, ptk->kck, frame, mic));
    memcpy(frame->key_mic_data, mic, sizeof(mic));
    assert(!memcmp(frame, eapol_key_data_4, sizeof(eapol_key_data_4)));
    l_free(frame);

    step3 = eapol_key_validate(eapol_key_data_5,
                               sizeof(eapol_key_data_5));
    assert(step3);
    assert(eapol_verify_ptk_3_of_4(step3, false));
    assert(!memcmp(anonce, step3->key_nonce, sizeof(step3->key_nonce)));

    assert(eapol_verify_mic(IE_RSN_AKM_SUITE_PSK, ptk->kck, step3));

    decrypted_key_data = eapol_decrypt_key_data(IE_RSN_AKM_SUITE_PSK,
                                                ptk->kek, step3,
                                                &decrypted_key_data_len);
    assert(decrypted_key_data[0] == 48);  // RSNE
    l_free(decrypted_key_data);

    step4 = eapol_key_validate(eapol_key_data_6,
                               sizeof(eapol_key_data_6));
    assert(step4);
    assert(eapol_verify_ptk_4_of_4(step4, false));

    l_free(ptk);
}

static void eapol_wpa2_handshake_test(const void *data)
{
    uint8_t anonce[32];
    uint8_t snonce[32];
    uint8_t mic[16];
    struct eapol_key *frame;
    uint8_t aa[] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 };
    uint8_t spa[] = { 0x02, 0x00, 0x00, 0x00, 0x01, 0x00 };
    const char *passphrase = "EasilyGuessedPassword";
    const char *ssid = "TestWPA";
    const unsigned char expected_psk[] = {
            0xbf, 0x9a, 0xa3, 0x15, 0x53, 0x00, 0x12, 0x5e,
            0x7a, 0x5e, 0xbb, 0x2a, 0x54, 0x9f, 0x8c, 0xd4,
            0xed, 0xab, 0x8e, 0xe1, 0x2e, 0x94, 0xbf, 0xc2,
            0x4b, 0x33, 0x57, 0xad, 0x04, 0x96, 0x65, 0xd9 };
    unsigned char psk[32];
    struct crypto_ptk *ptk;
    size_t ptk_len;
    bool ret;
    const struct eapol_key *ptk_step1;
    const struct eapol_key *ptk_step2;
    const struct eapol_key *ptk_step3;
    const struct eapol_key *ptk_step4;
    const struct eapol_key *gtk_step1;
    const struct eapol_key *gtk_step2;
    uint8_t *decrypted_key_data;
    size_t decrypted_key_data_len;

    ptk_step1 = eapol_key_validate(eapol_key_data_7,
                                   sizeof(eapol_key_data_7));
    assert(ptk_step1);
    assert(eapol_verify_ptk_1_of_4(ptk_step1));
    memcpy(anonce, ptk_step1->key_nonce, sizeof(ptk_step1->key_nonce));

    ptk_step2 = eapol_key_validate(eapol_key_data_8,
                                   sizeof(eapol_key_data_8));
    assert(ptk_step2);
    assert(eapol_verify_ptk_2_of_4(ptk_step2));
    memcpy(snonce, ptk_step2->key_nonce, sizeof(ptk_step2->key_nonce));

    assert(!crypto_psk_from_passphrase(passphrase, (uint8_t *) ssid,
                                       strlen(ssid), psk));
    assert(!memcmp(expected_psk, psk, sizeof(psk)));

    ptk_len = sizeof(struct crypto_ptk) +
              crypto_cipher_key_len(CRYPTO_CIPHER_CCMP);
    ptk = l_malloc(ptk_len);
    ret = crypto_derive_pairwise_ptk(psk, aa, spa, anonce, snonce,
                                     ptk, ptk_len, false);
    assert(ret);

    frame = eapol_create_ptk_2_of_4(EAPOL_PROTOCOL_VERSION_2004,
                                    EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_SHA1_AES,
                                    eapol_key_test_8.key_replay_counter,
                                    snonce, eapol_key_test_8.key_data_len,
                                    eapol_key_data_8 + sizeof(struct eapol_key),
                                    false);
    assert(frame);
    assert(eapol_calculate_mic(IE_RSN_AKM_SUITE_PSK, ptk->kck, frame, mic));
    memcpy(frame->key_mic_data, mic, sizeof(mic));
    assert(!memcmp(frame, eapol_key_data_8, sizeof(eapol_key_data_8)));
    l_free(frame);

    ptk_step3 = eapol_key_validate(eapol_key_data_9,
                                   sizeof(eapol_key_data_9));
    assert(ptk_step3);
    assert(eapol_verify_ptk_3_of_4(ptk_step3, false));
    assert(!memcmp(anonce, ptk_step3->key_nonce,
                   sizeof(ptk_step3->key_nonce)));

    assert(eapol_verify_mic(IE_RSN_AKM_SUITE_PSK, ptk->kck, ptk_step3));

    decrypted_key_data = eapol_decrypt_key_data(IE_RSN_AKM_SUITE_PSK,
                                                ptk->kek, ptk_step3,
                                                &decrypted_key_data_len);
    assert(decrypted_key_data[0] == 48);  // RSNE
    l_free(decrypted_key_data);

    ptk_step4 = eapol_key_validate(eapol_key_data_10,
                                   sizeof(eapol_key_data_10));
    assert(ptk_step4);
    assert(eapol_verify_ptk_4_of_4(ptk_step4, false));

    frame = eapol_create_ptk_4_of_4(EAPOL_PROTOCOL_VERSION_2004,
                                    EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_SHA1_AES,
                                    eapol_key_test_10.key_replay_counter, false);
    assert(frame);
    assert(eapol_calculate_mic(IE_RSN_AKM_SUITE_PSK, ptk->kck, frame, mic));
    memcpy(frame->key_mic_data, mic, sizeof(mic));
    assert(!memcmp(frame, eapol_key_data_10, sizeof(eapol_key_data_10)));
    l_free(frame);

    gtk_step1 = eapol_key_validate(eapol_key_data_11,
                                   sizeof(eapol_key_data_11));
    assert(gtk_step1);
    assert(eapol_verify_gtk_1_of_2(gtk_step1, false));

    decrypted_key_data = eapol_decrypt_key_data(IE_RSN_AKM_SUITE_PSK,
                                                ptk->kek, gtk_step1,
                                                &decrypted_key_data_len);
    assert(decrypted_key_data[0] == 221);  /* GTK KDE */
    assert(decrypted_key_data[2] == 0x00);
    assert(decrypted_key_data[3] == 0x0f);
    assert(decrypted_key_data[4] == 0xac);
    assert(decrypted_key_data[5] == 0x01);
    l_free(decrypted_key_data);

    gtk_step2 = eapol_key_validate(eapol_key_data_12,
                                   sizeof(eapol_key_data_12));
    assert(gtk_step2);
    assert(eapol_verify_gtk_2_of_2(gtk_step2, false));

    frame = eapol_create_gtk_2_of_2(EAPOL_PROTOCOL_VERSION_2004,
                                    EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_SHA1_AES,
                                    eapol_key_test_12.key_replay_counter, false, 0);
    assert(frame);
    assert(eapol_calculate_mic(IE_RSN_AKM_SUITE_PSK, ptk->kck, frame, mic));
    memcpy(frame->key_mic_data, mic, sizeof(mic));
    assert(!memcmp(frame, eapol_key_data_12, sizeof(eapol_key_data_12)));
    l_free(frame);

    l_free(ptk);
}

static void eapol_wpa_handshake_test(const void *data)
{
    uint8_t anonce[32];
    uint8_t snonce[32];
    uint8_t mic[16];
    struct eapol_key *frame;
    uint8_t aa[] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 };
    uint8_t spa[] = { 0x02, 0x00, 0x00, 0x00, 0x01, 0x00 };
    const char *passphrase = "EasilyGuessedPassword";
    const char *ssid = "TestWPA";
    const unsigned char expected_psk[] = {
            0xbf, 0x9a, 0xa3, 0x15, 0x53, 0x00, 0x12, 0x5e,
            0x7a, 0x5e, 0xbb, 0x2a, 0x54, 0x9f, 0x8c, 0xd4,
            0xed, 0xab, 0x8e, 0xe1, 0x2e, 0x94, 0xbf, 0xc2,
            0x4b, 0x33, 0x57, 0xad, 0x04, 0x96, 0x65, 0xd9 };
    unsigned char psk[32];
    struct crypto_ptk *ptk;
    size_t ptk_len;
    bool ret;
    const struct eapol_key *ptk_step1;
    const struct eapol_key *ptk_step2;
    const struct eapol_key *ptk_step3;
    const struct eapol_key *ptk_step4;
    const struct eapol_key *gtk_step1;
    const struct eapol_key *gtk_step2;
    uint8_t *decrypted_key_data;
    size_t decrypted_key_data_len;

    ptk_step1 = eapol_key_validate(eapol_key_data_13,
                                   sizeof(eapol_key_data_13));
    assert(ptk_step1);
    assert(eapol_verify_ptk_1_of_4(ptk_step1));
    memcpy(anonce, ptk_step1->key_nonce, sizeof(ptk_step1->key_nonce));

    ptk_step2 = eapol_key_validate(eapol_key_data_14,
                                   sizeof(eapol_key_data_14));
    assert(ptk_step2);
    assert(eapol_verify_ptk_2_of_4(ptk_step2));
    memcpy(snonce, ptk_step2->key_nonce, sizeof(ptk_step2->key_nonce));

    assert(!crypto_psk_from_passphrase(passphrase, (uint8_t *) ssid,
                                       strlen(ssid), psk));
    assert(!memcmp(expected_psk, psk, sizeof(psk)));

    ptk_len = sizeof(struct crypto_ptk) +
              crypto_cipher_key_len(CRYPTO_CIPHER_TKIP);
    ptk = l_malloc(ptk_len);
    ret = crypto_derive_pairwise_ptk(psk, aa, spa, anonce, snonce,
                                     ptk, ptk_len, false);
    assert(ret);

    frame = eapol_create_ptk_2_of_4(EAPOL_PROTOCOL_VERSION_2004,
                                    EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_MD5_ARC4,
                                    eapol_key_test_14.key_replay_counter,
                                    snonce, eapol_key_test_14.key_data_len,
                                    eapol_key_data_14 + sizeof(struct eapol_key),
                                    true);
    assert(frame);
    assert(eapol_calculate_mic(IE_RSN_AKM_SUITE_PSK, ptk->kck, frame, mic));
    memcpy(frame->key_mic_data, mic, sizeof(mic));
    assert(!memcmp(frame, eapol_key_data_14, sizeof(eapol_key_data_14)));
    l_free(frame);

    ptk_step3 = eapol_key_validate(eapol_key_data_15,
                                   sizeof(eapol_key_data_15));
    assert(ptk_step3);
    assert(eapol_verify_ptk_3_of_4(ptk_step3, true));
    assert(!memcmp(anonce, ptk_step3->key_nonce,
                   sizeof(ptk_step3->key_nonce)));

    assert(eapol_verify_mic(IE_RSN_AKM_SUITE_PSK, ptk->kck, ptk_step3));

    assert(ptk_step3->key_data[0] == IE_TYPE_VENDOR_SPECIFIC);
    assert(is_ie_wpa_ie(ptk_step3->key_data + 2,
                        ptk_step3->key_data_len - 2));

    ptk_step4 = eapol_key_validate(eapol_key_data_16,
                                   sizeof(eapol_key_data_16));
    assert(ptk_step4);
    assert(eapol_verify_ptk_4_of_4(ptk_step4, true));

    frame = eapol_create_ptk_4_of_4(EAPOL_PROTOCOL_VERSION_2004,
                                    EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_MD5_ARC4,
                                    eapol_key_test_16.key_replay_counter, true);
    assert(frame);
    assert(eapol_calculate_mic(IE_RSN_AKM_SUITE_PSK, ptk->kck, frame, mic));
    memcpy(frame->key_mic_data, mic, sizeof(mic));
    assert(!memcmp(frame, eapol_key_data_16, sizeof(eapol_key_data_16)));
    l_free(frame);

    gtk_step1 = eapol_key_validate(eapol_key_data_17,
                                   sizeof(eapol_key_data_17));
    assert(gtk_step1);
    assert(eapol_verify_gtk_1_of_2(gtk_step1, true));

    decrypted_key_data = eapol_decrypt_key_data(IE_RSN_AKM_SUITE_PSK,
                                                ptk->kek, gtk_step1,
                                                &decrypted_key_data_len);
    assert(decrypted_key_data_len == 32);
    l_free(decrypted_key_data);

    gtk_step2 = eapol_key_validate(eapol_key_data_18,
                                   sizeof(eapol_key_data_18));
    assert(gtk_step2);
    assert(eapol_verify_gtk_2_of_2(gtk_step2, true));

    frame = eapol_create_gtk_2_of_2(EAPOL_PROTOCOL_VERSION_2004,
                                    EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_MD5_ARC4,
                                    eapol_key_test_18.key_replay_counter, true,
                                    gtk_step1->wpa_key_id);
    assert(frame);
    assert(eapol_calculate_mic(IE_RSN_AKM_SUITE_PSK, ptk->kck, frame, mic));
    memcpy(frame->key_mic_data, mic, sizeof(mic));
    assert(!memcmp(frame, eapol_key_data_18, sizeof(eapol_key_data_18)));
    l_free(frame);

    l_free(ptk);
}

static int verify_step2(uint32_t ifindex,
                        const uint8_t *aa_addr, uint16_t proto,
                        const struct eapol_frame *ef, bool noencrypt,
                        void *user_data)
{

    printf("\nXXX function verify_step2 called XXX \n");
    const struct eapol_key *ek = (const struct eapol_key *) ef;
    size_t ek_len = sizeof(struct eapol_key) +
                    L_BE16_TO_CPU(ek->key_data_len);



    assert(ifindex == 1);
    assert(proto == ETH_P_PAE);
    assert(!memcmp(aa_addr, aa, 6));

    if((ek_len != expected_step2_frame_size)){
        printf("step2 different frame size\n");
       // assert(false);
        exit(1);
    }
    if(memcmp(ek, expected_step2_frame, expected_step2_frame_size)){
        printf("step2 memcmp failed\n");
       // assert(false);
        exit(1);

    }

  //  assert(ek_len == expected_step2_frame_size);
  //  assert(!memcmp(ek, expected_step2_frame, expected_step2_frame_size));

    verify_step2_called = true;

    return 0;
}

static int verify_step4(uint32_t ifindex,
                        const uint8_t *aa_addr, uint16_t proto,
                        const struct eapol_frame *ef, bool noencrypt,
                        void *user_data)
{
    printf("\nXXX function verify_step4 called XXX\n");
    const struct eapol_key *ek = (const struct eapol_key *) ef;
    size_t ek_len = sizeof(struct eapol_key) +
                    L_BE16_TO_CPU(ek->key_data_len);


    assert(ifindex == 1);
    assert(!memcmp(aa_addr, aa, 6));
    assert(proto == ETH_P_PAE);

    if((ek_len == expected_step4_frame_size)){
        printf("step4 different frame size\n");
       // assert(false);
        exit(1);
    }
    if(memcmp(ek, expected_step2_frame, expected_step2_frame_size)){
        printf("step4 memcmp failed\n");
        //assert(false);
        exit(1);
    }

//       assert(ek_len == expected_step4_frame_size);
  //  assert(!memcmp(ek, expected_step4_frame, expected_step4_frame_size));

    verify_step4_called = true;


    return 0;
}

static int verify_step2_gtk(uint32_t ifindex,
                            const uint8_t *aa_addr, uint16_t proto,
                            const struct eapol_frame *ef, bool noencrypt,
                            void *user_data)
{
    const struct eapol_key *ek = (const struct eapol_key *) ef;
    size_t ek_len = sizeof(struct eapol_key) +
                    L_BE16_TO_CPU(ek->key_data_len);

    assert(ifindex == 1);
    assert(!memcmp(aa_addr, aa, 6));
    assert(proto == ETH_P_PAE);
    assert(ek_len == expected_gtk_step2_frame_size);
    assert(!memcmp(ek, expected_gtk_step2_frame, expected_gtk_step2_frame_size));

    verify_gtk_step2_called = true;

    return 0;
}

static bool test_nonce(uint8_t nonce[])
{
    memcpy(nonce, snonce, 32);

    return true;
}



//XXX FUNCTION UNDER ANALYSIS
static void eapol_sm_test_ptk(const void *data)
{
    const unsigned char psk[] = {
            0xbf, 0x9a, 0xa3, 0x15, 0x53, 0x00, 0x12, 0x5e,
            0x7a, 0x5e, 0xbb, 0x2a, 0x54, 0x9f, 0x8c, 0xd4,
            0xed, 0xab, 0x8e, 0xe1, 0x2e, 0x94, 0xbf, 0xc2,
            0x4b, 0x33, 0x57, 0xad, 0x04, 0x96, 0x65, 0xd9 };
    const unsigned char ap_rsne[] = {
            0x30, 0x14, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x04,
            0x01, 0x00, 0x00, 0x0f, 0xac, 0x04, 0x01, 0x00,
            0x00, 0x0f, 0xac, 0x02, 0x00, 0x00 };
    static uint8_t ap_address[] = { 0x24, 0xa2, 0xe1, 0xec, 0x17, 0x04 };
    static uint8_t sta_address[] = { 0xa0, 0xa8, 0xcd, 0x1c, 0x7e, 0xc9 };
    bool r;
    struct handshake_state *hs;
    struct eapol_sm *sm;

    eapol_init();
    ///calling afl to read data from the input file

    printf("--- DEBUG IN eapol_sm_test_ptk --- \n\n");
    __afl_get_key_data();
    //create_eapol_key_data_afl();

    snonce = eapol_key_test_4.key_nonce ;
    __handshake_set_get_nonce_func(test_nonce);


    aa = ap_address;
    spa = sta_address;
    verify_step2_called = false;
    verify_step2_called = false;
    expected_step2_frame = eapol_key_data_4;
    expected_step2_frame_size = sizeof(eapol_key_data_4);
    verify_step4_called = false;
    expected_step4_frame = eapol_key_data_6;
    expected_step4_frame_size = sizeof(eapol_key_data_6);

    hs = test_handshake_state_new(1);
    sm = eapol_sm_new(hs);
    eapol_register(sm);

    /* key_data_3 uses 2004 while key_data_3 uses 2001, so force 2001 */
    eapol_sm_set_protocol_version(sm, EAPOL_PROTOCOL_VERSION_2001);

    handshake_state_set_pmk(hs, psk, sizeof(psk));
    handshake_state_set_authenticator_address(hs, aa);
    handshake_state_set_supplicant_address(hs, spa);


    r =  handshake_state_set_supplicant_rsn(hs,eapol_key_data_4 + sizeof(struct eapol_key));

    assert(r);
/*
    if(!r){
        printf("\nDEBUG INFO : handshake_state_set_supplicant failed\n");
    }
*/
    handshake_state_set_authenticator_rsn(hs, ap_rsne);
    eapol_start(sm);

    //XXX msg3 ---
    __eapol_set_tx_packet_func(verify_step2);
    __eapol_rx_packet(1, aa, ETH_P_PAE,__afl_key, len_frame1, false);
    //XXX the following assert is commented because it cannot be verified in any case

    if(verify_step2_called == true){
        printf("step 2 true\n");
    }

//     assert(verify_step2_called);

    //XXX msg4 ---

    printf("\n\n XXX Working on second frame XXX \n\n");


    __eapol_set_tx_packet_func(verify_step4);
    __eapol_rx_packet(1, aa, ETH_P_PAE, __afl_key1, len_frame2, false);
    //assert(verify_step4_called);

    if(verify_step4_called == true){
        printf("step 4 true\n");
    }

    eapol_sm_free(sm);
    handshake_state_free(hs);
    printf("Exit from test\n");
    eapol_exit();
}

/*
static void eapol_sm_test_ptk(const void *data)
{
    const unsigned char psk[] = {
            0xbf, 0x9a, 0xa3, 0x15, 0x53, 0x00, 0x12, 0x5e,
            0x7a, 0x5e, 0xbb, 0x2a, 0x54, 0x9f, 0x8c, 0xd4,
            0xed, 0xab, 0x8e, 0xe1, 0x2e, 0x94, 0xbf, 0xc2,
            0x4b, 0x33, 0x57, 0xad, 0x04, 0x96, 0x65, 0xd9 };
    const unsigned char ap_rsne[] = {
            0x30, 0x14, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x04,
            0x01, 0x00, 0x00, 0x0f, 0xac, 0x04, 0x01, 0x00,
            0x00, 0x0f, 0xac, 0x02, 0x00, 0x00 };
    static uint8_t ap_address[] = { 0x24, 0xa2, 0xe1, 0xec, 0x17, 0x04 };
    static uint8_t sta_address[] = { 0xa0, 0xa8, 0xcd, 0x1c, 0x7e, 0xc9 };
    bool r;
    struct handshake_state *hs;
    struct eapol_sm *sm;

    eapol_init();

    snonce = eapol_key_test_4.key_nonce;
    __handshake_set_get_nonce_func(test_nonce);

    aa = ap_address;
    spa = sta_address;
    verify_step2_called = false;
    expected_step2_frame = eapol_key_data_4;
    expected_step2_frame_size = sizeof(eapol_key_data_4);
    verify_step4_called = false;
    expected_step4_frame = eapol_key_data_6;
    expected_step4_frame_size = sizeof(eapol_key_data_6);

    hs = test_handshake_state_new(1);
    sm = eapol_sm_new(hs);
    eapol_register(sm);

    // key_data_3 uses 2004 while key_data_3 uses 2001, so force 2001
    eapol_sm_set_protocol_version(sm, EAPOL_PROTOCOL_VERSION_2001);

    handshake_state_set_pmk(hs, psk, sizeof(psk));
    handshake_state_set_authenticator_address(hs, aa);
    handshake_state_set_supplicant_address(hs, spa);

    r =  handshake_state_set_supplicant_rsn(hs,
                                            eapol_key_data_4 + sizeof(struct eapol_key));
    assert(r);

    handshake_state_set_authenticator_rsn(hs, ap_rsne);
    eapol_start(sm);

    __eapol_set_tx_packet_func(verify_step2);
    __eapol_rx_packet(1, aa, ETH_P_PAE, eapol_key_data_3,
                      sizeof(eapol_key_data_3), false);
    assert(verify_step2_called);

    __eapol_set_tx_packet_func(verify_step4);
    __eapol_rx_packet(1, aa, ETH_P_PAE, eapol_key_data_5,
                      sizeof(eapol_key_data_5), false);
    assert(verify_step4_called);

    eapol_sm_free(sm);
    handshake_state_free(hs);
    eapol_exit();
}
*/


static void eapol_sm_test_igtk(const void *data)
{
    const unsigned char psk[] = {
            0xbf, 0x9a, 0xa3, 0x15, 0x53, 0x00, 0x12, 0x5e,
            0x7a, 0x5e, 0xbb, 0x2a, 0x54, 0x9f, 0x8c, 0xd4,
            0xed, 0xab, 0x8e, 0xe1, 0x2e, 0x94, 0xbf, 0xc2,
            0x4b, 0x33, 0x57, 0xad, 0x04, 0x96, 0x65, 0xd9 };
    const unsigned char ap_rsne[] = {
            0x30, 0x1a, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x02, 0x01, 0x00,
            0x00, 0x0f, 0xac, 0x02, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x02,
            0x80, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xac, 0x06 };
    static uint8_t ap_address[] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 };
    static uint8_t sta_address[] = { 0x02, 0x00, 0x00, 0x00, 0x01, 0x00 };
    bool r;
    struct handshake_state *hs;
    struct eapol_sm *sm;

    eapol_init();

    snonce = eapol_key_test_30.key_nonce;
    __handshake_set_get_nonce_func(test_nonce);

    aa = ap_address;
    spa = sta_address;
    verify_step2_called = false;
    expected_step2_frame = eapol_key_data_30;
    expected_step2_frame_size = sizeof(eapol_key_data_30);
    verify_step4_called = false;
    expected_step4_frame = eapol_key_data_32;
    expected_step4_frame_size = sizeof(eapol_key_data_32);

    hs = test_handshake_state_new(1);
    sm = eapol_sm_new(hs);
    eapol_register(sm);

    /* key_data_29 uses 2004 while key_data_30 uses 2001, so force 2001 */
    eapol_sm_set_protocol_version(sm, EAPOL_PROTOCOL_VERSION_2001);

    handshake_state_set_pmk(hs, psk, sizeof(psk));
    handshake_state_set_authenticator_address(hs, aa);
    handshake_state_set_supplicant_address(hs, spa);

    r =  handshake_state_set_supplicant_rsn(hs,
                                            eapol_key_data_30 + sizeof(struct eapol_key));
    assert(r);

    handshake_state_set_authenticator_rsn(hs, ap_rsne);
    eapol_start(sm);

    __eapol_set_tx_packet_func(verify_step2);
    __eapol_rx_packet(1, aa, ETH_P_PAE, eapol_key_data_29,
                      sizeof(eapol_key_data_29), false);
    assert(verify_step2_called);

    __eapol_set_tx_packet_func(verify_step4);
    __eapol_rx_packet(1, aa, ETH_P_PAE, eapol_key_data_31,
                      sizeof(eapol_key_data_31), false);
    assert(verify_step4_called);

    eapol_sm_free(sm);
    handshake_state_free(hs);

    eapol_exit();
}

static void eapol_sm_test_wpa2_ptk_gtk(const void *data)
{
    const unsigned char psk[] = {
            0xbf, 0x9a, 0xa3, 0x15, 0x53, 0x00, 0x12, 0x5e,
            0x7a, 0x5e, 0xbb, 0x2a, 0x54, 0x9f, 0x8c, 0xd4,
            0xed, 0xab, 0x8e, 0xe1, 0x2e, 0x94, 0xbf, 0xc2,
            0x4b, 0x33, 0x57, 0xad, 0x04, 0x96, 0x65, 0xd9 };
    const unsigned char ap_rsne[] = {
            0x30, 0x12, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x04,
            0x01, 0x00, 0x00, 0x0f, 0xac, 0x04, 0x01, 0x00,
            0x00, 0x0f, 0xac, 0x02 };
    static uint8_t ap_address[] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 };
    static uint8_t sta_address[] = { 0x02, 0x00, 0x00, 0x00, 0x01, 0x00 };
    bool r;
    struct handshake_state *hs;
    struct eapol_sm *sm;

    eapol_init();

    snonce = eapol_key_test_8.key_nonce;
    __handshake_set_get_nonce_func(test_nonce);

    aa = ap_address;
    spa = sta_address;
    verify_step2_called = false;
    expected_step2_frame = eapol_key_data_8;
    expected_step2_frame_size = sizeof(eapol_key_data_8);
    verify_step4_called = false;
    expected_step4_frame = eapol_key_data_10;
    expected_step4_frame_size = sizeof(eapol_key_data_10);
    verify_gtk_step2_called = false;
    expected_gtk_step2_frame = eapol_key_data_12;
    expected_gtk_step2_frame_size = sizeof(eapol_key_data_12);

    hs = test_handshake_state_new(1);
    sm = eapol_sm_new(hs);
    eapol_register(sm);

    handshake_state_set_pmk(hs, psk, sizeof(psk));
    handshake_state_set_authenticator_address(hs, aa);
    handshake_state_set_supplicant_address(hs, spa);

    r = handshake_state_set_supplicant_rsn(hs,
                                           eapol_key_data_8 + sizeof(struct eapol_key));
   // assert(r);

    handshake_state_set_authenticator_rsn(hs, ap_rsne);
    eapol_start(sm);

    __eapol_set_tx_packet_func(verify_step2);
    __eapol_rx_packet(1, aa, ETH_P_PAE, eapol_key_data_7,sizeof(eapol_key_data_7), false);
   // assert(verify_step2_called);

    __eapol_set_tx_packet_func(verify_step4);
    __eapol_rx_packet(1, aa, ETH_P_PAE, eapol_key_data_9,sizeof(eapol_key_data_9), false);
  //  assert(verify_step4_called);

    __eapol_set_tx_packet_func(verify_step2_gtk);
    __eapol_rx_packet(1, aa, ETH_P_PAE, eapol_key_data_11,sizeof(eapol_key_data_11), false);
   // assert(verify_gtk_step2_called);

    eapol_sm_free(sm);
    handshake_state_free(hs);
    eapol_exit();
}

static void eapol_sm_test_wpa_ptk_gtk(const void *data)
{
    const unsigned char psk[] = {
            0xbf, 0x9a, 0xa3, 0x15, 0x53, 0x00, 0x12, 0x5e,
            0x7a, 0x5e, 0xbb, 0x2a, 0x54, 0x9f, 0x8c, 0xd4,
            0xed, 0xab, 0x8e, 0xe1, 0x2e, 0x94, 0xbf, 0xc2,
            0x4b, 0x33, 0x57, 0xad, 0x04, 0x96, 0x65, 0xd9 };
    const unsigned char ap_wpa_ie[] = {
            0xdd, 0x16, 0x00, 0x50, 0xf2, 0x01, 0x01, 0x00,
            0x00, 0x50, 0xf2, 0x02, 0x01, 0x00, 0x00, 0x50,
            0xf2, 0x02, 0x01, 0x00, 0x00, 0x50, 0xf2, 0x02 };
    static uint8_t ap_address[] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 };
    static uint8_t sta_address[] = { 0x02, 0x00, 0x00, 0x00, 0x01, 0x00 };
    bool r;
    struct handshake_state *hs;
    struct eapol_sm *sm;

    eapol_init();
    snonce = eapol_key_test_14.key_nonce;
    __handshake_set_get_nonce_func(test_nonce);

    aa = ap_address;
    spa = sta_address;
    verify_step2_called = false;
    expected_step2_frame = eapol_key_data_14;
    expected_step2_frame_size = sizeof(eapol_key_data_14);
    verify_step4_called = false;
    expected_step4_frame = eapol_key_data_16;
    expected_step4_frame_size = sizeof(eapol_key_data_16);
    verify_gtk_step2_called = false;
    expected_gtk_step2_frame = eapol_key_data_18;
    expected_gtk_step2_frame_size = sizeof(eapol_key_data_18);

    hs = test_handshake_state_new(1);
    sm = eapol_sm_new(hs);
    eapol_register(sm);

    handshake_state_set_pmk(hs, psk, sizeof(psk));
    handshake_state_set_authenticator_address(hs, ap_address);
    handshake_state_set_supplicant_address(hs, sta_address);
    r = handshake_state_set_supplicant_wpa(hs,
                                           eapol_key_data_14 + sizeof(struct eapol_key));
    assert(r);

    handshake_state_set_authenticator_wpa(hs, ap_wpa_ie);
    eapol_start(sm);

    __eapol_set_tx_packet_func(verify_step2);
    __eapol_rx_packet(1, ap_address, ETH_P_PAE, eapol_key_data_13,
                      sizeof(eapol_key_data_13), false);
    assert(verify_step2_called);

    __eapol_set_tx_packet_func(verify_step4);
    __eapol_rx_packet(1, ap_address, ETH_P_PAE, eapol_key_data_15,
                      sizeof(eapol_key_data_15), false);
    assert(verify_step4_called);

    __eapol_set_tx_packet_func(verify_step2_gtk);
    __eapol_rx_packet(1, ap_address, ETH_P_PAE, eapol_key_data_17,
                      sizeof(eapol_key_data_17), false);
    assert(verify_gtk_step2_called);

    eapol_sm_free(sm);
    handshake_state_free(hs);
    eapol_exit();
}

static void eapol_sm_test_wpa_ptk_gtk_2(const void *data)
{
    const unsigned char psk[] = {
            0xbf, 0x9a, 0xa3, 0x15, 0x53, 0x00, 0x12, 0x5e,
            0x7a, 0x5e, 0xbb, 0x2a, 0x54, 0x9f, 0x8c, 0xd4,
            0xed, 0xab, 0x8e, 0xe1, 0x2e, 0x94, 0xbf, 0xc2,
            0x4b, 0x33, 0x57, 0xad, 0x04, 0x96, 0x65, 0xd9 };
    const unsigned char ap_wpa_ie[] = {
            0xdd, 0x16, 0x00, 0x50, 0xf2, 0x01, 0x01, 0x00,
            0x00, 0x50, 0xf2, 0x02, 0x01, 0x00, 0x00, 0x50,
            0xf2, 0x02, 0x01, 0x00, 0x00, 0x50, 0xf2, 0x02 };
    static uint8_t ap_address[] = { 0x24, 0xa2, 0xe1, 0xec, 0x17, 0x04 };
    static uint8_t sta_address[] = { 0xa0, 0xa8, 0xcd, 0x1c, 0x7e, 0xc9 };
    bool r;
    struct handshake_state *hs;
    struct eapol_sm *sm;

    eapol_init();
    snonce = eapol_key_test_20.key_nonce;
    __handshake_set_get_nonce_func(test_nonce);

    aa = ap_address;
    spa = sta_address;
    verify_step2_called = false;
    expected_step2_frame = eapol_key_data_20;
    expected_step2_frame_size = sizeof(eapol_key_data_20);
    verify_step4_called = false;
    expected_step4_frame = eapol_key_data_22;
    expected_step4_frame_size = sizeof(eapol_key_data_22);
    verify_gtk_step2_called = false;
    expected_gtk_step2_frame = eapol_key_data_24;
    expected_gtk_step2_frame_size = sizeof(eapol_key_data_24);

    hs = test_handshake_state_new(1);
    sm = eapol_sm_new(hs);
    eapol_register(sm);

    handshake_state_set_pmk(hs, psk, sizeof(psk));
    handshake_state_set_authenticator_address(hs, ap_address);
    handshake_state_set_supplicant_address(hs, sta_address);

    r = handshake_state_set_supplicant_wpa(hs,
                                           eapol_key_data_20 + sizeof(struct eapol_key));
    assert(r);

    handshake_state_set_authenticator_wpa(hs, ap_wpa_ie);
    eapol_start(sm);

    __eapol_set_tx_packet_func(verify_step2);
    __eapol_rx_packet(1, ap_address, ETH_P_PAE, eapol_key_data_19,
                      sizeof(eapol_key_data_19), false);
    assert(verify_step2_called);

    __eapol_set_tx_packet_func(verify_step4);
    __eapol_rx_packet(1, ap_address, ETH_P_PAE, eapol_key_data_21,
                      sizeof(eapol_key_data_21), false);
    assert(verify_step4_called);

    __eapol_set_tx_packet_func(verify_step2_gtk);
    __eapol_rx_packet(1, ap_address, ETH_P_PAE, eapol_key_data_23,
                      sizeof(eapol_key_data_23), false);
    assert(verify_gtk_step2_called);

    eapol_sm_free(sm);
    handshake_state_free(hs);
    eapol_exit();
}

static void verify_install_tk(struct handshake_state *hs,
                              const uint8_t *tk, uint32_t cipher)
{
    struct test_handshake_state *ths =
    container_of(hs, struct test_handshake_state, super);

    assert(hs->ifindex == 1);
    assert(!memcmp(hs->aa, aa, 6));

    if (ths->tk) {
        assert(!memcmp(tk, ths->tk, 32));
        assert(cipher == CRYPTO_CIPHER_TKIP);
    }

    verify_install_tk_called = true;
}

static void verify_install_gtk(struct handshake_state *hs, uint8_t key_index,
                               const uint8_t *gtk, uint8_t gtk_len,
                               const uint8_t *rsc, uint8_t rsc_len,
                               uint32_t cipher)
{
    assert(hs->ifindex == 1);
    verify_install_gtk_called = true;
}

static inline size_t EKL(const struct eapol_key *frame)
{
    return sizeof(*frame) + L_BE16_TO_CPU(frame->key_data_len);
}

static struct eapol_key *UPDATED_REPLAY_COUNTER(const struct eapol_key *frame,
                                                uint64_t replay_counter,
                                                struct crypto_ptk *ptk)
{
    struct eapol_key *ret = l_memdup(frame, EKL(frame));
    uint8_t mic[16];

    ret->key_replay_counter = L_CPU_TO_BE64(replay_counter);
    memset(ret->key_mic_data, 0, sizeof(ret->key_mic_data));
    eapol_calculate_mic(IE_RSN_AKM_SUITE_PSK, ptk->kck, ret, mic);
    memcpy(ret->key_mic_data, mic, sizeof(mic));

    return ret;
}

static void eapol_sm_wpa2_retransmit_test(const void *data)
{
    static uint8_t ap_address[] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 };
    static uint8_t sta_address[] = { 0x02, 0x00, 0x00, 0x00, 0x01, 0x00 };
    const char *passphrase = "EasilyGuessedPassword";
    const char *ssid = "TestWPA";
    unsigned char psk[32];
    const unsigned char ap_rsne[] = {
            0x30, 0x12, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x04,
            0x01, 0x00, 0x00, 0x0f, 0xac, 0x04, 0x01, 0x00,
            0x00, 0x0f, 0xac, 0x02 };
    uint8_t new_snonce[32];
    struct crypto_ptk *ptk;
    size_t ptk_len;
    struct eapol_key *expect;
    struct eapol_key *retransmit;
    bool r;
    const struct eapol_key *ptk_step1;
    const struct eapol_key *ptk_step2;
    const struct eapol_key *ptk_step3;
    const struct eapol_key *ptk_step4;
    const struct eapol_key *gtk_step1;
    const struct eapol_key *gtk_step2;
    struct handshake_state *hs;
    struct eapol_sm *sm;
    uint64_t replay_counter;

    aa = ap_address;
    spa = sta_address;

    crypto_psk_from_passphrase(passphrase, (uint8_t *) ssid,
                               strlen(ssid), psk);

    eapol_init();
    hs = test_handshake_state_new(1);
    sm = eapol_sm_new(hs);
    eapol_register(sm);

    handshake_state_set_pmk(hs, psk, 32);
    handshake_state_set_authenticator_address(hs, ap_address);
    handshake_state_set_supplicant_address(hs, sta_address);

    r = handshake_state_set_supplicant_rsn(hs,
                                           eapol_key_data_8 + sizeof(struct eapol_key));
    assert(r);

    handshake_state_set_authenticator_rsn(hs, ap_rsne);
    eapol_start(sm);

    ptk_step1 = eapol_key_validate(eapol_key_data_7,
                                   sizeof(eapol_key_data_7));
    assert(ptk_step1);

    ptk_step2 = eapol_key_validate(eapol_key_data_8,
                                   sizeof(eapol_key_data_8));
    assert(ptk_step2);

    replay_counter = L_BE64_TO_CPU(ptk_step1->key_replay_counter);
    snonce = ptk_step2->key_nonce;
    __handshake_set_get_nonce_func(test_nonce);
    __handshake_set_install_tk_func(verify_install_tk);
    __handshake_set_install_gtk_func(verify_install_gtk);

    ptk_len = sizeof(struct crypto_ptk) +
              crypto_cipher_key_len(CRYPTO_CIPHER_CCMP);
    ptk = l_malloc(ptk_len);
    assert(crypto_derive_pairwise_ptk(psk, aa, spa,
                                      ptk_step1->key_nonce,
                                      ptk_step2->key_nonce,
                                      ptk, ptk_len, false));

    verify_step2_called = false;
    expected_step2_frame = (const uint8_t *) ptk_step2;
    expected_step2_frame_size = EKL(ptk_step2);
    __eapol_set_tx_packet_func(verify_step2);
    __eapol_rx_packet(1, ap_address, ETH_P_PAE,
                      (const uint8_t *) ptk_step1, EKL(ptk_step1),
                      false);
    assert(verify_step2_called);

    /* Detect whether we generate a new snonce when we shouldn't */
    memset(new_snonce, 0xff, sizeof(new_snonce));
    snonce = new_snonce;

    /*
     * Now retransmit frame 1 again without updating the counter.  This
     * seems to be legal since the STA should not update the replay counter
     * unless the packet has a MIC.
     */
    verify_step2_called = false;
    __eapol_rx_packet(1, ap_address, ETH_P_PAE,
                      (const uint8_t *) ptk_step1, EKL(ptk_step1),
                      false);
    assert(verify_step2_called);

    /* Now retransmit frame 1 with an updated counter */
    replay_counter += 1;
    retransmit = l_memdup(ptk_step1, EKL(ptk_step1));
    retransmit->key_replay_counter = L_CPU_TO_BE64(replay_counter);

    expect = UPDATED_REPLAY_COUNTER(ptk_step2, replay_counter, ptk);

    verify_step2_called = false;
    expected_step2_frame = (const uint8_t *) expect;
    expected_step2_frame_size = EKL(expect);
    __eapol_rx_packet(1, ap_address, ETH_P_PAE,
                      (const uint8_t *) retransmit, EKL(retransmit),
                      false);
    assert(verify_step2_called);

    l_free(expect);
    l_free(retransmit);

    ptk_step3 = eapol_key_validate(eapol_key_data_9,
                                   sizeof(eapol_key_data_9));
    assert(ptk_step3);

    ptk_step4 = eapol_key_validate(eapol_key_data_10,
                                   sizeof(eapol_key_data_10));
    assert(ptk_step4);

    verify_install_tk_called = false;
    verify_step4_called = false;
    expected_step4_frame = (const uint8_t *) ptk_step4;
    expected_step4_frame_size = EKL(ptk_step4);
    __eapol_set_tx_packet_func(verify_step4);
    __eapol_rx_packet(1, ap_address, ETH_P_PAE,
                      (const uint8_t *) ptk_step3, EKL(ptk_step3),
                      false);
    assert(verify_step4_called);
    assert(verify_install_tk_called);

    /*
     * Now retransmit message 3.  Make sure install_tk_called is false
     * this time.  This prevents KRACK style attacks.
     */
    replay_counter += 1;
    retransmit = UPDATED_REPLAY_COUNTER(ptk_step3, replay_counter, ptk);
    expect = UPDATED_REPLAY_COUNTER(ptk_step4, replay_counter, ptk);

    verify_install_tk_called = false;
    verify_step4_called = false;
    expected_step4_frame = (const uint8_t *) expect;
    expected_step4_frame_size = EKL(expect);
    __eapol_rx_packet(1, ap_address, ETH_P_PAE,
                      (const uint8_t *) retransmit, EKL(retransmit),
                      false);
    assert(verify_step4_called);
    assert(!verify_install_tk_called);

    l_free(expect);
    l_free(retransmit);

    gtk_step1 = eapol_key_validate(eapol_key_data_11,
                                   sizeof(eapol_key_data_11));
    assert(gtk_step1);

    gtk_step2 = eapol_key_validate(eapol_key_data_12,
                                   sizeof(eapol_key_data_12));
    assert(gtk_step2);

    /*
     * Now run the GTK handshake.  Since we retransmitted Step 3 above,
     * update the replay counter and perform the initial transmission
     */
    replay_counter += 1;
    retransmit = UPDATED_REPLAY_COUNTER(gtk_step1, replay_counter, ptk);
    expect = UPDATED_REPLAY_COUNTER(gtk_step2, replay_counter, ptk);

    verify_install_gtk_called = false;
    verify_gtk_step2_called = false;
    expected_gtk_step2_frame = (const uint8_t *) expect;
    expected_gtk_step2_frame_size = EKL(expect);
    __eapol_set_tx_packet_func(verify_step2_gtk);
    __eapol_rx_packet(1, ap_address, ETH_P_PAE,
                      (const uint8_t *) retransmit, EKL(retransmit),
                      false);
    assert(verify_gtk_step2_called);
    assert(verify_install_gtk_called);

    l_free(retransmit);
    l_free(expect);

    /* Now send a retransmit, make sure GTK isn't installed again */
    replay_counter += 1;
    retransmit = UPDATED_REPLAY_COUNTER(gtk_step1, replay_counter, ptk);
    expect = UPDATED_REPLAY_COUNTER(gtk_step2, replay_counter, ptk);

    verify_install_gtk_called = false;
    verify_gtk_step2_called = false;
    expected_gtk_step2_frame = (const uint8_t *) expect;
    expected_gtk_step2_frame_size = EKL(expect);
    __eapol_set_tx_packet_func(verify_step2_gtk);
    __eapol_rx_packet(1, ap_address, ETH_P_PAE,
                      (const uint8_t *) retransmit, EKL(retransmit),
                      false);
    assert(verify_gtk_step2_called);
    assert(!verify_install_gtk_called);

    l_free(retransmit);
    l_free(expect);

    l_free(ptk);

    eapol_sm_free(sm);
    handshake_state_free(hs);
    eapol_exit();
}

static const uint8_t eap_identity_req[] = {
        0x02, 0x00, 0x00, 0x05, 0x01, 0x01, 0x00, 0x05, 0x01
};
static const uint8_t eap_identity_resp[] = {
        0x02, 0x00, 0x00, 0x14, 0x02, 0x01, 0x00, 0x14, 0x01, 0x61, 0x62, 0x63,
        0x40, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2e, 0x63, 0x6f, 0x6d
};

struct eapol_8021x_tls_test_state {
    l_tls_write_cb_t app_data_cb;
    l_tls_ready_cb_t ready_cb;
    l_tls_disconnect_cb_t disconnect_cb;

    uint8_t method;

    struct l_tls *tls;
    uint8_t last_id;

    bool success;
    bool pending_req;
    bool tx_ack;

    uint8_t tx_buf[16384];
    unsigned int tx_buf_len, tx_buf_offset;

    uint8_t pmk[32];
};

static int verify_8021x_identity_resp(uint32_t ifindex,
                                      const uint8_t *aa_addr, uint16_t proto,
                                      const struct eapol_frame *ef,
                                      bool noencrypt,
                                      void *user_data)
{
    struct eapol_8021x_tls_test_state *s = user_data;
    size_t len = sizeof(struct eapol_header) +
                 L_BE16_TO_CPU(ef->header.packet_len);

    assert(ifindex == 1);
    assert(!memcmp(aa_addr, aa, 6));
    assert(proto == ETH_P_PAE);
    assert(len == sizeof(eap_identity_resp));
    assert(!memcmp(ef, eap_identity_resp, sizeof(eap_identity_resp)));

    assert(s->pending_req);
    s->pending_req = 0;

    return 0;
}

static int verify_8021x_tls_resp(uint32_t ifindex,
                                 const uint8_t *aa_addr, uint16_t proto,
                                 const struct eapol_frame *ef,
                                 bool noencrypt,
                                 void *user_data)
{
    struct eapol_8021x_tls_test_state *s = user_data;
    size_t len = sizeof(struct eapol_header) +
                 L_BE16_TO_CPU(ef->header.packet_len);
    size_t fragment_len, header_len;

    assert(ifindex == 1);
    assert(!memcmp(aa_addr, aa, 6));
    assert(proto == ETH_P_PAE);
    assert(len >= 10);
    assert(ef->header.protocol_version == EAPOL_PROTOCOL_VERSION_2004);
    assert(ef->header.packet_type == 0x00); /* EAPoL-EAP */

    assert(ef->data[0] == EAP_CODE_RESPONSE);
    assert(ef->data[1] == s->last_id);
    assert(ef->data[4] == s->method);
    assert((ef->data[5] & 0x3f) == 0); /* Flags */

    /* Expecting an ACK? */
    if (s->tx_buf_len || s->success) {
        assert(ef->data[5] == 0);
        assert(len == 10);
    }

    header_len = 6;
    fragment_len = l_get_be16(ef->data + 2) - header_len;

    if (ef->data[5] & 0x80) { /* L flag */
        assert(len >= 14);
        header_len += 4;
        fragment_len -= 4;
        assert(ef->data[5] & 0x40); /* M flag */
        assert(l_get_be32(ef->data + 6) > fragment_len);
    }
    s->tx_ack = !!(ef->data[5] & 0x40); /* M flag */

    assert(len == 4 + header_len + fragment_len);

    assert(s->pending_req);
    s->pending_req = 0;

    if (fragment_len)
        l_tls_handle_rx(s->tls, ef->data + header_len, fragment_len);

    return 0;
}

static void eapol_sm_test_tls_new_data(const uint8_t *data, size_t len,
                                       void *user_data)
{
    assert(false);
}

static void eapol_sm_test_tls_test_write(const uint8_t *data, size_t len,
                                         void *user_data)
{
    struct eapol_8021x_tls_test_state *s = user_data;

    assert(!s->tx_ack);
    assert(!s->tx_buf_offset);

    memcpy(s->tx_buf + s->tx_buf_len, data, len);
    s->tx_buf_len += len;
}

static void eapol_sm_test_tls_test_ready(const char *peer_identity,
                                         void *user_data)
{
    struct eapol_8021x_tls_test_state *s = user_data;

    assert(!s->tx_ack);
    /* TODO: require the right peer_identity */

    s->success = true;

    assert(l_tls_prf_get_bytes(s->tls, true, "client EAP encryption",
                               s->pmk, 32));
}

static void eapol_sm_test_tls_test_disconnected(enum l_tls_alert_desc reason,
                                                bool remote, void *user_data)
{
    l_info("Unexpected TLS alert: %d", reason);
    assert(false);
}

static void verify_handshake_successful(struct handshake_state *hs,
                                        enum handshake_event event,
                                        void *event_data, void *user_data)
{
    switch (event) {
        case HANDSHAKE_EVENT_FAILED:
            assert(false);
            break;
        default:
            break;
    }
}

static void eapol_sm_test_tls(struct eapol_8021x_tls_test_state *s,
                              const char *config)
{
    static const unsigned char ap_wpa_ie[] = {
            0xdd, 0x16, 0x00, 0x50, 0xf2, 0x01, 0x01, 0x00,
            0x00, 0x50, 0xf2, 0x02, 0x01, 0x00, 0x00, 0x50,
            0xf2, 0x02, 0x01, 0x00, 0x00, 0x50, 0xf2, 0x02 };
    static uint8_t ap_address[] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 };
    static uint8_t sta_address[] = { 0x02, 0x00, 0x00, 0x00, 0x01, 0x00 };
    bool r;
    struct handshake_state *hs;
    struct test_handshake_state *ths;
    struct eapol_sm *sm;
    struct l_settings *settings;
    uint8_t tx_buf[2000];
    size_t header_len, data_len, tx_len;
    bool start;
    uint8_t step1_buf[256], step2_buf[256], step3_buf[256], step4_buf[256];
    struct eapol_key *step1, *step2, *step3, *step4;
    uint8_t ptk_buf[64];
    struct crypto_ptk *ptk;

    aa = ap_address;
    spa = sta_address;

    eap_init(0);
    eapol_init();
    __handshake_set_get_nonce_func(test_nonce);

    hs = test_handshake_state_new(1);
    sm = eapol_sm_new(hs);
    eapol_register(sm);

    handshake_state_set_authenticator_address(hs, ap_address);
    handshake_state_set_supplicant_address(hs, sta_address);
    handshake_state_set_event_func(hs, verify_handshake_successful, NULL);
    __eapol_set_tx_user_data(s);

    r = handshake_state_set_supplicant_wpa(hs,
                                           eapol_key_data_14 + sizeof(struct eapol_key));
    assert(r);

    handshake_state_set_authenticator_wpa(hs, ap_wpa_ie);

    settings = l_settings_new();
    l_settings_load_from_data(settings, config, strlen(config));
    handshake_state_set_8021x_config(hs, settings);
    eapol_start(sm);

    l_settings_free(settings);

    __eapol_set_tx_packet_func(verify_8021x_identity_resp);
    s->pending_req = 1;
    __eapol_rx_packet(1, ap_address, ETH_P_PAE, eap_identity_req,
                      sizeof(eap_identity_req), false);
    assert(!s->pending_req);

    s->tls = l_tls_new(true, s->app_data_cb, eapol_sm_test_tls_test_write,
                       s->ready_cb, s->disconnect_cb, s);
    assert(s->tls);

    s->last_id = 1;
    s->success = false;
    s->tx_buf_len = 0;
    s->tx_buf_offset = 0;

    assert(l_tls_set_auth_data(s->tls, "ell/unit/cert-server.pem",
                               "ell/unit/cert-server-key-pkcs8.pem", NULL));
    assert(l_tls_set_cacert(s->tls, "ell/unit/cert-ca.pem"));

    start = 1;
    __eapol_set_tx_packet_func(verify_8021x_tls_resp);
    while (!s->success || s->tx_buf_len) {
        tx_len = 0;
        data_len = 1024 < s->tx_buf_len ? 1024 : s->tx_buf_len;
        header_len = 6;
        if (data_len < s->tx_buf_len && !s->tx_buf_offset)
            header_len += 4;
        tx_buf[tx_len++] = EAPOL_PROTOCOL_VERSION_2004;
        tx_buf[tx_len++] = 0x00; /* EAPoL-EAP */
        tx_buf[tx_len++] = (data_len + header_len) >> 8;
        tx_buf[tx_len++] = (data_len + header_len) >> 0;

        tx_buf[tx_len++] = EAP_CODE_REQUEST;
        tx_buf[tx_len++] = ++s->last_id;
        tx_buf[tx_len++] = (data_len + header_len) >> 8;
        tx_buf[tx_len++] = (data_len + header_len) >> 0;
        tx_buf[tx_len++] = s->method;
        tx_buf[tx_len++] = 0x00; /* Flags */

        if (start) {
            tx_buf[tx_len - 1] |= 0x20; /* S flag */
            start = 0;
        }

        if (data_len < s->tx_buf_len)
            tx_buf[tx_len - 1] |= 0x40; /* M flag */

        if (data_len < s->tx_buf_len && !s->tx_buf_offset) {
            tx_buf[tx_len - 1] |= 0x80; /* L flag */

            tx_buf[tx_len++] = s->tx_buf_len >> 24;
            tx_buf[tx_len++] = s->tx_buf_len >> 16;
            tx_buf[tx_len++] = s->tx_buf_len >>  8;
            tx_buf[tx_len++] = s->tx_buf_len >>  0;
        }

        memcpy(tx_buf + tx_len, s->tx_buf + s->tx_buf_offset, data_len);
        tx_len += data_len;
        s->tx_buf_offset += data_len;
        s->tx_buf_len -= data_len;

        if (!s->tx_buf_len)
            s->tx_buf_offset = 0;

        s->pending_req = 1;

        __eapol_rx_packet(1, ap_address, ETH_P_PAE,
                          tx_buf, tx_len, false);

        assert(!s->pending_req);

        while (s->tx_ack) {
            tx_len = 0;
            tx_buf[tx_len++] = EAPOL_PROTOCOL_VERSION_2004;
            tx_buf[tx_len++] = 0x00; /* EAPoL-EAP */
            tx_buf[tx_len++] = 0x00;
            tx_buf[tx_len++] = 0x06; /* Length */

            tx_buf[tx_len++] = EAP_CODE_REQUEST;
            tx_buf[tx_len++] = ++s->last_id;
            tx_buf[tx_len++] = 0x00;
            tx_buf[tx_len++] = 0x06; /* Length */
            tx_buf[tx_len++] = s->method;
            tx_buf[tx_len++] = 0x00; /* Flags */

            s->pending_req = 1;

            __eapol_rx_packet(1, ap_address, ETH_P_PAE,
                              tx_buf, tx_len, false);

            assert(!s->pending_req);
        }
    }

    l_tls_free(s->tls);

    tx_len = 0;
    tx_buf[tx_len++] = EAPOL_PROTOCOL_VERSION_2004;
    tx_buf[tx_len++] = 0x00; /* EAPoL-EAP */
    tx_buf[tx_len++] = 0x00;
    tx_buf[tx_len++] = 0x04; /* Length */

    tx_buf[tx_len++] = EAP_CODE_SUCCESS;
    tx_buf[tx_len++] = s->last_id;
    tx_buf[tx_len++] = 0x00;
    tx_buf[tx_len++] = 0x04; /* Length */

    __eapol_rx_packet(1, ap_address, ETH_P_PAE, tx_buf, tx_len, false);

    memcpy(step1_buf, eapol_key_data_13, sizeof(eapol_key_data_13));
    step1 = (struct eapol_key *)
            eapol_key_validate(step1_buf, sizeof(eapol_key_data_13));

    memcpy(step2_buf, eapol_key_data_14, sizeof(eapol_key_data_14));
    step2 = (struct eapol_key *)
            eapol_key_validate(step2_buf, sizeof(eapol_key_data_14));

    memcpy(step3_buf, eapol_key_data_15, sizeof(eapol_key_data_15));
    step3 = (struct eapol_key *)
            eapol_key_validate(step3_buf, sizeof(eapol_key_data_15));

    memcpy(step4_buf, eapol_key_data_16, sizeof(eapol_key_data_16));
    step4 = (struct eapol_key *)
            eapol_key_validate(step4_buf, sizeof(eapol_key_data_16));

    ptk = (struct crypto_ptk *) ptk_buf;
    crypto_derive_pairwise_ptk(s->pmk, sta_address, ap_address,
                               step1->key_nonce, step2->key_nonce,
                               ptk, 64, false);

    memset(step2->key_mic_data, 0, 16);
    assert(eapol_calculate_mic(IE_RSN_AKM_SUITE_PSK, ptk->kck, step2, step2->key_mic_data));

    memset(step3->key_mic_data, 0, 16);
    assert(eapol_calculate_mic(IE_RSN_AKM_SUITE_PSK, ptk->kck, step3, step3->key_mic_data));

    memset(step4->key_mic_data, 0, 16);
    assert(eapol_calculate_mic(IE_RSN_AKM_SUITE_PSK, ptk->kck, step4, step4->key_mic_data));

    snonce = step2->key_nonce;

    verify_step2_called = false;
    expected_step2_frame = step2_buf;
    expected_step2_frame_size = sizeof(eapol_key_data_14);

    __eapol_set_tx_packet_func(verify_step2);
    __eapol_rx_packet(1, ap_address, ETH_P_PAE,
                      step1_buf, sizeof(eapol_key_data_13), false);
    assert(verify_step2_called);

    verify_step4_called = false;
    verify_install_tk_called = false;
    expected_step4_frame = step4_buf;
    expected_step4_frame_size = sizeof(eapol_key_data_16);

    __eapol_set_tx_packet_func(verify_step4);
    __handshake_set_install_tk_func(verify_install_tk);
    ths = container_of(hs, struct test_handshake_state, super);
    ths->tk = ptk->tk;
    __eapol_rx_packet(1, ap_address, ETH_P_PAE,
                      step3_buf, sizeof(eapol_key_data_15), false);
    assert(verify_step4_called);
    assert(verify_install_tk_called);

    __handshake_set_install_tk_func(NULL);
    eapol_sm_free(sm);
    handshake_state_free(hs);
    eapol_exit();
    eap_exit();
}

static void eapol_sm_test_eap_tls(const void *data)
{
    static const char *eapol_8021x_config = "[Security]\n"
                                            "EAP-Method=TLS\n"
                                            "EAP-Identity=abc@example.com\n"
                                            "EAP-TLS-CACert=ell/unit/cert-ca.pem\n"
                                            "EAP-TLS-ClientCert=ell/unit/cert-client.pem\n"
                                            "EAP-TLS-ClientKey=ell/unit/cert-client-key-pkcs8.pem";
    struct eapol_8021x_tls_test_state s;

    s.app_data_cb = eapol_sm_test_tls_new_data;
    s.ready_cb = eapol_sm_test_tls_test_ready;
    s.disconnect_cb = eapol_sm_test_tls_test_disconnected;
    s.method = EAP_TYPE_TLS_EAP;

    eapol_sm_test_tls(&s, eapol_8021x_config);
}

static const uint8_t eap_ttls_eap_identity_avp[] = {
        0x00, 0x00, 0x00, 0x4f, 0x40, 0x00, 0x00, 0x1c, 0x02, 0x00, 0x00, 0x14,
        0x01, 0x61, 0x62, 0x63, 0x40, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65,
        0x2e, 0x63, 0x6f, 0x6d
};

static const uint8_t eap_ttls_eap_md5_challenge_avp[] = {
        0x00, 0x00, 0x00, 0x4f, 0x40, 0x00, 0x00, 0x1e, 0x01, 0xbb, 0x00, 0x16,
        0x04, 0x10, 0x3a, 0x34, 0x58, 0xc4, 0xf1, 0xa3, 0xdc, 0x45, 0xd0, 0xca,
        0x96, 0x33, 0x9b, 0x95, 0xb8, 0xd6, 0x00, 0x00
};

static const uint8_t eap_ttls_eap_md5_response_avp[] = {
        0x00, 0x00, 0x00, 0x4f, 0x40, 0x00, 0x00, 0x1e, 0x02, 0xbb, 0x00, 0x16,
        0x04, 0x10, 0x10, 0xcc, 0x62, 0x4c, 0x98, 0x2b, 0x82, 0xbd, 0x13, 0x4a,
        0x81, 0xcb, 0x70, 0x78, 0xcd, 0xc2,
};

struct eapol_8021x_eap_ttls_test_state {
    struct eapol_8021x_tls_test_state tls;
    bool challenge_sent;
};

static void eapol_sm_test_eap_ttls_new_data(const uint8_t *data, size_t len,
                                            void *user_data)
{
    struct eapol_8021x_eap_ttls_test_state *s = user_data;

    if (!s->challenge_sent) {
        assert(len == sizeof(eap_ttls_eap_identity_avp));
        assert(!memcmp(data, eap_ttls_eap_identity_avp, len));

        l_tls_write(s->tls.tls, eap_ttls_eap_md5_challenge_avp,
                    sizeof(eap_ttls_eap_md5_challenge_avp));

        s->challenge_sent = true;
    } else {
        assert(len == sizeof(eap_ttls_eap_md5_response_avp));
        assert(!memcmp(data, eap_ttls_eap_md5_response_avp, len));

        s->tls.success = true;
    }
}

static void eapol_sm_test_eap_ttls_test_ready(const char *peer_identity,
                                              void *user_data)
{
    struct eapol_8021x_eap_ttls_test_state *s = user_data;

    assert(!s->tls.tx_ack);
    /* TODO: require the right peer_identity */

    assert(l_tls_prf_get_bytes(s->tls.tls, true, "ttls keying material",
                               s->tls.pmk, 32));

    s->challenge_sent = false;
}

static void eapol_sm_test_eap_ttls_md5(const void *data)
{
    static const char *eapol_8021x_config = "[Security]\n"
                                            "EAP-Method=TTLS\n"
                                            "EAP-Identity=abc@example.com\n"
                                            "EAP-TTLS-CACert=ell/unit/cert-ca.pem\n"
                                            "EAP-TTLS-ClientCert=ell/unit/cert-client.pem\n"
                                            "EAP-TTLS-ClientKey=ell/unit/cert-client-key-pkcs8.pem\n"
                                            "EAP-TTLS-Phase2-Method=MD5\n"
                                            "EAP-TTLS-Phase2-Identity=abc@example.com\n"
                                            "EAP-TTLS-Phase2-Password=testpasswd";
    struct eapol_8021x_eap_ttls_test_state s;

    s.tls.app_data_cb = eapol_sm_test_eap_ttls_new_data;
    s.tls.ready_cb = eapol_sm_test_eap_ttls_test_ready;
    s.tls.disconnect_cb = eapol_sm_test_tls_test_disconnected;
    s.tls.method = EAP_TYPE_TTLS;

    eapol_sm_test_tls(&s.tls, eapol_8021x_config);
}

static void test_handshake_event(struct handshake_state *hs,
                                 enum handshake_event event,
                                 void *event_data,
                                 void *user_data)
{
    struct test_handshake_state *ths =
    container_of(hs, struct test_handshake_state, super);

    switch (event) {
        case HANDSHAKE_EVENT_FAILED:
            ths->handshake_failed = true;
            break;
        default:
            break;
    }
}

static const uint8_t eap_ttls_start_req[] = {
        0x02, 0x00, 0x00, 0x06, 0x01, 0x02, 0x00, 0x06, 0x15, 0x20
};
static const uint8_t eap_nak_tls[] = {
        0x02, 0x00, 0x00, 0x06, 0x02, 0x02, 0x00, 0x06, 0x03, 0x0d
};
static const uint8_t eap_failure[] = {
        0x02, 0x00, 0x00, 0x04, 0x04, 0x02, 0x00, 0x04
};

static int verify_8021x_eap_nak(uint32_t ifindex,
                                const uint8_t *aa_addr, uint16_t proto,
                                const struct eapol_frame *ef, bool noencrypt,
                                void *user_data)
{
    struct eapol_8021x_tls_test_state *s = user_data;
    size_t len = sizeof(struct eapol_header) +
                 L_BE16_TO_CPU(ef->header.packet_len);

    assert(ifindex == 1);
    assert(!memcmp(aa_addr, aa, 6));
    assert(proto == ETH_P_PAE);
    assert(len == sizeof(eap_nak_tls));
    assert(!memcmp(ef, eap_nak_tls, sizeof(eap_nak_tls)));

    assert(s->pending_req);
    s->pending_req = 0;

    return 0;
}
static void eapol_sm_test_eap_nak(const void *data)
{
    static const char *eapol_8021x_config = "[Security]\n"
                                            "EAP-Method=TLS\n"
                                            "EAP-Identity=abc@example.com\n"
                                            "EAP-TLS-CACert=ell/unit/cert-ca.pem\n"
                                            "EAP-TLS-ClientCert=ell/unit/cert-client.pem\n"
                                            "EAP-TLS-ClientKey=ell/unit/cert-client-key-pkcs8.pem";
    static const unsigned char ap_wpa_ie[] = {
            0xdd, 0x16, 0x00, 0x50, 0xf2, 0x01, 0x01, 0x00,
            0x00, 0x50, 0xf2, 0x02, 0x01, 0x00, 0x00, 0x50,
            0xf2, 0x02, 0x01, 0x00, 0x00, 0x50, 0xf2, 0x02 };
    static uint8_t ap_address[] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 };
    static uint8_t sta_address[] = { 0x02, 0x00, 0x00, 0x00, 0x01, 0x00 };
    bool r;
    struct handshake_state *hs;
    struct test_handshake_state *ths;

    struct eapol_sm *sm;
    struct l_settings *settings;
    struct eapol_8021x_tls_test_state s;

    aa = ap_address;
    spa = sta_address;

    eap_init(0);
    eapol_init();
    __handshake_set_get_nonce_func(test_nonce);

    hs = test_handshake_state_new(1);
    ths = container_of(hs, struct test_handshake_state, super);

    sm = eapol_sm_new(hs);
    eapol_register(sm);

    handshake_state_set_authenticator_address(hs, ap_address);
    handshake_state_set_supplicant_address(hs, sta_address);
    handshake_state_set_event_func(hs, test_handshake_event, NULL);
    __eapol_set_tx_user_data(&s);

    r = handshake_state_set_supplicant_wpa(hs,
                                           eapol_key_data_20 + sizeof(struct eapol_key));
    assert(r);

    handshake_state_set_authenticator_wpa(hs, ap_wpa_ie);

    settings = l_settings_new();
    l_settings_load_from_data(settings, eapol_8021x_config,
                              strlen(eapol_8021x_config));
    handshake_state_set_8021x_config(hs, settings);
    eapol_start(sm);

    l_settings_free(settings);

    __eapol_set_tx_packet_func(verify_8021x_identity_resp);
    s.pending_req = 1;
    __eapol_rx_packet(1, ap_address, ETH_P_PAE, eap_identity_req,
                      sizeof(eap_identity_req), false);
    assert(!s.pending_req);

    s.pending_req = 1;
    __eapol_set_tx_packet_func(verify_8021x_eap_nak);
    __eapol_rx_packet(1, ap_address, ETH_P_PAE, eap_ttls_start_req,
                      sizeof(eap_ttls_start_req), false);
    assert(!s.pending_req);

    ths->handshake_failed = false;
    __eapol_rx_packet(1, ap_address, ETH_P_PAE, eap_failure,
                      sizeof(eap_failure), false);
    assert(ths->handshake_failed);

    handshake_state_free(hs);
    eapol_exit();
    eap_exit();
}

static void eapol_ft_handshake_test(const void *data)
{
    const unsigned char psk[] = {
            0xbc, 0xcd, 0x17, 0x98, 0xef, 0x6c, 0xb8, 0x7f,
            0x2b, 0x54, 0x75, 0xfb, 0x98, 0x06, 0x57, 0xd2,
            0xea, 0x02, 0x6c, 0xe3, 0x68, 0xef, 0x3f, 0xca,
            0xda, 0x31, 0x48, 0x54, 0x3f, 0xee, 0x94, 0xf0 };
    const unsigned char ap_rsne[] = {
            0x30, 0x14, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x04,
            0x01, 0x00, 0x00, 0x0f, 0xac, 0x04, 0x01, 0x00,
            0x00, 0x0f, 0xac, 0x04, 0x81, 0x00 };
    const unsigned char own_rsne[] = {
            0x30, 0x12, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x04,
            0x01, 0x00, 0x00, 0x0f, 0xac, 0x04, 0x01, 0x00,
            0x00, 0x0f, 0xac, 0x04 };
    const uint8_t mde[] = { 0x36, 0x03, 0x12, 0x34, 0x01 };
    const uint8_t fte[] = {
            0x37, 0x62, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x01, 0x06, 0x02, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x03, 0x06, 0x64, 0x75,
            0x6d, 0x6d, 0x79, 0x30 };
    const uint8_t r0khid[] = "dummy0";
    const uint8_t r1khid[] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 };
    const uint8_t ap_address[] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 };
    const uint8_t sta_address[] = { 0x02, 0x00, 0x00, 0x00, 0x02, 0x00 };
    const char *ssid = "TestFT";
    struct handshake_state *hs;
    struct eapol_sm *sm;

    eapol_init();

    snonce = eapol_key_test_26.key_nonce;
    __handshake_set_get_nonce_func(test_nonce);

    aa = ap_address;
    spa = sta_address;
    verify_step2_called = false;
    expected_step2_frame = eapol_key_data_26;
    expected_step2_frame_size = sizeof(eapol_key_data_26);
    verify_step4_called = false;
    expected_step4_frame = eapol_key_data_28;
    expected_step4_frame_size = sizeof(eapol_key_data_28);

    hs = test_handshake_state_new(1);
    sm = eapol_sm_new(hs);
    eapol_register(sm);

    handshake_state_set_pmk(hs, psk, sizeof(psk));
    handshake_state_set_authenticator_address(hs, aa);
    handshake_state_set_supplicant_address(hs, spa);
    handshake_state_set_ssid(hs, (void *) ssid, strlen(ssid));

    handshake_state_set_supplicant_rsn(hs, own_rsne);
    handshake_state_set_authenticator_rsn(hs, ap_rsne);
    handshake_state_set_mde(hs, mde);
    handshake_state_set_fte(hs, fte);
    handshake_state_set_kh_ids(hs, r0khid, strlen((void *) r0khid), r1khid);
    eapol_start(sm);

    __eapol_set_tx_packet_func(verify_step2);
    __eapol_rx_packet(1, aa, ETH_P_PAE,
                      eapol_key_data_25, sizeof(eapol_key_data_25),
                      false);
    assert(verify_step2_called);

    __eapol_set_tx_packet_func(verify_step4);
    __eapol_rx_packet(1, aa, ETH_P_PAE,
                      eapol_key_data_27, sizeof(eapol_key_data_27),
                      false);
    assert(verify_step4_called);

    eapol_sm_free(sm);
    handshake_state_free(hs);
    eapol_exit();
}





int main(int argc, char *argv[])
{
    if (argc >= 1 && argv[1] != NULL) {
        __afl_input_filename = argv[1];
    }



    l_test_init(&argc, &argv);


/*
	l_test_add("/EAPoL Key/Key Frame 1",
			eapol_key_test, &eapol_key_test_1);
	l_test_add("/EAPoL Key/Key Frame 2",
			eapol_key_test, &eapol_key_test_2);
	l_test_add("/EAPoL Key/Key Frame 3",
			eapol_key_test, &eapol_key_test_3);
            */
   // l_test_add("/EAPoL Key/Key Frame 4",
     //          eapol_key_test, &eapol_key_test_4);
    // l_test_add("/EAPoL Key/Key Frame 4_afl",
    //          eapol_key_test, create_eapol_key_data_afl);
    //l_test_add("/EAPoL Key/Key Frame 5",
    //		eapol_key_test, &eapol_key_test_5);
   // l_test_add("/EAPoL Key/Key Frame 6",
     //          eapol_key_test, &eapol_key_test_6);
/*	l_test_add("/EAPoL Key/Key Frame 7",
			eapol_key_test, &eapol_key_test_7);
	l_test_add("/EAPoL Key/Key Frame 8",
			eapol_key_test, &eapol_key_test_8);
	l_test_add("/EAPoL Key/Key Frame 9",
			eapol_key_test, &eapol_key_test_9);
	l_test_add("/EAPoL Key/Key Frame 10",
			eapol_key_test, &eapol_key_test_10);
	l_test_add("/EAPoL Key/Key Frame 11",
			eapol_key_test, &eapol_key_test_11);
	l_test_add("/EAPoL Key/Key Frame 12",
			eapol_key_test, &eapol_key_test_12);
	l_test_add("/EAPoL Key/Key Frame 13",
			eapol_key_test, &eapol_key_test_13);
	l_test_add("/EAPoL Key/Key Frame 14",
			eapol_key_test, &eapol_key_test_14);
	l_test_add("/EAPoL Key/Key Frame 15",
			eapol_key_test, &eapol_key_test_15);
	l_test_add("/EAPoL Key/Key Frame 16",
			eapol_key_test, &eapol_key_test_16);
	l_test_add("/EAPoL Key/Key Frame 17",
			eapol_key_test, &eapol_key_test_17);
	l_test_add("/EAPoL Key/Key Frame 18",
			eapol_key_test, &eapol_key_test_18);
	l_test_add("/EAPoL Key/Key Frame 19",
			eapol_key_test, &eapol_key_test_19);
	l_test_add("/EAPoL Key/Key Frame 20",
			eapol_key_test, &eapol_key_test_20);
	l_test_add("/EAPoL Key/Key Frame 21",
			eapol_key_test, &eapol_key_test_21);
	l_test_add("/EAPoL Key/Key Frame 22",
			eapol_key_test, &eapol_key_test_22);
	l_test_add("/EAPoL Key/Key Frame 23",
			eapol_key_test, &eapol_key_test_23);
	l_test_add("/EAPoL Key/Key Frame 24",
			eapol_key_test, &eapol_key_test_24);
	l_test_add("/EAPoL Key/Key Frame 25",
			eapol_key_test, &eapol_key_test_25);
	l_test_add("/EAPoL Key/Key Frame 26",
			eapol_key_test, &eapol_key_test_26);
	l_test_add("/EAPoL Key/Key Frame 27",
			eapol_key_test, &eapol_key_test_27);
	l_test_add("/EAPoL Key/Key Frame 28",
			eapol_key_test, &eapol_key_test_28);
	l_test_add("/EAPoL Key/Key Frame 29",
			eapol_key_test, &eapol_key_test_29);
	l_test_add("/EAPoL Key/Key Frame 30",
			eapol_key_test, &eapol_key_test_30);
	l_test_add("/EAPoL Key/Key Frame 31",
			eapol_key_test, &eapol_key_test_31);
	l_test_add("/EAPoL Key/Key Frame 32",
			eapol_key_test, &eapol_key_test_32);
*/
    if (!l_checksum_is_supported(L_CHECKSUM_MD5, true) ||
        !l_checksum_is_supported(L_CHECKSUM_SHA1, true))
        goto done;

/*	l_test_add("/EAPoL Key/MIC Test 1",
			eapol_key_mic_test, &eapol_key_mic_test_1);
	l_test_add("/EAPoL Key/MIC Test 2",
			eapol_key_mic_test, &eapol_key_mic_test_2);

	l_test_add("/EAPoL Key/Calculate MIC Test 1",
			eapol_calculate_mic_test, &eapol_calculate_mic_test_1);

	if (!l_cipher_is_supported(L_CIPHER_AES) ||
			!l_cipher_is_supported(L_CIPHER_ARC4))
		goto done;

	l_test_add("EAPoL/WPA2 4-Way Handshake",
			&eapol_4way_test, NULL);

	l_test_add("EAPoL/WPA2 4-Way & GTK Handshake",
			&eapol_wpa2_handshake_test, NULL);

	l_test_add("EAPoL/WPA 4-Way & GTK Handshake",
			&eapol_wpa_handshake_test, NULL);
*/
    l_test_add("EAPoL/WPA2 PTK State Machine", &eapol_sm_test_ptk, NULL);
/*
	l_test_add("EAPoL IGTK & 4-Way Handshake",
			&eapol_sm_test_igtk, NULL);

	l_test_add("EAPoL/WPA2 PTK & GTK State Machine",
			&eapol_sm_test_wpa2_ptk_gtk, NULL);

	l_test_add("EAPoL/WPA PTK & GTK State Machine Test 1",
			&eapol_sm_test_wpa_ptk_gtk, NULL);

	l_test_add("EAPoL/WPA PTK & GTK State Machine Test 2",
			&eapol_sm_test_wpa_ptk_gtk_2, NULL);

	l_test_add("EAPoL/WPA2 Retransmit Test",
			&eapol_sm_wpa2_retransmit_test, NULL);

	if (l_cipher_is_supported(L_CIPHER_DES3_EDE_CBC) &&
			l_cipher_is_supported(L_CIPHER_AES_CBC) &&
			l_key_is_supported(L_KEY_FEATURE_RESTRICT |
							L_KEY_FEATURE_CRYPTO)) {
		l_test_add("EAPoL/8021x EAP-TLS & 4-Way Handshake",
					&eapol_sm_test_eap_tls, NULL);

		l_test_add("EAPoL/8021x EAP-TTLS+EAP-MD5 & 4-Way Handshake",
					&eapol_sm_test_eap_ttls_md5, NULL);
		l_test_add("EAPoL/8021x EAP NAK",
				&eapol_sm_test_eap_nak, NULL);
	}

	l_test_add("EAPoL/FT-Using-PSK 4-Way Handshake",
			&eapol_ft_handshake_test, NULL);
*/
    done:
    return l_test_run();
}



