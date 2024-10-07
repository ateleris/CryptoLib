#include "ut_ep_key_validation.h"
#include "crypto.h"
#include "crypto_error.h"
#include "sa_interface.h"
#include "utest.h"

UTEST(EP_KEY_VALIDATION, OTAR_0_140_142)
{
    remove("sa_save_file.bin");
    // Setup & Initialize CryptoLib
    Crypto_Config_CryptoLib(KEY_TYPE_INTERNAL, MC_TYPE_INTERNAL, SA_TYPE_INMEMORY, CRYPTOGRAPHY_TYPE_LIBGCRYPT, 
                            IV_INTERNAL, CRYPTO_TC_CREATE_FECF_TRUE, TC_PROCESS_SDLS_PDUS_TRUE, TC_HAS_PUS_HDR,
                            TC_IGNORE_SA_STATE_FALSE, TC_IGNORE_ANTI_REPLAY_TRUE, TC_UNIQUE_SA_PER_MAP_ID_FALSE,
                            TC_CHECK_FECF_FALSE, 0x3F, SA_INCREMENT_NONTRANSMITTED_IV_TRUE);

    // Crypto_Config_Add_Gvcid_Managed_Parameter(0, 0x0003, 0, TC_NO_FECF, TC_HAS_SEGMENT_HDRS, TC_OCF_NA, 1024, AOS_FHEC_NA, AOS_IZ_NA, 0);
    GvcidManagedParameters_t TC_0_Managed_Parameters = {0, 0x0003, 0, TC_NO_FECF, AOS_FHEC_NA, AOS_IZ_NA, 0, TC_HAS_SEGMENT_HDRS, 1024, TC_OCF_NA, 1};
    Crypto_Config_Add_Gvcid_Managed_Parameters(TC_0_Managed_Parameters);
    
    // Crypto_Config_Add_Gvcid_Managed_Parameter(0, 0x0003, 1, TC_NO_FECF, TC_HAS_SEGMENT_HDRS, TC_OCF_NA, 1024, AOS_FHEC_NA, AOS_IZ_NA, 0);
    GvcidManagedParameters_t TC_1_Managed_Parameters = {0, 0x0003, 1, TC_NO_FECF, AOS_FHEC_NA, AOS_IZ_NA, 0, TC_HAS_SEGMENT_HDRS, 1024, TC_OCF_NA, 1};
    Crypto_Config_Add_Gvcid_Managed_Parameters(TC_1_Managed_Parameters);
    
    Crypto_Init();
    SaInterface sa_if = get_sa_interface_inmemory();
    crypto_key_t* ekp = NULL;
    int status = CRYPTO_LIB_SUCCESS;

    // NOTE: Added Transfer Frame header to the plaintext
    char* buffer_nist_key_h = "000102030405060708090A0B0C0D0E0F000102030405060708090A0B0C0D0E0F";
    char* buffer_nist_iv_h = "b6ac8e4963f49207ffd6374b"; // The last valid IV that was seen by the SA
    char* buffer_OTAR_h = "2003009e00ff000000001880d037008c197f0b000100840000344892bbc54f5395297d4c37172f2a3c46f6a81c1349e9e26ac80985d8bbd55a5814c662e49fba52f99ba09558cd21cf268b8e50b2184137e80f76122034c580464e2f06d2659a50508bdfe9e9a55990ba4148af896d8a6eebe8b5d2258685d4ce217a20174fdd4f0efac62758c51b04e55710a47209c923b641d19a39001f9e986166f5ffd95555";
    //                    |2003009e00| = Primary Header
    //                              |ff| = Ext. Procs
    //                                |0000| = SPI
    //                                    |0000| = ARSN
    //                                        |1880| = CryptoLib App ID
    //                                            |d037| = seq, pktid
    //                                                |008c| = pkt_length
    //                                                    |197f| = pusv, ack, st
    //                                                        |0b| = sst, sid, spare
    //                                                          |0001| = PDU Tag
    //                                                              |0084| = PDU Length
    //                                                                  |0000| = Master Key ID
    //                                                                      |344892bbc54f5395297d4c37| = IV
    //                                                                                              |172f| = Encrypted Key ID
    //                                                                                                  |2a3c46f6a81c1349e9e26ac80985d8bbd55a5814c662e49fba52f99ba09558cd| = Encrypted Key
    //                                                                                                                                                                  |21cf| = Encrypted Key ID
    //                                                                                                                                                                      |268b8e50b2184137e80f76122034c580464e2f06d2659a50508bdfe9e9a55990| = Encrypted Key
    //                                                                                                                                                                                                                                      |ba41| = EKID
    //                                                                                                                                                                                                                                          |48af896d8a6eebe8b5d2258685d4ce217a20174fdd4f0efac62758c51b04e557| = EK
    //                                                                                                                                                                                                                                                                                                          |10a47209c923b641d19a39001f9e9861| = MAC
    //                                                                                                                                                                                                                                                                                                                                          |66f5ffd95555| = Trailer or Padding???
    
    uint8_t *buffer_nist_iv_b, *buffer_nist_key_b, *buffer_OTAR_b = NULL;
    int buffer_nist_iv_len, buffer_nist_key_len, buffer_OTAR_len = 0;

    // Setup Processed Frame For Decryption
    TC_t tc_nist_processed_frame;

    // Expose/setup SAs for testing
    SecurityAssociation_t* test_association;

    // Deactivate SA 1
    sa_if->sa_get_from_spi(1, &test_association);
    test_association->sa_state = SA_NONE;

    // Activate SA 0
    sa_if->sa_get_from_spi(0, &test_association);
    test_association->sa_state = SA_OPERATIONAL;
    test_association->ecs_len = 1;
    test_association->ecs = CRYPTO_CIPHER_NONE;
    test_association->est = 0;
    test_association->ast = 0;
    test_association->shsnf_len = 2;
    test_association->arsn_len = 2;
    test_association->arsnw = 5;
    test_association->iv_len = 12;
    // Insert key into keyring of SA 9
    hex_conversion(buffer_nist_key_h, (char**) &buffer_nist_key_b, &buffer_nist_key_len);
    ekp = key_if->get_key(test_association->ekid);
    memcpy(ekp->value, buffer_nist_key_b, buffer_nist_key_len);

    // Convert frames that will be processed
    hex_conversion(buffer_OTAR_h, (char**) &buffer_OTAR_b, &buffer_OTAR_len);
    // Convert/Set input IV
    hex_conversion(buffer_nist_iv_h, (char**) &buffer_nist_iv_b, &buffer_nist_iv_len);
    memcpy(test_association->iv, buffer_nist_iv_b, buffer_nist_iv_len);

    // Expect success on next valid IV && ARSN
    printf(KGRN "Checking  next valid IV && valid ARSN... should be able to receive it... \n" RESET);
    status = Crypto_TC_ProcessSecurity(buffer_OTAR_b, &buffer_OTAR_len, &tc_nist_processed_frame);
    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);

    printf("\n");
    Crypto_Shutdown();
    free(buffer_nist_iv_b);
    free(buffer_nist_key_b);
    free(buffer_OTAR_b);
}

