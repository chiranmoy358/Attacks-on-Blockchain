#include <bits/stdc++.h>
#include "data_structures.cpp"
#include "utility.cpp"

#define TXN_GEN 0
#define TXN_RCV 1
#define BLK_GEN 2
#define BLK_RCV 3
#define MINING_FEE 50 // coinbase
#define TXN_SIZE 8192 // 1KB in bits

using namespace std;

class P2P
{
private:
    int n, zeta, z0, z1, fastLinkSpeed = 100, slowLinkSpeed = 5, fastCpuCount = 0;
    double slowCPU, fastCPU, alpha, t_blk, t_txn;
    string mode;
    vector<Node *> miners;
    priority_queue<Event *, vector<Event *>, eventCompare> eventQueue;
    Attacker attacker;

public:
    P2P(int n, double alpha, int zeta, int z0, int z1, int t_blk, int t_txn, string mode) : n(n), alpha(alpha), zeta(zeta), z0(z0), z1(z1), t_blk(t_blk), t_txn(t_txn), mode(mode)
    {
        // initializing nodes with required attributes
        for (int i = 0; i < n; i++)
        {
            Node *newNode = new Node(i, i == 0 || randomUniform(0, 99) < z0, i == 0 || randomUniform(0, 99) < z1, n);
            miners.push_back(newNode);
            fastCpuCount += newNode->isFastCPU;
        }
        attacker = Attacker(miners[0]->blockchain->lastBlock);

        // calculate hasing power fraction
        slowCPU = (1.0 - alpha) / ((n - fastCpuCount - 1) + 10 * (fastCpuCount - 1));
        fastCPU = 10 * slowCPU;

        // generate connected graph
        generateGraph(miners, zeta, n);

        // populating event queue
        for (Node *node : miners)
        {
            // transaction generation event
            double timestamp = randomExponential(t_txn);
            TxnEvent *txnGen = new TxnEvent(TXN_GEN, node, timestamp, NULL);
            eventQueue.push(txnGen);

            // block generation event
            double hashPower = node->id == 0 ? alpha : (node->isFastCPU ? fastCPU : slowCPU);
            timestamp = randomExponential(t_blk / hashPower); // mean = t_blk / fraction of hashing power
            BlockEvent *blockGen = new BlockEvent(BLK_GEN, node, timestamp, createBlock(node));
            eventQueue.push(blockGen);
        }
    }

    // simulate for <eventCount> events
    void simulate(int eventCount)
    {
        int eventsProcessed = 0;
        cout << "\nSimulation Started in " << (mode == "self" ? "Selfish" : "Stubborn") << " mode" << endl;
        cout << "Progress:     ";
        while (!eventQueue.empty() && eventsProcessed++ < eventCount)
        {
            Event *e = eventQueue.top();

            // call handler function
            if (e->type == TXN_GEN)
                handleTxnGen((TxnEvent *)e);
            else if (e->type == TXN_RCV)
                handleTxnRcv((TxnEvent *)e);
            else if (e->type == BLK_GEN)
                handleBlkGen((BlockEvent *)e);
            else if (e->type == BLK_RCV)
                handleBlkRcv((BlockEvent *)e);
            else
                exit(1); // should be unreachable

            eventQueue.pop();
            delete e;

            if (eventsProcessed % 10000 == 0)
                cout << "\b\b\b\b" << setw(3) << eventsProcessed * 100 / eventCount << "%";
        }

        cout << "\rSimulation Summary:  " << endl;

        // Overall Summary
        cout << "\nFast CPU Miners: " << fastCpuCount << endl;
        cout << "Slow CPU Hashing Power: " << slowCPU << endl;
        cout << "Fast CPU Hashing Power: " << fastCPU << endl;
        cout << "Total Hashing Power: " << alpha + (fastCpuCount - 1) * fastCPU + (n - fastCpuCount - 1) * slowCPU << endl;

        cout << "\nAlpha: " << alpha << endl;
        cout << "Attacker's Connection Count: " << miners[0]->edges.size() << "(" << zeta << "%)" << endl;
        summary(miners, attacker, alpha * 100);

        // Simulation Output to Files
        string mode = "y";
        cout << "\nGenerate per-miner simulation data and blockchain tree (Note: graphviz must be installed) (y/n): ";
        cin >> mode;

        if (mode == "y" || mode == "Y")
        {
            system("mkdir Output 2> nul");
            cout << "Processing miner:   ";
            for (Node *node : miners)
            {
                cout << "\b\b\b" << setw(3) << node->id;
                minerSummary(node);
            }

            cout << "\rGenerating Attacker's Blockchain.";
            outputGraph(miners[0], attacker.stateMap);
            cout << "\rAttacker's Blockchain generated. " << endl;
            cout << "Generating a Honest Miner's Blockchain.";
            outputGraph(miners[1], attacker.stateMap);
            cout << "\rHonest Miner's Blockchain generated.   " << endl;
        }
    }

