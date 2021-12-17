/* Copyright (C) 2009 - 2022 National Aeronautics and Space Administration.
   All Foreign Rights are Reserved to the U.S. Government.

   This software is provided "as is" without any warranty of any kind, either expressed, implied, or statutory,
   including, but not limited to, any warranty that the software will conform to specifications, any implied warranties
   of merchantability, fitness for a particular purpose, and freedom from infringement, and any warranty that the
   documentation will conform to the program, or any warranty that the software will be error free.

   In no event shall NASA be liable for any damages, including, but not limited to direct, indirect, special or
   consequential damages, arising out of, resulting from, or in any way connected with the software or its
   documentation, whether or not based upon warranty, contract, tort or otherwise, and whether or not loss was sustained
   from, or arose out of the results of, or use of, the software, documentation or services provided hereunder.

   ITC Team
   NASA IV&V
   jstar-development-team@mail.nasa.gov
*/

/*
** Includes
*/
#include "crypto.h"

/*
** Key Management Services
*/
/**
 * @brief Function: Crypto_Key_OTAR
 * The OTAR Rekeying procedure shall have the following Service Parameters:
 * a- Key ID of the Master Key (Integer, unmanaged)
 * b- Size of set of Upload Keys (Integer, managed)
 * c- Set of Upload Keys (Integer[Session Key]; managed)
 * NOTE- The size of the session keys is mission specific.
 * a- Set of Key IDs of Upload Keys (Integer[Key IDs]; managed)
 * b- Set of Encrypted Upload Keys (Integer[Size of set of Key ID]; unmanaged)
 * c- Agreed Cryptographic Algorithm (managed)
 * @return int32: Success/Failure
 **/