UTEST(EP_KEY_VALIDATION, ACTIVATE_141_142)
{
    remove("sa_save_file.bin");
    uint8_t* ptr_enc_frame = NULL;
    // Setup & Initialize CryptoLib
    Crypto_Config_CryptoLib(KEY_TYPE_INTERNAL, MC_TYPE_INTERNAL, SA_TYPE_INMEMORY, CRYPTOGRAPHY_TYPE_LIBGCRYPT, 
                            IV_INTERNAL, CRYPTO_TC_CREATE_FECF_TRUE, TC_PROCESS_SDLS_PDUS_TRUE, TC_HAS_PUS_HDR,
                            TC_IGNORE_SA_STATE_FALSE, TC_IGNORE_ANTI_REPLAY_TRUE, TC_UNIQUE_SA_PER_MAP_ID_FALSE,
                            TC_CHECK_FECF_FALSE, 0x3F, SA_INCREMENT_NONTRANSMITTED_IV_TRUE);

    // Crypto_Config_Add_Gvcid_Managed_Parameter(0, 0x0003, 0, TC_NO_FECF, TC_HAS_SEGMENT_HDRS, TC_OCF_NA, 1024, AOS_FHEC_NA, AOS_IZ_NA, 0);
    GvcidManagedParameters_t TC_0_Managed_Parameters = {0, 0x0003, 0, TC_NO_FECF, AOS_FHEC_NA, AOS_IZ_NA, 0, TC_HAS_SEGMENT_HDRS, 1024, TC_OCF_NA, 1};
    Crypto_Config_Add_Gvcid_Managed_Parameters(TC_0_Managed_Parameters);
    
    // Crypto_Config_Add_Gvcid_Managed_Parameter(0, 0x0003, 1, TC_NO_FECF, TC_HAS_SEGMENT_HDRS, TC_OCF_NA, 1024, AOS_FHEC_NA, AOS_IZ_NA, 0);
    GvcidManagedParameters_t TC_1_Managed_Parameters = {0, 0x0003, 1, TC_NO_FECF, AOS_FHEC_NA, AOS_IZ_NA, 0, TC_HAS_SEGMENT_HDRS, 1024, TC_OCF_NA, 1};
    Crypto_Config_Add_Gvcid_Managed_Parameters(TC_1_Managed_Parameters);
    
    Crypto_Init();
    SaInterface sa_if = get_sa_interface_inmemory();
    crypto_key_t* ekp = NULL;
    int status = CRYPTO_LIB_SUCCESS;

    // NOTE: Added Transfer Frame header to the plaintext
    char* buffer_nist_key_h = "000102030405060708090A0B0C0D0E0F000102030405060708090A0B0C0D0E0F";
    char* buffer_nist_iv_h = "b6ac8e4963f49207ffd6374b"; // The last valid IV that was seen by the SA
    char* buffer_ACTIVATE_h = "2003001e00ff000000001880d038000c197f0b00020004008d008e82ebe4fc55555555";
    //                        |2003001e00| = Primary Header
    //                                  |ff| = SPI
    //                                    |00010000| = Security Header
    //                                            |1880| = CryptoLib App ID
    //                                                |d038| = seq, pktid
    //                                                    |000c| = pkt_length
    //                                                        |197f| = pusv, ack, st
    //                                                            |0b| = sst, sid, spare
    //                                                              |0002| = PDU Tag
    //                                                                  |0004| = PDU Length
    //                                                                      |008d| = Key ID (141)
    //                                                                          |008e| = Key ID (142)
    //                                                                              |82ebe4fc55555555| = Trailer???
    
    uint8_t *buffer_nist_iv_b, *buffer_nist_key_b, *buffer_ACTIVATE_b = NULL;
    int buffer_nist_iv_len, buffer_nist_key_len, buffer_ACTIVATE_len = 0;

    // Setup Processed Frame For Decryption
    TC_t tc_nist_processed_frame;

    // Expose/setup SAs for testing
    SecurityAssociation_t* test_association;

    // Deactivate SA 1
    sa_if->sa_get_from_spi(1, &test_association);
    test_association->sa_state = SA_NONE;

    // Activate SA 9
    sa_if->sa_get_from_spi(0, &test_association);
    test_association->sa_state = SA_OPERATIONAL;
    test_association->ecs_len = 1;
    test_association->ecs = CRYPTO_CIPHER_NONE;
    test_association->shsnf_len = 2;
    test_association->arsn_len = 2;
    test_association->arsnw = 5;
    test_association->iv_len = 12;

    // Insert key into keyring of SA 9
    hex_conversion(buffer_nist_key_h, (char**) &buffer_nist_key_b, &buffer_nist_key_len);
    ekp = key_if->get_key(test_association->ekid);
    memcpy(ekp->value, buffer_nist_key_b, buffer_nist_key_len);

    // Convert frames that will be processed
    hex_conversion(buffer_ACTIVATE_h, (char**) &buffer_ACTIVATE_b, &buffer_ACTIVATE_len);
    // Convert/Set input IV
    hex_conversion(buffer_nist_iv_h, (char**) &buffer_nist_iv_b, &buffer_nist_iv_len);
    memcpy(test_association->iv, buffer_nist_iv_b, buffer_nist_iv_len);

    // Expect success on next valid IV && ARSN
    printf(KGRN "Checking  next valid IV && valid ARSN... should be able to receive it... \n" RESET);
    status = Crypto_TC_ProcessSecurity(buffer_ACTIVATE_b, &buffer_ACTIVATE_len, &tc_nist_processed_frame);
    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);

    printf("\n");
    Crypto_Shutdown();
    free(ptr_enc_frame);
    free(buffer_nist_iv_b);
    free(buffer_nist_key_b);
    free(buffer_ACTIVATE_b);
}

