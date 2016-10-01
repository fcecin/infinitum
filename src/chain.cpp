// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "infinitum.h"
#include "chain.h"

#include <boost/foreach.hpp>

using namespace std;

/**
 * CChain implementation
 */
CAmount CChain::GetMinSpendableOutputValue(uint64_t nOutputBlockHeight, uint64_t nInputBlockHeight) const {
    // Example: Start 0, End 2. The transaction with the UTXO is somewhere inside Cycle 0,
    //  and the transaction that is trying to spend it is somewhere inside Cycle 2.
    // We have to check then what the dust vote result was at the end of Cycle 0 (vmin[0]),
    //  and then what the result was at the end of Cycle 1 (vmin[1]), which are the cycle
    //  borders that are crossed between the middle of Cycle 0 and the middle of Cycle 2.

    int nStartCycle = GetCycle(nOutputBlockHeight);
    int nEndCycle = GetCycle(nInputBlockHeight);

    CAmount nMaximumFound = 0; // The highest prune threshold crossed is the one that we apply.
    for (int nCycle = nStartCycle; nCycle < nEndCycle; ++nCycle)
	nMaximumFound = std::max(nMaximumFound, vMinSpendableOutputValues[nCycle]);
    return nMaximumFound;
}

// Infinitum:: new CChain::SetTip that updates vMinSpendableOutputValues as well as the vChain
void CChain::SetTip(CBlockIndex *pindex) {
    if (pindex == NULL) {
        vChain.clear();
        vMinSpendableOutputValues.clear(); // Infinitum:: update vMinSpendableOutputValues
        return;
    }

    // Height ints:
    // -1 = no genesis block
    //  0 = just the genesis block (block at height #0)
    int nNewChainHeight = pindex->nHeight;
    int nFirstCommonNodeHeight = nNewChainHeight;

    vChain.resize(nNewChainHeight + 1);
    while (pindex && vChain[pindex->nHeight] != pindex) {
        vChain[pindex->nHeight] = pindex;
        pindex = pindex->pprev;
	--nFirstCommonNodeHeight;
    }

    // From first common height to new height, the vminspendableoutputvalues need recomputing.
    // The interval touched by the first common height + 1 (changed block) is the first interval
    //   to recompute.
    int nStartCycle = GetCycle(nFirstCommonNodeHeight + 1);
    if (nStartCycle < 0)
        nStartCycle = 0; // The genesis block ("Cycle #-1") doesn't matter, we never "start" at it.

    // The end cycle is the first *whole* cycle given by the new chain height.
    // If the end cycle is less than the start cycle, then the spendable output values array
    //   is empty.
    // End cycle can be -1 if there is no end cycle, which resizes the vminspend vector to 0.
    int nEndCycle = GetCycle(nNewChainHeight);
    if (nNewChainHeight % INFINITUM_CHAIN_CYCLE_BLOCKS != 0)
        --nEndCycle; // New height block doesn't land squarely at the end of its cycle:
                     //   that cycle isn't whole so it doesn't apply any dust pruning yet.

    // Cycles past the end are deleted
    if (nEndCycle < -1)
        nEndCycle = -1; // So we don't resize(-1). It happens when pushing the genesis in.
    vMinSpendableOutputValues.resize(nEndCycle + 1); // Cycle #0 is the first one, so +1 to fit it in

    // Cycles from start to end are recomputed/re-tallied.
    // nEndCycle is less than nStartCycle for most of the calls to this method, in which
    //   case this loop doesn't run (i.e. nothing to update)
    for (int nCycle = nStartCycle; nCycle <= nEndCycle; ++nCycle) {

	// A simple safeguard:
	// For the first 8 cycles, the vote is 0 (no dust pruning).
	if (nCycle < 8) {
	  vMinSpendableOutputValues[nCycle] = 1;
	  continue;
	}

	// Element 0 = ndustvote 0's count; element 1 = ndustvote 1's count etc.
	std::vector<int> vVoteCounts;
	vVoteCounts.resize(256);

	int64_t nFirstHeight = 1 + (nCycle * INFINITUM_CHAIN_CYCLE_BLOCKS);
	int64_t nLastHeight = (nCycle + 1) * INFINITUM_CHAIN_CYCLE_BLOCKS;

	// Tally votes
	for (int i = nFirstHeight; i <= nLastHeight; ++i) {

	    // A simple safeguard:
	    // If the network difficulty is below 268,435,456 (roughly 1-2 petahashes
	    //   network speed) for ANY block in a cycle, then that cycle's vote for
	    //   the dust threshold is automatically 0 (no dust pruning).
	    if (vChain[i]->nBits > 0x190FFFFF) {
	      vVoteCounts[0] = INFINITUM_CHAIN_CYCLE_BLOCKS;
	      break;
	    }

	    int nVote = (vChain[i]->nVersion >> 8) & 0xFF;
	    ++vVoteCounts[nVote];
	}

	// Compute the winning vote
	CAmount nWinningVote = 0;

	// Will discard the lowest 5% votes and keep whatever the next vote is,
	//   so the dust value is the lowest common denominator among the 95% of miners
	//   that are on the side of wanting the highest dust value.
	int nDiscardBudget = INFINITUM_CHAIN_CYCLE_BLOCKS / 20;
	int nIndex = 0;
	BOOST_FOREACH(int nVoteCount, vVoteCounts) {
	  nDiscardBudget -= nVoteCount;
	  if (nDiscardBudget < 0) {
            // Votes for "0" say dust is 2^0, votes for "1" say dust is 2^1, etc.
	    nWinningVote = 1 << nIndex;
	    break;
	  }
	  ++nIndex;
	}

	// Update it
	vMinSpendableOutputValues[nCycle] = nWinningVote;
    }
}

