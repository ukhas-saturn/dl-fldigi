
/* SSDV - Slow Scan Digital Video                                        */
/*=======================================================================*/
/* Copyright 2011-2012 Philip Heron <phil@sanslogic.co.uk                */
/*                                                                       */
/* This program is free software: you can redistribute it and/or modify  */
/* it under the terms of the GNU General Public License as published by  */
/* the Free Software Foundation, either version 3 of the License, or     */
/* (at your option) any later version.                                   */
/*                                                                       */
/* This program is distributed in the hope that it will be useful,       */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of        */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         */
/* GNU General Public License for more details.                          */
/*                                                                       */
/* You should have received a copy of the GNU General Public License     */
/* along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#include <stdint.h>

#ifndef INC_SSDV_H
#define INC_SSDV_H
#ifdef __cplusplus
extern "C" {
#endif

#define SSDV_ERROR       (-1)
#define SSDV_OK          (0)
#define SSDV_FEED_ME     (1)
#define SSDV_HAVE_PACKET (2)
#define SSDV_BUFFER_FULL (3)
#define SSDV_EOI         (4)

/* Packet details */
#define SSDV_PKT_SIZE         (0x100)
#define SSDV_PKT_SIZE_HEADER  (0x0F)
#define SSDV_PKT_SIZE_CRC     (0x04)
#define SSDV_PKT_SIZE_RSCODES (0x20)
#define SSDV_PKT_SIZE_PAYLOAD (SSDV_PKT_SIZE - SSDV_PKT_SIZE_HEADER - SSDV_PKT_SIZE_CRC - SSDV_PKT_SIZE_RSCODES)
#define SSDV_PKT_SIZE_CRCDATA (SSDV_PKT_SIZE_HEADER + SSDV_PKT_SIZE_PAYLOAD - 1)

#define TBL_LEN (546) /* Maximum size of the DQT and DHT tables */
#define HBUFF_LEN (16) /* Extra space for reading marker data */
//#define COMPONENTS (3)

typedef struct
{
	/* Image information */
	uint16_t width;
	uint16_t height;
	uint32_t callsign;
	uint8_t  image_id;
	uint16_t packet_id;
	uint8_t  mcu_mode;  /* 0 = 2x2, 1 = 2x1, 2 = 1x2, 3 = 1x1           */
	uint16_t mcu_id;
	uint16_t mcu_count;
	uint16_t packet_mcu_id;
	uint8_t  packet_mcu_offset;
	
	/* Source buffer */
	uint8_t *inp;      /* Pointer to next input byte                    */
	size_t in_len;     /* Number of input bytes remaining               */
	size_t in_skip;    /* Number of input bytes to skip                 */
	
	/* Source bits */
	uint32_t workbits; /* Input bits currently being worked on          */
	uint8_t worklen;   /* Number of bits in the input bit buffer        */
	
	/* JPEG / Packet output buffer */
	uint8_t *out;      /* Pointer to the beginning of the output buffer */
	uint8_t *outp;     /* Pointer to the next output byte               */
	size_t out_len;    /* Number of output bytes remaining              */
	char out_stuff;    /* Flag to add stuffing bytes to output          */
	
	/* Output bits */
	uint32_t outbits;  /* Output bit buffer                             */
	uint8_t outlen;    /* Number of bits in the output bit buffer       */
	
	/* JPEG decoder state */
	enum {
		S_MARKER = 0,
		S_MARKER_LEN,
		S_MARKER_DATA,
		S_HUFF,
		S_INT,
		S_EOI,
	} state;
	uint16_t marker;    /* Current marker                               */
	uint16_t marker_len; /* Length of data following marker             */
	uint8_t *marker_data; /* Where to copy marker data too              */
	uint16_t marker_data_len; /* How much is there                      */
	uint8_t component;  /* 0 = Y, 1 = Cb, 2 = Cr                        */
	uint8_t ycparts;    /* Number of Y component parts per MCU          */
	uint8_t mcupart;    /* 0-3 = Y, 4 = Cb, 5 = Cr                      */
	uint8_t acpart;     /* 0 - 64; 0 = DC, 1 - 64 = AC                  */
	int dc[3];          /* DC value for each component                  */
	int adc[3];         /* DC adjusted value for each component         */
	uint8_t acrle;      /* RLE value for current AC value               */
	uint8_t accrle;     /* Accumulative RLE value                       */
	uint16_t dri;       /* Reset interval                               */
	enum {
		S_ENCODING = 0,
		S_DECODING,
	} mode;
	uint32_t reset_mcu; /* MCU block to do absolute encoding            */
	char needbits;      /* Number of bits needed to decode integer      */
	
	/* The input huffman and quantisation tables */
	uint8_t stbls[TBL_LEN + HBUFF_LEN];
	uint8_t *sdht[2][2], *sdqt[2];
	uint16_t stbl_len;
	
	/* The same for output */
	uint8_t dtbls[TBL_LEN];
	uint8_t *ddht[2][2], *ddqt[2];
	uint16_t dtbl_len;
	
} ssdv_t;