UTEST(EP_KEY_VALIDATION, DEACTIVATE_142)
{
    remove("sa_save_file.bin");
    uint8_t* ptr_enc_frame = NULL;
    // Setup & Initialize CryptoLib
    Crypto_Config_CryptoLib(KEY_TYPE_INTERNAL, MC_TYPE_INTERNAL, SA_TYPE_INMEMORY, CRYPTOGRAPHY_TYPE_LIBGCRYPT, 
                            IV_INTERNAL, CRYPTO_TC_CREATE_FECF_TRUE, TC_PROCESS_SDLS_PDUS_TRUE, TC_HAS_PUS_HDR,
                            TC_IGNORE_SA_STATE_FALSE, TC_IGNORE_ANTI_REPLAY_TRUE, TC_UNIQUE_SA_PER_MAP_ID_FALSE,
                            TC_CHECK_FECF_FALSE, 0x3F, SA_INCREMENT_NONTRANSMITTED_IV_TRUE);

    // Crypto_Config_Add_Gvcid_Managed_Parameter(0, 0x0003, 0, TC_NO_FECF, TC_HAS_SEGMENT_HDRS, TC_OCF_NA, 1024, AOS_FHEC_NA, AOS_IZ_NA, 0);
    GvcidManagedParameters_t TC_0_Managed_Parameters = {0, 0x0003, 0, TC_NO_FECF, AOS_FHEC_NA, AOS_IZ_NA, 0, TC_HAS_SEGMENT_HDRS, 1024, TC_OCF_NA, 1};
    Crypto_Config_Add_Gvcid_Managed_Parameters(TC_0_Managed_Parameters);
    
    // Crypto_Config_Add_Gvcid_Managed_Parameter(0, 0x0003, 1, TC_NO_FECF, TC_HAS_SEGMENT_HDRS, TC_OCF_NA, 1024, AOS_FHEC_NA, AOS_IZ_NA, 0);
    GvcidManagedParameters_t TC_1_Managed_Parameters = {0, 0x0003, 1, TC_NO_FECF, AOS_FHEC_NA, AOS_IZ_NA, 0, TC_HAS_SEGMENT_HDRS, 1024, TC_OCF_NA, 1};
    Crypto_Config_Add_Gvcid_Managed_Parameters(TC_1_Managed_Parameters);
    
    Crypto_Init();
    SaInterface sa_if = get_sa_interface_inmemory();
    crypto_key_t* ekp = NULL;
    int status = CRYPTO_LIB_SUCCESS;

    // NOTE: Added Transfer Frame header to the plaintext
    char* buffer_nist_key_h = "000102030405060708090A0B0C0D0E0F000102030405060708090A0B0C0D0E0F";
    char* buffer_nist_iv_h = "b6ac8e4963f49207ffd6374b"; // The last valid IV that was seen by the SA
    char* buffer_DEACTIVATE_h = "2003001c00ff000000001880d039000a197f0b00030002008e1f6d21c4555555555555";
    //                          |2003001c00| = Primary Header
    //                                    |ff| = SPI
    //                                      |00010000| = security Header
    //                                              |1880| = CryptoLib App ID
    //                                                  |d039| = seq, packet id
    //                                                      |000a| = Packet Length
    //                                                          |197f| = pusv, ack, st
    //                                                              |0b| = sst
    //                                                                |0003| = PDU Tag
    //                                                                    |0002| = PDU Length
    //                                                                        |008e| = Key ID (142)
    //                                                                            |1f6d82ebe4fc55555555| = Trailer???
    
    uint8_t *buffer_nist_iv_b, *buffer_nist_key_b, *buffer_DEACTIVATE_b = NULL;
    int buffer_nist_iv_len, buffer_nist_key_len, buffer_DEACTIVATE_len = 0;

    // Setup Processed Frame For Decryption
    TC_t tc_nist_processed_frame;

    // Expose/setup SAs for testing
    SecurityAssociation_t* test_association;

    // Deactivate SA 1
    sa_if->sa_get_from_spi(1, &test_association);
    test_association->sa_state = SA_NONE;

    // Activate SA 9
    sa_if->sa_get_from_spi(0, &test_association);
    test_association->sa_state = SA_OPERATIONAL;
    //test_association->ecs_len = 1;
    test_association->ecs = CRYPTO_CIPHER_NONE;
    test_association->est = 0;
    test_association->ast = 0;
    test_association->iv_len = 12;
    test_association->shsnf_len = 2;
    test_association->arsn_len = 2;
    test_association->arsnw = 5;

    // Insert key into keyring of SA 9
    hex_conversion(buffer_nist_key_h, (char**) &buffer_nist_key_b, &buffer_nist_key_len);
    ekp = key_if->get_key(142);
    memcpy(ekp->value, buffer_nist_key_b, buffer_nist_key_len);
    ekp->key_state = KEY_ACTIVE;

    // Convert frames that will be processed
    hex_conversion(buffer_DEACTIVATE_h, (char**) &buffer_DEACTIVATE_b, &buffer_DEACTIVATE_len);
    // Convert/Set input IV
    hex_conversion(buffer_nist_iv_h, (char**) &buffer_nist_iv_b, &buffer_nist_iv_len);
    memcpy(test_association->iv, buffer_nist_iv_b, buffer_nist_iv_len);

    // Expect success on next valid IV && ARSN
    printf(KGRN "Checking  next valid IV && valid ARSN... should be able to receive it... \n" RESET);
    status = Crypto_TC_ProcessSecurity(buffer_DEACTIVATE_b, &buffer_DEACTIVATE_len, &tc_nist_processed_frame);
    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);

    printf("\n");
    Crypto_Shutdown();
    free(ptr_enc_frame);
    free(buffer_nist_iv_b);
    free(buffer_nist_key_b);
    free(buffer_DEACTIVATE_b);
}