    // sends the block to all neighbours of a node
    void floodTxn(Node *node, Transaction *txn, double timestamp)
    {
        vector<Node *> &edges = miners[node->id]->edges;
        vector<double> &propDelay = miners[node->id]->propDelay;

        for (int i = 0; i < edges.size(); i++)
        {
            Node *destNode = edges[i];

            /* cij = linkSpeed (in bits/sec), pij (propagation delay) = pDelay, tDelay (transmission delay) = msg size/cij
               queueing delay (at source node) = dij = chosen from exponential dist with mean 96kbits/cij */
            double linkSpeed = (node->isFastLink && destNode->isFastLink ? fastLinkSpeed : slowLinkSpeed) * 1000000; // in bits
            double pDelay = propDelay[i], tDelay = TXN_SIZE / linkSpeed;                                             // in seconds
            double qDelay = randomExponential(96000 / linkSpeed);

            double newTimestamp = timestamp + qDelay + tDelay + pDelay;
            TxnEvent *newEvent = new TxnEvent(TXN_RCV, destNode, newTimestamp, txn);
            eventQueue.push(newEvent);
        }
    }

    // sends the block to all neighbours of a node
    void floodBlock(Node *node, Block *block, double timestamp)
    {
        vector<Node *> &edges = miners[node->id]->edges; // neighbours of the node
        vector<double> &propDelay = miners[node->id]->propDelay;

        for (int i = 0; i < edges.size(); i++)
        {
            Node *destNode = edges[i];

            double linkSpeed = (node->isFastLink && destNode->isFastLink ? fastLinkSpeed : slowLinkSpeed) * 1000000;
            double pDelay = propDelay[i], tDelay = (1 + block->transactions.size()) * TXN_SIZE / linkSpeed; // block size = (1 + # of txns) * 1024 KB
            double qDelay = randomExponential(96000 / linkSpeed);

            double newTimestamp = timestamp + qDelay + tDelay + pDelay;
            BlockEvent *newEvent = new BlockEvent(BLK_RCV, destNode, newTimestamp, block);
            eventQueue.push(newEvent);
        }
    }

    // handles TXN_GEN event, generate a transaction and sends to neighbours
    void handleTxnGen(TxnEvent *e)
    {
        Node *node = e->node;
        BlockChain *blockchain = node->blockchain;

        // low balance, defer transaction generation
        if (node->balanceLeft < 10)
        {
            eventQueue.push(new TxnEvent(TXN_GEN, node, e->timestamp + randomExponential(t_txn), NULL));
            return;
        }

        // randomly choose a peer
        int to_id;
        do
        {
            to_id = randomUniform(0, n - 1);
        } while (node->id == to_id);

        int amount = randomUniform(1, node->balanceLeft / 10); // randomly choose amount
        node->balanceLeft -= amount;

        Transaction *txn = new Transaction(get_uuid(), node->id, to_id, amount); // transaction created

        blockchain->pendingTxns.insert(txn->txnID);
        blockchain->allTxnRcvd.insert({txn->txnID, txn});
        floodTxn(node, txn, e->timestamp); // send to all neighbours

        // new event for next transaction generation
        eventQueue.push(new TxnEvent(TXN_GEN, node, e->timestamp + randomExponential(t_txn), NULL));
    }

