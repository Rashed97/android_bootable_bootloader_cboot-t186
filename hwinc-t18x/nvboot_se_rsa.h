/*
 * Copyright (c) 2006 - 2011 NVIDIA Corporation.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file nvboot_se_rsa.h
 *
 * Defines the parameters and data structure for SE's RSA engine
 *
 */

#ifndef INCLUDED_NVBOOT_SE_RSA_H
#define INCLUDED_NVBOOT_SE_RSA_H

#include "nvcommon.h"
#include "nvboot_config.h"
#include "nvboot_hash.h"
#include "nvboot_se_defs.h"

#if defined(__cplusplus)
extern "C"
{
#endif


/**
 * Defines SE RSA Key Slots
 */
typedef enum
{
    NvBootSeRsaKeySlot_1 = SE_RSA_KEY_PKT_KEY_SLOT_ONE,
    NvBootSeRsaKeySlot_2,
    NvBootSeRsaKeySlot_3,
    NvBootSeRsaKeySlot_4,

    // Specifies max number of SE RSA Key Slots
    NvBootSeRsaKeySlot_Num,

    NvBootSeRsaKeySlot_OEM_Key_Verify_Key_Slot = NvBootSeRsaKeySlot_1,
    NvBootSeRsaKeySlot_NV_Key_Verify_Key_Slot = NvBootSeRsaKeySlot_2,
} NvBootSeRsaKeySlot;

/**
 * Defines the RSA modulus length in bits and bytes used for PKC secure boot.
 */
enum {NVBOOT_SE_RSA_MODULUS_LENGTH_BITS = ARSE_RSA_MAX_MODULUS_SIZE};
enum {NVBOOT_SE_RSA_MODULUS_LENGTH_BYTES = NVBOOT_SE_RSA_MODULUS_LENGTH_BITS / 8};
enum {NVBOOT_SE_RSA_PUBLIC_KEY_LENGTH_BYTES = NVBOOT_SE_RSA_MODULUS_LENGTH_BYTES};

/**
 * Defines the RSA exponent  length in bits and bytes used for PKC secure boot.
 */
enum {NVBOOT_SE_RSA_EXPONENT_LENGTH_BITS = ARSE_RSA_MAX_EXPONENT_SIZE};
enum {NVBOOT_SE_RSA_EXPONENT_LENGTH_BYTES = NVBOOT_SE_RSA_EXPONENT_LENGTH_BITS / 8};

/**
 * Defines the RSA private key exponent size.
 */
enum {NVBOOT_SE_RSA_PRIVATE_KEY_EXPONENT_LENGTH_BYTES = NVBOOT_SE_RSA_EXPONENT_LENGTH_BYTES};

/**
 * Defines the RSA public key exponent size.
 */
enum {NVBOOT_SE_RSA_PUBLIC_KEY_EXPONENT_LENGTH_BYTES = NVBOOT_SE_RSA_EXPONENT_LENGTH_BYTES};

/**
 * Defines the size of an RSA-PSS signature in bits and bytes.
 */
enum {NVBOOT_SE_RSA_SIGNATURE_LENGTH_BITS = NVBOOT_SE_RSA_MODULUS_LENGTH_BITS};
enum {NVBOOT_SE_RSA_SIGNATURE_LENGTH_BYTES = NVBOOT_SE_RSA_SIGNATURE_LENGTH_BITS / 8};

/**
 * Defines the total size of an RSA key (taking into account both the
 * modulus and exponent).
 */
enum {NVBOOT_SE_RSA_KEY_SIZE_BYTES = NVBOOT_SE_RSA_PUBLIC_KEY_EXPONENT_LENGTH_BYTES +
                                NVBOOT_SE_RSA_MODULUS_LENGTH_BYTES};

/**
 * Defines the length of the RSASSA-PSS salt length (sLen) to use for RSASSA-PSS
 *  signature verifications
 */
enum {NVBOOT_SE_RSA_PSS_SALT_LENGTH_BITS = ARSE_SHA256_HASH_SIZE};
enum {NVBOOT_SE_RSA_PSS_SALT_LENGTH_BYTES = NVBOOT_SE_RSA_PSS_SALT_LENGTH_BITS / 8};
/**
 * Defines the Public Key Exponent used by the Boot ROM.
 */
enum {NVBOOT_SE_RSA_PUBLIC_KEY_EXPONENT = 0x00010001};

/*
 *  Defines the storage for an 2048-bit RSA key to be used by the SE.
 *  The SE expects Key inputs to be word aligned.
 */
typedef struct NvBootSeRsaKey2048Rec
{
    // The modulus size is 2048-bits.
    NvU32 Modulus[2048 / 8 / 4];
    // The exponent size is 2048-bits.
    NvU32 Exponent[2048 / 8 / 4];
} NvBootSeRsaKey2048;

/*
 *  Defines the storage for the RSA public key's modulus
 *  in the BCT
 */
typedef struct NvBootRsaKeyModulusRec
{
    /// The modulus size is 2048-bits.
	NvU32               Modulus[NVBOOT_SE_RSA_MODULUS_LENGTH_BITS / 8 / 4];
} NvBootRsaKeyModulus;

/*
 *  Defines the storage for an RSA exponent.
 */
typedef struct NvBootRsaKeyExponentRec
{
    /// The exponent size is 2048-bits.
	NvU32               Exponent[NVBOOT_SE_RSA_EXPONENT_LENGTH_BITS / 8 / 4];
} NvBootRsaKeyExponent;

typedef struct NvBootRsaPssSigRec
{
    /// The RSA-PSS signature length is equal to the
    /// length in octets of the RSA modulus.
    /// In our case, it's 2048-bits.
	NvU32               Signature[NVBOOT_SE_RSA_MODULUS_LENGTH_BITS / 8 / 4];
} NvBootRsaPssSig;

/**
 * Defines storage for the AES-CMAC signature and RSASSA-PSS signature.
 * Starting in T124, this is no longer a union.
 * This is to facilitate the use of a single image to flash to the device
 * that works on unfused (NvProduction) and production fused (ODM Secure PKC
 * only) chips. By separating the AES-CMAC and RSASSA-PSS signatures into
 * separate address space, a single image can be signed with both signature
 * types at the same time. Note, this method will not work with ODM Secure SBK
 * mode since the BCT and BL must be encrypted.
 * This is a customer request to reduce the logistical complexity
 * of maintaining multiple flash images as well the need to flash the device
 * multiple times.
 * See http://nvbugs/1217986.
 */
typedef struct NvBootObjectSignatureRec
{
    /// Specifies the AES-CMAC signature for the rest of the BCT structure if symmetric key
    /// encryption secure boot scheme is used.
    NvBootHash          CryptoHash;

    /// Specifies the RSASSA-PSS signature for the rest of the BCT structure if public
    /// key cryptography secure boot scheme is used.
    NvBootRsaPssSig     RsaPssSig;

} NvBootObjectSignature;

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVBOOT_SE_RSA_H