CBlockLocator CChain::GetLocator(const CBlockIndex *pindex) const {
    int nStep = 1;
    std::vector<uint256> vHave;
    vHave.reserve(32);

    if (!pindex)
        pindex = Tip();
    while (pindex) {
        vHave.push_back(pindex->GetBlockHash());
        // Stop when we have added the genesis block.
        if (pindex->nHeight == 0)
            break;
        // Exponentially larger steps back, plus the genesis block.
        int nHeight = std::max(pindex->nHeight - nStep, 0);
        if (Contains(pindex)) {
            // Use O(1) CChain index if possible.
            pindex = (*this)[nHeight];
        } else {
            // Otherwise, use O(log n) skiplist.
            pindex = pindex->GetAncestor(nHeight);
        }
        if (vHave.size() > 10)
            nStep *= 2;
    }

    return CBlockLocator(vHave);
}

const CBlockIndex *CChain::FindFork(const CBlockIndex *pindex) const {
    if (pindex == NULL) {
        return NULL;
    }
    if (pindex->nHeight > Height())
        pindex = pindex->GetAncestor(Height());
    while (pindex && !Contains(pindex))
        pindex = pindex->pprev;
    return pindex;
}

/** Turn the lowest '1' bit in the binary representation of a number into a '0'. */
int static inline InvertLowestOne(int n) { return n & (n - 1); }

/** Compute what height to jump back to with the CBlockIndex::pskip pointer. */
int static inline GetSkipHeight(int height) {
    if (height < 2)
        return 0;

    // Determine which height to jump back to. Any number strictly lower than height is acceptable,
    // but the following expression seems to perform well in simulations (max 110 steps to go back
    // up to 2**18 blocks).
    return (height & 1) ? InvertLowestOne(InvertLowestOne(height - 1)) + 1 : InvertLowestOne(height);
}

CBlockIndex* CBlockIndex::GetAncestor(int height)
{
    if (height > nHeight || height < 0)
        return NULL;

    CBlockIndex* pindexWalk = this;
    int heightWalk = nHeight;
    while (heightWalk > height) {
        int heightSkip = GetSkipHeight(heightWalk);
        int heightSkipPrev = GetSkipHeight(heightWalk - 1);
        if (pindexWalk->pskip != NULL &&
            (heightSkip == height ||
             (heightSkip > height && !(heightSkipPrev < heightSkip - 2 &&
                                       heightSkipPrev >= height)))) {
            // Only follow pskip if pprev->pskip isn't better than pskip->pprev.
            pindexWalk = pindexWalk->pskip;
            heightWalk = heightSkip;
        } else {
            assert(pindexWalk->pprev);
            pindexWalk = pindexWalk->pprev;
            heightWalk--;
        }
    }
    return pindexWalk;
}

const CBlockIndex* CBlockIndex::GetAncestor(int height) const
{
    return const_cast<CBlockIndex*>(this)->GetAncestor(height);
}

void CBlockIndex::BuildSkip()
{
    if (pprev)
        pskip = pprev->GetAncestor(GetSkipHeight(nHeight));
}

arith_uint256 GetBlockProof(const CBlockIndex& block)
{
    arith_uint256 bnTarget;
    bool fNegative;
    bool fOverflow;
    bnTarget.SetCompact(block.nBits, &fNegative, &fOverflow);
    if (fNegative || fOverflow || bnTarget == 0)
        return 0;
    // We need to compute 2**256 / (bnTarget+1), but we can't represent 2**256
    // as it's too large for a arith_uint256. However, as 2**256 is at least as large
    // as bnTarget+1, it is equal to ((2**256 - bnTarget - 1) / (bnTarget+1)) + 1,
    // or ~bnTarget / (nTarget+1) + 1.
    return (~bnTarget / (bnTarget + 1)) + 1;
}

int64_t GetBlockProofEquivalentTime(const CBlockIndex& to, const CBlockIndex& from, const CBlockIndex& tip, const Consensus::Params& params)
{
    arith_uint256 r;
    int sign = 1;
    if (to.nChainWork > from.nChainWork) {
        r = to.nChainWork - from.nChainWork;
    } else {
        r = from.nChainWork - to.nChainWork;
        sign = -1;
    }
    r = r * arith_uint256(params.nPowTargetSpacing) / GetBlockProof(tip);
    if (r.bits() > 63) {
        return sign * std::numeric_limits<int64_t>::max();
    }
    return sign * r.GetLow64();
}
