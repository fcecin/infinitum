// Copyright (c) 2009-2015 The Infinitum developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef INFINITUM_INFINITUM_H
#define INFINITUM_INFINITUM_H

#include "coins.h"
#include "primitives/transaction.h"

// How many blocks into a cycle?
// After every cycle, unspent transaction outputs become unspendable ("prune dust") if their 
//   value is below 2^n satoshis, where "n" is the "prune dust" vote result from the block
//   headers of that period. 
//
// At about 5 minutes per block, 52,560 blocks is about half a year.
//
static const int INFINITUM_CHAIN_CYCLE_BLOCKS = 52560;

// Unspent outputs become unspendable ("prune inactive") after they cross the 20th cycle
//   without being spent.
//
// With half-year cycles, 40 cycles is about 20 years.
//
static const int INFINITUM_TRANSACTION_EXPIRATION_CYCLES = 40;

// Get the cycle number for a block height.
int GetCycle(int nHeight);

// Count cycles between two block heights.
// nOutputHeight is the height of a past block containing the transaction that has the UTXO.
// nInputHeight is the height of a subsequent block that has a transaction that's trying to 
//   spend that UTXO.
int NumCyclesBetween(int nOutputHeight, int nInputHeight);

// Test cycles between two block heights against the transaction expiration cycle limit.
bool TooManyCyclesBetween(int nOutputHeight, int nInputHeight);

// Check for dust or inactivity pruned outputs that cannot be spent by any one of the 
//   inputs of a new transaction "tx" to be included in a block at "nHeight" considering 
//   the Chainstate database "view" of UTXOs.
bool IsSpendingPrunedInputs(const CCoinsViewCache &view, const CTransaction &tx, int nHeight, bool &fDustPruned);


#endif // INFINITUM_INFINITUM_H