int32_t Crypto_Key_OTAR(void)
{
    // Local variables
    SDLS_OTAR_t packet;
    int count = 0;
    int x = 0;
    int32_t status = CRYPTO_LIB_SUCCESS;
    int pdu_keys = (sdls_frame.pdu.pdu_len - 30) / (2 + KEY_SIZE);

    gcry_cipher_hd_t tmp_hd;
    gcry_error_t gcry_error = GPG_ERR_NO_ERROR;

    // Master Key ID
    packet.mkid = (sdls_frame.pdu.data[0] << 8) | (sdls_frame.pdu.data[1]);

    if (packet.mkid >= 128)
    {
        report.af = 1;
        if (log_summary.rs > 0)
        {
            Crypto_increment((uint8_t *)&log_summary.num_se, 4);
            log_summary.rs--;
            mc_log.blk[log_count].emt = MKID_INVALID_EID;
            mc_log.blk[log_count].emv[0] = 0x4E;
            mc_log.blk[log_count].emv[1] = 0x41;
            mc_log.blk[log_count].emv[2] = 0x53;
            mc_log.blk[log_count].emv[3] = 0x41;
            mc_log.blk[log_count++].em_len = 4;
        }
        printf(KRED "Error: MKID is not valid! \n" RESET);
        status = CRYPTO_LIB_ERROR;
        return status;
    }

    for (int count = 2; count < (2 + IV_SIZE); count++)
    { // Initialization Vector
        packet.iv[count - 2] = sdls_frame.pdu.data[count];
        // printf("packet.iv[%d] = 0x%02x\n", count-2, packet.iv[count-2]);
    }

    count = sdls_frame.pdu.pdu_len - MAC_SIZE;
    for (int w = 0; w < 16; w++)
    { // MAC
        packet.mac[w] = sdls_frame.pdu.data[count + w];
        // printf("packet.mac[%d] = 0x%02x\n", w, packet.mac[w]);
    }

    gcry_error = gcry_cipher_open(&(tmp_hd), GCRY_CIPHER_AES256, GCRY_CIPHER_MODE_GCM, GCRY_CIPHER_CBC_MAC);
    if ((gcry_error & GPG_ERR_CODE_MASK) != GPG_ERR_NO_ERROR)
    {
        printf(KRED "ERROR: gcry_cipher_open error code %d\n" RESET, gcry_error & GPG_ERR_CODE_MASK);
        status = CRYPTO_LIB_ERROR;
        return status;
    }
    gcry_error = gcry_cipher_setkey(tmp_hd, &(ek_ring[packet.mkid].value[0]), KEY_SIZE);
    if ((gcry_error & GPG_ERR_CODE_MASK) != GPG_ERR_NO_ERROR)
    {
        printf(KRED "ERROR: gcry_cipher_setkey error code %d\n" RESET, gcry_error & GPG_ERR_CODE_MASK);
        status = CRYPTO_LIB_ERROR;
        return status;
    }
    gcry_error = gcry_cipher_setiv(tmp_hd, &(packet.iv[0]), IV_SIZE);
    if ((gcry_error & GPG_ERR_CODE_MASK) != GPG_ERR_NO_ERROR)
    {
        printf(KRED "ERROR: gcry_cipher_setiv error code %d\n" RESET, gcry_error & GPG_ERR_CODE_MASK);
        status = CRYPTO_LIB_ERROR;
        return status;
    }
    gcry_error = gcry_cipher_decrypt(tmp_hd,
                                     &(sdls_frame.pdu.data[14]), // plaintext output
                                     pdu_keys * (2 + KEY_SIZE),  // length of data
                                     NULL,                       // in place decryption
                                     0                           // in data length
    );
    if ((gcry_error & GPG_ERR_CODE_MASK) != GPG_ERR_NO_ERROR)
    {
        printf(KRED "ERROR: gcry_cipher_decrypt error code %d\n" RESET, gcry_error & GPG_ERR_CODE_MASK);
        status = CRYPTO_LIB_ERROR;
        return status;
    }
    gcry_error = gcry_cipher_checktag(tmp_hd,
                                      &(packet.mac[0]), // tag input
                                      MAC_SIZE          // tag size
    );
    if ((gcry_error & GPG_ERR_CODE_MASK) != GPG_ERR_NO_ERROR)
    {
        printf(KRED "ERROR: gcry_cipher_checktag error code %d\n" RESET, gcry_error & GPG_ERR_CODE_MASK);
        status = CRYPTO_LIB_ERROR;
        return status;
    }
    gcry_cipher_close(tmp_hd);

    // Read in Decrypted Data
    for (int count = 14; x < pdu_keys; x++)
    { // Encrypted Key Blocks
        packet.EKB[x].ekid = (sdls_frame.pdu.data[count] << 8) | (sdls_frame.pdu.data[count + 1]);
        if (packet.EKB[x].ekid < 128)
        {
            report.af = 1;
            if (log_summary.rs > 0)
            {
                Crypto_increment((uint8_t *)&log_summary.num_se, 4);
                log_summary.rs--;
                mc_log.blk[log_count].emt = OTAR_MK_ERR_EID;
                mc_log.blk[log_count].emv[0] = 0x4E; // N
                mc_log.blk[log_count].emv[1] = 0x41; // A
                mc_log.blk[log_count].emv[2] = 0x53; // S
                mc_log.blk[log_count].emv[3] = 0x41; // A
                mc_log.blk[log_count++].em_len = 4;
            }
            printf(KRED "Error: Cannot OTAR master key! \n" RESET);
            status = CRYPTO_LIB_ERROR;
            return status;
        }
        else
        {
            count = count + 2;
            for (int y = count; y < (KEY_SIZE + count); y++)
            { // Encrypted Key
                packet.EKB[x].ek[y - count] = sdls_frame.pdu.data[y];
#ifdef SA_DEBUG
                printf("\t packet.EKB[%d].ek[%d] = 0x%02x\n", x, y - count, packet.EKB[x].ek[y - count]);
#endif

                // Setup Key Ring
                ek_ring[packet.EKB[x].ekid].value[y - count] = sdls_frame.pdu.data[y];
            }
            count = count + KEY_SIZE;

            // Set state to PREACTIVE
            ek_ring[packet.EKB[x].ekid].key_state = KEY_PREACTIVE;
        }
    }

#ifdef PDU_DEBUG
    printf("Received %d keys via master key %d: \n", pdu_keys, packet.mkid);
    for (int x = 0; x < pdu_keys; x++)
    {
        printf("%d) Key ID = %d, 0x", x + 1, packet.EKB[x].ekid);
        for (int y = 0; y < KEY_SIZE; y++)
        {
            printf("%02x", packet.EKB[x].ek[y]);
        }
        printf("\n");
    }
#endif

    return CRYPTO_LIB_SUCCESS;
}
/**
 * @brief Function: Crypto_Key_update
 * Updates the state of the all keys in the received SDLS EP PDU
 * @param state: uint8
 * @return uint32: Success/Failure
 **/