// UTEST(EP_KEY_VALIDATION, INVENTORY_132_134)
// {
//     remove("sa_save_file.bin");
//     // Setup & Initialize CryptoLib
//     Crypto_Config_CryptoLib(KEY_TYPE_INTERNAL, MC_TYPE_INTERNAL, SA_TYPE_INMEMORY, CRYPTOGRAPHY_TYPE_LIBGCRYPT, 
//                             IV_INTERNAL, CRYPTO_TC_CREATE_FECF_TRUE, TC_PROCESS_SDLS_PDUS_TRUE, TC_HAS_PUS_HDR,
//                             TC_IGNORE_SA_STATE_FALSE, TC_IGNORE_ANTI_REPLAY_TRUE, TC_UNIQUE_SA_PER_MAP_ID_FALSE,
//                             TC_CHECK_FECF_FALSE, 0x3F, SA_INCREMENT_NONTRANSMITTED_IV_TRUE);
//     // Crypto_Config_Add_Gvcid_Managed_Parameter(0, 0x0003, 0, TC_HAS_FECF, TC_HAS_SEGMENT_HDRS, TC_OCF_NA, 1024, AOS_FHEC_NA, AOS_IZ_NA, 0);
//     GvcidManagedParameters_t TC_0_Managed_Parameters = {0, 0x0003, 0, TC_NO_FECF, AOS_FHEC_NA, AOS_IZ_NA, 0, TC_HAS_SEGMENT_HDRS, 1024, TC_OCF_NA, 1};
//     Crypto_Config_Add_Gvcid_Managed_Parameters(TC_0_Managed_Parameters);
    
//     // Crypto_Config_Add_Gvcid_Managed_Parameter(0, 0x0003, 1, TC_HAS_FECF, TC_HAS_SEGMENT_HDRS, TC_OCF_NA, 1024, AOS_FHEC_NA, AOS_IZ_NA, 0);
//     //GvcidManagedParameters_t TC_1_Managed_Parameters = {0, 0x0003, 1, TC_NO_FECF, AOS_FHEC_NA, AOS_IZ_NA, 0, TC_HAS_SEGMENT_HDRS, 1024, TC_OCF_NA, 1};
//     //Crypto_Config_Add_Gvcid_Managed_Parameters(TC_1_Managed_Parameters);
    
//     int status = CRYPTO_LIB_SUCCESS;
//     status = Crypto_Init();
//     ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);
//     SaInterface sa_if = get_sa_interface_inmemory();
//     crypto_key_t* ekp = NULL;
    

//     // NOTE: Added Transfer Frame header to the plaintext
//     char* buffer_nist_key_h = "000102030405060708090A0B0C0D0E0F000102030405060708090A0B0C0D0E0F";
//     char* buffer_nist_iv_h = "000000000000000000000000"; // The last valid IV that was seen by the SA
//     char* buffer_INVENTORY_h = "2003001e00ff000000001880d03b000a197f0b00070004008400861f6d82ebe4fc55555555";
//                             // |2003001c00| = Primary Header
//                             //           |ff| = SPI
//                             //             |00000000| = security Header
//                             //                     |1880| = CryptoLib App ID
//                             //                         |d03b| = seq, packet id
//                             //                             |000a| = Packet Length
//                             //                                 |197f| = pusv, ack, st
//                             //                                     |0b| = sst
//                             //                                       |0007| = PDU Tag
//                             //                                           |0004| = PDU Length
//                             //                                               |0084| = Key ID (132)
//                             //                                                   |0086| = Key ID (134)
//                             //                                                       |1f6d82ebe4fc55555555| = Trailer???