typedef struct {
	uint32_t callsign;
	uint8_t  image_id;
	uint16_t packet_id;
	uint16_t width;
	uint16_t height;
	uint8_t  mcu_mode;
	uint8_t  mcu_offset;
	uint16_t mcu_id;
	uint16_t mcu_count;
} ssdv_packet_info_t;

/* WebP:
struct {
        // 0x55, 0x77;
        uint32_t callsign;
        uint8_t  image_id;
        uint8_t  subimage_id;
        uint16_t webp_flags;
        // 210 bytes data
        // 4 bytes crc;
        // 32 bytes fec;
        // = 256 bytes
} ssdv_packet_webp_t; 
*/

#define WEBP_DATA_OFFSET (2 +4 +2 +2)
#define WEBP_CRC_OFFSET (10 + 210)
#define WEBP_FEC_OFFSET (220 + 4)

/* RIFF Header: "RIFFxxxxWEBPVP8 yyyy", length 240 = 8 + 0xe8 = 20 + 0xdc = 20+10 + 210 byte payload*/
/* Extended header : 2 bytes of flags(and size), 00,0x9d,01,0x2a, 4bytes (size 48,0,48,0) = 10bytes */
#define WEBP_LEN (240)
#define WEBP_HEADER_LEN (30)
#define WEBP_DATA_LEN (WEBP_LEN - WEBP_HEADER_LEN)
#define WEBP_HEADER {   0x52, 0x49, 0x46, 0x46, 0xe8, 0x00, 0x00, 0x00,\
                        0x57, 0x45, 0x42, 0x50, 0x56, 0x50, 0x38, 0x20,\
                        0xdc, 0x00, 0x00, 0x00, 0x30, 0x08, 0x00, 0x9D,\
                        0x01, 0x2A, 0x30, 0x00, 0x30, 0x00, }
#define WEBP_FLAG_OFFSET (20)

extern int ssdv_hook_webp(const uint8_t *packet, const uint8_t *erasures);

/* Encoding */
extern char ssdv_enc_init(ssdv_t *s, char *callsign, uint8_t image_id);
extern char ssdv_enc_set_buffer(ssdv_t *s, uint8_t *buffer);
extern char ssdv_enc_get_packet(ssdv_t *s);
extern char ssdv_enc_feed(ssdv_t *s, uint8_t *buffer, size_t length);

/* Decoding */
extern char ssdv_dec_init(ssdv_t *s);
extern char ssdv_dec_set_buffer(ssdv_t *s, uint8_t *buffer, size_t length);
extern char ssdv_dec_feed(ssdv_t *s, uint8_t *packet);
extern char ssdv_dec_get_jpeg(ssdv_t *s, uint8_t **jpeg, size_t *length);

extern char ssdv_dec_is_packet(uint8_t *packet, int *errors, uint8_t *erasures);
extern void ssdv_dec_header(ssdv_packet_info_t *info, uint8_t *packet);

/* Callsign */
extern uint32_t ssdv_encode_callsign(char *callsign);
extern char *ssdv_decode_callsign(char *callsign, uint32_t code);

#ifdef __cplusplus
}
#endif
#endif