    // handles TXN_RCV event, validates a transaction and sends to neighbours
    void handleTxnRcv(TxnEvent *e)
    {
        Node *node = e->node;
        Transaction *txn = e->txn;
        BlockChain *blockchain = node->blockchain;

        // already received - prevents looping
        if (blockchain->allTxnRcvd.count(txn->txnID))
            return;

        // validate the transaction
        if (blockchain->lastBlock->balance[txn->from_id] < txn->amount)
            return;

        blockchain->pendingTxns.insert(txn->txnID);
        blockchain->allTxnRcvd.insert({txn->txnID, txn});

        floodTxn(node, txn, e->timestamp); // send to all neighbours
    }

    // creates a block for a node
    Block *createBlock(Node *node)
    {
        // the attacker mines on its private chain if it has any
        Block *lastBlock = node->id == 0 ? attacker.lastBlock : node->blockchain->lastBlock;
        Block *newBlock = new Block(lastBlock->blockID, get_uuid(), node->id, lastBlock->chainLen + 1, MINING_FEE);
        newBlock->balance = vector<int>(lastBlock->balance);

        // add transactions to the block
        for (string txnID : node->blockchain->pendingTxns)
        {
            if (newBlock->transactions.size() >= 1000)
                break;
            Transaction *txn = node->blockchain->allTxnRcvd[txnID];

            if (txn->amount > newBlock->balance[txn->from_id])
                continue; // not enough balance for this transaction

            // pick this txn and update balance vector
            newBlock->transactions.push_back(txn);
            newBlock->balance[txn->from_id] -= txn->amount;
            newBlock->balance[txn->to_id] += txn->amount;
        }

        // adding mining fees (coinbase)
        newBlock->balance[node->id] += newBlock->coinbase.amount;

        return newBlock;
    }

    // handles BLK_GEn event
    void handleBlkGen(BlockEvent *e)
    {
        Node *node = e->node;
        Block *newBlock = e->block;

        if (node->id == 0)
        {
            mode == "self" ? selfishGeneratesBlock(e) : stubbornGeneratesBlock(e);
            return;
        }

        // longest chain updated, create new block
        if (node->blockchain->lastBlock->blockID != newBlock->prevBlockID)
            delete newBlock;
        else
            validateAndForward(e); // validate and forward the block, update the longest chain if needed
    }

    // handles BLK_RCV event
    void handleBlkRcv(BlockEvent *e)
    {
        Node *node = e->node;
        BlockChain *blockchain = e->node->blockchain;
        Block *block = e->block;

        if (blockchain->allBlocks.count(block->blockID)) // duplicate block received
            return;

        if (e->node->id == 0)
            mode == "self" ? selfishReceiveHonestBlock(e) : stubbornReceiveHonestBlock(e);
        else
        {
            bool chainExtended = validateAndForward(e); // validate and forward the block, update the longest chain if needed

            if (chainExtended)
            {
                double timestamp = e->timestamp + randomExponential(t_blk / (node->isFastCPU ? fastCPU : slowCPU));
                BlockEvent *blockGen = new BlockEvent(BLK_GEN, node, timestamp, createBlock(node));
                eventQueue.push(blockGen);
            }
        }
    }

    // validate the block received, if valid, add to the blockchain and forward to neighbouring peers
    bool validateAndForward(BlockEvent *e)
    {
        Node *node = e->node;
        Block *block = e->block;
        BlockChain *blockchain = node->blockchain;
        unordered_map<string, Block *> &allBlocks = blockchain->allBlocks;

        allBlocks.insert({block->blockID, block});
        node->blockArrivalTime.insert({block->blockID, e->timestamp}); // note the arrival time

        // Check if parent block is present
        if (allBlocks.find(block->prevBlockID) == allBlocks.end())
        {
            blockchain->parentLessBlocks.insert(block->blockID);
            return false;
        }
        if (!validateBlock(node, block))
            return false;

        // update longest chain if needed
        bool chainExtended = blockchain->lastBlock->chainLen < block->chainLen;
        if (chainExtended)
            updateLongestChain(blockchain, block);

        floodBlock(node, block, e->timestamp);     // send to neighbouring peers
        checkParentLessBlocks(node, e->timestamp); // check if this block was parent of some other block

        return chainExtended;
    }