//     uint8_t *buffer_nist_iv_b, *buffer_nist_key_b, *buffer_INVENTORY_b = NULL;
//     int buffer_nist_iv_len, buffer_nist_key_len, buffer_INVENTORY_len = 0;

//     // Setup Processed Frame For Decryption
//     TC_t tc_nist_processed_frame;

//     // Expose/setup SAs for testing
//     SecurityAssociation_t* test_association;

//     // Deactivate SA 1
//     sa_if->sa_get_from_spi(1, &test_association);
//     test_association->sa_state = SA_NONE;

//     // Activate SA 9
//     sa_if->sa_get_from_spi(0, &test_association);
//     test_association->sa_state = SA_OPERATIONAL;
//     test_association->ecs_len = 1;
//     test_association->ecs = CRYPTO_CIPHER_NONE;
//     test_association->est = 0;
//     test_association->ast = 0;
//     test_association->shsnf_len = 2;
//     test_association->arsn_len = 2;
//     test_association->arsnw = 5;
//     test_association->iv_len = 12;

//     // Insert key into keyring of SA 9
//     hex_conversion(buffer_nist_key_h, (char**) &buffer_nist_key_b, &buffer_nist_key_len);
//     ekp = key_if->get_key(test_association->ekid);
//     memcpy(ekp->value, buffer_nist_key_b, buffer_nist_key_len);
//     // Convert frames that will be processed
//     hex_conversion(buffer_INVENTORY_h, (char**) &buffer_INVENTORY_b, &buffer_INVENTORY_len);
//     // Convert/Set input IV
//     hex_conversion(buffer_nist_iv_h, (char**) &buffer_nist_iv_b, &buffer_nist_iv_len);
//     memcpy(test_association->iv, buffer_nist_iv_b, buffer_nist_iv_len);
//     // Expect success on next valid IV && ARSN
//     printf(KGRN "Checking  next valid IV && valid ARSN... should be able to receive it... \n" RESET);
//     status = Crypto_TC_ProcessSecurity(buffer_INVENTORY_b, &buffer_INVENTORY_len, &tc_nist_processed_frame);
//     ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);

//     printf("\n");
//     Crypto_Shutdown();
//     free(buffer_nist_iv_b);
//     free(buffer_nist_key_b);
//     ASSERT_EQ(0,0);
// }