int32_t Crypto_Key_update(uint8_t state)
{ // Local variables
    SDLS_KEY_BLK_t packet;
    int count = 0;
    int pdu_keys = sdls_frame.pdu.pdu_len / 2;
#ifdef PDU_DEBUG
    printf("Keys ");
#endif
    // Read in PDU
    for (int x = 0; x < pdu_keys; x++)
    {
        packet.kblk[x].kid = (sdls_frame.pdu.data[count] << 8) | (sdls_frame.pdu.data[count + 1]);
        count = count + 2;
#ifdef PDU_DEBUG
        if (x != (pdu_keys - 1))
        {
            printf("%d, ", packet.kblk[x].kid);
        }
        else
        {
            printf("and %d ", packet.kblk[x].kid);
        }
#endif
    }
#ifdef PDU_DEBUG
    printf("changed to state ");
    switch (state)
    {
    case KEY_PREACTIVE:
        printf("PREACTIVE. \n");
        break;
    case KEY_ACTIVE:
        printf("ACTIVE. \n");
        break;
    case KEY_DEACTIVATED:
        printf("DEACTIVATED. \n");
        break;
    case KEY_DESTROYED:
        printf("DESTROYED. \n");
        break;
    case KEY_CORRUPTED:
        printf("CORRUPTED. \n");
        break;
    default:
        printf("ERROR. \n");
        break;
    }
#endif
    // Update Key State
    for (int x = 0; x < pdu_keys; x++)
    {
        if (packet.kblk[x].kid < 128)
        {
            report.af = 1;
            if (log_summary.rs > 0)
            {
                Crypto_increment((uint8_t *)&log_summary.num_se, 4);
                log_summary.rs--;
                mc_log.blk[log_count].emt = MKID_STATE_ERR_EID;
                mc_log.blk[log_count].emv[0] = 0x4E;
                mc_log.blk[log_count].emv[1] = 0x41;
                mc_log.blk[log_count].emv[2] = 0x53;
                mc_log.blk[log_count].emv[3] = 0x41;
                mc_log.blk[log_count++].em_len = 4;
            }
            printf(KRED "Error: MKID state cannot be changed! \n" RESET);
            // TODO: Exit
        }

        if (ek_ring[packet.kblk[x].kid].key_state == (state - 1))
        {
            ek_ring[packet.kblk[x].kid].key_state = state;
#ifdef PDU_DEBUG
            // printf("Key ID %d state changed to ", packet.kblk[x].kid);
#endif
        }
        else
        {
            if (log_summary.rs > 0)
            {
                Crypto_increment((uint8_t *)&log_summary.num_se, 4);
                log_summary.rs--;
                mc_log.blk[log_count].emt = KEY_TRANSITION_ERR_EID;
                mc_log.blk[log_count].emv[0] = 0x4E;
                mc_log.blk[log_count].emv[1] = 0x41;
                mc_log.blk[log_count].emv[2] = 0x53;
                mc_log.blk[log_count].emv[3] = 0x41;
                mc_log.blk[log_count++].em_len = 4;
            }
            printf(KRED "Error: Key %d cannot transition to desired state! \n" RESET, packet.kblk[x].kid);
        }
    }
    return CRYPTO_LIB_SUCCESS;
}

/**
 * @brief Function: Crypto_Key_inventory
 * @param ingest: uint8_t*
 * @return int32: count
 **/
int32_t Crypto_Key_inventory(uint8_t *ingest)
{
    // Local variables
    SDLS_KEY_INVENTORY_t packet;
    int count = 0;
    uint16_t range = 0;

    // Read in PDU
    packet.kid_first = ((uint8_t)sdls_frame.pdu.data[count] << 8) | ((uint8_t)sdls_frame.pdu.data[count + 1]);
    count = count + 2;
    packet.kid_last = ((uint8_t)sdls_frame.pdu.data[count] << 8) | ((uint8_t)sdls_frame.pdu.data[count + 1]);
    count = count + 2;

    // Prepare for Reply
    range = packet.kid_last - packet.kid_first;
    sdls_frame.pdu.pdu_len = 2 + (range * (2 + 1));
    sdls_frame.hdr.pkt_length = sdls_frame.pdu.pdu_len + 9;
    count = Crypto_Prep_Reply(ingest, 128);
    ingest[count++] = (range & 0xFF00) >> 8;
    ingest[count++] = (range & 0x00FF);
    for (uint16_t x = packet.kid_first; x < packet.kid_last; x++)
    { // Key ID
        ingest[count++] = (x & 0xFF00) >> 8;
        ingest[count++] = (x & 0x00FF);
        // Key State
        ingest[count++] = ek_ring[x].key_state;
    }
    return count;
}

/**
 * @brief Function: Crypto_Key_verify
 * @param ingest: uint8_t*
 * @param tc_frame: TC_t*
 * @return int32: count
 **/