    void updateLongestChain(BlockChain *blockchain, Block *block)
    {
        unordered_set<string> &pendingTxns = blockchain->pendingTxns;
        unordered_map<string, Block *> &allBlocks = blockchain->allBlocks;
        unordered_map<string, Transaction *> &allTxnRcvd = blockchain->allTxnRcvd;

        // Case 1: Chain Extension
        if (block->prevBlockID == blockchain->lastBlock->blockID)
        {
            // remove transactions from the pending set
            for (Transaction *txn : block->transactions)
            {
                if (pendingTxns.count(txn->txnID))
                    pendingTxns.erase(txn->txnID);
            }
        }
        else
        {
            // Case 2: new longest Chain: recompute pendingTxn set
            // pendingTxns will hold set of all transactions received
            for (auto &it : allTxnRcvd)
                pendingTxns.insert(it.first);

            // filter the set, by traversing the new longest chain
            Block *temp = block;
            while (temp->blockID != "0")
            {
                for (Transaction *txn : block->transactions)
                {
                    if (pendingTxns.count(txn->txnID))
                        pendingTxns.erase(txn->txnID);
                }

                if (!allBlocks.count(temp->prevBlockID))
                    break;

                temp = allBlocks[temp->prevBlockID];
            }
        }

        miners[blockchain->minerID]->balanceLeft = block->balance[blockchain->minerID];
        blockchain->lastBlock = block;
    }

    void checkParentLessBlocks(Node *node, double timestamp)
    {
        BlockChain *blockchain = node->blockchain;
        unordered_map<string, Block *> &allBlocks = blockchain->allBlocks;
        unordered_set<string> &invalidBlocks = blockchain->invalidBlocks;
        vector<string> parentFound;

        for (auto &blockID : blockchain->parentLessBlocks)
        {
            Block *block = allBlocks[blockID];
            if (invalidBlocks.find(block->prevBlockID) != invalidBlocks.end())
            {
                // parent block was invalid, thus this block is also invalid
                blockchain->invalidBlocks.insert(block->blockID);
                allBlocks.erase(block->blockID);
                parentFound.push_back(blockID);
                continue;
            }

            if (allBlocks.find(block->prevBlockID) == allBlocks.end()) // parent block still absent
                continue;

            parentFound.push_back(blockID);

            vector<int> prevBalance = vector<int>(allBlocks[block->prevBlockID]->balance);
            vector<int> &nextBalance = block->balance;

            // validating the block
            for (int i = 0; i < prevBalance.size(); i++)
            {
                if (prevBalance[i] != nextBalance[i] || nextBalance[i] < 0)
                {
                    blockchain->invalidBlocks.insert(block->blockID);
                    allBlocks.erase(block->blockID);
                    continue; // invalid block
                }
            }

            // validation successful, update longest chain if needed
            if (blockchain->lastBlock->chainLen < block->chainLen)
                updateLongestChain(blockchain, block);

            floodBlock(node, block, timestamp);
        }

        // parent found for these blocks, remove from the set
        for (string &blockID : parentFound)
            blockchain->parentLessBlocks.erase(blockID);
    }

    void selfishGeneratesBlock(BlockEvent *e)
    {
        Node *node = e->node;
        Block *block = e->block;
        BlockChain *blockchain = node->blockchain;
        unordered_map<string, Block *> &allBlocks = blockchain->allBlocks;
        unordered_set<string> &pendingTxns = blockchain->pendingTxns;

        // the honest chain was extended, new BLK_GEN will be created when the honest block was received
        if (attacker.lastBlock->blockID != block->prevBlockID)
        {
            delete block;
            return;
        }
        if (!validateBlock(node, block))
            return;

        attacker.blocksGenerated++;
        allBlocks.insert({block->blockID, block});
        node->blockArrivalTime.insert({block->blockID, e->timestamp}); // note the arrival time

        if (attacker.state == -1)
        {
            attacker.state = 0;
            floodBlock(node, block, e->timestamp);
        }
        else
        {
            attacker.state++;
            attacker.privateChain.push(block);
        }

        // attacker continues mining on its private block, update the txnSet and the balanceLeft
        attacker.lastBlock = block;
        attacker.stateMap.insert({block->blockID, attacker.state}); // resulting state
        node->balanceLeft = block->balance[0];

        for (Transaction *txn : block->transactions)
            pendingTxns.erase(txn->txnID);

        // new block generation event
        double timestamp = e->timestamp + randomExponential(t_blk / alpha);
        BlockEvent *blockGen = new BlockEvent(BLK_GEN, node, timestamp, createBlock(node));
        eventQueue.push(blockGen);
    }

