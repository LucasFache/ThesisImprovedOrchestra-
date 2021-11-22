/*
 * Copyright (c) 2018, Texas Instruments Incorporated - http://www.ti.com/
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/**
 * \addtogroup cc13xx-cc26xx-cpu
 * @{
 *
 * \defgroup cc13xx-cc26xx-trng True Random Number Generator for CC13xx/CC26xx.
 * @{
 *
 * \file
 *        Header file of True Random Number Generator for CC13xx/CC26xx.
 * \author
 *        Edvard Pettersen <e.pettersen@ti.com>
 */
/*---------------------------------------------------------------------------*/
#ifndef TRNG_ARCH_H_
#define TRNG_ARCH_H_
/*---------------------------------------------------------------------------*/
#include <contiki.h>
/*---------------------------------------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>
/*---------------------------------------------------------------------------*/
#define TRNG_WAIT_FOREVER       (~(uint32_t)0)
/*---------------------------------------------------------------------------*/
/**
 * \brief              Generates a stream of entropy from which you can create
 *                     a true random number from. This is a blocking function
 *                     call with a specified timeout.
 * \param entropy_buf  Buffer to store a stream of entropy.
 * \param entropy_len  Length of the entropy buffer.
 * \param timeout_us   How long to wait until timing out the operation. A
 *                     timeout of TRNG_WAIT_FOREVER blocks forever.
 * \return             true if successful; else, false.
 */
bool trng_rand(uint8_t *entropy_buf, size_t entropy_len, uint32_t timeout_us);
/*---------------------------------------------------------------------------*/
#endif /* TRNG_ARCH_H_ */
/*---------------------------------------------------------------------------*/
/**
 * @}
 * @}
 */