UTEST(EP_KEY_VALIDATION, VERIFY_132_134)
{
    remove("sa_save_file.bin");
    // Setup & Initialize CryptoLib
    Crypto_Config_CryptoLib(KEY_TYPE_INTERNAL, MC_TYPE_INTERNAL, SA_TYPE_INMEMORY, CRYPTOGRAPHY_TYPE_LIBGCRYPT, 
                            IV_INTERNAL, CRYPTO_TC_CREATE_FECF_TRUE, TC_PROCESS_SDLS_PDUS_TRUE, TC_HAS_PUS_HDR,
                            TC_IGNORE_SA_STATE_FALSE, TC_IGNORE_ANTI_REPLAY_TRUE, TC_UNIQUE_SA_PER_MAP_ID_FALSE,
                            TC_CHECK_FECF_FALSE, 0x3F, SA_INCREMENT_NONTRANSMITTED_IV_TRUE);
    // Crypto_Config_Add_Gvcid_Managed_Parameter(0, 0x0003, 0, TC_HAS_FECF, TC_HAS_SEGMENT_HDRS, TC_OCF_NA, 1024, AOS_FHEC_NA, AOS_IZ_NA, 0);
    GvcidManagedParameters_t TC_0_Managed_Parameters = {0, 0x0003, 0, TC_NO_FECF, AOS_FHEC_NA, AOS_IZ_NA, 0, TC_HAS_SEGMENT_HDRS, 1024, TC_OCF_NA, 1};
    Crypto_Config_Add_Gvcid_Managed_Parameters(TC_0_Managed_Parameters);
    
    // Crypto_Config_Add_Gvcid_Managed_Parameter(0, 0x0003, 1, TC_HAS_FECF, TC_HAS_SEGMENT_HDRS, TC_OCF_NA, 1024, AOS_FHEC_NA, AOS_IZ_NA, 0);
    GvcidManagedParameters_t TC_1_Managed_Parameters = {0, 0x0003, 1, TC_NO_FECF, AOS_FHEC_NA, AOS_IZ_NA, 0, TC_HAS_SEGMENT_HDRS, 1024, TC_OCF_NA, 1};
    Crypto_Config_Add_Gvcid_Managed_Parameters(TC_1_Managed_Parameters);
    
    int status = CRYPTO_LIB_SUCCESS;
    status = Crypto_Init();
    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);
    SaInterface sa_if = get_sa_interface_inmemory();
    crypto_key_t* ekp = NULL;

    // NOTE: Added Transfer Frame header to the plaintext
    char* buffer_nist_key_h = "000102030405060708090A0B0C0D0E0F000102030405060708090A0B0C0D0E0F";
    char* buffer_VERIFY_h = "2003003e00ff000000001880d03a002c197f0b00040024008471fc3ad5b1c36ad56bd5a5432315cdab008675c06302465bc6d5091a29957eebed35c00a6ed8";
    //                      |2003003e00| = Primary Header
    //                                |ff| = SPI
    //                                  |00000000| = Security header
    //                                          |1880| = CryptoLib App ID
    //                                              |d03a| = seq, packet id
    //                                                  |002c| = Packet Length
    //                                                      |197f| = pusv, ack, st
    //                                                          |0b| = sst
    //                                                            |0004| = PDU Tag
    //                                                                |0024| = PDU Length
    //                                                                    |0084| = Key ID (132)
    //                                                                        |71fc3ad5b1c36ad56bd5a5432315cdab| = Challenge
    //                                                                                                        |0086| = Key ID (134)
    //                                                                                                            |75c06302465bc6d5091a29957eebed35| = Challenge
    //                                                                                                                                            |c00a6ed8| = Trailer
    // TRUTH PDU                                                                                                                                        
    char* buffer_TRUTH_RESPONSE_h = "0880D03A0068197F0B000402E00084000000000000000000000001D8EAA795AFFAA0E951BB6CF0116192E16B1977D6723E92E01123CCEF548E2885008600000000000000000000000275C47F30CA26E64AF30C19EBFFE0B314849133E138AC65BC2806E520A90C96A8";
    //                         0880D03A0068 = Primary Header
    //                                    197F0B00 = PUS Header
    //                                           0402E0 = PDU Tag & Length
    //                                                0084 000000000000000000000001 D8EAA795AFFAA0E951BB6CF0116192E1 6B1977D6723E92E01123CCEF548E2885 =  #1: KID, IV, CHALLENGE, MAC
    //                                                0086 000000000000000000000002 75C47F30CA26E64AF30C19EBFFE0B314 849133E138AC65BC2806E520A90C96A8 =  #2: KID, IV, CHALLENGE, MAC
    uint8_t *buffer_nist_key_b, *buffer_VERIFY_b, *buffer_TRUTH_RESPONSE_b = NULL;
    int buffer_nist_key_len, buffer_VERIFY_len, buffer_TRUTH_RESPONSE_len = 0;

    // Setup Processed Frame For Decryption
    TC_t tc_nist_processed_frame = {0};

    // Expose/setup SAs for testing
    SecurityAssociation_t* test_association;

    // Deactivate SA 1
    sa_if->sa_get_from_spi(1, &test_association);
    test_association->sa_state = SA_NONE;

    // Activate SA 0
    sa_if->sa_get_from_spi(0, &test_association);
    test_association->sa_state = SA_OPERATIONAL;
    test_association->ecs_len = 0;
    test_association->ecs = CRYPTO_CIPHER_NONE;
    test_association->est = 0;
    test_association->ast = 0;
    test_association->shsnf_len = 2;
    test_association->arsn_len = 2;
    test_association->arsnw_len = 1;
    test_association->arsnw = 5;
    // Insert key into keyring of SA 9
    hex_conversion(buffer_nist_key_h, (char**) &buffer_nist_key_b, &buffer_nist_key_len);
    ekp = key_if->get_key(test_association->ekid);
    memcpy(ekp->value, buffer_nist_key_b, buffer_nist_key_len);

    // Convert frames that will be processed
    hex_conversion(buffer_VERIFY_h, (char**) &buffer_VERIFY_b, &buffer_VERIFY_len);

    hex_conversion(buffer_TRUTH_RESPONSE_h, (char**) &buffer_TRUTH_RESPONSE_b, &buffer_TRUTH_RESPONSE_len);

    // Expect success on next valid IV && ARSN
    printf(KGRN "Checking next valid IV && valid ARSN... should be able to receive it... \n" RESET);
    status = Crypto_TC_ProcessSecurity(buffer_VERIFY_b, &buffer_VERIFY_len, &tc_nist_processed_frame);
    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);
    printf("\n");

    // Check reply values
    uint16_t reply_length = 0;
    uint8_t sdls_ep_reply_local[1024];
    status = Crypto_Get_Sdls_Ep_Reply(&sdls_ep_reply_local[0], &reply_length);
    // Expect success
    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);
    
    // Print local copy
    printf("SDLS Reply LOCAL: 0x");
    for (int i =0; i < reply_length; i++)
    {
        printf("%02X", sdls_ep_reply_local[i]);
    }
    printf("\n\n");
    // Print Global copy for sanity check
    Crypto_Print_Sdls_Ep_Reply();

    // Let's compare everything. All three should match
    for (int i = 0; i < reply_length; i++)
    {
        // printf(" %02X \t %02X\n", buffer_TRUTH_RESPONSE_b[i], sdls_e/p_reply_local[i]);
        ASSERT_EQ(buffer_TRUTH_RESPONSE_b[i], sdls_ep_reply_local[i]);
        ASSERT_EQ(buffer_TRUTH_RESPONSE_b[i], sdls_ep_reply[i]);
        // printf("%02X", sdls_ep_reply[i]);
    }

    Crypto_Shutdown();
    free(buffer_nist_key_b);
    free(buffer_VERIFY_b);

}