    void selfishReceiveHonestBlock(BlockEvent *e)
    {
        Node *node = e->node;
        Block *block = e->block;
        BlockChain *blockchain = node->blockchain;
        unordered_map<string, Block *> &allBlocks = blockchain->allBlocks;
        unordered_map<string, Transaction *> &allTxnRcvd = blockchain->allTxnRcvd;
        unordered_set<string> &pendingTxns = blockchain->pendingTxns;

        allBlocks.insert({block->blockID, block});
        node->blockArrivalTime.insert({block->blockID, e->timestamp}); // note the arrival time

        // forks in honest chain is ignored
        if (blockchain->lastBlock->chainLen >= block->chainLen)
            return;
        if (!validateBlock(node, block))
            return;

        // else, the Longest honest chain is extended
        blockchain->lastBlock = block;

        if (attacker.state == -1 || attacker.state == 0)
        {
            attacker.state = 0;

            // -------- update balanceLeft and txnSet as per the honest block --------
            attacker.lastBlock = block; // attacker starts mining on this honest block
            node->balanceLeft = block->balance[0];

            for (auto &it : allTxnRcvd) // set of all TXNs
                pendingTxns.insert(it.first);

            // now traverse backward and filter the TXN set
            Block *temp = block;
            while (temp->blockID != "0")
            {
                for (Transaction *txn : block->transactions)
                {
                    if (pendingTxns.count(txn->txnID))
                        pendingTxns.erase(txn->txnID);
                }

                temp = allBlocks[temp->prevBlockID];
            }
            //-------------------------------------------------------------------------

            // new BLK_GEN event, i.e., attacker starts mining on this block
            double timestamp = e->timestamp + randomExponential(t_blk / alpha);
            BlockEvent *blockGen = new BlockEvent(BLK_GEN, node, timestamp, createBlock(node));
            eventQueue.push(blockGen);
        }
        else if (attacker.state == 1 || attacker.state == 2)
        {
            attacker.state -= 2;

            // release all the hidden blocks
            while (!attacker.privateChain.empty())
            {
                Block *privateBlock = attacker.privateChain.front();
                attacker.privateChain.pop();
                floodBlock(node, privateBlock, e->timestamp);
            }
        }
        else
        {
            // release one block, attacker continues mining on its private block
            attacker.state--;

            Block *privateBlock = attacker.privateChain.front();
            attacker.privateChain.pop();
            floodBlock(node, privateBlock, e->timestamp);
        }

        attacker.stateMap.insert({block->blockID, attacker.state}); // store resulting state
    }

    void stubbornGeneratesBlock(BlockEvent *e)
    {
        Node *node = e->node;
        Block *block = e->block;
        BlockChain *blockchain = node->blockchain;
        unordered_map<string, Block *> &allBlocks = blockchain->allBlocks;
        unordered_set<string> &pendingTxns = blockchain->pendingTxns;

        // the honest chain was extended, new BLK_GEN will be created when the honest block was received
        if (attacker.lastBlock->blockID != block->prevBlockID)
        {
            delete block;
            return;
        }
        if (!validateBlock(node, block))
            return;

        attacker.blocksGenerated++;
        allBlocks.insert({block->blockID, block});
        node->blockArrivalTime.insert({block->blockID, e->timestamp}); // note the arrival time

        attacker.state = attacker.state == -1 ? 1 : attacker.state + 1;
        attacker.privateChain.push(block);

        // attacker continues mining on its private block
        attacker.lastBlock = block;
        attacker.stateMap.insert({block->blockID, attacker.state}); // resulting state
        node->balanceLeft = block->balance[0];

        for (Transaction *txn : block->transactions)
            pendingTxns.erase(txn->txnID);

        // new block generation event
        double timestamp = e->timestamp + randomExponential(t_blk / alpha);
        BlockEvent *blockGen = new BlockEvent(BLK_GEN, node, timestamp, createBlock(node));
        eventQueue.push(blockGen);
    }

