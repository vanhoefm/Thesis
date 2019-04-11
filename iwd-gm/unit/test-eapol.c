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


//XXX define a new struct to handle the input data from AFL instead of using the static one
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



void * __afl_get_key_data_ptk( ){
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
        __afl_key = eapol_key_data_4;
        // __afl_key = eap1;
        __afl_key1= eapol_key_data_6;
       // __afl_key1=eap2;
        //printf("%d %d", sizeof(eap1), sizeof(eapol_key_data_4));
        len_frame1= sizeof(eapol_key_data_4);
        //len_frame1= sizeof(eap1);
        len_frame2= sizeof(eapol_key_data_6);
       // len_frame2= sizeof(eap2);
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
    }


    printf("\n");
    printf("\n------------------------------------------------------\n");

    fclose(fp);

    return(NULL);
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



void * __afl_get_key_data_igtk( ){
    FILE * fp1;
    int i;
    int data;
    int count=0;
    int expected_byte=0;
    int sz;
    size_t ret;

    fp1 = fopen(__afl_input_filename , "rb");
    if (fp1 == NULL){
        perror("!!! Warning: no input file for AFL. Tests will be executed with static data!!!\n");
        __afl_key = eapol_key_data_29;
        __afl_key1= eapol_key_data_31;
        len_frame1= sizeof(eapol_key_data_29);
        len_frame2= sizeof(eapol_key_data_31);
        return(NULL);
    }
    else{
        printf("+++ File open correctly! +++\n");
    }


    //printf("Reading Frame 1\n");
    fread(&len_frame1, sizeof(size_t), 1, fp1);
    if(len_frame1 <= 0 || len_frame1 > 1024 ){
        printf("\nLen frame1 is either < 0 or > of file size! %ld\n",len_frame1 );
        exit(EXIT_FAILURE);

    }

    printf("Frame 1 len: %ld\n\n",len_frame1);

    __afl_key = (char *)malloc(sizeof(char)* len_frame1);
    for (i=0 ; i <= len_frame1; i++){
        fread(&data, sizeof(uint8_t), 1, fp1);
        __afl_key[i] = data;
        printf("%d ", __afl_key[i]);
    }


    //  printf("\nReading Frame 2\n");
    fread(&len_frame2, sizeof(size_t), 1, fp1);
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
        fread(&data, sizeof(uint8_t), 1, fp1);
        __afl_key1[i] = data;
        printf("%d ", __afl_key1[i]);
    }
    printf("\n");
    printf("\n------------------------------------------------------\n");

    fclose(fp1);

    return(NULL);
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

    if((ek_len != expected_step4_frame_size)){
        printf("step4 different frame size\n");
       // assert(false);
        exit(1);
    }
    if(memcmp(ek, expected_step4_frame, expected_step4_frame_size)){
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
    __afl_get_key_data_ptk();
   // create_eapol_key_data_afl();

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
//     assert(verify_step2_called);

    if(verify_step2_called == true){
        printf("step 2 true\n");
    }

    //free(__afl_key);


    //XXX msg4 ---

    printf("\n\n XXX Working on second frame XXX \n\n");


    __eapol_set_tx_packet_func(verify_step4);
    __eapol_rx_packet(1, aa, ETH_P_PAE, __afl_key1, len_frame2, false);
    //assert(verify_step4_called);

    if(verify_step4_called == true){
        printf("step 4 true\n");
    }

    //free(__afl_key1);
    eapol_sm_free(sm);
    handshake_state_free(hs);
    printf("Exit from test\n");
    eapol_exit();
}




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
    printf("\n--- DEBUG IN eapol_sm_test_igtk --- \n\n");

    __afl_get_key_data_igtk();

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
    __eapol_rx_packet(1, aa, ETH_P_PAE, __afl_key,
                      len_frame1, false);
   // assert(verify_step2_called);
    if(verify_step2_called == true){
        printf("step 2 true\n");
    }

   // free(__afl_key);

    __eapol_set_tx_packet_func(verify_step4);
    __eapol_rx_packet(1, aa, ETH_P_PAE, __afl_key1,
                      len_frame2, false);
    //assert(verify_step4_called);
    if(verify_step4_called == true){
        printf("step 4 true\n");
    }
  //  free(__afl_key1);
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



    if (!l_checksum_is_supported(L_CHECKSUM_MD5, true) ||
        !l_checksum_is_supported(L_CHECKSUM_SHA1, true))
        goto done;


    l_test_add("EAPoL/WPA2 PTK State Machine", &eapol_sm_test_ptk, NULL);

	l_test_add("EAPoL IGTK & 4-Way Handshake",
			&eapol_sm_test_igtk, NULL);

    done:
    return l_test_run();
}