UTEST(EP_KEY_VALIDATION, VERIFY_APPLY_132_134)
{
    remove("sa_save_file.bin");
    // Setup & Initialize CryptoLib
    Crypto_Config_CryptoLib(KEY_TYPE_INTERNAL, MC_TYPE_INTERNAL, SA_TYPE_INMEMORY, CRYPTOGRAPHY_TYPE_LIBGCRYPT, 
                            IV_INTERNAL, CRYPTO_TC_CREATE_FECF_TRUE, TC_PROCESS_SDLS_PDUS_TRUE, TC_HAS_PUS_HDR,
                            TC_IGNORE_SA_STATE_FALSE, TC_IGNORE_ANTI_REPLAY_TRUE, TC_UNIQUE_SA_PER_MAP_ID_FALSE,
                            TC_CHECK_FECF_FALSE, 0x3F, SA_INCREMENT_NONTRANSMITTED_IV_TRUE);
    // Crypto_Config_Add_Gvcid_Managed_Parameter(0, 0x0003, 0, TC_HAS_FECF, TC_HAS_SEGMENT_HDRS, TC_OCF_NA, 1024, AOS_FHEC_NA, AOS_IZ_NA, 0);
    GvcidManagedParameters_t TC_0_Managed_Parameters = {0, 0x0003, 0, TC_NO_FECF, AOS_FHEC_NA, AOS_IZ_NA, 0, TC_HAS_SEGMENT_HDRS, 1024, TC_OCF_NA, 1};
    Crypto_Config_Add_Gvcid_Managed_Parameters(TC_0_Managed_Parameters);
    
    // Crypto_Config_Add_Gvcid_Managed_Parameter(0, 0x0003, 1, TC_HAS_FECF, TC_HAS_SEGMENT_HDRS, TC_OCF_NA, 1024, AOS_FHEC_NA, AOS_IZ_NA, 0);
    GvcidManagedParameters_t TC_1_Managed_Parameters = {0, 0x0003, 1, TC_NO_FECF, AOS_FHEC_NA, AOS_IZ_NA, 0, TC_HAS_SEGMENT_HDRS, 1024, TC_OCF_NA, 1};
    Crypto_Config_Add_Gvcid_Managed_Parameters(TC_1_Managed_Parameters);
    
    int status = CRYPTO_LIB_SUCCESS;
    status = Crypto_Init();
    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);
    SaInterface sa_if = get_sa_interface_inmemory();
    crypto_key_t* ekp = NULL;


    // NOTE: Added Transfer Frame header to the plaintext
    char* buffer_nist_key_h = "000102030405060708090A0B0C0D0E0F000102030405060708090A0B0C0D0E0F";
    char* buffer_nist_iv_h = "000000000000000000000001"; // The last valid IV that was seen by the SA
    char* buffer_RESPONSE_h = "0880d03a006584005c0000000000000000000000000001d8eaa795affaa0e951bb6cf0116192e16b1977d6723e92e01123ccef548e28857e0000000000000000000000000275c47f30b4ee759ffa9607237db92121d65d3ed1fe943fcf04535efd0e0fc793";

    uint8_t *buffer_nist_iv_b, *buffer_nist_key_b, *buffer_RESPONSE_b = NULL;
    int buffer_nist_iv_len, buffer_nist_key_len, buffer_RESPONSE_len  = 0;

    // Expose/setup SAs for testing
    SecurityAssociation_t* test_association;

    // Deactivate SA 1
    sa_if->sa_get_from_spi(1, &test_association);
    test_association->sa_state = SA_NONE;

    // Activate SA 9
    sa_if->sa_get_from_spi(0, &test_association);
    test_association->sa_state = SA_OPERATIONAL;
    test_association->ecs_len = 0;
    test_association->ecs = CRYPTO_CIPHER_NONE;
    test_association->est = 0;
    test_association->ast = 0;
    test_association->iv_len = 12;
    //test_association->shivf_len = 12;
    test_association->stmacf_len = 16;
    test_association->shsnf_len = 2;
    test_association->arsn_len = 2;
    test_association->arsnw_len = 1;
    test_association->arsnw = 5;
    // Insert key into keyring of SA 9
    hex_conversion(buffer_nist_key_h, (char**) &buffer_nist_key_b, &buffer_nist_key_len);
    ekp = key_if->get_key(test_association->ekid);
    memcpy(ekp->value, buffer_nist_key_b, buffer_nist_key_len);

    // Convert frames that will be processed
    // Convert/Set input IV
    hex_conversion(buffer_nist_iv_h, (char**) &buffer_nist_iv_b, &buffer_nist_iv_len);
    memcpy(test_association->iv, buffer_nist_iv_b, buffer_nist_iv_len);
    hex_conversion(buffer_RESPONSE_h, (char**) &buffer_RESPONSE_b, &buffer_RESPONSE_len);

    // Expect success on next valid IV && ARSN
    printf(KGRN "Checking  next valid IV && valid ARSN... should be able to receive it... \n" RESET);

    uint8_t* ptr_enc_frame = NULL;
    uint16_t enc_frame_len = 0;
    status = Crypto_TC_ApplySecurity((uint8_t*)buffer_RESPONSE_b, buffer_RESPONSE_len, &ptr_enc_frame, &enc_frame_len);

    printf("\n");
    Crypto_Shutdown();
    free(buffer_nist_iv_b);
    free(buffer_nist_key_b);
    free(buffer_RESPONSE_b);

}

    /*
    
    
    Swap to TM

    
     */