int32_t Crypto_Key_verify(uint8_t *ingest, TC_t *tc_frame)
{
    // Local variables
    SDLS_KEYV_CMD_t packet;
    int count = 0;
    int pdu_keys = sdls_frame.pdu.pdu_len / SDLS_KEYV_CMD_BLK_SIZE;

    gcry_error_t gcry_error = GPG_ERR_NO_ERROR;
    gcry_cipher_hd_t tmp_hd;
    uint8_t iv_loc;

    // uint8_t tmp_mac[MAC_SIZE];

#ifdef PDU_DEBUG
    printf("Crypto_Key_verify: Requested %d key(s) to verify \n", pdu_keys);
#endif

    // Read in PDU
    for (int x = 0; x < pdu_keys; x++)
    {
        // Key ID
        packet.blk[x].kid = ((uint8_t)sdls_frame.pdu.data[count] << 8) | ((uint8_t)sdls_frame.pdu.data[count + 1]);
        count = count + 2;
#ifdef PDU_DEBUG
        printf("Crypto_Key_verify: Block %d Key ID is %d \n", x, packet.blk[x].kid);
#endif
        // Key Challenge
        for (int y = 0; y < CHALLENGE_SIZE; y++)
        {
            packet.blk[x].challenge[y] = sdls_frame.pdu.data[count++];
        }
#ifdef PDU_DEBUG
        printf("\n");
#endif
    }

    // Prepare for Reply
    sdls_frame.pdu.pdu_len = pdu_keys * (2 + IV_SIZE + CHALLENGE_SIZE + CHALLENGE_MAC_SIZE);
    sdls_frame.hdr.pkt_length = sdls_frame.pdu.pdu_len + 9;
    count = Crypto_Prep_Reply(ingest, 128);

    for (int x = 0; x < pdu_keys; x++)
    { // Key ID
        ingest[count++] = (packet.blk[x].kid & 0xFF00) >> 8;
        ingest[count++] = (packet.blk[x].kid & 0x00FF);

        // Initialization Vector
        iv_loc = count;
        for (int y = 0; y < IV_SIZE; y++)
        {
            ingest[count++] = *(tc_frame->tc_sec_header.iv + y);
        }
        ingest[count - 1] = ingest[count - 1] + x + 1;

        // Encrypt challenge
        gcry_error = gcry_cipher_open(&(tmp_hd), GCRY_CIPHER_AES256, GCRY_CIPHER_MODE_GCM, GCRY_CIPHER_CBC_MAC);
        if ((gcry_error & GPG_ERR_CODE_MASK) != GPG_ERR_NO_ERROR)
        {
            printf(KRED "ERROR: gcry_cipher_open error code %d\n" RESET, gcry_error & GPG_ERR_CODE_MASK);
        }
        gcry_error = gcry_cipher_setkey(tmp_hd, &(ek_ring[packet.blk[x].kid].value[0]), KEY_SIZE);
        if ((gcry_error & GPG_ERR_CODE_MASK) != GPG_ERR_NO_ERROR)
        {
            printf(KRED "ERROR: gcry_cipher_setkey error code %d\n" RESET, gcry_error & GPG_ERR_CODE_MASK);
        }
        gcry_error = gcry_cipher_setiv(tmp_hd, &(ingest[iv_loc]), IV_SIZE);
        if ((gcry_error & GPG_ERR_CODE_MASK) != GPG_ERR_NO_ERROR)
        {
            printf(KRED "ERROR: gcry_cipher_setiv error code %d\n" RESET, gcry_error & GPG_ERR_CODE_MASK);
        }
        gcry_error = gcry_cipher_encrypt(tmp_hd,
                                         &(ingest[count]),              // ciphertext output
                                         CHALLENGE_SIZE,                // length of data
                                         &(packet.blk[x].challenge[0]), // plaintext input
                                         CHALLENGE_SIZE                 // in data length
        );
        if ((gcry_error & GPG_ERR_CODE_MASK) != GPG_ERR_NO_ERROR)
        {
            printf(KRED "ERROR: gcry_cipher_encrypt error code %d\n" RESET, gcry_error & GPG_ERR_CODE_MASK);
        }
        count = count + CHALLENGE_SIZE; // Don't forget to increment count!

        gcry_error = gcry_cipher_gettag(tmp_hd,
                                        &(ingest[count]),  // tag output
                                        CHALLENGE_MAC_SIZE // tag size
        );
        if ((gcry_error & GPG_ERR_CODE_MASK) != GPG_ERR_NO_ERROR)
        {
            printf(KRED "ERROR: gcry_cipher_gettag error code %d \n" RESET, gcry_error & GPG_ERR_CODE_MASK);
        }
        count = count + CHALLENGE_MAC_SIZE; // Don't forget to increment count!

        // Copy from tmp_mac into ingest
        // for( int y = 0; y < CHALLENGE_MAC_SIZE; y++)
        //{
        //    ingest[count++] = tmp_mac[y];
        //}
        gcry_cipher_close(tmp_hd);
    }

#ifdef PDU_DEBUG
    printf("Crypto_Key_verify: Response is %d bytes \n", count);
#endif

    return count;
}