    void stubbornReceiveHonestBlock(BlockEvent *e)
    {
        Node *node = e->node;
        Block *block = e->block;
        BlockChain *blockchain = node->blockchain;
        unordered_map<string, Block *> &allBlocks = blockchain->allBlocks;
        unordered_map<string, Transaction *> &allTxnRcvd = blockchain->allTxnRcvd;
        unordered_set<string> &pendingTxns = blockchain->pendingTxns;

        allBlocks.insert({block->blockID, block});
        node->blockArrivalTime.insert({block->blockID, e->timestamp}); // note the arrival time

        // forks in honest chain is ignored
        if (blockchain->lastBlock->chainLen >= block->chainLen)
            return;
        if (!validateBlock(node, block))
            return;

        // else, the Longest honest chain is extended
        blockchain->lastBlock = block;

        if (attacker.state == -1 || attacker.state == 0)
        {
            attacker.state = 0;

            // -------- update balanceLeft and txnSet as per the honest block --------
            attacker.lastBlock = block; // attacker starts mining on this honest block
            node->balanceLeft = block->balance[0];

            for (auto &it : allTxnRcvd) // set of all TXNs
                pendingTxns.insert(it.first);

            // now traverse backward and filter the TXN set
            Block *temp = block;
            while (temp->blockID != "0")
            {
                for (Transaction *txn : block->transactions)
                {
                    if (pendingTxns.count(txn->txnID))
                        pendingTxns.erase(txn->txnID);
                }

                temp = allBlocks[temp->prevBlockID];
            }
            //-------------------------------------------------------------------------

            // new BLK_GEN event, i.e., attacker starts mining on this block
            double timestamp = e->timestamp + randomExponential(t_blk / alpha);
            BlockEvent *blockGen = new BlockEvent(BLK_GEN, node, timestamp, createBlock(node));
            eventQueue.push(blockGen);
        }
        else if (attacker.state == 1)
        {
            // release the last private blocks, attacker now in competition state with honest miners
            // attacker continues mining on its private block
            attacker.state = -1;

            Block *privateBlock = attacker.privateChain.front();
            attacker.privateChain.pop();
            floodBlock(node, privateBlock, e->timestamp);
        }
        else
        {
            // release one block, attacker still have a lead
            // attacker continues mining on its private block
            attacker.state--;

            Block *privateBlock = attacker.privateChain.front();
            attacker.privateChain.pop();
            floodBlock(node, privateBlock, e->timestamp);
        }

        attacker.stateMap.insert({block->blockID, attacker.state}); // store resulting state
    }

    bool validateBlock(Node *node, Block *block)
    {
        BlockChain *blockchain = node->blockchain;
        unordered_map<string, Block *> &allBlocks = blockchain->allBlocks;
        unordered_map<string, Transaction *> &allTxnRcvd = blockchain->allTxnRcvd;

        vector<int> prevBalance = vector<int>(allBlocks[block->prevBlockID]->balance);
        vector<int> &nextBalance = block->balance;

        // simulating all transactions on the prev block balance vector
        prevBalance[block->minerID] += block->coinbase.amount; // adding coinbase txn (mining fees)
        for (Transaction *txn : block->transactions)
        {
            // new transaction
            if (allTxnRcvd.find(txn->txnID) == allTxnRcvd.end())
                allTxnRcvd.insert({txn->txnID, txn});

            prevBalance[txn->from_id] -= txn->amount;
            prevBalance[txn->to_id] += txn->amount;
        }

        // validating the block
        for (int i = 0; i < prevBalance.size(); i++)
        {
            if (prevBalance[i] != nextBalance[i] || nextBalance[i] < 0) // invalid block
            {
                blockchain->invalidBlocks.insert(block->blockID);
                allBlocks.erase(block->blockID);
                return false;
            }
        }

        return true;
    }
};

int main(int argc, char **argv)
{
    srand(time(NULL));

    if (argc < 5 || 5 < argc)
    {
        cout << "Invalid Parameter Count. Usage: ./main n alpha zeta mode" << endl;
        exit(1);
    }

    int n = stoi(argv[1]), alpha = stoi(argv[2]), zeta = stoi(argv[3]), z0 = 50, z1 = 50, eventCount = 750000;
    string mode = string(argv[4]);
    double t_blk = 600, t_txn = 1000000;

    P2P p2pSim(n, alpha / 100.0, zeta, z0, z1, t_blk, t_txn, mode);
    p2pSim.simulate(eventCount); // simulate <eventCount> events

    return 0;
}