UTEST(EP_KEY_VALIDATION, VERIFY_RESPONSE_132_134)
{
    remove("sa_save_file.bin");
    // Setup & Initialize CryptoLib
    Crypto_Config_CryptoLib(KEY_TYPE_INTERNAL, MC_TYPE_INTERNAL, SA_TYPE_INMEMORY, CRYPTOGRAPHY_TYPE_LIBGCRYPT, 
                            IV_INTERNAL, CRYPTO_TM_CREATE_FECF_TRUE, TC_PROCESS_SDLS_PDUS_TRUE, TC_NO_PUS_HDR,
                            TC_IGNORE_SA_STATE_TRUE, TC_IGNORE_ANTI_REPLAY_TRUE, TC_UNIQUE_SA_PER_MAP_ID_FALSE,
                            TM_CHECK_FECF_FALSE, 0x3F, SA_INCREMENT_NONTRANSMITTED_IV_TRUE);
    // Crypto_Config_Add_Gvcid_Managed_Parameter(0, 0x0003, 0, TC_HAS_FECF, TC_HAS_SEGMENT_HDRS, TC_OCF_NA, 1024, AOS_FHEC_NA, AOS_IZ_NA, 0);
    GvcidManagedParameters_t TM_0_Managed_Parameters = {0, 0x0003, 0, TM_NO_FECF, AOS_FHEC_NA, AOS_IZ_NA, 0, TM_SEGMENT_HDRS_NA, 1024, TM_NO_OCF, 1};
    Crypto_Config_Add_Gvcid_Managed_Parameters(TM_0_Managed_Parameters);
    
    // Crypto_Config_Add_Gvcid_Managed_Parameter(0, 0x0003, 1, TC_HAS_FECF, TC_HAS_SEGMENT_HDRS, TC_OCF_NA, 1024, AOS_FHEC_NA, AOS_IZ_NA, 0);
    GvcidManagedParameters_t TM_1_Managed_Parameters = {0, 0x0003, 1, TM_NO_FECF, AOS_FHEC_NA, AOS_IZ_NA, 0, TM_SEGMENT_HDRS_NA, 1024, TM_NO_OCF, 1};
    Crypto_Config_Add_Gvcid_Managed_Parameters(TM_1_Managed_Parameters);
    
    int status = CRYPTO_LIB_SUCCESS;
    printf("Fail Before INIT\n");
    status = Crypto_Init();
    printf("Fail After INIT\n");
    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);
    sa_if = get_sa_interface_inmemory();
    crypto_key_t* ekp = NULL;

    char* buffer_nist_key_h = "000102030405060708090A0B0C0D0E0F000102030405060708090A0B0C0D0E0F";
    char* buffer_nist_iv_h = "000000000000000000000001"; // The last valid IV that was seen by the SA
    char* buffer_RESPONSE_h = "0031020218000000001a0080ffff006084005c0084000000000000000000000001d8eaa795affaa0e951bb6cf0116192e16b1977d6723e92e01123ccef548e2885008600000000000000000000000275c47f30ca26e64af30c19ebffe0b314849133e138ac65bc2806e520a90c96a8216607ff000000390000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000007ff000000390000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000007ff000000390000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000007ff000000390000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000007ff000000390000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000007ff000000390000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000007ff000000390000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000007ff000000390000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000007ff000000390000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000007ff000000390000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000007ff000000390000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000007ff000000390000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000007ff000000390000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000007ff000000390000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000007ff000000390000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000007ff0000003900000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001000000f844";
    //char* buffer_RESPONSE_h = "02C0000018000880d03a006584005c0000000000000000000000000001d8eaa795affaa0e951bb6cf0116192e16b1977d6723e92e01123ccef548e28857e0000000000000000000000000275c47f30b4ee759ffa9607237db92121d65d3ed1fe943fcf04535efd0e0fc793";
    //                        |02C000001801| = Pri Header
    //                                    |0880d03a| = Sec Header
    //                                            |0065| = Entire Pkt Length
    //                                                |84| = PDU Tag
    //                                                  |005c| = PDU length
    //                                                      |0000| = kid ???
    //                                                          |000000000000000000000001| = IV
    //                                                                                  |d8eaa795affaa0e951bb6cf0116192e1| = Enc Chall
    //                                                                                                                  |6b1977d6723e92e01123ccef548e2885| = Chall MAC
    //                                                                                                                                                  |7e00| = kid ???
    //                                                                                                                                                      |000000000000000000000002| = IV
    //                                                                                                                                                                              |75c47f30b4ee759ffa9607237db92121| = ENC Chall
    //                                                                                                                                                                                                              |d65d3ed1fe943fcf04535efd0e0fc793| = Chall MAC
    uint8_t *buffer_nist_iv_b, *buffer_nist_key_b, *buffer_RESPONSE_b = NULL;
    int buffer_nist_iv_len, buffer_nist_key_len, buffer_RESPONSE_len = 0;

    uint8_t* tm_nist_processed_frame;

    SecurityAssociation_t* test_association;

    // Deactivate SA 1
    sa_if->sa_get_from_spi(1, &test_association);
    test_association->sa_state = SA_NONE;

    // Activate SA 0
    sa_if->sa_get_from_spi(0, &test_association);
    test_association->sa_state = SA_OPERATIONAL;
    test_association->ecs_len = 0;
    test_association->ecs = CRYPTO_CIPHER_NONE;
    test_association->est = 0;
    test_association->ast = 0;
    test_association->iv_len = 12;
    //test_association->shivf_len = 12;
    test_association->stmacf_len = 16;
    test_association->shsnf_len = 2;
    test_association->arsn_len = 2;
    test_association->arsnw_len = 1;
    test_association->arsnw = 5;
    // Insert key into keyring of SA 9
    hex_conversion(buffer_nist_key_h, (char**) &buffer_nist_key_b, &buffer_nist_key_len);
    ekp = key_if->get_key(test_association->ekid);
    memcpy(ekp->value, buffer_nist_key_b, buffer_nist_key_len);

    // Convert frames that will be processed
    hex_conversion(buffer_RESPONSE_h, (char**) &buffer_RESPONSE_b, &buffer_RESPONSE_len);
    // Convert/Set input IV
    hex_conversion(buffer_nist_iv_h, (char**) &buffer_nist_iv_b, &buffer_nist_iv_len);
    memcpy(test_association->iv, buffer_nist_iv_b, buffer_nist_iv_len);

    uint16_t length;
    
    status = Crypto_TM_ProcessSecurity((uint8_t*)buffer_RESPONSE_b, buffer_RESPONSE_len, &tm_nist_processed_frame, &length);

    printf("\n");
    Crypto_Shutdown();
    free(buffer_nist_iv_b);
    free(buffer_nist_key_b);
    free(buffer_RESPONSE_b);
}
UTEST_MAIN();